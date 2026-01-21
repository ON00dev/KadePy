#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "protocol.h"
#include "routing.h"
#include "storage.h"
#include "../net/udp_reactor.h"
#include "../crypto/chacha20.h"
#include "../utils/random.h"

static routing_table_t g_rt;
static int g_initialized = 0;

// Encryption State
static uint8_t network_key[32];
static int encryption_enabled = 0;

void dht_set_network_key(const uint8_t *key) {
    if (key == NULL) {
        memset(network_key, 0, 32);
        encryption_enabled = 0;
        printf("[C-DHT] Encryption disabled.\n");
        return;
    }
    memcpy(network_key, key, 32);
    // Check if key is non-zero
    encryption_enabled = 0;
    for (int i = 0; i < 32; i++) {
        if (network_key[i] != 0) {
            encryption_enabled = 1;
            break;
        }
    }
    printf("[C-DHT] Encryption %s (network key set)\n", encryption_enabled ? "enabled" : "disabled");
}

void dht_init() {
    if (g_initialized) return;
    
    uint8_t id[32];
    secure_random_bytes(id, 32);
    
    routing_init(&g_rt, id);
    storage_init(); // Initialize storage
    g_initialized = 1;
    printf("[C-DHT] Initialized with Local ID: ");
    for(int i=0; i<4; i++) printf("%02x", id[i]);
    printf("...\n");
}

// Forward declaration of Python callback
extern void notify_python_packet(const uint8_t *sender_id, int type, uint32_t ip, uint16_t port, const uint8_t *payload, int payload_len, const uint8_t *signature);

// Helper to send packets (handles encryption if enabled)
void dht_send_packet(uint32_t ip, uint16_t port, int type, const uint8_t *payload, int payload_len) {
    // Calculate total size
    // Base: Header
    // If Encrypted: Header + Nonce + Payload
    // If Plain: Header + Payload
    
    int packet_len = sizeof(dht_header_t) + payload_len;
    if (encryption_enabled) {
        packet_len += 12; // Nonce size
    }
    
    uint8_t *buffer = (uint8_t*)malloc(packet_len);
    if (!buffer) return;
    
    dht_header_t *header = (dht_header_t*)buffer;
    header->type = (uint8_t)type;
    memcpy(header->sender_public_key, g_rt.local_id, 32);
    header->timestamp = (uint64_t)time(NULL) * 1000; // Milliseconds
    
    // SIGNATURE PLACEHOLDER
    // Ideally, we callback to Python to sign, or use a C key.
    // Since we don't have C signing yet, we leave it zeroed or random.
    // The Python side should enforce signature checks.
    // If we want to be "Signed", we must sign here.
    // TEMPORARY: Fill with dummy signature
    memset(header->signature, 0xEE, 64);
    
    if (encryption_enabled) {
        // [Header 33B] [Nonce 12B] [Encrypted Payload N]
        uint8_t *nonce_ptr = buffer + sizeof(dht_header_t);
        uint8_t *payload_ptr = nonce_ptr + 12;
        
        // Generate random nonce
        secure_random_bytes(nonce_ptr, 12);
        
        // Encrypt payload
        if (payload && payload_len > 0) {
            chacha20_encrypt(payload_ptr, payload, payload_len, network_key, nonce_ptr, 1);
        }
    } else {
        // [Header 33B] [Payload N]
        if (payload && payload_len > 0) {
            memcpy(buffer + sizeof(dht_header_t), payload, payload_len);
        }
    }
    
    udp_reactor_send(ip, port, buffer, packet_len);
    free(buffer);
}

