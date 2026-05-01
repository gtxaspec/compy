/**
 * @file
 * @brief An RTP/JPEG data transport.
 *
 * Packetizes baseline JPEG frames into RTP packets per RFC 2435. Supports
 * YUV 4:2:0 and 4:2:2 subsampling, custom quantization tables (Q=255),
 * and restart marker intervals (DRI).
 *
 * @see RTP Payload Format for JPEG-compressed Video:
 * <https://datatracker.ietf.org/doc/html/rfc2435>
 */

#pragma once

#include <compy/droppable.h>
#include <compy/rtp_transport.h>

#include <stdbool.h>
#include <stddef.h>

#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * The default value for #Compy_JpegTransportConfig.max_fragment_size.
 */
#define COMPY_JPEG_DEFAULT_MAX_FRAGMENT_SIZE 1200

/**
 * The configuration structure for #Compy_JpegTransport.
 */
typedef struct {
    /**
     * The maximum RTP payload size per fragment in bytes (excluding the
     * 12-byte RTP header). Includes the 8-byte JPEG RTP header and any
     * quantization table data. Scan data is fragmented to fit within this
     * limit.
     */
    size_t max_fragment_size;
} Compy_JpegTransportConfig;

/**
 * Returns the default #Compy_JpegTransportConfig.
 *
 * The default values are:
 *
 *  - `max_fragment_size` is #COMPY_JPEG_DEFAULT_MAX_FRAGMENT_SIZE.
 */
Compy_JpegTransportConfig
Compy_JpegTransportConfig_default(void) COMPY_PRIV_MUST_USE;

/**
 * An RTP/JPEG data transport.
 */
typedef struct Compy_JpegTransport Compy_JpegTransport;

/**
 * Creates a new RTP/JPEG transport with the default configuration.
 *
 * @param[in] t The underlying RTP transport (payload type 26, clock
 * 90000 Hz).
 *
 * @pre `t != NULL`
 */
Compy_JpegTransport *
Compy_JpegTransport_new(Compy_RtpTransport *t) COMPY_PRIV_MUST_USE;

/**
 * Creates a new RTP/JPEG transport with a custom configuration.
 *
 * @param[in] t The underlying RTP transport (payload type 26, clock
 * 90000 Hz).
 * @param[in] config The transmission configuration structure.
 *
 * @pre `t != NULL`
 */
Compy_JpegTransport *Compy_JpegTransport_new_with_config(
    Compy_RtpTransport *t,
    Compy_JpegTransportConfig config) COMPY_PRIV_MUST_USE;

/**
 * Sends a complete JPEG frame as one or more RTP packets.
 *
 * The frame is parsed to extract dimensions, quantization tables, and
 * scan data. Scan data is
 * [fragmented](https://datatracker.ietf.org/doc/html/rfc2435#section-3.1)
 * according to the configured max_fragment_size. The RTP marker bit is
 * set on the last fragment.
 *
 * Only baseline DCT JPEG (SOF0) is supported. Custom quantization
 * tables are always sent in-band (Q=255 per RFC 2435 Section 3.1.8).
 *
 * @param[out] self The RTP/JPEG transport.
 * @param[in] ts The RTP timestamp for this frame.
 * @param[in] jpeg_frame The complete JPEG frame (SOI through EOI).
 *
 * @pre `self != NULL`
 *
 * @return -1 if an I/O or parse error occurred, 0 on success.
 */
int Compy_JpegTransport_send_frame(
    Compy_JpegTransport *self, Compy_RtpTimestamp ts,
    U8Slice99 jpeg_frame) COMPY_PRIV_MUST_USE;

/**
 * Returns whether the underlying transport's send buffer is full.
 *
 * @pre `self != NULL`
 */
bool Compy_JpegTransport_is_full(Compy_JpegTransport *self);

/**
 * Implements #Compy_Droppable_IFACE for #Compy_JpegTransport.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the
 * macro usage.
 */
declImplExtern99(Compy_Droppable, Compy_JpegTransport);
