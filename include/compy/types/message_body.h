/**
 * @file
 * @brief An RTSP message body.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>

#include <stdbool.h>
#include <stddef.h>

#include <slice99.h>

/**
 * An RTSP message body.
 */
typedef CharSlice99 Compy_MessageBody;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_MessageBody_parse(
    Compy_MessageBody *restrict self, CharSlice99 input,
    size_t content_length) COMPY_PRIV_MUST_USE;

/**
 * Returns an empty message body.
 */
Compy_MessageBody Compy_MessageBody_empty(void) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_MessageBody_eq(
    const Compy_MessageBody *restrict lhs,
    const Compy_MessageBody *restrict rhs) COMPY_PRIV_MUST_USE;