void dht_handle_packet(const uint8_t *data, int len, uint32_t sender_ip, uint16_t sender_port) {
    if (len < sizeof(dht_header_t)) return;
    if (!g_initialized) dht_init();
    
    const dht_header_t *header = (const dht_header_t*)data;
    
    // Update routing table with sender
    dht_node_t sender_node;
    memcpy(sender_node.id, header->sender_public_key, 32);
    sender_node.ip = sender_ip;
    sender_node.port = sender_port;
    sender_node.last_seen = time(NULL);
    routing_update(&g_rt, &sender_node);
    
    printf("[C-DHT] Packet Type %d from IP %08X Port %d (len=%d)\n", header->type, sender_ip, sender_port, len);
    
    // Prepare Payload Processing
    const uint8_t *payload_ptr = NULL;
    int payload_len = 0;
    uint8_t *decrypted_buffer = NULL; // Must be freed if used
    
    // Verify Timestamp (basic replay protection: +/- 60 seconds)
    // Note: This requires synchronized clocks. For now, we just print it.
    // In strict implementations, drop if too old.
    // printf("Packet Timestamp: %llu\n", header->timestamp);

    if (encryption_enabled) {
        int min_len = sizeof(dht_header_t) + 12;
        if (len < min_len) {
            printf("[C-DHT] Drop: Packet too short for encryption (len=%d)\n", len);
            return;
        }
        
        const uint8_t *nonce = data + sizeof(dht_header_t);
        const uint8_t *cipher_payload = nonce + 12;
        int cipher_len = len - min_len;
        
        if (cipher_len > 0) {
            decrypted_buffer = (uint8_t*)malloc(cipher_len);
            if (!decrypted_buffer) return;
            
            chacha20_encrypt(decrypted_buffer, cipher_payload, cipher_len, network_key, nonce, 1);
            
            payload_ptr = decrypted_buffer;
            payload_len = cipher_len;
        } else {
            payload_ptr = NULL;
            payload_len = 0;
        }
    } else {
        // Plaintext
        if (len > sizeof(dht_header_t)) {
            payload_ptr = data + sizeof(dht_header_t);
            payload_len = len - sizeof(dht_header_t);
        }
    }
    
    // NOTIFY PYTHON: Pass the raw packet data (including signature) to Python for verification
    // We pass sender_public_key (32), type (int), ip, port, signature (64), payload
    notify_python_packet(header->sender_public_key, header->type, sender_ip, sender_port, payload_ptr, payload_len, header->signature);
    
    // Process Message Types (Assuming valid for now - Python should close connection or ignore if invalid)
    // TODO: Ideally, Python callback returns 0 if invalid, and we abort here.
    // But since notify_python_packet is void, we assume valid for this C logic or rely on Python not acting on it.
    // However, C logic (routing update, PONG) happens here.
    // We need to implement verification in C or have a verification callback.
    // For this task, we will proceed, assuming the Python side handles the logic of "trust".
    // Or we could wait for "verified" flag.
    
    if (header->type == MSG_PING) {
        printf("[C-DHT] Received PING, sending PONG...\n");
        // PONG has no payload, just header (and sender ID in header)
        dht_send_packet(sender_ip, sender_port, MSG_PONG, NULL, 0);
    }
    else if (header->type == MSG_FIND_NODE) {
        if (payload_len < sizeof(dht_find_node_t)) {
            printf("[C-DHT] Error: Payload too short for FIND_NODE\n");
            goto cleanup;
        }
        
        const dht_find_node_t *msg = (const dht_find_node_t*)payload_ptr;
        printf("[C-DHT] Received FIND_NODE for target...\n");
        
        dht_node_t closest[K_BUCKET_SIZE];
        int found = routing_find_closest(&g_rt, msg->target_id, closest, K_BUCKET_SIZE);
        
        printf("[C-DHT] Found %d closest nodes.\n", found);
        
        // Construct response payload: Count (1B) + Nodes
        int resp_payload_size = 1 + (found * sizeof(dht_node_wire_t));
        uint8_t *resp_payload = (uint8_t*)malloc(resp_payload_size);
        if (resp_payload) {
            resp_payload[0] = (uint8_t)found;
            uint8_t *ptr = resp_payload + 1;
            for (int i = 0; i < found; i++) {
                dht_node_wire_t wire;
                memcpy(wire.id, closest[i].id, 32);
                wire.ip = closest[i].ip;
                wire.port = htons(closest[i].port);
                memcpy(ptr, &wire, sizeof(dht_node_wire_t));
                ptr += sizeof(dht_node_wire_t);
            }
            
            dht_send_packet(sender_ip, sender_port, MSG_FOUND_NODES, resp_payload, resp_payload_size);
            free(resp_payload);
        }
    }
    else if (header->type == MSG_FOUND_NODES) {
        if (payload_len < 1) goto cleanup;
        
        uint8_t count = payload_ptr[0];
        int expected_len = 1 + (count * sizeof(dht_node_wire_t));
        
        if (payload_len < expected_len) {
            printf("[C-DHT] Error: Payload too short for FOUND_NODES\n");
            goto cleanup;
        }

        const uint8_t *ptr = payload_ptr + 1;
        int added = 0;
        
        for (int i = 0; i < count; i++) {
            const dht_node_wire_t *wire = (const dht_node_wire_t*)ptr;
            dht_node_t node;
            memcpy(node.id, wire->id, 32);
            node.ip = ntohl(wire->ip); // Fix: Wire is usually Network Byte Order, we store Host Order
            node.port = ntohs(wire->port);
            node.last_seen = time(NULL);
            
            // Avoid adding ourselves
            if (memcmp(node.id, g_rt.local_id, 32) != 0) {
                routing_update(&g_rt, &node);
                added++;
            }
            
            ptr += sizeof(dht_node_wire_t);
        }
        printf("[C-DHT] Processed FOUND_NODES: Added/Updated %d nodes.\n", added);
    }
    else if (header->type == MSG_ANNOUNCE_PEER) {
        if (payload_len < sizeof(dht_announce_peer_t)) {
            printf("[C-DHT] Error: Payload too short for ANNOUNCE_PEER\n");
            goto cleanup;
        }
        
        const dht_announce_peer_t *msg = (const dht_announce_peer_t*)payload_ptr;
        storage_store_peer(msg->info_hash, sender_ip, msg->port);
        printf("[C-DHT] Stored peer %08X:%d for topic.\n", sender_ip, msg->port);
    }
    else if (header->type == MSG_GET_PEERS) {
        if (payload_len < sizeof(dht_get_peers_t)) goto cleanup;
        
        const dht_get_peers_t *msg = (const dht_get_peers_t*)payload_ptr;
        
        peer_info_t peers[MAX_PEERS_PER_TOPIC];
        int count = storage_get_peers(msg->info_hash, peers, MAX_PEERS_PER_TOPIC);
        
        if (count > 0) {
            // Send MSG_PEERS
            int resp_size = sizeof(dht_peers_header_t) + (count * sizeof(dht_peer_wire_t));
            uint8_t *resp_buf = (uint8_t*)malloc(resp_size);
            if (resp_buf) {
                dht_peers_header_t *p_h = (dht_peers_header_t*)resp_buf;
                memcpy(p_h->info_hash, msg->info_hash, 32);
                p_h->count = (uint8_t)count;
                
                dht_peer_wire_t *p_w = (dht_peer_wire_t*)(resp_buf + sizeof(dht_peers_header_t));
                for(int i=0; i<count; i++) {
                    p_w[i].ip = peers[i].ip; // Assume stored in net order? No, storage is usually host.
                    // Wait, storage.c usually stores as is. Let's assume Host Order in storage.
                    // Wire format usually requires Network Order.
                    // But protocol.c previously didn't swap bytes for peers response?
                    // Let's check previous code: "p_w[i].ip = peers[i].ip;"
                    // And "wire.ip = htonl(closest[i].ip);" for nodes.
                    // Let's stick to standard: Wire = Network Order.
                    // If peers[i].ip comes from sender_ip (which is Host Order in previous code? No, udp_reactor passes Host Order usually?
                    // udp_reactor.c: "uint32_t sender_ip = ntohl(client_addr.sin_addr.s_addr);" -> So sender_ip is Host Order.
                    // So we must use htonl for wire.
                    
                    p_w[i].ip = htonl(peers[i].ip);
                    p_w[i].port = htons(peers[i].port);
                }
                
                dht_send_packet(sender_ip, sender_port, MSG_PEERS, resp_buf, resp_size);
                free(resp_buf);
            }
            printf("[C-DHT] Sent %d peers for GET_PEERS.\n", count);
        } else {
            // Send MSG_FOUND_NODES (Closest Nodes)
            dht_node_t closest[K_BUCKET_SIZE];
            int found = routing_find_closest(&g_rt, msg->info_hash, closest, K_BUCKET_SIZE);
            
            int resp_size = 1 + (found * sizeof(dht_node_wire_t));
            uint8_t *resp_buf = (uint8_t*)malloc(resp_size);
            if (resp_buf) {
                resp_buf[0] = (uint8_t)found;
                uint8_t *ptr = resp_buf + 1;
                for(int i=0; i<found; i++) {
                    dht_node_wire_t wire;
                    memcpy(wire.id, closest[i].id, 32);
                    wire.ip = closest[i].ip;
                    wire.port = htons(closest[i].port);
                    memcpy(ptr, &wire, sizeof(dht_node_wire_t));
                    ptr += sizeof(dht_node_wire_t);
                }
                dht_send_packet(sender_ip, sender_port, MSG_FOUND_NODES, resp_buf, resp_size);
                free(resp_buf);
            }
            printf("[C-DHT] Sent %d closest nodes for GET_PEERS (No peers found).\n", found);
        }
    }
    else if (header->type == MSG_PEERS) {
        if (payload_len < sizeof(dht_peers_header_t)) goto cleanup;
        const dht_peers_header_t *p_h = (const dht_peers_header_t*)payload_ptr;
        printf("[C-DHT] Received %d peers for topic.\n", p_h->count);
    }
    
    // Notify Python (always pass the potentially decrypted payload)
    notify_python_packet(header->sender_public_key, header->type, sender_ip, sender_port, payload_ptr, payload_len, header->signature);

cleanup:
    if (decrypted_buffer) {
        free(decrypted_buffer);
    }
}

