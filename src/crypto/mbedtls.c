#include <compy/priv/crypto.h>
#include <compy/tls.h> /* Compy_TlsCipherPreference enum */

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <mbedtls/version.h>

#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

/*
 * mbedTLS 4.0 uses PSA Crypto internally for RNG, removing ctr_drbg
 * and entropy from the public API. Version 3.x requires explicit RNG
 * setup.
 */
/*
 * mbedTLS version handling:
 *   2.x: legacy crypto, pk_parse_keyfile(ctx, path, pwd) — 3 args
 *   3.x: legacy crypto, pk_parse_keyfile(ctx, path, pwd, rng, rng_ctx) — 5 args
 *   4.x: PSA Crypto, pk_parse_keyfile(ctx, path, pwd) — 3 args
 */
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include <psa/crypto.h>
#else
/* mbedTLS 2.x and 3.x use legacy crypto APIs */
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#define COMPY_MBEDTLS_LEGACY
#endif

/* Only 3.x added RNG param to pk_parse_keyfile */
#if MBEDTLS_VERSION_NUMBER >= 0x03000000 && MBEDTLS_VERSION_NUMBER < 0x04000000
#define COMPY_MBEDTLS_PK_NEEDS_RNG
#endif

/* --- TLS operations --- */

typedef struct {
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pkey;
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
#endif
} MbedTlsCtx;

typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_net_context net;
} MbedTlsConn;

