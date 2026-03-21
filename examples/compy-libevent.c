#include "compy-libevent.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>

/* --- BevWriter: Compy_Writer backed by a bufferevent --- */

typedef struct bufferevent BevWriter;

static ssize_t BevWriter_write(VSelf, CharSlice99 data) {
    VSELF(BevWriter);
    if (bufferevent_write(self, data.ptr, data.len) == -1) {
        return -1;
    }
    return (ssize_t)data.len;
}

static void BevWriter_lock(VSelf) {
    VSELF(BevWriter);
    bufferevent_lock(self);
}

static void BevWriter_unlock(VSelf) {
    VSELF(BevWriter);
    bufferevent_unlock(self);
}

static size_t BevWriter_filled(VSelf) {
    VSELF(BevWriter);
    struct evbuffer *buf = bufferevent_get_output(self);
    return evbuffer_get_length(buf);
}

static int BevWriter_writef(VSelf, const char *restrict fmt, ...) {
    VSELF(BevWriter);
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) {
        bufferevent_write(self, tmp, (size_t)n);
    }
    return n;
}

static int BevWriter_vwritef(VSelf, const char *restrict fmt, va_list ap) {
    VSELF(BevWriter);
    char tmp[1024];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    if (n > 0) {
        bufferevent_write(self, tmp, (size_t)n);
    }
    return n;
}

impl(Compy_Writer, BevWriter);

Compy_Writer compy_bev_writer(struct bufferevent *bev) {
    assert(bev);
    return DYN(BevWriter, Compy_Writer, bev);
}

/* --- Libevent callback context --- */

#define PARSE_BUF_SIZE 4096

typedef struct {
    Compy_Controller controller;
    Compy_Writer writer;
    char buf[PARSE_BUF_SIZE];
    size_t buf_len;
} LibeventCtx;

void *compy_libevent_ctx(Compy_Controller controller) {
    LibeventCtx *ctx = calloc(1, sizeof *ctx);
    assert(ctx);
    ctx->controller = controller;
    ctx->buf_len = 0;
    return ctx;
}

void compy_libevent_cb(struct bufferevent *bev, void *arg) {
    LibeventCtx *ctx = arg;
    ctx->writer = compy_bev_writer(bev);

    struct evbuffer *input = bufferevent_get_input(bev);
    size_t avail;

    while ((avail = evbuffer_get_length(input)) > 0) {
        size_t space = PARSE_BUF_SIZE - ctx->buf_len;
        if (space == 0) {
            /* Buffer full, discard and reset */
            ctx->buf_len = 0;
            space = PARSE_BUF_SIZE;
        }

        size_t to_read = avail < space ? avail : space;
        evbuffer_remove(input, ctx->buf + ctx->buf_len, to_read);
        ctx->buf_len += to_read;

        /* Try to parse a complete request */
        Compy_Request req = Compy_Request_uninit();
        CharSlice99 data =
            CharSlice99_new(ctx->buf, ctx->buf_len);

        Compy_ParseResult result = Compy_Request_parse(&req, data);

        if (Compy_ParseResult_is_complete(result)) {
            /* Extract the consumed byte count */
            size_t consumed = 0;
            match(result) {
                of(Compy_ParseResult_Success, status) {
                    match(*status) {
                        of(Compy_ParseStatus_Complete, offset) {
                            consumed = *offset;
                        }
                        otherwise {}
                    }
                }
                otherwise {}
            }

            compy_dispatch(ctx->writer, ctx->controller, &req);

            /* Shift remaining data */
            if (consumed > 0 && consumed < ctx->buf_len) {
                memmove(ctx->buf, ctx->buf + consumed,
                        ctx->buf_len - consumed);
                ctx->buf_len -= consumed;
            } else {
                ctx->buf_len = 0;
            }
        } else if (Compy_ParseResult_is_failure(result)) {
            /* Parse error, discard buffer */
            ctx->buf_len = 0;
        }
        /* If partial, loop to read more data */
    }
}

void compy_libevent_ctx_free(void *arg) {
    LibeventCtx *ctx = arg;
    if (ctx) {
        VCALL_SUPER(ctx->controller, Compy_Droppable, drop);
        free(ctx);
    }
}