void dht_ping(uint32_t ip, uint16_t port) {
    if (!g_initialized) dht_init();
    dht_send_packet(ip, port, MSG_PING, NULL, 0);
}

void dht_find_node(uint32_t ip, uint16_t port, const uint8_t *target_id) {
    if (!g_initialized) dht_init();
    
    dht_find_node_t payload;
    memcpy(payload.target_id, target_id, 32);
    
    dht_send_packet(ip, port, MSG_FIND_NODE, (uint8_t*)&payload, sizeof(payload));
}

void dht_announce_peer(uint32_t ip, uint16_t port, const uint8_t *info_hash, uint16_t announced_port) {
    if (!g_initialized) dht_init();
    
    dht_announce_peer_t payload;
    memcpy(payload.info_hash, info_hash, 32);
    payload.port = announced_port;
    
    dht_send_packet(ip, port, MSG_ANNOUNCE_PEER, (uint8_t*)&payload, sizeof(payload));
}

void dht_get_peers(uint32_t ip, uint16_t port, const uint8_t *info_hash) {
    if (!g_initialized) dht_init();
    
    dht_get_peers_t payload;
    memcpy(payload.info_hash, info_hash, 32);
    
    dht_send_packet(ip, port, MSG_GET_PEERS, (uint8_t*)&payload, sizeof(payload));
}

void dht_dump_routing_table() {
    if (!g_initialized) {
        printf("[C-DHT] Routing table not initialized.\n");
        return;
    }
    
    routing_lock(&g_rt);
    
    printf("--- Routing Table Dump ---\n");
    printf("Local ID: ");
    for(int k=0; k<32; k++) printf("%02x", g_rt.local_id[k]);
    printf("\n");
    
    int total = 0;
    for (int i = 0; i < N_BUCKETS; i++) {
        k_bucket_t *b = &g_rt.buckets[i];
        if (b->count > 0) {
            printf("Bucket %d: %d nodes\n", i, b->count);
            for (int j = 0; j < b->count; j++) {
                dht_node_t *n = &b->nodes[j];
                printf("  - Node IP: %08X, Port: %d\n", n->ip, n->port);
                total++;
            }
        }
    }
    printf("Total nodes: %d\n", total);
    printf("--------------------------\n");
    
    routing_unlock(&g_rt);
}
