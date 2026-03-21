#include <compy/types/header_map.h>

#include "../macros.h"
#include "parsing.h"
#include <compy/util.h>

#include <assert.h>
#include <string.h>

#include <alloca.h>

Compy_HeaderMap Compy_HeaderMap_empty(void) {
    Compy_HeaderMap self;
    memset(self.headers, '\0', sizeof self.headers);
    self.len = 0;
    return self;
}

bool Compy_HeaderMap_find(
    const Compy_HeaderMap *restrict self, CharSlice99 key,
    CharSlice99 *restrict value) {
    assert(self);

    for (size_t i = 0; i < self->len; i++) {
        if (CharSlice99_primitive_eq(self->headers[i].key, key)) {
            if (value != NULL) {
                *value = self->headers[i].value;
            }
            return true;
        }
    }

    return false;
}

bool Compy_HeaderMap_contains_key(
    const Compy_HeaderMap *restrict self, CharSlice99 key) {
    assert(self);
    return Compy_HeaderMap_find(self, key, NULL);
}

void Compy_HeaderMap_append(
    Compy_HeaderMap *restrict self, Compy_Header h) {
    assert(self);
    assert(!Compy_HeaderMap_is_full(self));

    self->headers[self->len] = h;
    self->len++;
}

ssize_t Compy_HeaderMap_serialize(
    const Compy_HeaderMap *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    ssize_t result = 0;

    for (size_t i = 0; i < self->len; i++) {
        CHK_WRITE_ERR(result, Compy_Header_serialize(&self->headers[i], w));
    }

    CHK_WRITE_ERR(result, VCALL(w, write, COMPY_CRLF));

    return result;
}

Compy_ParseResult
Compy_HeaderMap_parse(Compy_HeaderMap *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    self->len = 0;

    while (true) {
        if (CharSlice99_primitive_ends_with(
                input, CharSlice99_from_str("\r"))) {
            return Compy_ParseResult_partial();
        }
        if (CharSlice99_primitive_starts_with(input, COMPY_CRLF)) {
            return Compy_ParseResult_complete(
                (input.ptr - backup.ptr) + COMPY_CRLF.len);
        }
        if (Compy_HeaderMap_is_full(self)) {
            return Compy_ParseResult_Failure(
                Compy_ParseError_HeaderMapOverflow());
        }

        Compy_Header header = {0};
        MATCH(Compy_Header_parse(&header, input));
        Compy_HeaderMap_append(self, header);
    }
}

bool Compy_HeaderMap_eq(
    const Compy_HeaderMap *restrict lhs,
    const Compy_HeaderMap *restrict rhs) {
    assert(lhs);
    assert(rhs);

    if (lhs->len != rhs->len) {
        return false;
    }

    const size_t len = lhs->len;

    for (size_t i = 0; i < len; i++) {
        if (!Compy_Header_eq(&lhs->headers[i], &rhs->headers[i])) {
            return false;
        }
    }

    return true;
}

bool Compy_HeaderMap_is_full(const Compy_HeaderMap *restrict self) {
    assert(self);
    return COMPY_HEADER_MAP_CAPACITY == self->len;
}

int compy_scanf_header(
    const Compy_HeaderMap *restrict headers, CharSlice99 key,
    const char *restrict fmt, ...) {
    assert(headers);
    assert(fmt);

    CharSlice99 val;
    const bool val_found = Compy_HeaderMap_find(headers, key, &val);
    if (!val_found) {
        return -1;
    }

    va_list ap;
    va_start(ap, fmt);
    const int ret = vsscanf(CharSlice99_alloca_c_str(val), fmt, ap);
    va_end(ap);

    return ret;
}
