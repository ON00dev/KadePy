#include "chacha20.h"
#include <string.h>

#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) ( \
    a += b, d ^= a, d = ROTL(d, 16), \
    c += d, b ^= c, b = ROTL(b, 12), \
    a += b, d ^= a, d = ROTL(d, 8), \
    c += d, b ^= c, b = ROTL(b, 7))

static void chacha20_block(uint32_t *out, const uint32_t *in) {
    int i;
    uint32_t x[16];
    for (i = 0; i < 16; ++i) x[i] = in[i];
    for (i = 0; i < 10; ++i) {
        QR(x[0], x[4], x[ 8], x[12]);
        QR(x[1], x[5], x[ 9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[ 8], x[13]);
        QR(x[3], x[4], x[ 9], x[14]);
    }
    for (i = 0; i < 16; ++i) out[i] = x[i] + in[i];
}

void chacha20_encrypt(uint8_t *out, const uint8_t *in, size_t len, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    uint32_t state[16];
    uint32_t block[16];
    uint8_t block_u8[64];
    const char *constants = "expand 32-byte k";
    size_t j;
    
    // Initial State
    // "expand 32-byte k" is 16 bytes: 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 (Little Endian)
    // We can just cast constants string
    memcpy(state, constants, 16);
    
    // Key
    memcpy(&state[4], key, 32);
    
    // Counter
    state[12] = counter;
    
    // Nonce
    memcpy(&state[13], nonce, 12);
    
    while (len > 0) {
        chacha20_block(block, state);
        state[12]++; // Increment counter
        
        // Output bytes
        memcpy(block_u8, block, 64);
        
        size_t chunk = (len < 64) ? len : 64;
        for (j = 0; j < chunk; j++) {
            out[j] = in[j] ^ block_u8[j];
        }
        
        len -= chunk;
        in += chunk;
        out += chunk;
    }
}
