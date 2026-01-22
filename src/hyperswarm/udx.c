#include "udx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if HAS_SODIUM
#include <sodium.h>
#endif

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#endif

// Helper for time (ms)
uint32_t udx_get_time_ms() {
#ifdef _WIN32
    return GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

udx_socket_t* udx_create(int port) {
    udx_socket_t* udx = (udx_socket_t*)malloc(sizeof(udx_socket_t));
    if (!udx) return NULL;

    udx->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udx->socket_fd < 0) {
        free(udx);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(udx->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(udx->socket_fd);
#else
        close(udx->socket_fd);
#endif
        free(udx);
        return NULL;
    }

    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(udx->socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(udx->socket_fd, F_GETFL, 0);
    fcntl(udx->socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif

    udx->local_id = 12345; // Random ID in real impl
    udx->next_seq = 1;
    udx->last_ack = 0;
    memset(udx->pending, 0, sizeof(udx->pending));
    
    return udx;
}

void udx_destroy(udx_socket_t* udx) {
    if (udx) {
#ifdef _WIN32
        closesocket(udx->socket_fd);
#else
        close(udx->socket_fd);
#endif
        free(udx);
    }
}

int udx_send_to(udx_socket_t* udx, const char* ip, int port, const uint8_t* data, size_t len) {
    if (!udx) return -1;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dest.sin_addr);

    return sendto(udx->socket_fd, (const char*)data, (int)len, 0, (struct sockaddr*)&dest, sizeof(dest));
}

// Internal helper to send packet with specific header values
int udx_send_raw(udx_socket_t* udx, const char* ip, int port, uint8_t type, uint32_t seq, uint32_t ack, const uint8_t* data, size_t len) {
    if (!udx) return -1;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dest.sin_addr);

    // Prepare packet with header
    size_t packet_len = sizeof(udx_header_t) + len;
    uint8_t* packet = (uint8_t*)malloc(packet_len);
    if (!packet) return -1;
    
    udx_header_t* header = (udx_header_t*)packet;
    header->type = type;
    header->conn_id = udx->local_id;
    header->seq = seq;
    header->ack = ack;
    
    if (len > 0 && data) {
        memcpy(packet + sizeof(udx_header_t), data, len);
    }

    // Debug packet content
    // if (packet_len > sizeof(udx_header_t)) {
    //    printf("[UDX Raw] Packet Payload Head: %02x %02x\n", packet[sizeof(udx_header_t)], packet[sizeof(udx_header_t)+1]);
    // }
    
    int sent = sendto(udx->socket_fd, (const char*)packet, (int)packet_len, 0, (struct sockaddr*)&dest, sizeof(dest));
    free(packet);
    
    return sent;
}

int udx_send(udx_socket_t* udx, const char* ip, int port, uint8_t type, const uint8_t* data, size_t len, const uint8_t* key) {
    // Check for reliability requirement
    int reliable = (type == UDX_TYPE_DATA || type == UDX_TYPE_SYN || type == UDX_TYPE_FIN);
    int slot_idx = -1;

    if (reliable) {
        for (int i=0; i<MAX_PENDING_PACKETS; i++) {
            if (!udx->pending[i].active) {
                slot_idx = i;
                break;
            }
        }
        if (slot_idx == -1) {
            // No slots available for reliable packet
            return -1; 
        }
    }

    uint8_t final_data[1024];
    size_t final_len = len;
    
    // Copy data by default
    if (len > 0) {
        if (len > 1024) return -1;
        memcpy(final_data, data, len);
    }

    // Prepare Sequence Number (used as Nonce if encrypted)
    uint32_t seq = udx->next_seq++;

    #if HAS_SODIUM
    if (key && len > 0) {
        if (len + crypto_secretbox_MACBYTES > 1024) return -1;
        
        unsigned char n[crypto_secretbox_NONCEBYTES];
        memset(n, 0, sizeof(n));
        // Use 32-bit seq as nonce (padded with zeros)
        // Ensure receiver uses the same seq to decrypt
        memcpy(n, &seq, 4); 

        // Encrypt
        uint8_t ciphertext[1024];
        if (crypto_secretbox_easy(ciphertext, final_data, len, n, key) != 0) {
             return -1;
        }
        memcpy(final_data, ciphertext, len + crypto_secretbox_MACBYTES);
        final_len = len + crypto_secretbox_MACBYTES;
    }
    #endif

    int ret = udx_send_raw(udx, ip, port, type, seq, udx->last_ack, final_data, final_len);

    // Track reliable packets
    if (reliable && slot_idx != -1 && ret > 0) {
        udx->pending[slot_idx].active = 1;
        udx->pending[slot_idx].seq = seq;
        udx->pending[slot_idx].type = type;
        udx->pending[slot_idx].timestamp = udx_get_time_ms();
        udx->pending[slot_idx].retries = 0;
        udx->pending[slot_idx].len = final_len;
        if (final_len > 0) memcpy(udx->pending[slot_idx].data, final_data, final_len);
        // Warning: strcpy is unsafe, use strncpy or similar if possible, but ip is small
#ifdef _MSC_VER
        strncpy_s(udx->pending[slot_idx].dest_ip, sizeof(udx->pending[slot_idx].dest_ip), ip, 15);
#else
        strncpy(udx->pending[slot_idx].dest_ip, ip, 15);
#endif
        udx->pending[slot_idx].dest_ip[15] = '\0';
        udx->pending[slot_idx].dest_port = port;
    }
    return ret;
}

void udx_send_ack(udx_socket_t* udx, const char* ip, int port, uint32_t ack_seq) {
    // ACKs don't consume sequence numbers
    udx_send_raw(udx, ip, port, UDX_TYPE_ACK, udx->next_seq, ack_seq, NULL, 0);
}

int udx_recv(udx_socket_t* udx, uint8_t* buffer, size_t max_len, char* sender_ip, int* sender_port, uint8_t* type_out, uint32_t* seq_out) {
    if (!udx) return -1;
    
    struct sockaddr_in sender;
    int sender_len = sizeof(sender);
    uint8_t raw_buf[2048]; // Max UDP usually < 1500
    
    int received = recvfrom(udx->socket_fd, (char*)raw_buf, sizeof(raw_buf), 0, (struct sockaddr*)&sender, &sender_len);
    
    if (received > 0) {
        char debug_ip[16];
        inet_ntop(AF_INET, &sender.sin_addr, debug_ip, 16);
        // printf("[UDX Raw] Received %d bytes from %s:%d\n", received, debug_ip, ntohs(sender.sin_port));

         // Check packet type to distinguish UDX from others (DHT)
        uint8_t type = raw_buf[0];
        if (type < 0x80) {
            // Non-UDX packet (DHT, etc)
            if (type_out) *type_out = type;
            
            size_t copy_len = (size_t)received;
            if (copy_len > max_len) copy_len = max_len;
            memcpy(buffer, raw_buf, copy_len);
            
            if (sender_ip) {
                inet_ntop(AF_INET, &sender.sin_addr, sender_ip, 16);
            }
            if (sender_port) *sender_port = ntohs(sender.sin_port);
            
            return (int)copy_len;
        }

        if (received < sizeof(udx_header_t)) {
            // Invalid packet (too short)
            return 0; 
        }
        
        udx_header_t* header = (udx_header_t*)raw_buf;
        if (type_out) *type_out = header->type;
        if (seq_out) *seq_out = header->seq;
        
        char current_ip[16];
        inet_ntop(AF_INET, &sender.sin_addr, current_ip, 16);
        int current_port = ntohs(sender.sin_port);

        // Handle ACK
        if (header->type == UDX_TYPE_ACK) {
             uint32_t ack = header->ack;
             for (int i=0; i<MAX_PENDING_PACKETS; i++) {
                 if (udx->pending[i].active && udx->pending[i].seq == ack) {
                     udx->pending[i].active = 0;
                     // printf("[UDX] Packet %d acknowledged\n", ack);
                     break;
                 }
             }
        }

        // Auto-ACK DATA packets
        if (header->type == UDX_TYPE_DATA || header->type == UDX_TYPE_SYN || header->type == UDX_TYPE_FIN) {
            udx->last_ack = header->seq;
            udx_send_ack(udx, current_ip, current_port, header->seq);
        }
        
        // Extract payload
        size_t payload_len = received - sizeof(udx_header_t);
        if (payload_len > max_len) payload_len = max_len;
        
        if (payload_len > 0) {
            memcpy(buffer, raw_buf + sizeof(udx_header_t), payload_len);
        }
        
        if (sender_ip) {
#ifdef _MSC_VER
            strcpy_s(sender_ip, 16, current_ip);
#else
            strcpy(sender_ip, current_ip);
#endif
        }
        if (sender_port) *sender_port = current_port;
        
        return (int)payload_len;
    }
    
    return received; // 0 or -1 (error/wouldblock)
}

void udx_tick(udx_socket_t* udx) {
    if (!udx) return;
    uint32_t now = udx_get_time_ms();
    for (int i=0; i<MAX_PENDING_PACKETS; i++) {
        if (udx->pending[i].active) {
            // Exponential backoff: 500ms, 1000ms, 2000ms, 4000ms...
            uint32_t timeout = 500 * (1 << udx->pending[i].retries);
            
            if (now - udx->pending[i].timestamp > timeout) {
                if (udx->pending[i].retries < 5) { // Increased max retries to 5
                     udx->pending[i].retries++;
                     udx->pending[i].timestamp = now;
                     printf("[UDX] Retransmitting seq %d to %s:%d (Attempt %d, Timeout %dms)\n", 
                            udx->pending[i].seq, udx->pending[i].dest_ip, udx->pending[i].dest_port, 
                            udx->pending[i].retries, timeout);
                     
                     udx_send_raw(udx, udx->pending[i].dest_ip, udx->pending[i].dest_port, 
                                  udx->pending[i].type, udx->pending[i].seq, udx->last_ack, 
                                  udx->pending[i].data, udx->pending[i].len);
                } else {
                     printf("[UDX] Packet %d dropped after max retries\n", udx->pending[i].seq);
                     udx->pending[i].active = 0;
                     // TODO: Notify application of connection failure?
                }
            }
        }
    }
}

int udx_get_local_port(udx_socket_t* udx) {
    if (!udx) return 0;
    struct sockaddr_in sin;
    int len = sizeof(sin);
    if (getsockname(udx->socket_fd, (struct sockaddr *)&sin, &len) == -1) return 0;
    return ntohs(sin.sin_port);
}
