#ifndef DHT_STORAGE_H
#define DHT_STORAGE_H

#include <stdint.h>

#define MAX_PEERS_PER_TOPIC 32

typedef struct {
    uint32_t ip;
    uint16_t port;
    uint64_t last_seen; // Timestamp (ms)
} peer_info_t;

typedef struct topic_entry_s {
    uint8_t info_hash[32];
    peer_info_t peers[MAX_PEERS_PER_TOPIC];
    int peer_count;
    struct topic_entry_s *next;
} topic_entry_t;

// Initialize storage system
void storage_init();

// Store a peer for a specific topic (thread-safe)
// Returns 1 if added, 0 if already exists or full
int storage_store_peer(const uint8_t *info_hash, uint32_t ip, uint16_t port);

// Get peers for a topic (thread-safe)
// Returns number of peers found. Copies up to max_peers into out_peers.
int storage_get_peers(const uint8_t *info_hash, peer_info_t *out_peers, int max_peers);

// Remove peers that haven't been seen for timeout_ms
void storage_cleanup(uint64_t timeout_ms);

#endif