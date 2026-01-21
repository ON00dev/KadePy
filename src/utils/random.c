#include "random.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

void secure_random_bytes(void *buffer, size_t len) {
#ifdef _WIN32
    NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)buffer, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        fprintf(stderr, "[C-RNG] Critical Error: BCryptGenRandom failed with status 0x%x\n", status);
        // Fallback inseguro em caso de falha crítica (não deve acontecer)
        uint8_t *p = (uint8_t*)buffer;
        for (size_t i = 0; i < len; i++) {
            p[i] = rand() % 256;
        }
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[C-RNG] Critical Error: Failed to open /dev/urandom\n");
        // Fallback
        uint8_t *p = (uint8_t*)buffer;
        for (size_t i = 0; i < len; i++) {
            p[i] = rand() % 256;
        }
        return;
    }
    ssize_t result = read(fd, buffer, len);
    if (result != (ssize_t)len) {
        fprintf(stderr, "[C-RNG] Critical Error: Failed to read from /dev/urandom\n");
    }
    close(fd);
#endif
}
