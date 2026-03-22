#include <compy/types/request_uri.h>

#include "parsing.h"

#include <assert.h>

Compy_ParseResult
Compy_RequestUri_parse(Compy_RequestUri *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(compy_match_whitespaces(input));
    CharSlice99 uri = input;
    MATCH(compy_match_non_whitespaces(input));
    uri = CharSlice99_from_ptrdiff(uri.ptr, input.ptr);

    *self = uri;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_RequestUri_eq(
    const Compy_RequestUri *restrict lhs,
    const Compy_RequestUri *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return CharSlice99_primitive_eq(*lhs, *rhs);
}
