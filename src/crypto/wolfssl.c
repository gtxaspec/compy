#include <compy/priv/crypto.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/random.h>

/* --- TLS operations --- */

typedef struct {
    WOLFSSL_CTX *ctx;
} WolfTlsCtx;

typedef struct {
    WOLFSSL *ssl;
} WolfTlsConn;

static Compy_CryptoTlsCtx *
wolf_ctx_new(const char *cert_path, const char *key_path) {
    wolfSSL_Init();

    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_server_method());
    if (!ctx)
        return NULL;

    if (wolfSSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) !=
        SSL_SUCCESS) {
        wolfSSL_CTX_free(ctx);
        return NULL;
    }

    if (wolfSSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) !=
        SSL_SUCCESS) {
        wolfSSL_CTX_free(ctx);
        return NULL;
    }

    WolfTlsCtx *self = malloc(sizeof *self);
    if (!self) {
        wolfSSL_CTX_free(ctx);
        return NULL;
    }

    self->ctx = ctx;
    return self;
}

static void wolf_ctx_free(Compy_CryptoTlsCtx *ctx) {
    WolfTlsCtx *c = ctx;
    if (c) {
        wolfSSL_CTX_free(c->ctx);
        free(c);
    }
    wolfSSL_Cleanup();
}

static Compy_CryptoTlsConn *wolf_accept(Compy_CryptoTlsCtx *ctx, int fd) {
    WolfTlsCtx *c = ctx;
    WOLFSSL *ssl = wolfSSL_new(c->ctx);
    if (!ssl)
        return NULL;

    wolfSSL_set_fd(ssl, fd);
    if (wolfSSL_accept(ssl) != SSL_SUCCESS) {
        wolfSSL_free(ssl);
        return NULL;
    }

    WolfTlsConn *conn = malloc(sizeof *conn);
    if (!conn) {
        wolfSSL_free(ssl);
        return NULL;
    }
    conn->ssl = ssl;
    return conn;
}

static void wolf_conn_free(Compy_CryptoTlsConn *conn) {
    WolfTlsConn *c = conn;
    if (c) {
        wolfSSL_free(c->ssl);
        free(c);
    }
}

static ssize_t
wolf_write(Compy_CryptoTlsConn *conn, const void *data, size_t len) {
    WolfTlsConn *c = conn;
    if (len > INT_MAX)
        len = INT_MAX;
    int ret = wolfSSL_write(c->ssl, data, (int)len);
    return ret > 0 ? (ssize_t)ret : -1;
}

static ssize_t wolf_read(Compy_CryptoTlsConn *conn, void *buf, size_t len) {
    WolfTlsConn *c = conn;
    if (len > INT_MAX)
        len = INT_MAX;
    int ret = wolfSSL_read(c->ssl, buf, (int)len);
    if (ret > 0)
        return (ssize_t)ret;
    int err = wolfSSL_get_error(c->ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN)
        return 0;
    return -1;
}

static int wolf_shutdown(Compy_CryptoTlsConn *conn) {
    WolfTlsConn *c = conn;
    return wolfSSL_shutdown(c->ssl) >= 0 ? 0 : -1;
}

static size_t wolf_pending(Compy_CryptoTlsConn *conn) {
    WolfTlsConn *c = conn;
    return (size_t)wolfSSL_pending(c->ssl);
}

/* --- SRTP operations --- */

static void
wolf_aes128_ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    Aes aes;
    wc_AesInit(&aes, NULL, INVALID_DEVID);
    wc_AesSetKey(&aes, key, 16, NULL, AES_ENCRYPTION);
    wc_AesEcbEncrypt(&aes, out, in, 16);
    wc_AesFree(&aes);
}

static void wolf_hmac_sha1(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[20]) {
    Hmac hmac;
    wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    wc_HmacSetKey(&hmac, WC_SHA, key, (word32)key_len);
    wc_HmacUpdate(&hmac, data, (word32)data_len);
    wc_HmacFinal(&hmac, out);
    wc_HmacFree(&hmac);
}

static int wolf_random_bytes(uint8_t *buf, size_t len) {
    WC_RNG rng;
    if (wc_InitRng(&rng) != 0)
        return -1;
    int ret = wc_RNG_GenerateBlock(&rng, buf, (word32)len);
    wc_FreeRng(&rng);
    return ret == 0 ? 0 : -1;
}

/* --- Singletons --- */

const Compy_CryptoTlsOps compy_crypto_tls_ops = {
    .ctx_new = wolf_ctx_new,
    .ctx_free = wolf_ctx_free,
    .accept = wolf_accept,
    .conn_free = wolf_conn_free,
    .write = wolf_write,
    .read = wolf_read,
    .shutdown = wolf_shutdown,
    .pending = wolf_pending,
};

const Compy_CryptoSrtpOps compy_crypto_srtp_ops = {
    .aes128_ecb = wolf_aes128_ecb,
    .hmac_sha1 = wolf_hmac_sha1,
    .random_bytes = wolf_random_bytes,
};
