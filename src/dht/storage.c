#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "storage.h"

static topic_entry_t *g_topics = NULL;
static CRITICAL_SECTION g_storage_lock;
static int g_storage_initialized = 0;

void storage_init() {
    if (!g_storage_initialized) {
        InitializeCriticalSection(&g_storage_lock);
        g_topics = NULL;
        g_storage_initialized = 1;
    }
}

int storage_store_peer(const uint8_t *info_hash, uint32_t ip, uint16_t port) {
    if (!g_storage_initialized) storage_init();
    
    EnterCriticalSection(&g_storage_lock);
    
    // Find topic
    topic_entry_t *curr = g_topics;
    while (curr) {
        if (memcmp(curr->info_hash, info_hash, 32) == 0) {
            // Found topic, check if peer exists
            for (int i = 0; i < curr->peer_count; i++) {
                if (curr->peers[i].ip == ip && curr->peers[i].port == port) {
                    // Update timestamp
                    curr->peers[i].last_seen = GetTickCount64();
                    LeaveCriticalSection(&g_storage_lock);
                    return 0; // Already exists (but updated)
                }
            }
            
            // Add peer if space
            if (curr->peer_count < MAX_PEERS_PER_TOPIC) {
                curr->peers[curr->peer_count].ip = ip;
                curr->peers[curr->peer_count].port = port;
                curr->peers[curr->peer_count].last_seen = GetTickCount64();
                curr->peer_count++;
                LeaveCriticalSection(&g_storage_lock);
                return 1;
            } else {
                // Full - for now, replace random or drop? Drop for simplicity.
                LeaveCriticalSection(&g_storage_lock);
                return 0;
            }
        }
        curr = curr->next;
    }
    
    // Topic not found, create new
    topic_entry_t *new_topic = (topic_entry_t*)malloc(sizeof(topic_entry_t));
    if (!new_topic) {
        LeaveCriticalSection(&g_storage_lock);
        return 0;
    }
    
    memcpy(new_topic->info_hash, info_hash, 32);
    new_topic->peers[0].ip = ip;
    new_topic->peers[0].port = port;
    new_topic->peers[0].last_seen = GetTickCount64();
    new_topic->peer_count = 1;
    new_topic->next = g_topics;
    g_topics = new_topic;
    
    LeaveCriticalSection(&g_storage_lock);
    return 1;
}

int storage_get_peers(const uint8_t *info_hash, peer_info_t *out_peers, int max_peers) {
    if (!g_storage_initialized) storage_init();
    
    EnterCriticalSection(&g_storage_lock);
    
    topic_entry_t *curr = g_topics;
    while (curr) {
        if (memcmp(curr->info_hash, info_hash, 32) == 0) {
            int count = (curr->peer_count < max_peers) ? curr->peer_count : max_peers;
            memcpy(out_peers, curr->peers, count * sizeof(peer_info_t));
            LeaveCriticalSection(&g_storage_lock);
            return count;
        }
        curr = curr->next;
    }
    
    LeaveCriticalSection(&g_storage_lock);
    return 0;
}

void storage_cleanup(uint64_t timeout_ms) {
    if (!g_storage_initialized) return;
    EnterCriticalSection(&g_storage_lock);
    
    uint64_t now = GetTickCount64();
    topic_entry_t *curr = g_topics;
    topic_entry_t *prev = NULL;
    
    while (curr) {
        // Filter peers in this topic
        int write_idx = 0;
        for (int i = 0; i < curr->peer_count; i++) {
            if (now - curr->peers[i].last_seen < timeout_ms) {
                if (write_idx != i) {
                    curr->peers[write_idx] = curr->peers[i];
                }
                write_idx++;
            }
        }
        curr->peer_count = write_idx;
        
        // Remove topic if empty
        if (curr->peer_count == 0) {
            // Remove curr
            topic_entry_t *to_free = curr;
            if (prev) {
                prev->next = curr->next;
                curr = curr->next;
            } else {
                g_topics = curr->next;
                curr = g_topics;
            }
            free(to_free);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    
    LeaveCriticalSection(&g_storage_lock);
}
