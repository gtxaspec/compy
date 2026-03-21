/**
 * @file
 * @brief An RTSP method.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>

#include <stdbool.h>

#include <slice99.h>

/**
 * An RTSP method.
 */
typedef CharSlice99 Compy_Method;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_Method_parse(
    Compy_Method *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_Method_eq(
    const Compy_Method *restrict lhs,
    const Compy_Method *restrict rhs) COMPY_PRIV_MUST_USE;

/**
 * `OPTIONS`.
 */
#define COMPY_METHOD_OPTIONS (CharSlice99_from_str("OPTIONS"))

/**
 * `DESCRIBE`.
 */
#define COMPY_METHOD_DESCRIBE (CharSlice99_from_str("DESCRIBE"))

/**
 * `ANNOUNCE`.
 */
#define COMPY_METHOD_ANNOUNCE (CharSlice99_from_str("ANNOUNCE"))

/**
 * `SETUP`.
 */
#define COMPY_METHOD_SETUP (CharSlice99_from_str("SETUP"))

/**
 * `PLAY`.
 */
#define COMPY_METHOD_PLAY (CharSlice99_from_str("PLAY"))

/**
 * `PAUSE`.
 */
#define COMPY_METHOD_PAUSE (CharSlice99_from_str("PAUSE"))

/**
 * `TEARDOWN`.
 */
#define COMPY_METHOD_TEARDOWN (CharSlice99_from_str("TEARDOWN"))

/**
 * `GET_PARAMETER`.
 */
#define COMPY_METHOD_GET_PARAMETER (CharSlice99_from_str("GET_PARAMETER"))

/**
 * `SET_PARAMETER`.
 */
#define COMPY_METHOD_SET_PARAMETER (CharSlice99_from_str("SET_PARAMETER"))

/**
 * `REDIRECT`.
 */
#define COMPY_METHOD_REDIRECT (CharSlice99_from_str("REDIRECT"))

/**
 * `RECORD`.
 */
#define COMPY_METHOD_RECORD (CharSlice99_from_str("RECORD"))
