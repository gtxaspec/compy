/**
 * @file
 * @brief An RTSP version.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>
#include <compy/writer.h>

#include <stdbool.h>
#include <stdint.h>

#include <slice99.h>

/**
 * An RTSP version.
 */
typedef struct {
    /**
     * The major number.
     */
    uint8_t major;

    /**
     * The minor number.
     */
    uint8_t minor;
} Compy_RtspVersion;

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
ssize_t Compy_RtspVersion_serialize(
    const Compy_RtspVersion *restrict self, Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Parses @p data to @p self.
 *
 * @pre `self != NULL`
 */
Compy_ParseResult Compy_RtspVersion_parse(
    Compy_RtspVersion *restrict self, CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_RtspVersion_eq(
    const Compy_RtspVersion *restrict lhs,
    const Compy_RtspVersion *restrict rhs) COMPY_PRIV_MUST_USE;
