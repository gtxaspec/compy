/**
 * @file
 * @brief An RTSP request.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>
#include <compy/types/header_map.h>
#include <compy/types/message_body.h>
#include <compy/types/request_line.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP request.
 */
typedef struct {
    /**
     * The request line.
     */
    Compy_RequestLine start_line;

    /**
     * The header map.
     */
    Compy_HeaderMap header_map;

    /**
     * The message body.
     */
    Compy_MessageBody body;

    /**
     * The sequence number for an RTSP request/response pair.
     */
    uint32_t cseq;
} Compy_Request;

/**
 * Returns an RTSP request suitable for being parsed.
 */
Compy_Request Compy_Request_uninit(void) COMPY_PRIV_MUST_USE;

/**
 * Serialises @p self into @p w.
 *
 * If `CSeq` and `Content-Length` are not present in
 * #Compy_Request.header_map, they will be taken from #Compy_Request.cseq
 * and #Compy_Request.body, respectively, and serialised as first headers
 * automatically.
 *
 * @param[in] self The instance to be serialised.
 * @param[in] w The writer to be provided with serialised data.
 *
 * @return The number of bytes written or a negative value on error.
 *
 * @pre `self != NULL`
 * @pre `w.self && w.vptr`
 */
ssize_t Compy_Request_serialize(
    const Compy_Request *restrict self, Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_Request_parse(
    Compy_Request *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_Request_eq(
    const Compy_Request *restrict lhs,
    const Compy_Request *restrict rhs) COMPY_PRIV_MUST_USE;
