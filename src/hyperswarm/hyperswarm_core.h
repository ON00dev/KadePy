#ifndef HYPERSWARM_CORE_H
#define HYPERSWARM_CORE_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "udx.h"
#include "holepunch.h"
#include "dht/routing.h"

// Check for libsodium
#if __has_include(<sodium.h>)
#include <sodium.h>
#define HAS_SODIUM 1
#else
#define HAS_SODIUM 0
// Mock constants for compilation if missing
#define crypto_box_PUBLICKEYBYTES 32
#define crypto_box_SECRETKEYBYTES 32
#define crypto_scalarmult_SCALARBYTES 32
#define crypto_scalarmult_BYTES 32
#define crypto_sign_PUBLICKEYBYTES 32
#define crypto_sign_SECRETKEYBYTES 64
#define crypto_sign_BYTES 64
#endif

typedef enum {
    HS_STATE_NONE,
    HS_STATE_INITIATOR_SEND_E,
    HS_STATE_INITIATOR_AWAIT_E_EE_S_ES,
    HS_STATE_INITIATOR_SEND_S_SE,
    HS_STATE_RESPONDER_AWAIT_E,
    HS_STATE_RESPONDER_SEND_E_EE_S_ES,
    HS_STATE_RESPONDER_AWAIT_S_SE,
    HS_STATE_ESTABLISHED
} HandshakeStateEnum;

typedef struct {
    int running;
    udx_socket_t* udx;
    holepunch_t hp;
    
    // Identity Keys (Static - X25519 for Noise)
    unsigned char static_pk[crypto_box_PUBLICKEYBYTES];
    unsigned char static_sk[crypto_box_SECRETKEYBYTES];

    // Signing Keys (Ed25519 for DHT)
    unsigned char sign_pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sign_sk[crypto_sign_SECRETKEYBYTES];

    // DHT Routing Table
    routing_table_t routing;
    
    // Handshake State
    HandshakeStateEnum hs_state;
    unsigned char ephemeral_pk[crypto_box_PUBLICKEYBYTES];
    unsigned char ephemeral_sk[crypto_box_SECRETKEYBYTES];
    unsigned char remote_static_pk[crypto_box_PUBLICKEYBYTES];
    unsigned char remote_ephemeral_pk[crypto_box_PUBLICKEYBYTES];
    
    // Session Keys (Rx/Tx)
    unsigned char rx_key[32];
    unsigned char tx_key[32];
    uint64_t rx_nonce;
    uint64_t tx_nonce;

    // Active DHT Lookup (Simple Single-Lookup support)
    int lookup_active;
    uint8_t lookup_target[32];
    uint32_t lookup_last_activity;
} HyperswarmState;

HyperswarmState* hyperswarm_create();
void hyperswarm_destroy(HyperswarmState* state);
void hyperswarm_join(HyperswarmState* state, const char* topic_hex);
void hyperswarm_leave(HyperswarmState* state, const char* topic_hex);
void hyperswarm_poll(HyperswarmState* state);
int hyperswarm_get_port(HyperswarmState* state);
void hyperswarm_send_debug(HyperswarmState* state, const char* ip, int port, const char* msg);
void hyperswarm_add_peer(HyperswarmState* state, const char* ip, int port, const char* id_hex);

#endif // HYPERSWARM_CORE_H
