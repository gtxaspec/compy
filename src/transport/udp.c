#include <compy/transport.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_RETRANSMITS 10

typedef struct {
    int fd;
} Compy_UdpTransport;

declImpl(Compy_Transport, Compy_UdpTransport);

static int send_packet(Compy_UdpTransport *self, struct msghdr message);
static int
new_sockaddr(struct sockaddr *addr, int af, const void *ip, uint16_t port);

Compy_Transport compy_transport_udp(int fd) {
    assert(fd >= 0);

    Compy_UdpTransport *self = malloc(sizeof *self);
    assert(self);
    self->fd = fd;

    return DYN(Compy_UdpTransport, Compy_Transport, self);
}

static void Compy_UdpTransport_drop(VSelf) {
    VSELF(Compy_UdpTransport);
    assert(self);

    free(self);
}

impl(Compy_Droppable, Compy_UdpTransport);

static int Compy_UdpTransport_transmit(VSelf, Compy_IoVecSlice bufs) {
    VSELF(Compy_UdpTransport);
    assert(self);

    const struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = bufs.ptr,
        .msg_iovlen = bufs.len,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
    };

    return send_packet(self, msg);
}

static bool Compy_UdpTransport_is_full(VSelf) {
    VSELF(Compy_UdpTransport);
    (void)self;

    return false;
}

impl(Compy_Transport, Compy_UdpTransport);

static int send_packet(Compy_UdpTransport *self, struct msghdr message) {
    // Try to retransmit a packet several times on `EMSGSIZE`. The kernel
    // will fragment an IP packet because if `IP_PMTUDISC_WANT` is set.
    size_t i = MAX_RETRANSMITS;
    do {
        const ssize_t ret = sendmsg(self->fd, &message, 0);

        if (ret != -1) {
            return 0;
        }
        if (EMSGSIZE != errno) {
            return -1;
        }

        assert(-1 == ret);
        assert(EMSGSIZE == errno);

        // Try to retransmit the packet one more time.
        i--;
    } while (i > 0);

    return -1;
}

int compy_dgram_socket(int af, const void *restrict addr, uint16_t port) {
    struct sockaddr_storage dest;
    memset(&dest, '\0', sizeof dest);
    if (new_sockaddr((struct sockaddr *)&dest, af, addr, port) == -1) {
        return -1;
    }

    int fd;
    if ((fd = socket(af, SOCK_DGRAM, 0)) == -1) {
        goto fail;
    }

    if (connect(
            fd, (const struct sockaddr *)&dest,
            AF_INET == af ? sizeof(struct sockaddr_in)
                          : sizeof(struct sockaddr_in6)) == -1) {
        perror("connect");
        goto fail;
    }

    const int enable_pmtud = 1;
    if (setsockopt(
            fd, IPPROTO_IP, IP_PMTUDISC_WANT, &enable_pmtud,
            sizeof enable_pmtud) == -1) {
        perror("setsockopt IP_PMTUDISC_WANT");
        goto fail;
    }

    return fd;

fail:
    close(fd);
    return -1;
}

static int
new_sockaddr(struct sockaddr *addr, int af, const void *ip, uint16_t port) {
    switch (af) {
    case AF_INET: {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        addr_in->sin_family = AF_INET;
        memcpy(&addr_in->sin_addr, ip, sizeof(struct in_addr));
        addr_in->sin_port = htons(port);
        return 0;
    }
    case AF_INET6: {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        addr_in6->sin6_family = AF_INET6;
        memcpy(&addr_in6->sin6_addr, ip, sizeof(struct in6_addr));
        addr_in6->sin6_port = htons(port);
        return 0;
    }
    default:
        errno = EAFNOSUPPORT;
        return -1;
    }
}

int compy_recv_dgram_socket(int af, uint16_t port) {
    struct sockaddr_storage bind_addr;
    memset(&bind_addr, 0, sizeof bind_addr);

    socklen_t addr_len;

    switch (af) {
    case AF_INET: {
        struct sockaddr_in *a = (struct sockaddr_in *)&bind_addr;
        a->sin_family = AF_INET;
        a->sin_addr.s_addr = htonl(INADDR_ANY);
        a->sin_port = htons(port);
        addr_len = sizeof(struct sockaddr_in);
        break;
    }
    case AF_INET6: {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&bind_addr;
        a->sin6_family = AF_INET6;
        a->sin6_addr = in6addr_any;
        a->sin6_port = htons(port);
        addr_len = sizeof(struct sockaddr_in6);
        break;
    }
    default:
        errno = EAFNOSUPPORT;
        return -1;
    }

    int fd;
    if ((fd = socket(af, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }

    const int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    if (bind(fd, (const struct sockaddr *)&bind_addr, addr_len) == -1) {
        close(fd);
        return -1;
    }

    return fd;
}

void *compy_sockaddr_ip(const struct sockaddr *restrict addr) {
    assert(addr);

    switch (addr->sa_family) {
    case AF_INET:
        return (void *)&((struct sockaddr_in *)addr)->sin_addr;
    case AF_INET6:
        return (void *)&((struct sockaddr_in6 *)addr)->sin6_addr;
    default:
        return NULL;
    }
}
