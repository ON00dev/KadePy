#include "holepunch.h"
#include <stdio.h>
#include <string.h>

void holepunch_init(holepunch_t* hp) {
    hp->state = HOLEPUNCH_STATE_IDLE;
    hp->attempts = 0;
    memset(hp->target_ip, 0, 16);
    hp->target_port = 0;
    hp->last_attempt_time = 0;
}

void holepunch_start(holepunch_t* hp, const char* target_ip, int target_port) {
    hp->state = HOLEPUNCH_STATE_PUNCHING;
    hp->attempts = 0;
    strncpy(hp->target_ip, target_ip, 15);
    hp->target_port = target_port;
    hp->last_attempt_time = 0;
    printf("[Holepunch] Starting holepunch to %s:%d\n", target_ip, target_port);
}

void holepunch_tick(holepunch_t* hp, udx_socket_t* udx) {
    if (hp->state != HOLEPUNCH_STATE_PUNCHING) return;

    uint32_t now = udx_get_time_ms();

    // Send every 500ms
    if (now - hp->last_attempt_time < 500) {
        return;
    }

    if (hp->attempts >= 10) {
        printf("[Holepunch] Given up on %s:%d (Max attempts reached)\n", hp->target_ip, hp->target_port);
        hp->state = HOLEPUNCH_STATE_FAILED;
        return;
    }

    // Send a holepunch packet
    const char* payload = "HOLEPUNCH";
    udx_send(udx, hp->target_ip, hp->target_port, UDX_TYPE_HOLEPUNCH, (const uint8_t*)payload, strlen(payload), NULL);
    
    hp->attempts++;
    hp->last_attempt_time = now;
    printf("[Holepunch] Sent packet %d to %s:%d\n", hp->attempts, hp->target_ip, hp->target_port);
}

void holepunch_on_packet(holepunch_t* hp, const char* ip, int port) {
    // If we receive ANY packet from the target, we consider the hole punched
    if (hp->state == HOLEPUNCH_STATE_PUNCHING) {
        if (strcmp(hp->target_ip, ip) == 0 && hp->target_port == port) {
            printf("[Holepunch] Success! Connected to %s:%d\n", ip, port);
            hp->state = HOLEPUNCH_STATE_CONNECTED;
        }
    }
}
