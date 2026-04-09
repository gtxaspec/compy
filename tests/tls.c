#include <compy/tls.h>

#include <greatest.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

/* Embedded self-signed test certificate and key */
static const char test_cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCzCCAfOgAwIBAgIUO1jxNaG9TbaAzn+/VCl3YxCXe3owDQYJKoZIhvcNAQEL\n"
    "BQAwFTETMBEGA1UEAwwKY29tcHktdGVzdDAeFw0yNjAzMjExMDM1MDZaFw0yNjAz\n"
    "MjIxMDM1MDZaMBUxEzARBgNVBAMMCmNvbXB5LXRlc3QwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQC93cOwh7GWYTc69e0USX7n6W+L1KX8OWO8f6sxwesc\n"
    "SoT+tf8Y2U6KBPOZk9hzlXS10FUgG3wHBNFhaX8MwBR5BcWXJfbW+y4TnjKn3tuB\n"
    "cNWht+v1ePkH3XqH1F4iBYOWf7hY2BNiSTzUso6ZNrmp25L9VtuTOx6C5PO4UO1u\n"
    "KKYM3Hi4Vu/Ye8PJZl9JFKfM+YErrOKURMr8NiuZY2lHb/9CTojH2Div4uwiclG9\n"
    "PUVVSqoYafaPgVYPSJxXOVB3OoLfKeXl8mRh4qdzQfFTN/+DyyM58IebZKJj2zmu\n"
    "lBT41fiko3FpKFWL3QbncP68owSarPVGSmgPTSTX+clLAgMBAAGjUzBRMB0GA1Ud\n"
    "DgQWBBTr+WODrVhQgxCBXtby6vBg7q1RdzAfBgNVHSMEGDAWgBTr+WODrVhQgxCB\n"
    "Xtby6vBg7q1RdzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCw\n"
    "u2hF0Ml3Oc/MDWzvhw6EICv8zms25OnFnBhtNyfm/yeg/8F9QlybqmBXGswoU1om\n"
    "v/BwSccps0Nda6eVSAt9lZLKTpx38E0AUcetbWJxe2mJ3YbziQfHawQOC5Bd9Qak\n"
    "sHN32cBKbmoKUTUax/H5aPnPQnmaDJUcaTVkap5dapLtVf4XQaKlrNFVgNbrMGOi\n"
    "T+FRqbzR1pC0VjrLGuaTMAP9cEYMtWOtwFIse4DduSxHPVPTS4oOM6KrCamjwjJB\n"
    "K/ZPwt1Tw1gqIUDJYExdAWqEyyNqgT3n8tJszg2/DqQESpKEkppf3fKCmOd4JE4s\n"
    "gGGLz/NTjsWBXLkbTB28\n"
    "-----END CERTIFICATE-----\n";

