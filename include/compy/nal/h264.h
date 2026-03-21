/**
 * @file
 * @brief [H.264](https://en.wikipedia.org/wiki/Advanced_Video_Coding) NAL
 * representation.
 *
 * @see H.264 spec: <https://www.itu.int/rec/T-REC-H.264>.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * The size of an H.264 NAL header.
 */
#define COMPY_H264_NAL_HEADER_SIZE sizeof(uint8_t)

/**
 * The size of an H.264 fragmentation unit (FU) header.
 *
 * @see <https://datatracker.ietf.org/doc/html/rfc6184#section-5.8>
 */
#define COMPY_H264_FU_HEADER_SIZE                                           \
    (/* fu-identifier */ sizeof(uint8_t) + /* fu-header */ sizeof(uint8_t))

/**
 * An H.264 NAL header.
 */
typedef struct {
    /**
     * `forbidden_zero_bit` f(1).
     */
    bool forbidden_zero_bit;

    /**
     * `nal_ref_idc` (u2).
     */
    uint8_t ref_idc;

    /**
     * `unit_type` u(5).
     */
    uint8_t unit_type;
} Compy_H264NalHeader;

/**
 * An H.264 NAL unit.
 */
typedef struct {
    /**
     * The NAL header of this NAL unit.
     */
    Compy_H264NalHeader header;

    /**
     * The payload data of this NAL unit (not including the header).
     */
    U8Slice99 payload;
} Compy_H264NalUnit;

/**
 * Parses an H.264 NAL header from @p byte_header.
 */
Compy_H264NalHeader
Compy_H264NalHeader_parse(uint8_t byte_header) COMPY_PRIV_MUST_USE;

/**
 * Converts @p self to a single octet representation.
 */
uint8_t Compy_H264NalHeader_serialize(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is VPS.
 */
bool Compy_H264NalHeader_is_vps(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is SPS.
 */
bool Compy_H264NalHeader_is_sps(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is PPS.
 */
bool Compy_H264NalHeader_is_pps(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is a coded slice IDR.
 */
bool Compy_H264NalHeader_is_coded_slice_idr(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Checks whether @p self is a coded slice non-IDR.
 */
bool Compy_H264NalHeader_is_coded_slice_non_idr(Compy_H264NalHeader self)
    COMPY_PRIV_MUST_USE;

/**
 * Writes a FU header of @p self to @p buffer.
 *
 * @param[in] self The header to write.
 * @param[out] buffer The memory area capable of storing
 * #COMPY_H264_FU_HEADER_SIZE bytes.
 * @param[in] is_first_fragment The indication of a start of the FU.
 * @param[in] is_last_fragment The indication of an end of the FU.
 *
 * @see <https://datatracker.ietf.org/doc/html/rfc6184#section-5.8>
 */
void Compy_H264NalHeader_write_fu_header(
    Compy_H264NalHeader self, uint8_t buffer[restrict],
    bool is_first_fragment, bool is_last_fragment);

/**
 * Unspecified.
 */
#define COMPY_H264_NAL_UNIT_UNSPECIFIED 0

/**
 * Coded slice of a non-IDR picture.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_NON_IDR 1

/**
 * Coded slice data partition A.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_DATA_PARTITION_A 2

/**
 * Coded slice data partition B.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_DATA_PARTITION_B 3

/**
 * Coded slice data partition C.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_DATA_PARTITION_C 4

/**
 * Coded slice of an IDR picture.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_IDR 5

/**
 * Supplemental enhancement information (SEI).
 */
#define COMPY_H264_NAL_UNIT_SEI 6

/**
 * Sequence parameter set.
 */
#define COMPY_H264_NAL_UNIT_SPS 7

/**
 * Picture parameter set.
 */
#define COMPY_H264_NAL_UNIT_PPS 8

/**
 * Access unit delimiter.
 */
#define COMPY_H264_NAL_UNIT_AUD 9

/**
 * End of sequence.
 */
#define COMPY_H264_NAL_UNIT_END_OF_SEQUENCE 10

/**
 * End of stream.
 */
#define COMPY_H264_NAL_UNIT_END_OF_STREAM 11

/**
 * Filler data.
 */
#define COMPY_H264_NAL_UNIT_FILLER 12

/**
 * Sequence parameter set extension.
 */
#define COMPY_H264_NAL_UNIT_SPS_EXT 13

/**
 * Prefix NAL unit.
 */
#define COMPY_H264_NAL_UNIT_PREFIX 14

/**
 * Subset sequence parameter set.
 */
#define COMPY_H264_NAL_UNIT_SUBSET_SPS 15

/**
 * Depth parameter set.
 */
#define COMPY_H264_NAL_UNIT_DPS 16

/**
 * Coded slice of an auxiliary coded picture without partitioning.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_AUX 19

/**
 * Coded slice extension.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_EXT 20

/**
 * Coded slice extension for a depth view component or a 3D-AVC texture view
 * component.
 */
#define COMPY_H264_NAL_UNIT_CODED_SLICE_EXT_DEPTH_VIEW 21
