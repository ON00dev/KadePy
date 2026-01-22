#ifndef UDX_H
#define UDX_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Packet Types (High range for UDX, Low range for DHT)
#define UDX_TYPE_DATA 0x80
#define UDX_TYPE_ACK  0x81
#define UDX_TYPE_SYN  0x82
#define UDX_TYPE_FIN  0x83
#define UDX_TYPE_HOLEPUNCH 0x84

#define MAX_PENDING_PACKETS 32

typedef struct {
    uint32_t seq;
    uint32_t timestamp; // ms
    int retries;
    uint8_t type;
    size_t len;
    uint8_t data[1024]; // Max payload
    char dest_ip[16];
    int dest_port;
    int active;
} udx_pending_packet_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint32_t conn_id;
    uint32_t seq;
    uint32_t ack;
} udx_header_t;
#pragma pack(pop)

typedef struct {
    int socket_fd;
    uint32_t local_id;
    
    // Reliability State
    uint32_t next_seq;
    uint32_t last_ack;

    udx_pending_packet_t pending[MAX_PENDING_PACKETS];
} udx_socket_t;

udx_socket_t* udx_create(int port);
void udx_destroy(udx_socket_t* udx);

// Sends a UDX packet with header
// If key provided, encrypts payload using crypto_secretbox (XSalsa20-Poly1305) with seq as nonce
int udx_send(udx_socket_t* udx, const char* ip, int port, uint8_t type, const uint8_t* data, size_t len, const uint8_t* key);

// Sends a raw packet without UDX header (for DHT/other protocols)
int udx_send_to(udx_socket_t* udx, const char* ip, int port, const uint8_t* data, size_t len);

// Receives a UDX packet, strips header, returns payload length
// Returns -1 on error, 0 on no data (if non-blocking)
// Fills type_out with packet type and seq_out with sequence number (for decryption)
int udx_recv(udx_socket_t* udx, uint8_t* buffer, size_t max_len, char* sender_ip, int* sender_port, uint8_t* type_out, uint32_t* seq_out);

// Periodically call this to handle retransmissions
void udx_tick(udx_socket_t* udx);

int udx_get_local_port(udx_socket_t* udx);

uint32_t udx_get_time_ms();

#endif // UDX_H
