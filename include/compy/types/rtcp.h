/**
 * @file
 * @brief <a href="https://datatracker.ietf.org/doc/html/rfc3550#section-6">RFC
 * 3550 Section 6</a>-compliant RTCP packet types.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>

#include <stddef.h>
#include <stdint.h>

#include <slice99.h>

#define COMPY_RTCP_SR   200
#define COMPY_RTCP_RR   201
#define COMPY_RTCP_SDES 202
#define COMPY_RTCP_BYE  203
#define COMPY_RTCP_APP  204

#define COMPY_RTCP_SDES_CNAME 1

#define COMPY_RTCP_MAX_PACKET_SIZE 512

/**
 * RTCP common header (RFC 3550 Section 6.4.1).
 */
typedef struct {
    uint8_t version;
    uint8_t padding;
    uint8_t count;
    uint8_t packet_type;
    uint16_t length;
} Compy_RtcpHeader;

/**
 * RTCP Sender Report sender info (RFC 3550 Section 6.4.1).
 */
typedef struct {
    uint32_t ssrc;
    uint32_t ntp_timestamp_msw;
    uint32_t ntp_timestamp_lsw;
    uint32_t rtp_timestamp;
    uint32_t sender_packet_count;
    uint32_t sender_octet_count;
} Compy_RtcpSenderInfo;

/**
 * RTCP Receiver Report block (RFC 3550 Section 6.4.2).
 */
typedef struct {
    uint32_t ssrc;
    uint8_t fraction_lost;
    uint32_t cumulative_lost;
    uint32_t extended_highest_seq;
    uint32_t interarrival_jitter;
    uint32_t last_sr;
    uint32_t delay_since_last_sr;
} Compy_RtcpReportBlock;

/**
 * Serializes a compound SR + SDES packet into @p buffer.
 *
 * @param[in] info The sender report information.
 * @param[in] cname The CNAME string for the SDES item.
 * @param[out] buffer Output buffer, must be at least
 * #COMPY_RTCP_MAX_PACKET_SIZE bytes.
 *
 * @pre `cname != NULL`
 * @pre `buffer != NULL`
 *
 * @return The total number of bytes written.
 */
size_t Compy_RtcpSenderReport_serialize(
    Compy_RtcpSenderInfo info, const char *cname,
    uint8_t buffer[restrict]) COMPY_PRIV_MUST_USE;

/**
 * Serializes a BYE packet into @p buffer.
 *
 * @param[in] ssrc The SSRC to include in the BYE.
 * @param[out] buffer Output buffer, must be at least 8 bytes.
 *
 * @pre `buffer != NULL`
 *
 * @return The total number of bytes written.
 */
size_t Compy_RtcpBye_serialize(
    uint32_t ssrc, uint8_t buffer[restrict]) COMPY_PRIV_MUST_USE;

/**
 * Deserializes an RTCP common header from @p data.
 *
 * @param[out] self The parsed header.
 * @param[in] data The buffer to parse.
 * @param[in] len Length of @p data.
 *
 * @pre `self != NULL`
 * @pre `data != NULL`
 *
 * @return 0 on success, -1 if data is too short or version is not 2.
 */
int Compy_RtcpHeader_deserialize(
    Compy_RtcpHeader *restrict self, const uint8_t *data,
    size_t len) COMPY_PRIV_MUST_USE;

/**
 * Deserializes an RTCP Receiver Report block from @p data.
 *
 * @param[out] self The parsed report block.
 * @param[in] data The buffer to parse (must point past the RTCP header).
 * @param[in] len Length of @p data.
 *
 * @pre `self != NULL`
 * @pre `data != NULL`
 *
 * @return 0 on success, -1 if data is too short.
 */
int Compy_RtcpReportBlock_deserialize(
    Compy_RtcpReportBlock *restrict self, const uint8_t *data,
    size_t len) COMPY_PRIV_MUST_USE;
