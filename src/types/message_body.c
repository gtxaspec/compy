#include <compy/types/message_body.h>

#include "parsing.h"

#include <assert.h>

Compy_ParseResult Compy_MessageBody_parse(
    Compy_MessageBody *restrict self, CharSlice99 input,
    size_t content_length) {
    assert(self);

    if (input.len < content_length) {
        return Compy_ParseResult_partial();
    }

    if (0 == content_length) {
        *self = CharSlice99_empty();
        const size_t offset = 0;
        return Compy_ParseResult_complete(offset);
    }

    *self = CharSlice99_new(input.ptr, content_length);

    return Compy_ParseResult_complete(content_length);
}

Compy_MessageBody Compy_MessageBody_empty(void) {
    return CharSlice99_empty();
}

bool Compy_MessageBody_eq(
    const Compy_MessageBody *restrict lhs,
    const Compy_MessageBody *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return CharSlice99_primitive_eq(*lhs, *rhs);
}
