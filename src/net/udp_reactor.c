#include "udp_reactor.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <process.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define SOCKET_ERRNO WSAGetLastError()
    
    static int g_winsock_initialized = 0;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <pthread.h>
    #include <errno.h>
    #include <string.h>

    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET(s) close(s)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define SOCKET_ERRNO errno
#endif

#include "../dht/protocol.h"

static socket_t g_socket = INVALID_SOCKET;
static int g_running = 0;
static uint16_t g_port = 0;

// Thread function
#ifdef _WIN32
void reactor_loop(void *arg) {
#else
void* reactor_loop(void *arg) {
#endif
    (void)arg; // Unused
    char buffer[4096];
    struct sockaddr_in client_addr;
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    printf("[C-Thread] UDP Reactor loop started.\n");

    while (g_running) {
        if (!IS_VALID_SOCKET(g_socket)) break;

        // Use select to avoid blocking forever and allow clean shutdown
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_socket, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select((int)g_socket + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            printf("[C-Thread] Select error: %d\n", SOCKET_ERRNO);
            break;
        }
        
        if (activity == 0) {
            continue; // Timeout
        }

        int bytes_received = recvfrom(g_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        
        if (bytes_received == SOCKET_ERROR) {
            printf("[C-Thread] recvfrom failed: %d\n", SOCKET_ERRNO);
        } else {
            // Passar para o handler DHT
            // IP já vem em Network Byte Order do recvfrom
            // Mas nossa struct dht_node_t usa Host Byte Order
            // E o handler espera Host Byte Order para facilitar
            
            // ntohl para converter Network (Big Endian) -> Host (Little Endian on x86)
            dht_handle_packet((uint8_t*)buffer, bytes_received, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
        }
    }
    printf("[C-Thread] Reactor loop finished.\n");
#ifndef _WIN32
    return NULL;
#endif
}

int udp_reactor_start(int *port) {
#ifdef _WIN32
    if (!g_winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return -1;
        }
        g_winsock_initialized = 1;
    }
#endif

    if (IS_VALID_SOCKET(g_socket)) {
        CLOSE_SOCKET(g_socket);
    }

    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!IS_VALID_SOCKET(g_socket)) {
        return -1;
    }

    // Set non-blocking (optional, but we use select)
    // u_long mode = 1;
    // ioctlsocket(g_socket, FIONBIO, &mode);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)*port);

    if (bind(g_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(g_socket);
        return -1;
    }

    // Get assigned port if 0
    if (*port == 0) {
#ifdef _WIN32
        int len = sizeof(server_addr);
#else
        socklen_t len = sizeof(server_addr);
#endif
        if (getsockname(g_socket, (struct sockaddr*)&server_addr, &len) != SOCKET_ERROR) {
            *port = ntohs(server_addr.sin_port);
            printf("[C-Reactor] Bound to random port: %d\n", *port);
        }
    }
    g_port = (uint16_t)*port;
    g_running = 1;

    // Start Thread
#ifdef _WIN32
    _beginthread(reactor_loop, 0, NULL);
#else
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, reactor_loop, NULL) != 0) {
        CLOSE_SOCKET(g_socket);
        return -1;
    }
    pthread_detach(thread_id); // Detach so we don't need to join
#endif

    return 0;
}

void udp_reactor_stop() {
    g_running = 0;
    if (IS_VALID_SOCKET(g_socket)) {
        CLOSE_SOCKET(g_socket);
        g_socket = INVALID_SOCKET;
    }
#ifdef _WIN32
    if (g_winsock_initialized) {
        WSACleanup();
        g_winsock_initialized = 0;
    }
#endif
}

int udp_reactor_send(uint32_t ip, uint16_t port, const uint8_t *data, int len) {
    if (!IS_VALID_SOCKET(g_socket)) return -1;

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = htonl(ip); // Convert Host to Network
    dest_addr.sin_port = htons(port);      // Convert Host to Network

    int sent = sendto(g_socket, (const char*)data, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent == SOCKET_ERROR) {
        printf("[C-Reactor] sendto failed: %d\n", SOCKET_ERRNO);
        return -1;
    }
    return sent;
}
