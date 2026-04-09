#include <compy/tls.h>

#include <compy/priv/crypto.h>

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- TLS Context --- */

struct Compy_TlsContext {
    Compy_CryptoTlsCtx *crypto_ctx;
};

Compy_TlsContext *Compy_TlsContext_new(Compy_TlsConfig config) {
    assert(config.cert_path);
    assert(config.key_path);

    Compy_CryptoTlsCtx *crypto_ctx =
        compy_crypto_tls_ops.ctx_new(config.cert_path, config.key_path);
    if (!crypto_ctx) {
        return NULL;
    }

    Compy_TlsContext *self = malloc(sizeof *self);
    if (!self) {
        compy_crypto_tls_ops.ctx_free(crypto_ctx);
        return NULL;
    }

    self->crypto_ctx = crypto_ctx;
    return self;
}

void Compy_TlsContext_free(Compy_TlsContext *ctx) {
    if (ctx) {
        compy_crypto_tls_ops.ctx_free(ctx->crypto_ctx);
        free(ctx);
    }
}

/* --- TLS Connection --- */

struct Compy_TlsConn {
    Compy_CryptoTlsConn *crypto_conn;
    pthread_mutex_t mtx;
};

Compy_TlsConn *Compy_TlsConn_accept(Compy_TlsContext *ctx, int fd) {
    assert(ctx);
    assert(fd >= 0);

    Compy_CryptoTlsConn *crypto_conn =
        compy_crypto_tls_ops.accept(ctx->crypto_ctx, fd);
    if (!crypto_conn) {
        return NULL;
    }

    Compy_TlsConn *self = malloc(sizeof *self);
    if (!self) {
        compy_crypto_tls_ops.conn_free(crypto_conn);
        return NULL;
    }

    self->crypto_conn = crypto_conn;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&self->mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return self;
}

ssize_t compy_tls_read(Compy_TlsConn *conn, void *buf, size_t len) {
    assert(conn);
    assert(buf);
    pthread_mutex_lock(&conn->mtx);
    ssize_t ret = compy_crypto_tls_ops.read(conn->crypto_conn, buf, len);
    pthread_mutex_unlock(&conn->mtx);
    return ret;
}

int compy_tls_shutdown(Compy_TlsConn *conn) {
    if (!conn) {
        return -1;
    }
    pthread_mutex_lock(&conn->mtx);
    int ret = compy_crypto_tls_ops.shutdown(conn->crypto_conn);
    pthread_mutex_unlock(&conn->mtx);
    return ret;
}

void Compy_TlsConn_free(Compy_TlsConn *conn) {
    if (conn) {
        compy_crypto_tls_ops.conn_free(conn->crypto_conn);
        pthread_mutex_destroy(&conn->mtx);
        free(conn);
    }
}

/* --- TLS Writer (implements Compy_Writer_IFACE) --- */

typedef Compy_TlsConn TlsWriter;

static ssize_t TlsWriter_write(VSelf, CharSlice99 data) {
    VSELF(TlsWriter);
    return compy_crypto_tls_ops.write(self->crypto_conn, data.ptr, data.len);
}

static void TlsWriter_lock(VSelf) {
    VSELF(TlsWriter);
    pthread_mutex_lock(&self->mtx);
}

static void TlsWriter_unlock(VSelf) {
    VSELF(TlsWriter);
    pthread_mutex_unlock(&self->mtx);
}

static size_t TlsWriter_filled(VSelf) {
    VSELF(TlsWriter);
    return compy_crypto_tls_ops.pending(self->crypto_conn);
}

static int TlsWriter_writef(VSelf, const char *restrict fmt, ...) {
    VSELF(TlsWriter);
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        pthread_mutex_lock(&self->mtx);
        compy_crypto_tls_ops.write(self->crypto_conn, buf, (size_t)n);
        pthread_mutex_unlock(&self->mtx);
    }
    return n;
}

static int TlsWriter_vwritef(VSelf, const char *restrict fmt, va_list ap) {
    VSELF(TlsWriter);
    char buf[1024];
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    if (n > 0) {
        pthread_mutex_lock(&self->mtx);
        compy_crypto_tls_ops.write(self->crypto_conn, buf, (size_t)n);
        pthread_mutex_unlock(&self->mtx);
    }
    return n;
}

impl(Compy_Writer, TlsWriter);

Compy_Writer compy_tls_writer(Compy_TlsConn *conn) {
    assert(conn);
    return DYN(TlsWriter, Compy_Writer, conn);
}
