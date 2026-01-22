#include "hyperswarm_core.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../dht/protocol.h"

// Helper to print hex
void print_hex(const char* label, const unsigned char* data, int len) {
    printf("%s: ", label);
    for(int i=0; i<len; i++) printf("%02x", data[i]);
    printf("\n");
}

// Helper: Hex to Bin
void hex_to_bin(const char* hex, uint8_t* bin, int bin_len) {
    for (int i = 0; i < bin_len; i++) {
#ifdef _MSC_VER
        unsigned int temp;
        sscanf_s(hex + 2*i, "%02x", &temp);
        bin[i] = (uint8_t)temp;
#else
        sscanf(hex + 2*i, "%02hhx", &bin[i]);
#endif
    }
}

// Helper to sign packet
void hyperswarm_sign_packet(HyperswarmState* state, dht_header_t* header, const uint8_t* payload, int payload_len) {
#if HAS_SODIUM
    // Signature covers: type + timestamp + payload
    // We cannot sign the header itself because it contains the signature.
    // Protocol convention: Sign(type + sender_pk + timestamp + payload)
    
    unsigned char signed_msg[1024]; // Temp buffer
    int msg_len = 1 + 32 + 8 + payload_len;
    if (msg_len > 1024) return; // Too big
    
    uint8_t* ptr_buf = signed_msg;
    *ptr_buf++ = header->type;
    memcpy(ptr_buf, header->sender_public_key, 32); ptr_buf += 32;
    memcpy(ptr_buf, &header->timestamp, 8); ptr_buf += 8;
    if (payload && payload_len > 0) {
        memcpy(ptr_buf, payload, payload_len);
    }
    
    crypto_sign_detached(header->signature, NULL, signed_msg, msg_len, state->sign_sk);
#else
    memset(header->signature, 0xEE, 64); // Dummy
#endif
}

void hyperswarm_send_find_node(HyperswarmState* state, const char* ip, int port, const uint8_t* target_id) {
    int packet_len = sizeof(dht_header_t) + sizeof(dht_find_node_t);
    uint8_t* buffer = (uint8_t*)malloc(packet_len);
    if (!buffer) return;

    dht_header_t* header = (dht_header_t*)buffer;
    memset(header, 0, sizeof(dht_header_t));
    header->type = MSG_FIND_NODE;
    memcpy(header->sender_public_key, state->sign_pk, 32);
    header->timestamp = udx_get_time_ms();

    dht_find_node_t* payload = (dht_find_node_t*)(buffer + sizeof(dht_header_t));
    memcpy(payload->target_id, target_id, 32);

    hyperswarm_sign_packet(state, header, (uint8_t*)payload, sizeof(dht_find_node_t));
    udx_send_to(state->udx, ip, port, buffer, packet_len);
    free(buffer);
    printf("[C-Native] Sent FIND_NODE to %s:%d\n", ip, port);
}

