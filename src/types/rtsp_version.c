#include <compy/types/rtsp_version.h>

#include "parsing.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <alloca.h>

ssize_t Compy_RtspVersion_serialize(
    const Compy_RtspVersion *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    return VCALL(
        w, writef, "RTSP/%" PRIu8 ".%" PRIu8, self->major, self->minor);
}

Compy_ParseResult
Compy_RtspVersion_parse(Compy_RtspVersion *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    MATCH(compy_match_whitespaces(input));
    MATCH(compy_match_str(input, "RTSP/"));

    CharSlice99 major = input;
    MATCH(compy_match_numeric(input));
    major = CharSlice99_from_ptrdiff(major.ptr, input.ptr);
    MATCH(compy_match_char(input, '.'));

    CharSlice99 minor = input;
    MATCH(compy_match_numeric(input));
    minor = CharSlice99_from_ptrdiff(minor.ptr, input.ptr);

    uint8_t major_int, minor_int;

    if (sscanf(CharSlice99_alloca_c_str(major), "%" SCNu8, &major_int) != 1) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_TypeMismatch(Compy_ParseType_Int, major));
    }

    if (sscanf(CharSlice99_alloca_c_str(minor), "%" SCNu8, &minor_int) != 1) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_TypeMismatch(Compy_ParseType_Int, minor));
    }

    self->major = major_int;
    self->minor = minor_int;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_RtspVersion_eq(
    const Compy_RtspVersion *restrict lhs,
    const Compy_RtspVersion *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return lhs->major == rhs->major && lhs->minor == rhs->minor;
}
