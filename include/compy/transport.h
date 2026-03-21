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
#define Compy_Transport_IFACE                                                  \
                                                                               \
    /*                                                                         \
     * Transmits a slice of I/O vectors @p bufs.                               \
     *                                                                         \
     * @return -1 if an I/O error occurred and sets `errno` appropriately, 0   \
     * on success.                                                             \
     */                                                                        \
    vfunc99(int, transmit, VSelf99, Compy_IoVecSlice bufs)                     \
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
    Compy_Writer w, uint8_t channel_id, size_t max_buffer) COMPY_PRIV_MUST_USE;

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
void *
compy_sockaddr_ip(const struct sockaddr *restrict addr) COMPY_PRIV_MUST_USE;

#ifdef COMPY_HAS_TLS

/**
 * SRTP crypto suite.
 */
typedef enum {
    Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80,
    Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32,
} Compy_SrtpSuite;

/**
 * SRTP keying material (master key + master salt).
 */
typedef struct {
    uint8_t master_key[16];  /**< 128-bit master key. */
    uint8_t master_salt[14]; /**< 112-bit master salt. */
} Compy_SrtpKeyMaterial;

/**
 * Creates a new SRTP transport wrapping a UDP transport.
 *
 * Encrypts outgoing RTP packets using AES-128-CM and authenticates
 * with HMAC-SHA1 per RFC 3711. Ownership of @p inner is transferred.
 *
 * @param[in] inner The UDP transport to wrap.
 * @param[in] suite The SRTP crypto suite.
 * @param[in] key The keying material (copied).
 *
 * @pre `inner.self && inner.vptr`
 * @pre `key != NULL`
 */
Compy_Transport compy_transport_srtp(
    Compy_Transport inner, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

/**
 * Creates a new SRTCP transport wrapping a UDP transport.
 *
 * Encrypts outgoing RTCP packets per RFC 3711 Section 3.4. Uses
 * SRTCP-specific key derivation labels and appends the SRTCP index
 * (with E-flag) before the authentication tag.
 *
 * @param[in] inner The UDP transport to wrap (ownership transferred).
 * @param[in] suite The crypto suite (same as for SRTP).
 * @param[in] key The keying material (same master key/salt as SRTP).
 *
 * @pre `inner.self && inner.vptr`
 * @pre `key != NULL`
 */
Compy_Transport compy_transport_srtcp(
    Compy_Transport inner, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

/**
 * Opaque SRTP/SRTCP receive-side decryption context.
 */
typedef struct Compy_SrtpRecvCtx Compy_SrtpRecvCtx;

/**
 * Creates a new SRTP receive context for decrypting incoming packets.
 *
 * Derives both SRTP and SRTCP session keys from the master key material.
 *
 * @param[in] suite The SRTP crypto suite.
 * @param[in] key The keying material (same as used for send).
 *
 * @pre `key != NULL`
 */
Compy_SrtpRecvCtx *compy_srtp_recv_new(
    Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

/**
 * Frees an SRTP receive context.
 */
void compy_srtp_recv_free(Compy_SrtpRecvCtx *ctx);

/**
 * Decrypts and authenticates an incoming SRTP packet in-place.
 *
 * Verifies the HMAC authentication tag, strips it, and decrypts the
 * payload. On success, @p len is updated to the decrypted packet size
 * (original size minus auth tag).
 *
 * @param[in] ctx The SRTP receive context.
 * @param[in,out] data The SRTP packet (modified in-place).
 * @param[in,out] len Packet length (reduced on success).
 *
 * @return 0 on success, -1 on authentication failure or parse error.
 */
int compy_srtp_recv_unprotect(
    Compy_SrtpRecvCtx *ctx, uint8_t *data, size_t *len) COMPY_PRIV_MUST_USE;

/**
 * Decrypts and authenticates an incoming SRTCP packet in-place.
 *
 * Verifies the HMAC authentication tag, strips the tag and SRTCP index,
 * and decrypts the payload. On success, @p len is updated to the
 * decrypted RTCP packet size.
 *
 * @param[in] ctx The SRTP receive context.
 * @param[in,out] data The SRTCP packet (modified in-place).
 * @param[in,out] len Packet length (reduced on success).
 *
 * @return 0 on success, -1 on authentication failure or parse error.
 */
int compy_srtcp_recv_unprotect(
    Compy_SrtpRecvCtx *ctx, uint8_t *data, size_t *len) COMPY_PRIV_MUST_USE;

/**
 * Generates random SRTP master key and salt.
 *
 * @return 0 on success, -1 if CSPRNG fails.
 */
int compy_srtp_generate_key(Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

/**
 * Formats an SDP `a=crypto:` attribute value.
 *
 * Output format: `"<tag> <suite> inline:<base64(key||salt)>"`
 *
 * @param[out] buf Output buffer (at least 128 bytes recommended).
 * @param[in] buf_len Size of @p buf.
 * @param[in] tag The crypto tag number (usually 1).
 * @param[in] suite The SRTP crypto suite.
 * @param[in] key The keying material.
 *
 * @return Characters written, or -1 on error.
 */
int compy_srtp_format_crypto_attr(
    char *buf, size_t buf_len, int tag, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

/**
 * Parses an SDP `a=crypto:` attribute value.
 *
 * @param[in] attr_value The attribute value string.
 * @param[out] suite The parsed crypto suite.
 * @param[out] key The parsed keying material.
 *
 * @return 0 on success, -1 on parse error.
 */
int compy_srtp_parse_crypto_attr(
    const char *attr_value, Compy_SrtpSuite *suite,
    Compy_SrtpKeyMaterial *key) COMPY_PRIV_MUST_USE;

#endif /* COMPY_HAS_TLS */
