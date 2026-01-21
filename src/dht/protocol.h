#ifndef DHT_PROTOCOL_H
#define DHT_PROTOCOL_H

#include <stdint.h>

enum MsgType {
    MSG_PING = 0,
    MSG_PONG = 1,
    MSG_FIND_NODE = 2,
    MSG_FOUND_NODES = 3,
    MSG_ANNOUNCE_PEER = 4,
    MSG_GET_PEERS = 5,
    MSG_PEERS = 6
};

#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint8_t sender_id[32];
} dht_header_t;

// Payload para MSG_FIND_NODE
typedef struct {
    uint8_t target_id[32];
} dht_find_node_t;

// Payload para MSG_ANNOUNCE_PEER
typedef struct {
    uint8_t info_hash[32];
    uint16_t port; // Port being announced (IP is from sender)
} dht_announce_peer_t;

// Payload para MSG_GET_PEERS
typedef struct {
    uint8_t info_hash[32];
} dht_get_peers_t;

// Payload para MSG_PEERS (Response)
// Header + info_hash + count + peers...
// Peer format: IP (4) + Port (2)
typedef struct {
    uint8_t info_hash[32];
    uint8_t count;
} dht_peers_header_t;

typedef struct {
    uint32_t ip;
    uint16_t port;
} dht_peer_wire_t;

// Payload para MSG_FOUND_NODES
typedef struct {
    uint8_t count;
    // Seguido por count * dht_node_wire_t
} dht_found_nodes_header_t;
#pragma pack(pop)

// Parse e processamento básico
void dht_handle_packet(const uint8_t *data, int len, uint32_t sender_ip, uint16_t sender_port);

// Inicializa o subsistema DHT (routing table, local ID)
void dht_init();

// Envia um PING para um nó específico
void dht_ping(uint32_t ip, uint16_t port);

// Envia um FIND_NODE para um nó específico
void dht_find_node(uint32_t ip, uint16_t port, const uint8_t *target_id);

// Envia MSG_ANNOUNCE_PEER
void dht_announce_peer(uint32_t ip, uint16_t port, const uint8_t *info_hash, uint16_t announced_port);

// Envia MSG_GET_PEERS
void dht_get_peers(uint32_t ip, uint16_t port, const uint8_t *info_hash);

// Debug: Imprime a tabela de roteamento no stdout
void dht_dump_routing_table();

// Define a chave de rede (32 bytes). Se NULL ou zeros, criptografia é desativada.
void dht_set_network_key(const uint8_t *key);

// Envia um pacote genérico (útil para aplicações customizadas)
void dht_send_packet(uint32_t ip, uint16_t port, int type, const uint8_t *payload, int payload_len);

#endif
