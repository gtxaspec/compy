/**
 * @file
 * @brief An RTP data transport.
 */

#pragma once

#include <compy/droppable.h>
#include <compy/transport.h>

#include <stdbool.h>
#include <stdint.h>

#include <datatype99.h>
#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * An RTP data transport.
 */
typedef struct Compy_RtpTransport Compy_RtpTransport;

/**
 * An RTP timestamp.
 *
 * ## Variants
 *
 *  - `Raw` -- The value to be assigned to #Compy_RtpHeader.timestamp without
 * further conversion.
 *  - `SysClockUs` -- The timestamp value in microseconds derived from a system
 * clock (e.g., `clock_gettime`). It should be used when a raw timestamp cannot
 * be computed, as typically occurs with real-time video.
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */

// clang-format off
datatype99(
    Compy_RtpTimestamp,
    (Compy_RtpTimestamp_Raw, uint32_t),
    (Compy_RtpTimestamp_SysClockUs, uint64_t)
);
// clang-format on

/**
 * Creates a new RTP transport from the underlying level-4 protocol @p t.
 *
 * @param[in] t The level-4 protocol (such as TCP or UDP).
 * @param[in] payload_ty The RTP payload type. The list of payload types is
 * available here: <https://en.wikipedia.org/wiki/RTP_payload_formats>.
 * @param[in] clock_rate The RTP clock rate of @p payload_ty (HZ).
 *
 * @pre `t.self && t.vptr`
 * @pre The `rand` PRNG must be set up via `srand`.
 */
Compy_RtpTransport *Compy_RtpTransport_new(
    Compy_Transport t, uint8_t payload_ty,
    uint32_t clock_rate) COMPY_PRIV_MUST_USE;

/**
 * Sends an RTP packet.
 *
 * @param[out] self The RTP transport for sending this packet.
 * @param[in] ts The RTP timestamp for this packet.
 * @param[in] marker The RTP marker flag.
 * @param[in] payload_header The payload header. Can be `U8Slice99_empty()`.
 * @param[in] payload The payload data.
 *
 * @pre `self != NULL`
 *
 * @return -1 if an I/O error occurred and sets `errno` appropriately, 0 on
 * success.
 */
int Compy_RtpTransport_send_packet(
    Compy_RtpTransport *self, Compy_RtpTimestamp ts, bool marker,
    U8Slice99 payload_header, U8Slice99 payload) COMPY_PRIV_MUST_USE;

/**
 * Implements #Compy_Droppable_IFACE for #Compy_RtpTransport.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
declImplExtern99(Compy_Droppable, Compy_RtpTransport);

bool Compy_RtpTransport_is_full(Compy_RtpTransport *self);

/**
 * Returns the SSRC identifier of this transport.
 *
 * @pre `self != NULL`
 */
uint32_t Compy_RtpTransport_get_ssrc(const Compy_RtpTransport *self);

/**
 * Sets the SSRC identifier of this transport.
 *
 * Call before sending any packets. Used when the SSRC must match
 * a value declared in the SDP (e.g., WebRTC a=ssrc lines).
 *
 * @pre `self != NULL`
 */
void Compy_RtpTransport_set_ssrc(Compy_RtpTransport *self, uint32_t ssrc);

/**
 * Sets a persistent one-byte RTP header extension (RFC 8285).
 *
 * The extension is included in every outgoing packet. Profile is
 * 0xBEDE (one-byte header format). The extension element is:
 * [id(4 bits) | len-1(4 bits) | value(len bytes)].
 *
 * @param[in] id Extension ID (1-14).
 * @param[in] value Extension value bytes.
 * @param[in] len Length of value (1-8 bytes).
 *
 * @pre `self != NULL`
 * @pre `id >= 1 && id <= 14`
 * @pre `len >= 1 && len <= 8`
 */
void Compy_RtpTransport_set_extension(
    Compy_RtpTransport *self, uint8_t id, const uint8_t *value, uint8_t len);

/**
 * Returns the current RTP sequence number of this transport.
 *
 * @pre `self != NULL`
 */
uint16_t Compy_RtpTransport_get_seq(const Compy_RtpTransport *self);

/**
 * Returns the total number of RTP packets sent.
 *
 * @pre `self != NULL`
 */
uint32_t Compy_RtpTransport_get_packet_count(const Compy_RtpTransport *self);

/**
 * Returns the total number of payload octets sent (excludes RTP headers).
 *
 * @pre `self != NULL`
 */
uint32_t Compy_RtpTransport_get_octet_count(const Compy_RtpTransport *self);

/**
 * Returns the RTP timestamp of the last sent packet.
 *
 * @pre `self != NULL`
 */
uint32_t
Compy_RtpTransport_get_last_rtp_timestamp(const Compy_RtpTransport *self);
