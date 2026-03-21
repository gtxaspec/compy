#include <compy/types/method.h>

#include "parsing.h"

#include <assert.h>

Compy_ParseResult
Compy_Method_parse(Compy_Method *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(compy_match_whitespaces(input));
    CharSlice99 method = input;
    MATCH(compy_match_ident(input));
    method = CharSlice99_from_ptrdiff(method.ptr, input.ptr);

    *self = method;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_Method_eq(
    const Compy_Method *restrict lhs, const Compy_Method *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return CharSlice99_primitive_eq(*lhs, *rhs);
}
