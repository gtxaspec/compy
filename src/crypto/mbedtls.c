#include <compy/priv/crypto.h>

#include <stdlib.h>
#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

/* --- TLS operations --- */

typedef struct {
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pkey;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
} MbedTlsCtx;

typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_net_context net;
} MbedTlsConn;

static Compy_CryptoTlsCtx *
mbed_ctx_new(const char *cert_path, const char *key_path) {
    MbedTlsCtx *self = calloc(1, sizeof *self);
    if (!self)
        return NULL;

    mbedtls_ssl_config_init(&self->conf);
    mbedtls_x509_crt_init(&self->cert);
    mbedtls_pk_init(&self->pkey);
    mbedtls_entropy_init(&self->entropy);
    mbedtls_ctr_drbg_init(&self->ctr_drbg);

    if (mbedtls_ctr_drbg_seed(
            &self->ctr_drbg, mbedtls_entropy_func, &self->entropy,
            (const unsigned char *)"compy", 5) != 0) {
        goto fail;
    }

    if (mbedtls_x509_crt_parse_file(&self->cert, cert_path) != 0)
        goto fail;
    if (mbedtls_pk_parse_keyfile(&self->pkey, key_path, NULL) != 0)
        goto fail;

    if (mbedtls_ssl_config_defaults(
            &self->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        goto fail;
    }

    mbedtls_ssl_conf_rng(&self->conf, mbedtls_ctr_drbg_random, &self->ctr_drbg);
    mbedtls_ssl_conf_ca_chain(&self->conf, self->cert.next, NULL);
    if (mbedtls_ssl_conf_own_cert(&self->conf, &self->cert, &self->pkey) != 0) {
        goto fail;
    }

    return self;

fail:
    mbedtls_pk_free(&self->pkey);
    mbedtls_x509_crt_free(&self->cert);
    mbedtls_ssl_config_free(&self->conf);
    mbedtls_entropy_free(&self->entropy);
    mbedtls_ctr_drbg_free(&self->ctr_drbg);
    free(self);
    return NULL;
}

static void mbed_ctx_free(Compy_CryptoTlsCtx *ctx) {
    MbedTlsCtx *c = ctx;
    if (c) {
        mbedtls_pk_free(&c->pkey);
        mbedtls_x509_crt_free(&c->cert);
        mbedtls_ssl_config_free(&c->conf);
        mbedtls_entropy_free(&c->entropy);
        mbedtls_ctr_drbg_free(&c->ctr_drbg);
        free(c);
    }
}

static Compy_CryptoTlsConn *mbed_accept(Compy_CryptoTlsCtx *ctx, int fd) {
    MbedTlsCtx *c = ctx;

    MbedTlsConn *conn = calloc(1, sizeof *conn);
    if (!conn)
        return NULL;

    mbedtls_ssl_init(&conn->ssl);
    mbedtls_net_init(&conn->net);
    conn->net.fd = fd;

    if (mbedtls_ssl_setup(&conn->ssl, &c->conf) != 0) {
        free(conn);
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
            return NULL;
        }
    }

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
    int ret = mbedtls_ssl_write(&c->ssl, data, len);
    return ret >= 0 ? (ssize_t)ret : -1;
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
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in, out);
    mbedtls_aes_free(&aes);
}

static void mbed_hmac_sha1(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[20]) {
    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), key, key_len, data,
        data_len, out);
}

static int mbed_random_bytes(uint8_t *buf, size_t len) {
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
};

const Compy_CryptoSrtpOps compy_crypto_srtp_ops = {
    .aes128_ecb = mbed_aes128_ecb,
    .hmac_sha1 = mbed_hmac_sha1,
    .random_bytes = mbed_random_bytes,
};
