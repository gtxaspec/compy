/**
 * @file
 * @brief RTSPS (RTSP over TLS) support.
 *
 * Provides server-side TLS for encrypted RTSP signaling. The application
 * creates a TLS context (loading cert/key), accepts TLS connections, and
 * uses the TLS writer for encrypted I/O. The TLS reader decrypts incoming
 * RTSP requests.
 *
 * Requires a compiled TLS backend (wolfSSL, mbedTLS, OpenSSL, or BearSSL).
 */

#pragma once

#include <compy/writer.h>

#include <stddef.h>

#include <unistd.h>

#include <compy/priv/compiler_attrs.h>

/**
 * TLS configuration.
 */
typedef struct {
    const char *cert_path; /**< Path to PEM certificate file. */
    const char *key_path;  /**< Path to PEM private key file. */
} Compy_TlsConfig;

/**
 * Opaque TLS context (holds loaded cert/key, shared across connections).
 */
typedef struct Compy_TlsContext Compy_TlsContext;

/**
 * Creates a new TLS context from configuration.
 *
 * @param[in] config Certificate and key file paths.
 *
 * @pre `config.cert_path != NULL`
 * @pre `config.key_path != NULL`
 *
 * @return A new TLS context, or NULL on failure (cert/key load error).
 */
Compy_TlsContext *
Compy_TlsContext_new(Compy_TlsConfig config) COMPY_PRIV_MUST_USE;

/**
 * Frees a TLS context.
 */
void Compy_TlsContext_free(Compy_TlsContext *ctx);

/**
 * TLS ciphersuite preference presets.
 *
 * Selected via Compy_TlsContext_set_cipher_preference(). These let the
 * application steer the server toward a particular cipher family when
 * the choice has meaningful performance or compliance implications on
 * the target platform. The actual TLS cipher suite enums are managed
 * inside compy; applications only pick the preset.
 */
typedef enum {
    /** Backend defaults (typically GCM-first in TLS 1.3). */
    COMPY_TLS_CIPHER_DEFAULT = 0,

    /**
     * TLS 1.3: allow only CHACHA20-POLY1305-SHA256. Clients that offer
     * it will select it; clients that don't fall back to TLS 1.2.
     *
     * TLS 1.2: prefer AES-CBC-SHA256 → CCM → GCM (order chosen for
     * servers where AES runs through a hardware engine like /dev/aes
     * but there is no HW GHASH, so the CBC/CCM path has less per-record
     * overhead than GCM).
     *
     * Motivation: on Ingenic T-series and similar slow MIPS SoCs each
     * AES-GCM TLS record pays an ioctl + DMA setup + 4-bit-table GHASH.
     * Scalar ChaCha20-Poly1305 in userspace wins at typical RTSP-over-
     * TLS bitrates (~1-5 Mbps) because it has no per-record fixed cost.
     */
    COMPY_TLS_CIPHER_CHACHA20_ONLY,
} Compy_TlsCipherPreference;

/**
 * Sets the ciphersuite preference for all connections accepted through
 * this context. Must be called before the first Compy_TlsConn_accept().
 *
 * @param[in] ctx TLS context.
 * @param[in] pref Preset to apply.
 *
 * @return 0 on success; -1 if the backend does not support ciphersuite
 *         preference, or if @p pref is not a recognized preset.
 *
 * @pre `ctx != NULL`
 */
int Compy_TlsContext_set_cipher_preference(
    Compy_TlsContext *ctx, Compy_TlsCipherPreference pref);

/**
 * Opaque per-connection TLS state.
 */
typedef struct Compy_TlsConn Compy_TlsConn;

/**
 * Performs server-side TLS handshake on @p fd.
 *
 * This is a blocking call that completes the full TLS handshake.
 *
 * @param[in] ctx The TLS context with loaded cert/key.
 * @param[in] fd The connected socket file descriptor.
 *
 * @pre `ctx != NULL`
 * @pre `fd >= 0`
 *
 * @return A new TLS connection, or NULL on handshake failure.
 */
Compy_TlsConn *
Compy_TlsConn_accept(Compy_TlsContext *ctx, int fd) COMPY_PRIV_MUST_USE;

/**
 * Creates a Compy_Writer backed by a TLS connection.
 *
 * All data written through this writer is encrypted via TLS before
 * being sent on the underlying socket.
 *
 * @param[in] conn The TLS connection.
 *
 * @pre `conn != NULL`
 */
Compy_Writer compy_tls_writer(Compy_TlsConn *conn) COMPY_PRIV_MUST_USE;

/**
 * Reads decrypted data from a TLS connection.
 *
 * Used by the event-loop integration to read RTSP requests arriving
 * over an encrypted connection.
 *
 * @param[in] conn The TLS connection.
 * @param[out] buf Buffer to read into.
 * @param[in] len Maximum bytes to read.
 *
 * @pre `conn != NULL`
 * @pre `buf != NULL`
 *
 * @return Bytes read, 0 on EOF, -1 on error.
 */
ssize_t
compy_tls_read(Compy_TlsConn *conn, void *buf, size_t len) COMPY_PRIV_MUST_USE;

/**
 * Shuts down the TLS connection gracefully.
 *
 * @return 0 on success, -1 on error.
 */
int compy_tls_shutdown(Compy_TlsConn *conn);

/**
 * Frees per-connection TLS state.
 */
void Compy_TlsConn_free(Compy_TlsConn *conn);
