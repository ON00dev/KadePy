#ifndef DHT_NODE_H
#define DHT_NODE_H

#include <stdint.h>
#include <time.h>

#define ID_SIZE 32

#pragma pack(push, 1)
typedef struct {
    uint8_t id[ID_SIZE];
    uint32_t ip;      // IPv4 em network byte order
    uint16_t port;    // Port em network byte order
    // Removendo last_seen da estrutura serializada ou mantendo?
    // Para protocolo, não enviamos last_seen. Vamos criar uma struct separada para protocolo se precisar.
    // Mas por simplicidade agora, vamos assumir que dht_node_t é a estrutura em memória.
    // Para wire format, vamos criar dht_node_wire_t.
} dht_node_wire_t;
#pragma pack(pop)

typedef struct {
    uint8_t id[ID_SIZE];
    uint32_t ip;      // IPv4 em network byte order
    uint16_t port;    // Port em network byte order
    time_t last_seen;
} dht_node_t;

#endif