static const char test_key_pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC93cOwh7GWYTc6\n"
    "9e0USX7n6W+L1KX8OWO8f6sxwescSoT+tf8Y2U6KBPOZk9hzlXS10FUgG3wHBNFh\n"
    "aX8MwBR5BcWXJfbW+y4TnjKn3tuBcNWht+v1ePkH3XqH1F4iBYOWf7hY2BNiSTzU\n"
    "so6ZNrmp25L9VtuTOx6C5PO4UO1uKKYM3Hi4Vu/Ye8PJZl9JFKfM+YErrOKURMr8\n"
    "NiuZY2lHb/9CTojH2Div4uwiclG9PUVVSqoYafaPgVYPSJxXOVB3OoLfKeXl8mRh\n"
    "4qdzQfFTN/+DyyM58IebZKJj2zmulBT41fiko3FpKFWL3QbncP68owSarPVGSmgP\n"
    "TSTX+clLAgMBAAECggEAHUMS+F+JbrBP903UuqYzIWH4V5gv9sYPiYsxossMxcD0\n"
    "a184UCeZs6rcsmQ3XWUA5k6T+A1UWa6T6Iv+UUQ/Q4GwuMFYoDx5FvvwObAeq33s\n"
    "5u42NxBsbRpk2o9P7LVl2OmZS1QN9L1t2ygj0xg8mCGnr1TWJYRciPbpWxRZ63qi\n"
    "psurMvCC0/OwHD42NkRrKrciM+rEIpYM4VOj6pxQrYhGHtHzRdDI2CD4WraxpFZJ\n"
    "fiau/a/HOyQn3wW96lecV+aYrUTIDPag6R70etvuZgyECHCqVHD+KMCjK7PVSZz/\n"
    "sYdY3GXFdYVF6+UveH8eTRMza6YIvO+ASOVUR3KUqQKBgQD+pUVkL2h+bdx0mYn4\n"
    "OkApe6P+nJx990gvTxXL/Ixl4Nb/KQBWGenRerMQMS89gpp3akx2nY2qunQ8Xsiv\n"
    "Afi+X8sAzqmSkqCIAo414AbRltSMCcLQenEhPBxTojxFJv+WKAKirRbGh5d+owXP\n"
    "pGZ8zEn/PkkaFptiMblwECy8UwKBgQC+4En3o3ZtJZGVUbBQyuoAJUA/VEbFHn8W\n"
    "IpaxF2Qg5NS/fcj32KY3OzlM+kvgo1BSjSXJbQchv5lpWFvPxz3xVryoOaaABAKF\n"
    "R5jvzKCpzgftXP6VdECEBRvuFLCiP101pKkMzTgyAIglxGD4ilXGh+zVsLPJEK4r\n"
    "EzRnSv3gKQKBgQCBpfXBf12tWRPwDagwSMAYHsotPOF0RO1soNBRLkDxMdtkyCRS\n"
    "shjYvabjbJdHsvIgMG/DyI2zSgEaJ4v3hoJ1suHxhEbTyHGRLfPnvCrclPWcYu/c\n"
    "IyrsJ+WoMyaKcsOYWMCWwJei9rAMGsY/pM1FZuGZ4cVoUfBEu1pVkvaj3wKBgBM+\n"
    "6jZd9QLsVtGHew+qZg29s8yu4rPdb1L8CdWxVhc6+3iTZkAXSCspfY2VbzEnRmIM\n"
    "eVLl+2Ibv4wvrJI2tLgw1rTfmzotLVPi9Di5mTmF3KbCSakoH5kwKrDGxUiWuv89\n"
    "qw3vY0snYpLsShrFWAC8k9S4DGipleYh+ZawqQiBAoGAIHOpQf/CGfBaD4u0r6Ep\n"
    "pG5jujsVKbD6Jfz6+rDQyde0qogdNe76mJgkll2UDThYZE7qI4ZQIvghtKVlf2bD\n"
    "LvRaXG5LAcTt64H70yb1zAv/Au7QdzFtlCUH8tSbRMzFXx2W3s9EOWBVJiqJgRta\n"
    "TfoAhjXLK1mvL4SC3htUmbQ=\n"
    "-----END PRIVATE KEY-----\n";

static const char *cert_path = "/tmp/compy_test_cert.pem";
static const char *key_path = "/tmp/compy_test_key.pem";

static void write_test_files(void) {
    FILE *f;
    f = fopen(cert_path, "w");
    fputs(test_cert_pem, f);
    fclose(f);
    f = fopen(key_path, "w");
    fputs(test_key_pem, f);
    fclose(f);
}

/* --- mbedTLS client for testing --- */

typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_net_context net;
} TestTlsClient;

