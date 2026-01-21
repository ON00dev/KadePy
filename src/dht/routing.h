#ifndef DHT_ROUTING_H
#define DHT_ROUTING_H

#include "node.h"

#define K_BUCKET_SIZE 20
#define N_BUCKETS 256 // 8 * 32 bytes = 256 bits

typedef struct {
    dht_node_t nodes[K_BUCKET_SIZE];
    int count;
} k_bucket_t;

typedef struct {
    uint8_t local_id[ID_SIZE];
    k_bucket_t buckets[N_BUCKETS];
    void *lock; // Opaque pointer to thread synchronization primitive
} routing_table_t;

// Inicializa a tabela de roteamento com o ID local
void routing_init(routing_table_t *rt, const uint8_t *local_id);

// Thread-safe access helpers
void routing_lock(routing_table_t *rt);
void routing_unlock(routing_table_t *rt);

// Adiciona ou atualiza um nó na tabela
int routing_update(routing_table_t *rt, const dht_node_t *node);

// Encontra os K nós mais próximos de um target_id
int routing_find_closest(routing_table_t *rt, const uint8_t *target_id, dht_node_t *out_nodes, int max_count);

// Utilitário: XOR distance check (0 se igual, >0 se diferente)
void xor_distance(const uint8_t *id1, const uint8_t *id2, uint8_t *out_dist);

#endif