void hyperswarm_handle_dht_packet(HyperswarmState* state, const char* sender_ip, int sender_port, const uint8_t* buffer, int len) {
    if (len < sizeof(dht_header_t)) return;
    dht_header_t* header = (dht_header_t*)buffer;

#if HAS_SODIUM
    // Verify signature logic
    // Msg = type + sender_pk + timestamp + payload
    // Payload length = len - sizeof(dht_header_t)
    int payload_len = len - sizeof(dht_header_t);
    if (payload_len < 0) return;

    unsigned char signed_msg[1024];
    int msg_len = 1 + 32 + 8 + payload_len;
    
    if (msg_len > 1024) {
        printf("[C-Native] Packet too big for verification from %s:%d\n", sender_ip, sender_port);
        return; 
    }
    
    uint8_t* ptr_buf = signed_msg;
    *ptr_buf++ = header->type;
    memcpy(ptr_buf, header->sender_public_key, 32); ptr_buf += 32;
    memcpy(ptr_buf, &header->timestamp, 8); ptr_buf += 8;
    if (payload_len > 0) {
        memcpy(ptr_buf, buffer + sizeof(dht_header_t), payload_len);
    }
    
    if (crypto_sign_verify_detached(header->signature, signed_msg, msg_len, header->sender_public_key) != 0) {
        printf("[C-Native] Invalid signature from %s:%d (Type: %d)\n", sender_ip, sender_port, header->type);
        return;
    }
    // printf("[C-Native] Valid signature from %s:%d\n", sender_ip, sender_port);
#endif

    switch (header->type) {
        case MSG_PING: {
            printf("[C-Native] Received PING from %s:%d\n", sender_ip, sender_port);
            dht_header_t response;
            memset(&response, 0, sizeof(response));
            response.type = MSG_PONG;
            memcpy(response.sender_public_key, state->sign_pk, 32);
            response.timestamp = udx_get_time_ms();
            
            hyperswarm_sign_packet(state, &response, NULL, 0);
            udx_send_to(state->udx, sender_ip, sender_port, (uint8_t*)&response, sizeof(response));
            printf("[C-Native] Sent PONG to %s:%d\n", sender_ip, sender_port);
            break;
        }
        case MSG_PONG: {
            printf("[C-Native] Received PONG from %s:%d\n", sender_ip, sender_port);
            // TODO: Update bucket
            break;
        }
        case MSG_FIND_NODE: {
             if (len < sizeof(dht_header_t) + sizeof(dht_find_node_t)) return;
             dht_find_node_t* payload = (dht_find_node_t*)(buffer + sizeof(dht_header_t));
             
             printf("[C-Native] Received FIND_NODE from %s:%d\n", sender_ip, sender_port);

             // Find closest
             dht_node_t closest[8];
             int count = routing_find_closest(&state->routing, payload->target_id, closest, 8);
             
             // Construct MSG_FOUND_NODES
             // Size: Header + (Count byte + nodes)
             int response_len = sizeof(dht_header_t) + 1 + (count * sizeof(dht_node_wire_t));
             uint8_t* response_buf = (uint8_t*)malloc(response_len);
             if (!response_buf) return;
             
             dht_header_t* resp_header = (dht_header_t*)response_buf;
             resp_header->type = MSG_FOUND_NODES;
             memcpy(resp_header->sender_public_key, state->sign_pk, 32);
             resp_header->timestamp = udx_get_time_ms();
             
             uint8_t* ptr = response_buf + sizeof(dht_header_t);
             *ptr++ = (uint8_t)count;
             
             for(int i=0; i<count; i++) {
                 dht_node_wire_t wire;
                 memcpy(wire.id, closest[i].id, 32);
                 wire.ip = closest[i].ip;
                 wire.port = closest[i].port;
                 memcpy(ptr, &wire, sizeof(dht_node_wire_t));
                 ptr += sizeof(dht_node_wire_t);
             }
             
             hyperswarm_sign_packet(state, resp_header, response_buf + sizeof(dht_header_t), response_len - sizeof(dht_header_t));
             udx_send_to(state->udx, sender_ip, sender_port, response_buf, response_len);
             free(response_buf);
             printf("[C-Native] Sent FOUND_NODES (%d nodes) to %s:%d\n", count, sender_ip, sender_port);
             break;
        }
        case MSG_FOUND_NODES: {
             if (len < sizeof(dht_header_t) + 1) return;
             uint8_t* ptr = (uint8_t*)(buffer + sizeof(dht_header_t));
             int count = *ptr++;
             
             printf("[C-Native] Received FOUND_NODES (%d nodes) from %s:%d\n", count, sender_ip, sender_port);
             
             int expected_len = sizeof(dht_header_t) + 1 + (count * sizeof(dht_node_wire_t));
             if (len < expected_len) return;
             
             for(int i=0; i<count; i++) {
                 dht_node_wire_t* wire = (dht_node_wire_t*)ptr;
                 dht_node_t node;
                 memcpy(node.id, wire->id, 32);
                 node.ip = wire->ip;
                 node.port = wire->port;
                 node.last_seen = time(NULL);
                 
                 routing_update(&state->routing, &node);
                 
                 // Active Lookup: Query new nodes if lookup is active
                 if (state->lookup_active) {
                     struct in_addr ip_addr;
                     ip_addr.s_addr = node.ip;
                     char* node_ip = inet_ntoa(ip_addr);
                     int node_port = ntohs(node.port);
                     
                     hyperswarm_send_find_node(state, node_ip, node_port, state->lookup_target);
                 }
                 
                 ptr += sizeof(dht_node_wire_t);
             }
             break;
        }
    }
}

