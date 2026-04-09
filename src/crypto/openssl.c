#include <compy/priv/crypto.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

/* --- TLS operations --- */

typedef struct {
    SSL_CTX *ssl_ctx;
} OsslTlsCtx;

typedef struct {
    SSL *ssl;
} OsslTlsConn;

static Compy_CryptoTlsCtx *
ossl_ctx_new(const char *cert_path, const char *key_path) {
    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        return NULL;
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_path, SSL_FILETYPE_PEM) !=
        1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path, SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    OsslTlsCtx *ctx = malloc(sizeof *ctx);
    if (!ctx) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    ctx->ssl_ctx = ssl_ctx;
    return ctx;
}

static void ossl_ctx_free(Compy_CryptoTlsCtx *ctx) {
    OsslTlsCtx *c = ctx;
    if (c) {
        SSL_CTX_free(c->ssl_ctx);
        free(c);
    }
}

static Compy_CryptoTlsConn *ossl_accept(Compy_CryptoTlsCtx *ctx, int fd) {
    OsslTlsCtx *c = ctx;

    SSL *ssl = SSL_new(c->ssl_ctx);
    if (!ssl) {
        return NULL;
    }

    if (SSL_set_fd(ssl, fd) != 1) {
        SSL_free(ssl);
        return NULL;
    }

    if (SSL_accept(ssl) != 1) {
        SSL_free(ssl);
        return NULL;
    }

    OsslTlsConn *conn = malloc(sizeof *conn);
    if (!conn) {
        SSL_free(ssl);
        return NULL;
    }

    conn->ssl = ssl;
    return conn;
}

static void ossl_conn_free(Compy_CryptoTlsConn *conn) {
    OsslTlsConn *c = conn;
    if (c) {
        SSL_free(c->ssl);
        free(c);
    }
}

static ssize_t
ossl_write(Compy_CryptoTlsConn *conn, const void *data, size_t len) {
    OsslTlsConn *c = conn;
    if (len > INT_MAX)
        len = INT_MAX;
    int ret = SSL_write(c->ssl, data, (int)len);
    return ret > 0 ? (ssize_t)ret : -1;
}

static ssize_t ossl_read(Compy_CryptoTlsConn *conn, void *buf, size_t len) {
    OsslTlsConn *c = conn;
    if (len > INT_MAX)
        len = INT_MAX;
    int ret = SSL_read(c->ssl, buf, (int)len);
    if (ret > 0) {
        return (ssize_t)ret;
    }
    int err = SSL_get_error(c->ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN) {
        return 0; /* EOF */
    }
    return -1;
}

static int ossl_shutdown(Compy_CryptoTlsConn *conn) {
    OsslTlsConn *c = conn;
    return SSL_shutdown(c->ssl) >= 0 ? 0 : -1;
}

static size_t ossl_pending(Compy_CryptoTlsConn *conn) {
    OsslTlsConn *c = conn;
    return (size_t)SSL_pending(c->ssl);
}

/* --- SRTP operations --- */

static void
ossl_aes128_ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return;
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int outl = 0;
    EVP_EncryptUpdate(ctx, out, &outl, in, 16);
    EVP_CIPHER_CTX_free(ctx);
}

static void ossl_hmac_sha1(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[20]) {
    unsigned int md_len = 20;
    HMAC(EVP_sha1(), key, (int)key_len, data, data_len, out, &md_len);
}

static int ossl_random_bytes(uint8_t *buf, size_t len) {
    if (len > INT_MAX)
        return -1;
    return RAND_bytes(buf, (int)len) == 1 ? 0 : -1;
}

/* --- Singletons --- */

const Compy_CryptoTlsOps compy_crypto_tls_ops = {
    .ctx_new = ossl_ctx_new,
    .ctx_free = ossl_ctx_free,
    .accept = ossl_accept,
    .conn_free = ossl_conn_free,
    .write = ossl_write,
    .read = ossl_read,
    .shutdown = ossl_shutdown,
    .pending = ossl_pending,
};

const Compy_CryptoSrtpOps compy_crypto_srtp_ops = {
    .aes128_ecb = ossl_aes128_ecb,
    .hmac_sha1 = ossl_hmac_sha1,
    .random_bytes = ossl_random_bytes,
};
