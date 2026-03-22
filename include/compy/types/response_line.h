/**
 * @file
 * @brief An RTSP response line.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>
#include <compy/types/reason_phrase.h>
#include <compy/types/rtsp_version.h>
#include <compy/types/status_code.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP response line.
 */
typedef struct {
    /**
     * The RTSP version used.
     */
    Compy_RtspVersion version;

    /**
     * The status code.
     */
    Compy_StatusCode code;

    /**
     * The reason phrase.
     */
    Compy_ReasonPhrase reason;
} Compy_ResponseLine;

/**
 * Serialises @p self into @p w.
 *
 * @param[in] self The instance to be serialised.
 * @param[in] w The writer to be provided with serialised data.
 *
 * @return The number of bytes written or a negative value on error.
 *
 * @pre `self != NULL`
 * @pre `w.self && w.vptr`
 */
ssize_t Compy_ResponseLine_serialize(
    const Compy_ResponseLine *restrict self,
    Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_ResponseLine_parse(
    Compy_ResponseLine *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_ResponseLine_eq(
    const Compy_ResponseLine *restrict lhs,
    const Compy_ResponseLine *restrict rhs) COMPY_PRIV_MUST_USE;