HyperswarmState* hyperswarm_create() {
    HyperswarmState* state = (HyperswarmState*)malloc(sizeof(HyperswarmState));
    if (state) {
        state->running = 1;
        state->hs_state = HS_STATE_NONE;

        // Initialize UDX
        state->udx = udx_create(0);
        holepunch_init(&state->hp);

        // Initialize Routing Table with random ID
        uint8_t local_id[32];
        for(int i=0; i<32; i++) local_id[i] = rand() % 256;
        routing_init(&state->routing, local_id);

#if HAS_SODIUM
        if (sodium_init() < 0) {
            printf("[Hyperswarm] Libsodium initialization failed!\n");
        } else {
            // Generate Static Identity (X25519 for Noise)
            crypto_box_keypair(state->static_pk, state->static_sk);
            print_hex("[Hyperswarm] My Static PK", state->static_pk, 32);
            
            // Generate Signing Keys (Ed25519 for DHT)
            crypto_sign_keypair(state->sign_pk, state->sign_sk);
            print_hex("[Hyperswarm] My Sign PK", state->sign_pk, 32);
        }
#else
        printf("[Hyperswarm] WARNING: Libsodium not found. Crypto disabled.\n");
        // Use random bytes for dummy keys so we don't send zeros
        for(int i=0; i<32; i++) state->sign_pk[i] = rand() % 256;
#endif
    }
    return state;
}

void hyperswarm_destroy(HyperswarmState* state) {
    if (state) {
        state->running = 0;
        udx_destroy(state->udx);
        free(state);
    }
}

void hyperswarm_add_peer(HyperswarmState* state, const char* ip, int port, const char* id_hex) {
    if (!state) return;
    dht_node_t node;
    memset(&node, 0, sizeof(node));
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) == 1) {
        node.ip = addr.s_addr;
    } else {
        node.ip = 0;
    }
    
    node.port = htons(port);
    // TODO: parse hex id
    // For now random ID if null
    for(int i=0; i<32; i++) node.id[i] = rand() % 256;
    
    routing_update(&state->routing, &node);
    printf("[C-Native] Added peer %s:%d to routing table\n", ip, port);
}

void hyperswarm_join(HyperswarmState* state, const char* topic_hex) {
    printf("[C-Native] Joining topic: %s\n", topic_hex);

    // Parse topic
    uint8_t target_id[32]; 
    hex_to_bin(topic_hex, target_id, 32);

    // Start Lookup
    state->lookup_active = 1;
    memcpy(state->lookup_target, target_id, 32);
    state->lookup_last_activity = udx_get_time_ms();

    // Find closest peers in routing table
    dht_node_t closest[8];
    int count = routing_find_closest(&state->routing, target_id, closest, 8);
    
    if (count > 0) {
        printf("[C-Native] Found %d peers in DHT. Starting lookup & connection...\n", count);
        for(int i=0; i<count; i++) {
            struct in_addr ip_addr;
            ip_addr.s_addr = closest[i].ip;
            char ip_str[16];
            inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
            int port = ntohs(closest[i].port);
            
            // 1. Send FIND_NODE to find more peers
            hyperswarm_send_find_node(state, ip_str, port, target_id);

            // 2. Initiate Handshake (Aggressive join)
            holepunch_start(&state->hp, ip_str, port);
            
            uint8_t packet[33];
            packet[0] = 0x01; // Msg Type 1: Handshake 'e'
            
#if HAS_SODIUM
            crypto_box_keypair(state->ephemeral_pk, state->ephemeral_sk);
            memcpy(packet + 1, state->ephemeral_pk, 32);
#else
            memset(packet + 1, 0xAB, 32);
#endif
            udx_send(state->udx, ip_str, port, UDX_TYPE_DATA, packet, 33, NULL);
        }
    } else {
        printf("[C-Native] No peers found. Waiting for bootstrap or incoming...\n");
        // Fallback to localhost for testing
        const char* peer_ip = "127.0.0.1";
        int peer_port = 8000;
        
        // Send FIND_NODE to bootstrap
        hyperswarm_send_find_node(state, peer_ip, peer_port, target_id);
    }
}

