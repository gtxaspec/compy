#include <compy/priv/crypto.h>

#include <stdlib.h>
#include <string.h>

#include <bearssl.h>

/* --- TLS operations --- */

/*
 * BearSSL's server-side API requires more manual setup than other
 * libraries. The I/O model uses an internal engine buffer rather
 * than direct fd read/write.
 */

typedef struct {
    br_ssl_server_context sc;
    br_x509_certificate chain[1];
    size_t chain_len;
    br_skey_decoder_context skey;
    unsigned char *cert_buf;
    size_t cert_buf_len;
    unsigned char *key_buf;
    size_t key_buf_len;
} BearTlsCtx;

typedef struct {
    br_ssl_server_context *sc;
    br_sslio_context ioc;
    int fd;
} BearTlsConn;

static unsigned char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    unsigned char *buf = malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    *len = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    return buf;
}

/*
 * BearSSL requires low-level I/O callbacks. These use direct
 * read()/write() on the fd.
 */
static int bear_low_read(void *ctx, unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = read(fd, buf, len);
    if (n <= 0)
        return -1;
    return (int)n;
}

static int bear_low_write(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = write(fd, buf, len);
    if (n <= 0)
        return -1;
    return (int)n;
}

static Compy_CryptoTlsCtx *
bear_ctx_new(const char *cert_path, const char *key_path) {
    /*
     * BearSSL certificate and key parsing is complex. This is a
     * simplified implementation that handles common PEM formats.
     * For production, use br_pem_decoder or a helper library.
     *
     * NOTE: This is a stub — BearSSL PEM parsing requires significant
     * additional code. A full implementation would use br_pem_decoder
     * to extract DER from PEM, then br_x509_decoder and br_skey_decoder.
     */
    (void)cert_path;
    (void)key_path;

    /* BearSSL requires DER-encoded certificates and keys.
     * Full PEM parsing is left as a TODO for production use. */
    return NULL;
}

static void bear_ctx_free(Compy_CryptoTlsCtx *ctx) {
    BearTlsCtx *c = ctx;
    if (c) {
        free(c->cert_buf);
        free(c->key_buf);
        free(c);
    }
}

static Compy_CryptoTlsConn *bear_accept(Compy_CryptoTlsCtx *ctx, int fd) {
    (void)ctx;
    (void)fd;
    /* Requires bear_ctx_new to succeed first */
    return NULL;
}

static void bear_conn_free(Compy_CryptoTlsConn *conn) {
    BearTlsConn *c = conn;
    if (c)
        free(c);
}

static ssize_t
bear_write(Compy_CryptoTlsConn *conn, const void *data, size_t len) {
    BearTlsConn *c = conn;
    int ret = br_sslio_write_all(&c->ioc, data, len);
    if (ret < 0)
        return -1;
    br_sslio_flush(&c->ioc);
    return (ssize_t)len;
}

static ssize_t bear_read(Compy_CryptoTlsConn *conn, void *buf, size_t len) {
    BearTlsConn *c = conn;
    int ret = br_sslio_read(&c->ioc, buf, len);
    return ret < 0 ? -1 : (ssize_t)ret;
}

static int bear_shutdown(Compy_CryptoTlsConn *conn) {
    BearTlsConn *c = conn;
    br_sslio_close(&c->ioc);
    return 0;
}

static size_t bear_pending(Compy_CryptoTlsConn *conn) {
    (void)conn;
    return 0;
}

/* --- SRTP operations --- */

static void
bear_aes128_ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    /*
     * BearSSL doesn't have a direct ECB API. Use CTR mode with counter=0
     * on a single block, which is equivalent to ECB for one block.
     */
    br_aes_big_ctr_keys ctr;
    br_aes_big_ctr_init(&ctr, key, 16);

    /* For a single block with IV=0 and ctr=0, CTR XORs plaintext with
     * AES(key, 0), so we pass zeros as input to get the raw keystream,
     * then XOR manually. Simpler: just use cbcenc with zero IV. */
    br_aes_big_cbcenc_keys cbcenc;
    br_aes_big_cbcenc_init(&cbcenc, key, 16);

    uint8_t iv[16] = {0};
    memcpy(out, in, 16);
    br_aes_big_cbcenc_run(&cbcenc, iv, out, 16);
}

static void bear_hmac_sha1(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[20]) {
    br_hmac_key_context kc;
    br_hmac_key_init(&kc, &br_sha1_vtable, key, key_len);

    br_hmac_context hc;
    br_hmac_init(&hc, &kc, 0);
    br_hmac_update(&hc, data, data_len);
    br_hmac_out(&hc, out);
}

static int bear_random_bytes(uint8_t *buf, size_t len) {
    br_hmac_drbg_context rng;
    br_hmac_drbg_init(&rng, &br_sha256_vtable, NULL, 0);

    /* Seed from /dev/urandom */
    unsigned char seed[32];
    FILE *f = fopen("/dev/urandom", "r");
    if (!f)
        return -1;
    if (fread(seed, 1, sizeof seed, f) != sizeof seed) {
        fclose(f);
        return -1;
    }
    fclose(f);

    br_hmac_drbg_update(&rng, seed, sizeof seed);
    br_hmac_drbg_generate(&rng, buf, len);
    return 0;
}

/* --- Singletons --- */

const Compy_CryptoTlsOps compy_crypto_tls_ops = {
    .ctx_new = bear_ctx_new,
    .ctx_free = bear_ctx_free,
    .accept = bear_accept,
    .conn_free = bear_conn_free,
    .write = bear_write,
    .read = bear_read,
    .shutdown = bear_shutdown,
    .pending = bear_pending,
};

const Compy_CryptoSrtpOps compy_crypto_srtp_ops = {
    .aes128_ecb = bear_aes128_ecb,
    .hmac_sha1 = bear_hmac_sha1,
    .random_bytes = bear_random_bytes,
};