static Compy_CryptoTlsCtx *
mbed_ctx_new(const char *cert_path, const char *key_path) {
#ifndef COMPY_MBEDTLS_LEGACY
    /* mbedTLS 4.x: PSA must be initialized before any crypto ops */
    psa_crypto_init();
#endif

    MbedTlsCtx *self = calloc(1, sizeof *self);
    if (!self)
        return NULL;

    mbedtls_ssl_config_init(&self->conf);
    mbedtls_x509_crt_init(&self->cert);
    mbedtls_pk_init(&self->pkey);

#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_entropy_init(&self->entropy);
    mbedtls_ctr_drbg_init(&self->ctr_drbg);

    if (mbedtls_ctr_drbg_seed(
            &self->ctr_drbg, mbedtls_entropy_func, &self->entropy,
            (const unsigned char *)"compy", 5) != 0) {
        goto fail;
    }
#endif

    if (mbedtls_x509_crt_parse_file(&self->cert, cert_path) != 0)
        goto fail;

#ifdef COMPY_MBEDTLS_PK_NEEDS_RNG
    /* mbedTLS 3.x: pk_parse_keyfile requires RNG args */
    if (mbedtls_pk_parse_keyfile(
            &self->pkey, key_path, NULL, mbedtls_ctr_drbg_random,
            &self->ctr_drbg) != 0)
        goto fail;
#else
    /* mbedTLS 2.x and 4.x: 3-arg form */
    if (mbedtls_pk_parse_keyfile(&self->pkey, key_path, NULL) != 0)
        goto fail;
#endif

    if (mbedtls_ssl_config_defaults(
            &self->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        goto fail;
    }

#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_ssl_conf_rng(&self->conf, mbedtls_ctr_drbg_random, &self->ctr_drbg);
#endif
    mbedtls_ssl_conf_ca_chain(&self->conf, self->cert.next, NULL);
    if (mbedtls_ssl_conf_own_cert(&self->conf, &self->cert, &self->pkey) != 0) {
        goto fail;
    }

    return self;

fail:
    mbedtls_pk_free(&self->pkey);
    mbedtls_x509_crt_free(&self->cert);
    mbedtls_ssl_config_free(&self->conf);
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_entropy_free(&self->entropy);
    mbedtls_ctr_drbg_free(&self->ctr_drbg);
#endif
    free(self);
    return NULL;
}

static void mbed_ctx_free(Compy_CryptoTlsCtx *ctx) {
    MbedTlsCtx *c = ctx;
    if (c) {
        mbedtls_pk_free(&c->pkey);
        mbedtls_x509_crt_free(&c->cert);
        mbedtls_ssl_config_free(&c->conf);
#ifdef COMPY_MBEDTLS_LEGACY
        mbedtls_entropy_free(&c->entropy);
        mbedtls_ctr_drbg_free(&c->ctr_drbg);
#endif
        free(c);
    }
}

/*
 * Ciphersuite preference presets. mbedtls stores the passed list by
 * pointer, so it must have static storage duration.
 *
 * CHACHA20_ONLY list:
 *   TLS 1.3: only CHACHA20-POLY1305-SHA256 — forces clients that offer
 *       it to pick it; others fall back to TLS 1.2.
 *   TLS 1.2: ordered for best performance on servers with HW AES but
 *       no HW GHASH — CBC-SHA256 → CCM → GCM, then ChaCha20 as tail.
 */
static const int mbed_cipher_chacha20_only[] = {
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
#endif
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_RSA_WITH_AES_128_CCM_8,
    MBEDTLS_TLS_RSA_WITH_AES_128_CCM,
    MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
    0};

static int mbed_ctx_set_cipher_preference(Compy_CryptoTlsCtx *ctx, int pref) {
    MbedTlsCtx *c = ctx;
    switch ((Compy_TlsCipherPreference)pref) {
    case COMPY_TLS_CIPHER_DEFAULT:
        /* Passing NULL restores mbedtls's built-in default list. */
        mbedtls_ssl_conf_ciphersuites(&c->conf, NULL);
        return 0;
    case COMPY_TLS_CIPHER_CHACHA20_ONLY:
        mbedtls_ssl_conf_ciphersuites(&c->conf, mbed_cipher_chacha20_only);
        /* Server-preference order affects TLS 1.2 selection only;
         * TLS 1.3 is steered by narrowing the allow-list above.
         * The API is only compiled into mbedtls when both TLS 1.2 and
         * the server role are enabled — some stripped-down builds omit
         * it, in which case the allow-list alone still does the right
         * thing (the client's TLS 1.2 preference picks from a set we
         * already biased toward HW-friendly suites). */
#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_PROTO_TLS1_2)
        mbedtls_ssl_conf_preference_order(
            &c->conf, MBEDTLS_SSL_SRV_CIPHERSUITE_ORDER_SERVER);
#endif
        return 0;
    }
    return -1;
}

static Compy_CryptoTlsConn *mbed_accept(Compy_CryptoTlsCtx *ctx, int fd) {
    MbedTlsCtx *c = ctx;

    struct timeval tv = {.tv_sec = 10, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    MbedTlsConn *conn = calloc(1, sizeof *conn);
    if (!conn) {
        tv = (struct timeval){0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        return NULL;
    }

    mbedtls_ssl_init(&conn->ssl);
    mbedtls_net_init(&conn->net);
    conn->net.fd = fd;

    if (mbedtls_ssl_setup(&conn->ssl, &c->conf) != 0) {
        free(conn);
        tv = (struct timeval){0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        return NULL;
    }

    mbedtls_ssl_set_bio(
        &conn->ssl, &conn->net, mbedtls_net_send, mbedtls_net_recv, NULL);

    int ret;
    while ((ret = mbedtls_ssl_handshake(&conn->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_ssl_free(&conn->ssl);
            free(conn);
            tv = (struct timeval){0};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            return NULL;
        }
    }

    tv = (struct timeval){0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    return conn;
}

static void mbed_conn_free(Compy_CryptoTlsConn *conn) {
    MbedTlsConn *c = conn;
    if (c) {
        mbedtls_ssl_free(&c->ssl);
        free(c);
    }
}

static ssize_t
mbed_write(Compy_CryptoTlsConn *conn, const void *data, size_t len) {
    MbedTlsConn *c = conn;
    const uint8_t *p = data;
    size_t remaining = len;
    while (remaining > 0) {
        int ret = mbedtls_ssl_write(&c->ssl, p, remaining);
        if (ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
            ret == MBEDTLS_ERR_SSL_WANT_READ)
            continue;
        if (ret < 0)
            return -1;
        p += ret;
        remaining -= (size_t)ret;
    }
    return (ssize_t)len;
}

static ssize_t mbed_read(Compy_CryptoTlsConn *conn, void *buf, size_t len) {
    MbedTlsConn *c = conn;
    int ret = mbedtls_ssl_read(&c->ssl, buf, len);
    if (ret >= 0)
        return (ssize_t)ret;
    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        return 0;
    return -1;
}

static int mbed_shutdown(Compy_CryptoTlsConn *conn) {
    MbedTlsConn *c = conn;
    return mbedtls_ssl_close_notify(&c->ssl) == 0 ? 0 : -1;
}

static size_t mbed_pending(Compy_CryptoTlsConn *conn) {
    MbedTlsConn *c = conn;
    return mbedtls_ssl_get_bytes_avail(&c->ssl);
}

/* --- SRTP operations --- */

static void
mbed_aes128_ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in, out);
    mbedtls_aes_free(&aes);
#else
    /* mbedTLS 4.x: use PSA Crypto */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 128);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);

    psa_key_id_t kid;
    psa_import_key(&attr, key, 16, &kid);

    size_t olen;
    psa_cipher_encrypt(kid, PSA_ALG_ECB_NO_PADDING, in, 16, out, 16, &olen);
    psa_destroy_key(kid);
#endif
}

static void mbed_aes128_ctr(
    const uint8_t key[16], const uint8_t iv[16], uint8_t *data, size_t len) {
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    size_t nc_off = 0;
    uint8_t nonce_counter[16];
    uint8_t stream_block[16] = {0};
    memcpy(nonce_counter, iv, 16);
    mbedtls_aes_crypt_ctr(
        &aes, len, &nc_off, nonce_counter, stream_block, data, data);
    mbedtls_aes_free(&aes);
#else
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 128);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_CTR);

    psa_key_id_t kid;
    if (psa_import_key(&attr, key, 16, &kid) != PSA_SUCCESS)
        return;

    psa_cipher_operation_t op = PSA_CIPHER_OPERATION_INIT;
    if (psa_cipher_encrypt_setup(&op, kid, PSA_ALG_CTR) != PSA_SUCCESS ||
        psa_cipher_set_iv(&op, iv, 16) != PSA_SUCCESS) {
        psa_cipher_abort(&op);
        psa_destroy_key(kid);
        return;
    }

    size_t olen;
    if (psa_cipher_update(&op, data, len, data, len, &olen) != PSA_SUCCESS) {
        psa_cipher_abort(&op);
        psa_destroy_key(kid);
        return;
    }
    psa_cipher_finish(&op, data + olen, len - olen, &olen);
    psa_destroy_key(kid);
#endif
}