void hyperswarm_handle_handshake_packet(HyperswarmState* state, const char* sender_ip, int sender_port, uint8_t* buffer, int len) {
    if (len < 1) return;
    uint8_t msg_type = buffer[0];

    printf("[Noise] Received Handshake Msg Type %d from %s:%d (Len: %d)\n", msg_type, sender_ip, sender_port, len);

    if (msg_type == 0x01) { // Msg 1: 'e' (Initiator -> Responder)
        if (len < 33) return;
        
#if HAS_SODIUM
        printf("[Noise] Processing 'e'...\n");
        // 1. Store remote ephemeral 're'
        memcpy(state->remote_ephemeral_pk, buffer + 1, 32);

        // 2. Generate our ephemeral 'e'
        crypto_box_keypair(state->ephemeral_pk, state->ephemeral_sk);

        // 3. Perform DH(e, re) -> 'ee'
        unsigned char k[crypto_scalarmult_SCALARBYTES];
        if (crypto_scalarmult(k, state->ephemeral_sk, state->remote_ephemeral_pk) != 0) {
             printf("[Noise] Error: DH(e, re) failed\n");
             return;
        }
        
        // 4. Derive key for encrypting our static 's': key_s = hash(ee)
        unsigned char key_s[crypto_secretbox_KEYBYTES];
        crypto_generichash(key_s, sizeof(key_s), k, crypto_scalarmult_SCALARBYTES, NULL, 0);

        // 5. Encrypt our static_pk -> encrypted_s
        unsigned char encrypted_s[crypto_box_PUBLICKEYBYTES + crypto_secretbox_MACBYTES];
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        memset(nonce, 0, sizeof(nonce)); // Use 0 for handshake msg
        crypto_secretbox_easy(encrypted_s, state->static_pk, crypto_box_PUBLICKEYBYTES, nonce, key_s);

        // 6. Calculate 'es' = DH(s, re) (My static, Remote ephemeral)
        // Note: We don't use it in Msg 2 payload, but it's part of Noise state.
        // We will skip strict state updates for this prototype.

        // 7. Send Msg 2: [0x02] [e (32)] [encrypted_s (48)]
        uint8_t response[1 + 32 + 48];
        response[0] = 0x02;
        memcpy(response + 1, state->ephemeral_pk, 32);
        memcpy(response + 33, encrypted_s, 48);

        udx_send(state->udx, sender_ip, sender_port, UDX_TYPE_DATA, response, sizeof(response), NULL);
        printf("[Noise] Sent Handshake Msg 2 (Encrypted Identity) to %s:%d\n", sender_ip, sender_port);
        
        state->hs_state = HS_STATE_RESPONDER_AWAIT_S_SE;
#else
        printf("[Noise] Libsodium missing. Cannot process 'e'.\n");
#endif
    }
    else if (msg_type == 0x02) { // Msg 2: 'e', 'ee', 's', 'es' (Responder -> Initiator)
#if HAS_SODIUM
        printf("[Noise] Processing Msg 2...\n");
        // Payload: [0x02] [re (32)] [encrypted_rs (48)]
        if (len < 1 + 32 + 48) return;
        
        memcpy(state->remote_ephemeral_pk, buffer + 1, 32);
        
        // 1. Perform DH(e, re) -> 'ee'
        unsigned char k[crypto_scalarmult_SCALARBYTES];
        crypto_scalarmult(k, state->ephemeral_sk, state->remote_ephemeral_pk);
        
        // 2. Derive key_s = hash(ee)
        unsigned char key_s[crypto_secretbox_KEYBYTES];
        crypto_generichash(key_s, sizeof(key_s), k, crypto_scalarmult_SCALARBYTES, NULL, 0);
        
        // 3. Decrypt remote 's'
        unsigned char encrypted_rs[48];
        memcpy(encrypted_rs, buffer + 33, 48);
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        memset(nonce, 0, sizeof(nonce));
        
        if (crypto_secretbox_open_easy(state->remote_static_pk, encrypted_rs, 48, nonce, key_s) != 0) {
            printf("[Noise] Error: Failed to decrypt remote identity!\n");
            return;
        }
        printf("[Noise] Verified Remote Identity!\n");
        
        // 4. Calculate 'es' = DH(e, rs) (My ephemeral, Remote static)
        unsigned char es[crypto_scalarmult_SCALARBYTES];
        crypto_scalarmult(es, state->ephemeral_sk, state->remote_static_pk);
        
        // 5. Derive key_s2 = hash(es)
        unsigned char key_s2[crypto_secretbox_KEYBYTES];
        crypto_generichash(key_s2, sizeof(key_s2), es, crypto_scalarmult_SCALARBYTES, NULL, 0);
        
        // 6. Encrypt my static_pk -> encrypted_my_s
        unsigned char encrypted_my_s[48];
        crypto_secretbox_easy(encrypted_my_s, state->static_pk, 32, nonce, key_s2);
        
        // 7. Send Msg 3: [0x03] [encrypted_my_s (48)]
        uint8_t response[1 + 48];
        response[0] = 0x03;
        memcpy(response + 1, encrypted_my_s, 48);
        
        udx_send(state->udx, sender_ip, sender_port, UDX_TYPE_DATA, response, sizeof(response), NULL);
        printf("[Noise] Sent Handshake Msg 3 to %s:%d\n", sender_ip, sender_port);
        
        // Split: Derive Transport Keys
        // k1 = H(es || "1"), k2 = H(es || "2")
        // Initiator: tx=k1, rx=k2
        unsigned char k1[32], k2[32];
        uint8_t salt1[] = "1";
        uint8_t salt2[] = "2";
        crypto_generichash(k1, 32, es, sizeof(es), salt1, 1);
        crypto_generichash(k2, 32, es, sizeof(es), salt2, 1);
        
        memcpy(state->tx_key, k1, 32);
        memcpy(state->rx_key, k2, 32);
        state->tx_nonce = 0;
        state->rx_nonce = 0;

        printf("[Noise] Handshake Established (Initiator)!\n");
        state->hs_state = HS_STATE_ESTABLISHED;
#endif
    }
    else if (msg_type == 0x03) { // Msg 3: 's', 'se' (Initiator -> Responder)
#if HAS_SODIUM
        printf("[Noise] Processing Msg 3...\n");
        // Payload: [0x03] [encrypted_rs (48)]
        if (len < 1 + 48) return;
        
        // 1. Calculate 'es' = DH(s, re) (My static, Remote ephemeral)
        // We are Responder. My static = static_sk. Remote ephemeral = remote_ephemeral_pk.
        unsigned char es[crypto_scalarmult_SCALARBYTES];
        crypto_scalarmult(es, state->static_sk, state->remote_ephemeral_pk);
        
        // 2. Derive key_s2 = hash(es)
        unsigned char key_s2[crypto_secretbox_KEYBYTES];
        crypto_generichash(key_s2, sizeof(key_s2), es, crypto_scalarmult_SCALARBYTES, NULL, 0);
        
        // 3. Decrypt Initiator's S
        unsigned char encrypted_rs[48];
        memcpy(encrypted_rs, buffer + 1, 48);
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        memset(nonce, 0, sizeof(nonce));
        
        if (crypto_secretbox_open_easy(state->remote_static_pk, encrypted_rs, 48, nonce, key_s2) != 0) {
             printf("[Noise] Error: Failed to decrypt initiator identity!\n");
             return;
        }
        
        // Split: Derive Transport Keys
        // k1 = H(es || "1"), k2 = H(es || "2")
        // Responder: tx=k2, rx=k1
        unsigned char k1[32], k2[32];
        uint8_t salt1[] = "1";
        uint8_t salt2[] = "2";
        crypto_generichash(k1, 32, es, sizeof(es), salt1, 1);
        crypto_generichash(k2, 32, es, sizeof(es), salt2, 1);
        
        memcpy(state->tx_key, k2, 32);
        memcpy(state->rx_key, k1, 32);
        state->tx_nonce = 0;
        state->rx_nonce = 0;

        state->hs_state = HS_STATE_ESTABLISHED;
        printf("[Noise] Handshake Established (Responder)! Remote verified.\n");
#endif
    }
}