static int test_client_init(TestTlsClient *c, int fd) {
    mbedtls_ssl_init(&c->ssl);
    mbedtls_ssl_config_init(&c->conf);
    mbedtls_entropy_init(&c->entropy);
    mbedtls_ctr_drbg_init(&c->ctr_drbg);
    mbedtls_net_init(&c->net);
    c->net.fd = fd;

    if (mbedtls_ctr_drbg_seed(
            &c->ctr_drbg, mbedtls_entropy_func, &c->entropy,
            (const unsigned char *)"test", 4) != 0)
        return -1;

    if (mbedtls_ssl_config_defaults(
            &c->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT) != 0)
        return -1;

    mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->ctr_drbg);

    if (mbedtls_ssl_setup(&c->ssl, &c->conf) != 0)
        return -1;

    mbedtls_ssl_set_bio(
        &c->ssl, &c->net, mbedtls_net_send, mbedtls_net_recv, NULL);

    int ret;
    while ((ret = mbedtls_ssl_handshake(&c->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            return -1;
    }
    return 0;
}

static void test_client_free(TestTlsClient *c) {
    mbedtls_ssl_free(&c->ssl);
    mbedtls_ssl_config_free(&c->conf);
    mbedtls_entropy_free(&c->entropy);
    mbedtls_ctr_drbg_free(&c->ctr_drbg);
}

/* Server accept runs in a thread since handshake blocks */
typedef struct {
    Compy_TlsContext *ctx;
    int fd;
    Compy_TlsConn *conn; /* output */
} AcceptArgs;

static void *accept_thread(void *arg) {
    AcceptArgs *a = arg;
    a->conn = Compy_TlsConn_accept(a->ctx, a->fd);
    return NULL;
}

/* Helper: set up a TLS connection (server + client) over socketpair */
typedef struct {
    Compy_TlsContext *ctx;
    Compy_TlsConn *server;
    TestTlsClient client;
    int server_fd;
    int client_fd;
} TlsTestPair;

static int tls_pair_init(TlsTestPair *p) {
    write_test_files();
    p->ctx = Compy_TlsContext_new(
        (Compy_TlsConfig){.cert_path = cert_path, .key_path = key_path});
    if (!p->ctx)
        return -1;

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        return -1;
    p->server_fd = fds[0];
    p->client_fd = fds[1];

    /* Server accepts in a thread, client connects in main thread */
    AcceptArgs args = {.ctx = p->ctx, .fd = p->server_fd};
    pthread_t tid;
    pthread_create(&tid, NULL, accept_thread, &args);

    if (test_client_init(&p->client, p->client_fd) != 0) {
        pthread_join(tid, NULL);
        return -1;
    }

    pthread_join(tid, NULL);
    p->server = args.conn;
    return p->server ? 0 : -1;
}

static void tls_pair_free(TlsTestPair *p) {
    compy_tls_shutdown(p->server);
    mbedtls_ssl_close_notify(&p->client.ssl);
    Compy_TlsConn_free(p->server);
    test_client_free(&p->client);
    close(p->server_fd);
    close(p->client_fd);
    Compy_TlsContext_free(p->ctx);
}

/* --- Tests --- */

TEST tls_context_load_valid(void) {
    write_test_files();

    Compy_TlsContext *ctx = Compy_TlsContext_new(
        (Compy_TlsConfig){.cert_path = cert_path, .key_path = key_path});

    ASSERT(ctx != NULL);

    Compy_TlsContext_free(ctx);
    PASS();
}

TEST tls_context_load_invalid(void) {
    Compy_TlsContext *ctx = Compy_TlsContext_new(
        (Compy_TlsConfig){.cert_path = "/nonexistent", .key_path = "/also_no"});
    ASSERT_EQ(NULL, ctx);
    PASS();
}

TEST tls_handshake(void) {
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));
    tls_pair_free(&p);
    PASS();
}

TEST tls_write_read_roundtrip(void) {
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));

    /* Server writes via compy writer, client reads */
    Compy_Writer w = compy_tls_writer(p.server);
    ssize_t written = VCALL(w, write, CharSlice99_from_str("Hello from compy"));
    ASSERT_EQ(16, written);

    char buf[64] = {0};
    int n = mbedtls_ssl_read(&p.client.ssl, (unsigned char *)buf, sizeof buf);
    ASSERT_EQ(16, n);
    ASSERT_STR_EQ("Hello from compy", buf);

    tls_pair_free(&p);
    PASS();
}

