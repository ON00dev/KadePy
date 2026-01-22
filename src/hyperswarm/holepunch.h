#ifndef HOLEPUNCH_H
#define HOLEPUNCH_H

#include "udx.h"

#define HOLEPUNCH_STATE_IDLE 0
#define HOLEPUNCH_STATE_PUNCHING 1
#define HOLEPUNCH_STATE_CONNECTED 2
#define HOLEPUNCH_STATE_FAILED 3

typedef struct {
    int state;
    int attempts;
    char target_ip[16];
    int target_port;
    uint32_t last_attempt_time;
} holepunch_t;

void holepunch_init(holepunch_t* hp);
void holepunch_start(holepunch_t* hp, const char* target_ip, int target_port);
void holepunch_tick(holepunch_t* hp, udx_socket_t* udx);
void holepunch_on_packet(holepunch_t* hp, const char* ip, int port);

#endif // HOLEPUNCH_H
