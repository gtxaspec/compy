/**
 * @file
 * @brief An RTSP request URI.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP request URI.
 */
typedef CharSlice99 Compy_RequestUri;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_RequestUri_parse(
    Compy_RequestUri *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_RequestUri_eq(
    const Compy_RequestUri *restrict lhs,
    const Compy_RequestUri *restrict rhs) COMPY_PRIV_MUST_USE;
