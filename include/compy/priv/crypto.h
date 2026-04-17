/**
 * @file
 * @brief Backend-agnostic cryptographic operations for TLS and SRTP.
 *
 * This header defines function pointer structs that each TLS backend
 * (wolfSSL, mbedTLS, OpenSSL) implements. The compiled backend
 * populates the extern const singletons. Only one backend is linked at
 * a time, selected via CMake options.
 *
 * This is a private header; do not include directly from application code.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <unistd.h>

/**
 * Opaque TLS context handle (backend-specific).
 */
typedef void Compy_CryptoTlsCtx;

/**
 * Opaque TLS connection handle (backend-specific).
 */
typedef void Compy_CryptoTlsConn;

/**
 * TLS operations provided by the compiled backend.
 */
typedef struct {
    /** Create a server TLS context from PEM cert and key files. */
    Compy_CryptoTlsCtx *(*ctx_new)(const char *cert_path, const char *key_path);

    /** Free a TLS context. */
    void (*ctx_free)(Compy_CryptoTlsCtx *ctx);

    /** Perform server-side TLS handshake on fd. Returns NULL on failure. */
    Compy_CryptoTlsConn *(*accept)(Compy_CryptoTlsCtx *ctx, int fd);

    /** Free a TLS connection. */
    void (*conn_free)(Compy_CryptoTlsConn *conn);

    /** Write plaintext data through TLS. Returns bytes written or -1. */
    ssize_t (*write)(Compy_CryptoTlsConn *conn, const void *data, size_t len);

    /** Read decrypted data from TLS. Returns bytes read, 0 on EOF, -1 on
     * error. */
    ssize_t (*read)(Compy_CryptoTlsConn *conn, void *buf, size_t len);

    /** Graceful TLS shutdown. Returns 0 on success. */
    int (*shutdown)(Compy_CryptoTlsConn *conn);

    /** Bytes buffered internally by the TLS library (for filled()). */
    size_t (*pending)(Compy_CryptoTlsConn *conn);

    /**
     * Apply a ciphersuite preference preset to the context. Optional:
     * backends that leave this NULL signal that cipher preference is
     * unsupported. The @p pref value is a Compy_TlsCipherPreference
     * cast to int; kept as int here so this private header does not
     * depend on the public tls.h enum for ABI stability.
     *
     * Must be callable before the first accept() on the context.
     * Returns 0 on success, -1 on unknown preset or backend error.
     */
    int (*ctx_set_cipher_preference)(Compy_CryptoTlsCtx *ctx, int pref);
} Compy_CryptoTlsOps;

/**
 * SRTP cryptographic operations provided by the compiled backend.
 *
 * These primitives are used to implement SRTP (RFC 3711) without
 * depending on libsrtp.
 */
typedef struct {
    /**
     * AES-128 ECB encrypt a single 16-byte block.
     * Used for key derivation (srtp_kdf).
     */
    void (*aes128_ecb)(
        const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

    /**
     * AES-128 CTR mode encrypt/decrypt in-place.
     * The 16-byte IV has a counter at bytes [14..15] starting from 0.
     * Used for SRTP payload encryption (called per-packet).
     */
    void (*aes128_ctr)(
        const uint8_t key[16], const uint8_t iv[16], uint8_t *data, size_t len);

    /**
     * HMAC-SHA1. Writes 20-byte digest to @p out.
     */
    void (*hmac_sha1)(
        const uint8_t *key, size_t key_len, const uint8_t *data,
        size_t data_len, uint8_t out[20]);

    /**
     * Fill buffer with cryptographically secure random bytes.
     * Returns 0 on success, -1 on failure.
     */
    int (*random_bytes)(uint8_t *buf, size_t len);
} Compy_CryptoSrtpOps;

/**
 * The TLS operations singleton, defined by the compiled backend.
 */
extern const Compy_CryptoTlsOps compy_crypto_tls_ops;

/**
 * The SRTP operations singleton, defined by the compiled backend.
 */
extern const Compy_CryptoSrtpOps compy_crypto_srtp_ops;
