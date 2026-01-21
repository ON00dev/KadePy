#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "routing.h"

// Calcula o índice do bucket baseado na distância XOR (prefixo comum)
static int get_bucket_index(const uint8_t *local_id, const uint8_t *target_id) {
    for (int i = 0; i < ID_SIZE; i++) {
        uint8_t xor_val = local_id[i] ^ target_id[i];
        if (xor_val == 0) continue;
        
        // Encontrou o primeiro byte diferente. Agora ache o bit.
        // __builtin_clz seria ideal no GCC, mas vamos de portable C para MSVC
        int bit = 0;
        if (xor_val & 0x80) bit = 7;
        else if (xor_val & 0x40) bit = 6;
        else if (xor_val & 0x20) bit = 5;
        else if (xor_val & 0x10) bit = 4;
        else if (xor_val & 0x08) bit = 3;
        else if (xor_val & 0x04) bit = 2;
        else if (xor_val & 0x02) bit = 1;
        else bit = 0;
        
        // Exemplo: byte 0 diferente, bit 7 diff -> bucket index alto (perto) ou baixo (longe)?
        // Kademlia: index = N_BITS - 1 - prefix_len
        // Aqui vamos simplificar: (i * 8) + (7 - bit) -> bucket 0 é o mais longe (primeiro bit difere)
        return (i * 8) + (7 - bit);
    }
    return N_BUCKETS - 1; // IDs iguais
}

void routing_lock(routing_table_t *rt) {
    if (rt && rt->lock) {
        EnterCriticalSection((CRITICAL_SECTION*)rt->lock);
    }
}

void routing_unlock(routing_table_t *rt) {
    if (rt && rt->lock) {
        LeaveCriticalSection((CRITICAL_SECTION*)rt->lock);
    }
}

void routing_init(routing_table_t *rt, const uint8_t *local_id) {
    memset(rt, 0, sizeof(routing_table_t));
    memcpy(rt->local_id, local_id, ID_SIZE);
    
    rt->lock = malloc(sizeof(CRITICAL_SECTION));
    if (rt->lock) {
        InitializeCriticalSection((CRITICAL_SECTION*)rt->lock);
    }
}

int routing_update(routing_table_t *rt, const dht_node_t *node) {
    routing_lock(rt);
    int bucket_idx = get_bucket_index(rt->local_id, node->id);
    k_bucket_t *bucket = &rt->buckets[bucket_idx];
    
    // Simplificação: Apenas adiciona se tiver espaço. 
    // Kademlia real faria ping no mais antigo se cheio.
    
    // Verifica se já existe
    for (int i = 0; i < bucket->count; i++) {
        if (memcmp(bucket->nodes[i].id, node->id, ID_SIZE) == 0) {
            // Atualiza (move para o fim = mais recente)
            // Por simplicidade, apenas atualiza timestamp aqui
            bucket->nodes[i] = *node; 
            routing_unlock(rt);
            return 1;
        }
    }
    
    if (bucket->count < K_BUCKET_SIZE) {
        bucket->nodes[bucket->count++] = *node;
        routing_unlock(rt);
        return 1;
    }
    
    routing_unlock(rt);
    return 0; // Bucket cheio
}

// Comparador para qsort
static const uint8_t *sort_target;
static int compare_distance(const void *a, const void *b) {
    const dht_node_t *na = (const dht_node_t *)a;
    const dht_node_t *nb = (const dht_node_t *)b;
    
    for (int i = 0; i < ID_SIZE; i++) {
        uint8_t dist_a = na->id[i] ^ sort_target[i];
        uint8_t dist_b = nb->id[i] ^ sort_target[i];
        if (dist_a < dist_b) return -1;
        if (dist_a > dist_b) return 1;
    }
    return 0;
}

int routing_find_closest(routing_table_t *rt, const uint8_t *target_id, dht_node_t *out_nodes, int max_count) {
    routing_lock(rt);
    
    // Alocar dinamicamente para evitar stack overflow
    dht_node_t *temp_buffer = (dht_node_t*)malloc(sizeof(dht_node_t) * N_BUCKETS * K_BUCKET_SIZE);
    if (!temp_buffer) {
        routing_unlock(rt);
        return 0;
    }
    
    int count = 0;
    
    // Varre buckets ao redor do target (simplificado: varre todos com nós)
    for (int i = 0; i < N_BUCKETS; i++) {
        k_bucket_t *b = &rt->buckets[i];
        for (int j = 0; j < b->count; j++) {
            if (count < K_BUCKET_SIZE * N_BUCKETS) {
                temp_buffer[count++] = b->nodes[j];
            }
        }
    }
    
    sort_target = target_id;
    qsort(temp_buffer, count, sizeof(dht_node_t), compare_distance);
    
    int result_count = (count < max_count) ? count : max_count;
    memcpy(out_nodes, temp_buffer, sizeof(dht_node_t) * result_count);
    
    free(temp_buffer);
    routing_unlock(rt);
    return result_count;
}

void xor_distance(const uint8_t *id1, const uint8_t *id2, uint8_t *out_dist) {
    for (int i = 0; i < ID_SIZE; i++) {
        out_dist[i] = id1[i] ^ id2[i];
    }
}
