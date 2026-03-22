/**
 * @file
 * @brief An RTSP request line.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>
#include <compy/types/method.h>
#include <compy/types/request_uri.h>
#include <compy/types/rtsp_version.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP request line.
 */
typedef struct {
    /**
     * The method used.
     */
    Compy_Method method;

    /**
     * The request URI.
     */
    Compy_RequestUri uri;

    /**
     * The RTSP version used.
     */
    Compy_RtspVersion version;
} Compy_RequestLine;

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
ssize_t Compy_RequestLine_serialize(
    const Compy_RequestLine *restrict self, Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_RequestLine_parse(
    Compy_RequestLine *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_RequestLine_eq(
    const Compy_RequestLine *restrict lhs,
    const Compy_RequestLine *restrict rhs) COMPY_PRIV_MUST_USE;
