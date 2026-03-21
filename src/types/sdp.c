#include <compy/types/sdp.h>

#include "../macros.h"
#include "parsing.h"
#include <compy/util.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

ssize_t Compy_SdpLine_serialize(
    const Compy_SdpLine *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    return COMPY_WRITE_SLICES(
        w, {
               CharSlice99_new((char *)&self->ty, 1),
               CharSlice99_from_str("="),
               self->value,
               COMPY_CRLF,
           });
}

ssize_t compy_sdp_printf(
    Compy_Writer w, Compy_SdpType ty, const char *fmt, ...) {
    assert(w.self && w.vptr);
    assert(fmt);

    ssize_t result = 0;

    va_list ap;
    va_start(ap, fmt);

    CHK_WRITE_ERR(result, VCALL(w, writef, "%c=", ty));
    CHK_WRITE_ERR(result, VCALL(w, vwritef, fmt, ap));
    CHK_WRITE_ERR(result, VCALL(w, write, COMPY_CRLF));

    va_end(ap);

    return result;
}
