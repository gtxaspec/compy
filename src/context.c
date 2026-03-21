#include <compy/context.h>

#include <compy/types/header_map.h>
#include <compy/types/response.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Compy_Context {
    Compy_Writer writer;
    uint32_t cseq;
    Compy_HeaderMap header_map;
    Compy_MessageBody body;
    ssize_t ret;
};

Compy_Context *Compy_Context_new(Compy_Writer w, uint32_t cseq) {
    assert(w.self && w.vptr);

    Compy_Context *self = malloc(sizeof *self);
    assert(self);
    self->writer = w;
    self->cseq = cseq;
    self->header_map = Compy_HeaderMap_empty();
    self->body = Compy_MessageBody_empty();
    self->ret = 0;

    return self;
}

Compy_Writer Compy_Context_get_writer(const Compy_Context *ctx) {
    assert(ctx);
    return ctx->writer;
}

uint32_t Compy_Context_get_cseq(const Compy_Context *ctx) {
    assert(ctx);
    return ctx->cseq;
}

ssize_t Compy_Context_get_ret(const Compy_Context *ctx) {
    assert(ctx);
    return ctx->ret;
}

void compy_vheader(
    Compy_Context *ctx, CharSlice99 key, const char *restrict fmt,
    va_list list) {
    assert(ctx);
    assert(fmt);

    va_list list_copy;
    va_copy(list_copy, list);

    const int space_required = vsnprintf(NULL, 0, fmt, list_copy);
    assert(space_required > 0);
    char *value = malloc(space_required + 1 /* null character */);
    assert(value);

    const int bytes_written __attribute__((unused)) =
        vsprintf(value, fmt, list);
    assert(space_required == bytes_written);

    const Compy_Header h = {key, CharSlice99_from_str(value)};
    Compy_HeaderMap_append(&ctx->header_map, h);
}

void compy_header(
    Compy_Context *ctx, CharSlice99 key, const char *restrict fmt, ...) {
    assert(ctx);
    assert(fmt);

    va_list ap;
    va_start(ap, fmt);
    compy_vheader(ctx, key, fmt, ap);
    va_end(ap);
}

void compy_body(Compy_Context *ctx, Compy_MessageBody body) {
    assert(ctx);
    ctx->body = body;
}

ssize_t compy_respond(
    Compy_Context *ctx, Compy_StatusCode code, const char *reason) {
    assert(ctx);
    assert(reason);

    const Compy_Response response = {
        .start_line =
            {
                .version = {.major = 1, .minor = 0},
                .code = code,
                .reason = CharSlice99_from_str((char *)reason),
            },
        .header_map = ctx->header_map,
        .body = ctx->body,
        .cseq = ctx->cseq,
    };

    ctx->ret = Compy_Response_serialize(&response, ctx->writer);
    return ctx->ret;
}

ssize_t compy_respond_ok(Compy_Context *ctx) {
    assert(ctx);
    return compy_respond(ctx, COMPY_STATUS_OK, "OK");
}

ssize_t compy_respond_internal_error(Compy_Context *ctx) {
    assert(ctx);
    return compy_respond(
        ctx, COMPY_STATUS_INTERNAL_SERVER_ERROR, "Internal error");
}

void Compy_Context_drop(VSelf) {
    VSELF(Compy_Context);
    assert(self);

    for (size_t i = 0; i < self->header_map.len; i++) {
        free(self->header_map.headers[i].value.ptr);
    }

    free(self);
}

implExtern(Compy_Droppable, Compy_Context);
