#include <compy/types/reason_phrase.h>

#include "parsing.h"

#include <assert.h>

Compy_ParseResult Compy_ReasonPhrase_parse(
    Compy_ReasonPhrase *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(compy_match_whitespaces(input));
    CharSlice99 phrase = input;
    MATCH(compy_match_until_crlf(input));
    phrase = CharSlice99_from_ptrdiff(phrase.ptr, input.ptr - strlen("\r\n"));

    *self = phrase;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_ReasonPhrase_eq(
    const Compy_ReasonPhrase *restrict lhs,
    const Compy_ReasonPhrase *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return CharSlice99_primitive_eq(*lhs, *rhs);
}
