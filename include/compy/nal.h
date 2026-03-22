/**
 * @file
 * @brief A generic [NAL (Network Abstraction
 * Layer)](https://en.wikipedia.org/wiki/Network_Abstraction_Layer)
 * representation.
 */

#pragma once

#include <compy/nal/h264.h>
#include <compy/nal/h265.h>

#include <stdbool.h>
#include <stdint.h>

#include <datatype99.h>
#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * A generic NAL header (either H.264 or H.265).
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */

// clang-format off
datatype99(
    Compy_NalHeader,
    (Compy_NalHeader_H264, Compy_H264NalHeader),
    (Compy_NalHeader_H265, Compy_H265NalHeader)
);
// clang-format on

/**
 * Returns the NAL unit type of @p self.
 */
uint8_t Compy_NalHeader_unit_type(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Computes the size of @p self in bytes.
 */
size_t Compy_NalHeader_size(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Computes the size of a fragmentation unit (FU) header @p self.
 *
 * @see H.264 Fragmentation Units (FUs):
 * <https://datatracker.ietf.org/doc/html/rfc6184#section-5.8>
 * @see H.265 Fragmentation Units (FUs):
 * <https://datatracker.ietf.org/doc/html/rfc7798#section-4.4.3>
 */
size_t Compy_NalHeader_fu_size(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is VPS.
 */
bool Compy_NalHeader_is_vps(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is SPS.
 */
bool Compy_NalHeader_is_sps(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is PPS.
 */
bool Compy_NalHeader_is_pps(Compy_NalHeader self) COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is a coded slice IDR.
 */
bool Compy_NalHeader_is_coded_slice_idr(Compy_NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is a coded slice non-IDR.
 */
bool Compy_NalHeader_is_coded_slice_non_idr(Compy_NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Writes the binary representation of @p self into @p buffer.
 *
 * @param[in] self The header to write.
 * @param[out] buffer The memory area capable of storing
 * `Compy_NalHeader_size(self)` bytes.
 */
void Compy_NalHeader_serialize(Compy_NalHeader self, uint8_t buffer[restrict]);

/**
 * Writes a FU header of @p self to @p buffer.
 *
 * @param[in] self The header to write.
 * @param[out] buffer The memory area capable of storing
 * `Compy_NalHeader_fu_size(self)` bytes.
 * @param[in] is_first_fragment The indication of a start of the FU.
 * @param[in] is_last_fragment The indication of an end of the FU.
 *
 * @see H.264 Fragmentation Units (FUs):
 * <https://datatracker.ietf.org/doc/html/rfc6184#section-5.8>
 * @see H.265 Fragmentation Units (FUs):
 * <https://datatracker.ietf.org/doc/html/rfc7798#section-4.4.3>
 */
void Compy_NalHeader_write_fu_header(
    Compy_NalHeader self, uint8_t buffer[restrict], bool is_first_fragment,
    bool is_last_fragment);

/**
 * A generic NAL unit (either H.264 or H.265).
 */
typedef struct {
    /**
     * The header of this NAL unit.
     */
    Compy_NalHeader header;

    /**
     * The payload data of this NAL unit (not including the header).
     */
    U8Slice99 payload;
} Compy_NalUnit;

/**
 * Creates a generic NAL FU header.
 */
uint8_t compy_nal_fu_header(
    bool is_first_fragment, bool is_last_fragment,
    uint8_t unit_type) COMPY_PRIV_MUST_USE;

/**
 * A function that tests whether @p data starts with some start code.
 *
 * @return The number of start code bytes. If there is no start code in the
 * beginning of @p data, returns 0.
 *
 * @see #compy_test_start_code_3b
 * @see #compy_test_start_code_4b
 */
typedef size_t (*Compy_NalStartCodeTester)(U8Slice99 data);

/**
 * Returns a start code tester for @p data.
 *
 * If @p data does not begin with a start code (either `0x000001` or
 * `0x00000001`), returns `NULL`.
 *
 * This function is useful when you have some `*.h264` bitstream file and you
 * want to determine what start code it uses -- just pass the beginning of this
 * file to #compy_determine_start_code and invoke the returned tester
 * multiple times afterwards.
 */
Compy_NalStartCodeTester
compy_determine_start_code(U8Slice99 data) COMPY_PRIV_MUST_USE;

/**
 * The 3-byte start code tester (`0x000001`).
 */
size_t compy_test_start_code_3b(U8Slice99 data);

/**
 * The 3-byte start code tester (`0x00000001`).
 */
size_t compy_test_start_code_4b(U8Slice99 data);
