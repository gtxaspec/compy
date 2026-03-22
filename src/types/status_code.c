#include <compy/types/status_code.h>

#include "parsing.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <alloca.h>

ssize_t Compy_StatusCode_serialize(
    const Compy_StatusCode *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    return VCALL(w, writef, "%" PRIu16, *self);
}

Compy_ParseResult
Compy_StatusCode_parse(Compy_StatusCode *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(compy_match_whitespaces(input));
    CharSlice99 code = input;
    MATCH(compy_match_numeric(input));
    code = CharSlice99_from_ptrdiff(code.ptr, input.ptr);

    Compy_StatusCode code_int;
    if (sscanf(CharSlice99_alloca_c_str(code), "%" SCNu16, &code_int) != 1) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_TypeMismatch(Compy_ParseType_Int, code));
    }

    *self = code_int;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_StatusCode_eq(
    const Compy_StatusCode *restrict lhs,
    const Compy_StatusCode *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return *lhs == *rhs;
}
