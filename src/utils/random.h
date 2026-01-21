#ifndef KADEPY_RANDOM_H
#define KADEPY_RANDOM_H

#include <stdint.h>
#include <stddef.h>

// Preenche o buffer com bytes aleatórios criptograficamente seguros
void secure_random_bytes(void *buffer, size_t len);

#endif
