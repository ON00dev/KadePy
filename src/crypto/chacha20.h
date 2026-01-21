#ifndef CHACHA20_H
#define CHACHA20_H

#include <stdint.h>
#include <stddef.h>

void chacha20_encrypt(uint8_t *out, const uint8_t *in, size_t len, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter);

#endif
