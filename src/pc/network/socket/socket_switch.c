#ifdef __SWITCH__

#include "socket_linux.h"
#include "pc/debuglog.h"
#include <string.h>

SOCKET socket_initialize(void) {
    // Switch does not support IPv6 sockets, so use IPv4 and keep the shared socket code IPv6-mapped.
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR("socket failed with error %d", SOCKET_LAST_ERROR);
        return INVALID_SOCKET;
    }

    // set non-blocking mode
    int rc = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    if (rc == (int)INVALID_SOCKET) {
        LOG_ERROR("fcntl failed with error: %d", rc);
        return INVALID_SOCKET;
    }

    LOG_INFO("socket initialized.");

    return sock;
}

void socket_shutdown(SOCKET socket) {
    if (socket == INVALID_SOCKET) { return; }
    int rc = closesocket(socket);
    if (rc == (int)SOCKET_ERROR) {
        LOG_ERROR("closesocket failed with error %d\n", SOCKET_LAST_ERROR);
    }
}

static void socket_switch_map_ipv4_to_ipv6(struct sockaddr_in6 *addr6, const struct sockaddr_in *addr4) {
    memset(addr6, 0, sizeof(*addr6));
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = addr4->sin_port;
    addr6->sin6_addr.s6_addr[10] = 0xff;
    addr6->sin6_addr.s6_addr[11] = 0xff;
    memcpy(&addr6->sin6_addr.s6_addr[12], &addr4->sin_addr, sizeof(addr4->sin_addr));
}

static int socket_switch_bind(SOCKET socket, const SOCKADDR *addr, RX_ADDR_SIZE_TYPE addrSize) {
    const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;
    struct sockaddr_in addr4;
    (void)addrSize;

    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = addr6->sin6_port;
    addr4.sin_addr.s_addr = INADDR_ANY;

    return bind(socket, (SOCKADDR *)&addr4, sizeof(addr4));
}

static int socket_switch_sendto(SOCKET socket, const char *buffer, size_t bufferLength, int flags, const struct sockaddr *addr, RX_ADDR_SIZE_TYPE addrSize) {
    const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;
    struct sockaddr_in addr4;
    (void)addrSize;

    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = addr6->sin6_port;
    memcpy(&addr4.sin_addr, &addr6->sin6_addr.s6_addr[12], sizeof(addr4.sin_addr));

    return sendto(socket, buffer, bufferLength, flags, (struct sockaddr *)&addr4, sizeof(addr4));
}

static int socket_switch_recvfrom(SOCKET socket, char *buffer, size_t bufferLength, int flags, struct sockaddr *addr, RX_ADDR_SIZE_TYPE *addrSize) {
    struct sockaddr_in addr4;
    RX_ADDR_SIZE_TYPE addr4Size = sizeof(addr4);
    int rc = recvfrom(socket, buffer, bufferLength, flags, (struct sockaddr *)&addr4, &addr4Size);

    if (rc != SOCKET_ERROR && addr != NULL) {
        socket_switch_map_ipv4_to_ipv6((struct sockaddr_in6 *)addr, &addr4);
        if (addrSize != NULL) { *addrSize = sizeof(struct sockaddr_in6); }
    }

    return rc;
}

static int socket_switch_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    struct addrinfo ipv4Hints;

    if (hints != NULL) {
        ipv4Hints = *hints;
    } else {
        memset(&ipv4Hints, 0, sizeof(ipv4Hints));
    }
    ipv4Hints.ai_family = AF_INET;

    return getaddrinfo(node, service, &ipv4Hints, res);
}

#define bind socket_switch_bind
#define sendto socket_switch_sendto
#define recvfrom socket_switch_recvfrom
#define getaddrinfo socket_switch_getaddrinfo

#include "socket.c"

#undef getaddrinfo
#undef recvfrom
#undef sendto
#undef bind

#endif
