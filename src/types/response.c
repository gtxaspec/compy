#include <compy/types/response.h>

#include "../macros.h"
#include "parsing.h"

#include <assert.h>
#include <inttypes.h>

#include <alloca.h>

#include <slice99.h>

Compy_Response Compy_Response_uninit(void) {
    Compy_Response self;
    memset(&self, '\0', sizeof self);
    self.header_map = Compy_HeaderMap_empty();
    return self;
}

ssize_t Compy_Response_serialize(
    const Compy_Response *restrict self, Compy_Writer w) {
    assert(self);
    assert(w.self && w.vptr);

    ssize_t result = 0;

    CHK_WRITE_ERR(
        result, Compy_ResponseLine_serialize(&self->start_line, w));

    if (!Compy_HeaderMap_contains_key(
            &self->header_map, COMPY_HEADER_C_SEQ)) {
        const Compy_Header cseq = {
            COMPY_HEADER_C_SEQ,
            CharSlice99_alloca_fmt("%" PRIu32, self->cseq),
        };
        CHK_WRITE_ERR(result, Compy_Header_serialize(&cseq, w));
    }

    if (!Compy_HeaderMap_contains_key(
            &self->header_map, COMPY_HEADER_CONTENT_LENGTH) &&
        !CharSlice99_is_empty(self->body)) {
        const Compy_Header content_length = {
            COMPY_HEADER_CONTENT_LENGTH,
            CharSlice99_alloca_fmt("%zd", self->body.len),
        };
        CHK_WRITE_ERR(result, Compy_Header_serialize(&content_length, w));
    }

    CHK_WRITE_ERR(result, Compy_HeaderMap_serialize(&self->header_map, w));
    if (!CharSlice99_is_empty(self->body)) {
        CHK_WRITE_ERR(result, VCALL(w, write, self->body));
    }

    return result;
}

Compy_ParseResult
Compy_Response_parse(Compy_Response *restrict self, CharSlice99 input) {
    assert(self);

    const CharSlice99 backup = input;

    // TODO: implement proper parsing of interleaved binary data.
    for (;;) {
        if (input.len < sizeof(uint32_t)) {
            return Compy_ParseResult_partial();
        }

        if ('$' == input.ptr[0]) {
            uint8_t channel_id = 0;
            uint16_t payload_len = 0;
            compy_parse_interleaved_header(
                (const uint8_t *)input.ptr, &channel_id, &payload_len);

            input = CharSlice99_advance(input, sizeof(uint32_t));

            if (input.len < (size_t)payload_len) {
                return Compy_ParseResult_partial();
            }
            input = CharSlice99_advance(input, (size_t)payload_len);
        } else {
            break;
        }
    }

    MATCH(Compy_ResponseLine_parse(&self->start_line, input));
    MATCH(Compy_HeaderMap_parse(&self->header_map, input));

    CharSlice99 content_length;
    size_t content_length_int = 0;
    const bool content_length_is_found = Compy_HeaderMap_find(
        &self->header_map, COMPY_HEADER_CONTENT_LENGTH, &content_length);

    if (content_length_is_found) {
        if (sscanf(
                CharSlice99_alloca_c_str(content_length), "%zd",
                &content_length_int) != 1) {
            return Compy_ParseResult_Failure(
                Compy_ParseError_ContentLength(content_length));
        }
    }

    MATCH(Compy_MessageBody_parse(&self->body, input, content_length_int));

    CharSlice99 cseq_value;
    const bool cseq_found = Compy_HeaderMap_find(
        &self->header_map, COMPY_HEADER_C_SEQ, &cseq_value);
    if (!cseq_found) {
        return Compy_ParseResult_Failure(Compy_ParseError_MissingCSeq());
    }

    uint32_t cseq;
    if (sscanf(CharSlice99_alloca_c_str(cseq_value), "%" SCNu32, &cseq) != 1) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_InvalidCSeq(cseq_value));
    }

    self->cseq = cseq;

    return Compy_ParseResult_complete(input.ptr - backup.ptr);
}

bool Compy_Response_eq(
    const Compy_Response *restrict lhs,
    const Compy_Response *restrict rhs) {
    assert(lhs);
    assert(rhs);

    return Compy_ResponseLine_eq(&lhs->start_line, &rhs->start_line) &&
           Compy_HeaderMap_eq(&lhs->header_map, &rhs->header_map) &&
           Compy_MessageBody_eq(&lhs->body, &rhs->body) &&
           lhs->cseq == rhs->cseq;
}