static void mbed_hmac_sha1(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[20]) {
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), key, key_len, data,
        data_len, out);
#else
    /* mbedTLS 4.x: use PSA MAC */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_1));

    psa_key_id_t kid;
    psa_import_key(&attr, key, key_len, &kid);

    size_t mac_len;
    psa_mac_compute(
        kid, PSA_ALG_HMAC(PSA_ALG_SHA_1), data, data_len, out, 20, &mac_len);
    psa_destroy_key(kid);
#endif
}

static int mbed_random_bytes(uint8_t *buf, size_t len) {
#ifdef COMPY_MBEDTLS_LEGACY
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int ret = -1;
    if (mbedtls_ctr_drbg_seed(
            &ctr_drbg, mbedtls_entropy_func, &entropy,
            (const unsigned char *)"compy_rng", 9) == 0) {
        if (mbedtls_ctr_drbg_random(&ctr_drbg, buf, len) == 0) {
            ret = 0;
        }
    }

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
#else
    /* mbedTLS 4.x: use PSA random (ensure initialized) */
    psa_crypto_init();
    return psa_generate_random(buf, len) == PSA_SUCCESS ? 0 : -1;
#endif
}

/* --- Singletons --- */

const Compy_CryptoTlsOps compy_crypto_tls_ops = {
    .ctx_new = mbed_ctx_new,
    .ctx_free = mbed_ctx_free,
    .accept = mbed_accept,
    .conn_free = mbed_conn_free,
    .write = mbed_write,
    .read = mbed_read,
    .shutdown = mbed_shutdown,
    .pending = mbed_pending,
    .ctx_set_cipher_preference = mbed_ctx_set_cipher_preference,
};

const Compy_CryptoSrtpOps compy_crypto_srtp_ops = {
    .aes128_ecb = mbed_aes128_ecb,
    .aes128_ctr = mbed_aes128_ctr,
    .hmac_sha1 = mbed_hmac_sha1,
    .random_bytes = mbed_random_bytes,
};