void hyperswarm_leave(HyperswarmState* state, const char* topic_hex) {
    printf("[C-Native] Leaving topic: %s\n", topic_hex);
}

void hyperswarm_poll(HyperswarmState* state) {
    if (!state || !state->running) return;

    // 1. Poll UDX
    uint8_t buffer[2048];
    char sender_ip[16];
    int sender_port;
    uint8_t type;
    uint32_t seq;

    int len;
    while ((len = udx_recv(state->udx, buffer, sizeof(buffer), sender_ip, &sender_port, &type, &seq)) > 0) {
        
        // Notify Holepuncher
        holepunch_on_packet(&state->hp, sender_ip, sender_port);

        if (type < 0x80) {
             // DHT Packet
             hyperswarm_handle_dht_packet(state, sender_ip, sender_port, buffer, len);
        }
        else if (type == UDX_TYPE_HOLEPUNCH) {
             printf("[C-Native] Received HOLEPUNCH from %s:%d\n", sender_ip, sender_port);
             return;
        }
        else if (type == UDX_TYPE_DATA) {
            // Handle Handshake Packets or Data
            if (len > 0 && buffer[0] >= 0x01 && buffer[0] <= 0x03) {
                 hyperswarm_handle_handshake_packet(state, sender_ip, sender_port, buffer, len);
            }
            else {
#if HAS_SODIUM
                 int decrypted = 0;
                 if (state->hs_state == HS_STATE_ESTABLISHED && len >= crypto_secretbox_MACBYTES) {
                     unsigned char n[crypto_secretbox_NONCEBYTES];
                     memset(n, 0, sizeof(n));
                     memcpy(n, &seq, 4);
                     
                     uint8_t plain[1024];
                     if (crypto_secretbox_open_easy(plain, buffer, len, n, state->rx_key) == 0) {
                         int plain_len = len - crypto_secretbox_MACBYTES;
                         printf("[C-Native] Received Encrypted MSG: %.*s\n", plain_len, plain);
                         decrypted = 1;
                     } else {
                         printf("[Debug] Decryption failed for seq %d (Len: %d)\n", seq, len);
                         printf("[Debug] Full RxKey: ");
                         for(int i=0; i<32; i++) printf("%02x", state->rx_key[i]);
                         printf("\n");
                         printf("[Debug] Full Nonce: ");
                         for(int i=0; i<crypto_secretbox_NONCEBYTES; i++) printf("%02x", n[i]);
                         printf("\n");
                         printf("[Debug] Full Ciphertext (first 32): ");
                         for(int i=0; i<32 && i<len; i++) printf("%02x", buffer[i]);
                         printf("\n");
                     }
                 }
                 if (!decrypted)
#endif
                 printf("[C-Native] Received DATA %d bytes: %.*s\n", len, len, buffer);
            }
        }
    }

    // 2. Tick Holepunching
    holepunch_tick(&state->hp, state->udx);

    // 3. Tick UDX (Retransmissions)
    udx_tick(state->udx);

    // 4. DHT Lookup Timeout
    if (state->lookup_active) {
        if (udx_get_time_ms() - state->lookup_last_activity > 5000) { // 5s timeout
            printf("[C-Native] DHT Lookup timed out or finished.\n");
            state->lookup_active = 0;
        }
    }
}

int hyperswarm_get_port(HyperswarmState* state) {
    if (!state || !state->udx) return 0;
    return udx_get_local_port(state->udx);
}

void hyperswarm_send_debug(HyperswarmState* state, const char* ip, int port, const char* msg) {
    if (!state || !state->udx) return;
    
    const uint8_t* key = NULL;
#if HAS_SODIUM
    if (state->hs_state == HS_STATE_ESTABLISHED) {
        key = state->tx_key;
        printf("[Debug] Sending ENCRYPTED packet to %s:%d. TxKey[0]: %02x\n", ip, port, key[0]);
        printf("[Debug] Full TxKey: ");
        for(int i=0; i<32; i++) printf("%02x", key[i]);
        printf("\n");
    } else {
        printf("[Debug] Sending PLAINTEXT packet (State: %d)\n", state->hs_state);
    }
#endif

    udx_send(state->udx, ip, port, UDX_TYPE_DATA, (const uint8_t*)msg, strlen(msg), key);
}
