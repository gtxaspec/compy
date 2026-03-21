/**
 * @file
 * @brief RTSP data transport (level 4) implementations.
 */

#pragma once

#include <compy/droppable.h>
#include <compy/io_vec.h>
#include <compy/writer.h>

#include <interface99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * A transport-level RTSP data transmitter.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
#define Compy_Transport_IFACE                                               \
                                                                               \
    /*                                                                         \
     * Transmits a slice of I/O vectors @p bufs.                               \
     *                                                                         \
     * @return -1 if an I/O error occurred and sets `errno` appropriately, 0   \
     * on success.                                                             \
     */                                                                        \
    vfunc99(int, transmit, VSelf99, Compy_IoVecSlice bufs)                  \
    vfunc99(bool, is_full, VSelf99)

/**
 * The superinterfaces of #Compy_Transport_IFACE.
 */
#define Compy_Transport_EXTENDS (Compy_Droppable)

/**
 * Defines the `Compy_Transport` interface.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
interface99(Compy_Transport);

/**
 * Creates a new TCP transport.
 *
 * @param[in] w The writer to be provided with data.
 * @param[in] channel_id A one-byte channel identifier, as defined in
 * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.12>.
 *
 * @pre `w.self && w.vptr`
 */
Compy_Transport compy_transport_tcp(
    Compy_Writer w, uint8_t channel_id,
    size_t max_buffer) COMPY_PRIV_MUST_USE;

/**
 * Creates a new UDP transport.
 *
 * Strictly speaking, it can handle any datagram-oriented protocol, not
 * necessarily UDP. E.g., you may use a `SOCK_SEQPACKET` socket for local
 * communication.
 *
 * @param[in] fd The socket file descriptor to be provided with data.
 *
 * @pre `fd >= 0`
 */
Compy_Transport compy_transport_udp(int fd) COMPY_PRIV_MUST_USE;

/**
 * Creates a new datagram socket suitable for #compy_transport_udp.
 *
 * The algorithm is:
 *  1. Create a socket using `socket(af, SOCK_DGRAM, 0)`.
 *  2. Connect this socket to @p addr with @p port.
 *  3. Set the `IP_PMTUDISC_WANT` option to allow IP fragmentation.
 *
 * @param[in] af The socket namespace. Can be `AF_INET` or `AF_INET6`; if none
 * of them, returns -1 and sets `errno` to `EAFNOSUPPORT`.
 * @param[in] addr The destination IP address: `struct in_addr` for `AF_INET`
 * and `struct in6_addr` for `AF_INET6`.
 * @param[in] port The destination IP port in the host byte order.
 *
 * @return A valid file descriptor or -1 on error (and sets `errno`
 * appropriately).
 */
int compy_dgram_socket(int af, const void *restrict addr, uint16_t port)
    COMPY_PRIV_MUST_USE;

/**
 * Creates a new datagram socket bound to a local port for receiving.
 *
 * Mirrors #compy_dgram_socket (which creates a connected send socket).
 * Suitable for receiving backchannel RTP or RTCP data.
 *
 * @param[in] af The address family (`AF_INET` or `AF_INET6`).
 * @param[in] port The local port to bind to (host byte order).
 *
 * @return A valid file descriptor or -1 on error.
 */
int compy_recv_dgram_socket(int af, uint16_t port) COMPY_PRIV_MUST_USE;

/**
 * Returns a pointer to the IP address of @p addr.
 *
 * Currently, only `AF_INET` and `AF_INET6` are supported. Otherwise, `NULL` is
 * returned.
 *
 * @pre `addr != NULL`
 */
void *compy_sockaddr_ip(const struct sockaddr *restrict addr)
    COMPY_PRIV_MUST_USE;
