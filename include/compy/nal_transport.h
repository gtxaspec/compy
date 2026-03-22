/**
 * @file
 * @brief An RTP/NAL data transport.
 *
 * @see RTP Payload Format for H.264 Video:
 * <https://datatracker.ietf.org/doc/html/rfc6184>
 * @see RTP Payload Format for High Efficiency Video Coding (HEVC):
 * <https://datatracker.ietf.org/doc/html/rfc7798>
 */

#pragma once

#include <compy/droppable.h>
#include <compy/nal.h>
#include <compy/rtp_transport.h>

#include <stddef.h>
#include <stdint.h>

#include <compy/priv/compiler_attrs.h>

/**
 * The default value for #Compy_NalTransportConfig.max_h264_nalu_size.
 */
#define COMPY_MAX_H264_NALU_SIZE 1200

/**
 * The default value for #Compy_NalTransportConfig.max_h265_nalu_size.
 */
#define COMPY_MAX_H265_NALU_SIZE 4096

/**
 * The configuration structure for #Compy_NalTransport.
 */
typedef struct {
    /**
     * The maximum size of an H.264 NAL unit (including the header).
     */
    size_t max_h264_nalu_size;

    /**
     * The maximum size of an H.265 NAL unit (including the header).
     */
    size_t max_h265_nalu_size;
} Compy_NalTransportConfig;

/**
 * Returns the default #Compy_NalTransportConfig.
 *
 * The default values are:
 *
 *  - `max_h264_nalu_size` is #COMPY_MAX_H264_NALU_SIZE.
 *  - `max_h265_nalu_size` is #COMPY_MAX_H265_NALU_SIZE.
 */
Compy_NalTransportConfig
Compy_NalTransportConfig_default(void) COMPY_PRIV_MUST_USE;

/**
 * An RTP/NAL data transport.
 */
typedef struct Compy_NalTransport Compy_NalTransport;

/**
 * Creates a new RTP/NAL transport with the default configuration.
 *
 * @param[in] t The underlying RTP transport.
 *
 * @pre `t != NULL`
 */
Compy_NalTransport *
Compy_NalTransport_new(Compy_RtpTransport *t) COMPY_PRIV_MUST_USE;

/**
 * Creates a new RTP/NAL transport with a custom configuration.
 *
 * @param[in] t The underlying RTP transport.
 * @param[in] config The transmission configuration structure.
 *
 * @pre `t != NULL`
 */
Compy_NalTransport *Compy_NalTransport_new_with_config(
    Compy_RtpTransport *t, Compy_NalTransportConfig config) COMPY_PRIV_MUST_USE;

/**
 * Sends an RTP/NAL packet.
 *
 * If @p nalu is larger than the limit values from #Compy_NalTransportConfig
 * (configured via #Compy_NalTransport_new),
 * @p nalu will be
 * [fragmented](https://datatracker.ietf.org/doc/html/rfc6184#section-5.8).
 *
 * @param[out] self The RTP/NAL transport for sending this packet.
 * @param[in] ts The RTP timestamp for this packet.
 * @param[in] nalu The NAL unit of this RTP packet.
 *
 * @pre `self != NULL`
 *
 * @return -1 if an I/O error occurred and sets `errno` appropriately, 0 on
 * success.
 */
int Compy_NalTransport_send_packet(
    Compy_NalTransport *self, Compy_RtpTimestamp ts,
    Compy_NalUnit nalu) COMPY_PRIV_MUST_USE;

/**
 * Implements #Compy_Droppable_IFACE for #Compy_NalTransport.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
declImplExtern99(Compy_Droppable, Compy_NalTransport);

bool Compy_NalTransport_is_full(Compy_NalTransport *self);
