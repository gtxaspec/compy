#include <compy/types/header.h>

#include "parsing.h"
#include <compy/util.h>

#include <assert.h>
#include <string.h>

ssize_t Compy_Header_serialize(
    const Compy_Header *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    return COMPY_WRITE_SLICES(
        w, {
               self->key,
               CharSlice99_from_str(": "),
               self->value,
               COMPY_CRLF,
           });
}

Compy_ParseResult
Compy_Header_parse(Compy_Header *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    Compy_Header header;

    MATCH(compy_match_whitespaces(input));
    header.key = input;
    MATCH(compy_match_header_name(input));
    header.key = CharSlice99_from_ptrdiff(header.key.ptr, input.ptr);

    MATCH(compy_match_whitespaces(input));
    MATCH(compy_match_char(input, ':'));
    MATCH(compy_match_whitespaces(input));

    header.value = input;
    MATCH(compy_match_until_crlf(input));
    header.value =
        CharSlice99_from_ptrdiff(header.value.ptr, input.ptr - strlen("\r\n"));

    *self = header;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_Header_eq(
    const Compy_Header *restrict lhs, const Compy_Header *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return CharSlice99_primitive_eq(lhs->key, rhs->key) &&
           CharSlice99_primitive_eq(lhs->value, rhs->value);
}