TEST tls_writef(void) {
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));

    Compy_Writer w = compy_tls_writer(p.server);
    int written = VCALL(w, writef, "RTSP/1.0 %d %s\r\n", 200, "OK");
    ASSERT(written > 0);

    char buf[64] = {0};
    int n = mbedtls_ssl_read(&p.client.ssl, (unsigned char *)buf, sizeof buf);
    ASSERT(n > 0);
    ASSERT_STR_EQ("RTSP/1.0 200 OK\r\n", buf);

    tls_pair_free(&p);
    PASS();
}

TEST tls_client_to_server_read(void) {
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));

    /* Client writes, server reads via compy_tls_read */
    const char *msg = "GET_PARAMETER rtsp://camera RTSP/1.0\r\n\r\n";
    int ret = mbedtls_ssl_write(
        &p.client.ssl, (const unsigned char *)msg, strlen(msg));
    ASSERT_EQ((int)strlen(msg), ret);

    char buf[128] = {0};
    ssize_t n = compy_tls_read(p.server, buf, sizeof buf);
    ASSERT_EQ((ssize_t)strlen(msg), n);
    ASSERT_MEM_EQ(msg, buf, strlen(msg));

    tls_pair_free(&p);
    PASS();
}

TEST tls_writer_locked_multi_write(void) {
    /* This tests the pattern RSD uses: lock, write header, write payload,
     * unlock. The client should receive both writes as a single TLS stream. */
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));

    Compy_Writer w = compy_tls_writer(p.server);

    VCALL(w, lock);
    char hdr[] = {'$', 0x00, 0x00, 0x05};
    VCALL(w, write, CharSlice99_new(hdr, 4));
    char payload[] = "HELLO";
    VCALL(w, write, CharSlice99_new(payload, 5));
    VCALL(w, unlock);

    char buf[64] = {0};
    int total = 0;
    while (total < 9) {
        int n = mbedtls_ssl_read(
            &p.client.ssl, (unsigned char *)buf + total,
            (size_t)(sizeof buf - total));
        ASSERT(n > 0);
        total += n;
    }
    ASSERT_EQ(9, total);
    ASSERT_EQ('$', buf[0]);
    ASSERT_EQ(0, buf[1]);
    ASSERT_MEM_EQ("HELLO", buf + 4, 5);

    tls_pair_free(&p);
    PASS();
}

TEST tls_multiple_writes(void) {
    /* Send 100 small messages — exercises the TLS record layer */
    TlsTestPair p;
    ASSERT_EQ(0, tls_pair_init(&p));

    Compy_Writer w = compy_tls_writer(p.server);
    const char *msg = "FRAME\n";
    size_t msg_len = strlen(msg);

    for (int i = 0; i < 100; i++) {
        VCALL(w, lock);
        ssize_t ret = VCALL(w, write, CharSlice99_from_str((char *)msg));
        VCALL(w, unlock);
        ASSERT_EQ((ssize_t)msg_len, ret);
    }

    /* Read all on client side */
    char buf[1024] = {0};
    size_t total = 0;
    while (total < 100 * msg_len) {
        int n = mbedtls_ssl_read(
            &p.client.ssl, (unsigned char *)buf + total, sizeof buf - total);
        ASSERT(n > 0);
        total += (size_t)n;
    }
    ASSERT_EQ(100 * msg_len, total);

    /* Verify every 6th byte is 'F' (start of "FRAME\n") */
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ('F', buf[i * msg_len]);
    }

    tls_pair_free(&p);
    PASS();
}

SUITE(tls) {
    RUN_TEST(tls_context_load_valid);
    RUN_TEST(tls_context_load_invalid);
    RUN_TEST(tls_handshake);
    RUN_TEST(tls_write_read_roundtrip);
    RUN_TEST(tls_writef);
    RUN_TEST(tls_client_to_server_read);
    RUN_TEST(tls_writer_locked_multi_write);
    RUN_TEST(tls_multiple_writes);
}
