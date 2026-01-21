#ifndef UDP_REACTOR_H
#define UDP_REACTOR_H

#include <stdint.h>

// Envia um pacote UDP para o IP/Porta especificados
int udp_reactor_send(uint32_t ip, uint16_t port, const uint8_t *data, int len);

// Inicia o reactor em uma thread separada.
// Se *port for 0, escolhe uma porta aleatória e atualiza *port.
// Retorna 0 em sucesso, erro < 0.
int udp_reactor_start(int *port);

// Para o reactor e fecha o socket.
void udp_reactor_stop();

#endif
