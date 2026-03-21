/**
 * @file
 * @brief An RTSP reason phrase.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP reason phrase.
 */
typedef CharSlice99 Compy_ReasonPhrase;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_ReasonPhrase_parse(
    Compy_ReasonPhrase *restrict self,
    CharSlice99 data) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_ReasonPhrase_eq(
    const Compy_ReasonPhrase *restrict lhs,
    const Compy_ReasonPhrase *restrict rhs) COMPY_PRIV_MUST_USE;
