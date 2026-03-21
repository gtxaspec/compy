#include <compy/types/request_line.h>

#include "../macros.h"
#include "parsing.h"
#include <compy/util.h>

#include <assert.h>
#include <string.h>

ssize_t Compy_RequestLine_serialize(
    const Compy_RequestLine *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    ssize_t result = 0;

    CHK_WRITE_ERR(result, VCALL(w, write, self->method));
    CHK_WRITE_ERR(result, VCALL(w, write, CharSlice99_from_str(" ")));
    CHK_WRITE_ERR(result, VCALL(w, write, self->uri));
    CHK_WRITE_ERR(result, VCALL(w, write, CharSlice99_from_str(" ")));
    CHK_WRITE_ERR(result, Compy_RtspVersion_serialize(&self->version, w));
    CHK_WRITE_ERR(result, VCALL(w, write, COMPY_CRLF));

    return result;
}

Compy_ParseResult Compy_RequestLine_parse(
    Compy_RequestLine *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(Compy_Method_parse(&self->method, input));
    MATCH(Compy_RequestUri_parse(&self->uri, input));
    MATCH(Compy_RtspVersion_parse(&self->version, input));
    MATCH(compy_match_str(input, "\r\n"));

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_RequestLine_eq(
    const Compy_RequestLine *restrict lhs,
    const Compy_RequestLine *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return Compy_Method_eq(&lhs->method, &rhs->method) &&
           Compy_RequestUri_eq(&lhs->uri, &rhs->uri) &&
           Compy_RtspVersion_eq(&lhs->version, &rhs->version);
}
