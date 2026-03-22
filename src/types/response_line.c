#include <compy/types/response_line.h>

#include "../macros.h"
#include "parsing.h"
#include <compy/util.h>

#include <assert.h>
#include <string.h>

ssize_t Compy_ResponseLine_serialize(
    const Compy_ResponseLine *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    ssize_t result = 0;

    CHK_WRITE_ERR(result, Compy_RtspVersion_serialize(&self->version, w));
    CHK_WRITE_ERR(result, VCALL(w, write, CharSlice99_from_str(" ")));
    CHK_WRITE_ERR(result, Compy_StatusCode_serialize(&self->code, w));
    CHK_WRITE_ERR(result, VCALL(w, write, CharSlice99_from_str(" ")));
    CHK_WRITE_ERR(result, VCALL(w, write, self->reason));
    CHK_WRITE_ERR(result, VCALL(w, write, COMPY_CRLF));

    return result;
}

Compy_ParseResult
Compy_ResponseLine_parse(Compy_ResponseLine *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(Compy_RtspVersion_parse(&self->version, input));
    MATCH(Compy_StatusCode_parse(&self->code, input));
    MATCH(Compy_ReasonPhrase_parse(&self->reason, input));

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_ResponseLine_eq(
    const Compy_ResponseLine *restrict lhs,
    const Compy_ResponseLine *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return Compy_RtspVersion_eq(&lhs->version, &rhs->version) &&
           lhs->code == rhs->code &&
           Compy_ReasonPhrase_eq(&lhs->reason, &rhs->reason);
}
