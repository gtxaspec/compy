/**
 * @file
 * @brief Unified receive path for RTCP and backchannel RTP data.
 *
 * Provides the #Compy_AudioReceiver interface for applications to receive
 * backchannel audio, and the #Compy_RtpReceiver demuxer that routes incoming
 * data to either RTCP handling or audio callbacks.
 */

#pragma once

#include <compy/droppable.h>
#include <compy/rtcp.h>
#include <compy/types/rtp.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <interface99.h>
#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * Application callback interface for receiving backchannel audio data.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
#define Compy_AudioReceiver_IFACE                                              \
    vfunc99(                                                                   \
        void, on_audio, VSelf99, uint8_t payload_type, uint32_t timestamp,     \
        uint32_t ssrc, U8Slice99 payload)

interface99(Compy_AudioReceiver);

#define COMPY_CHANNEL_RTCP 0
#define COMPY_CHANNEL_RTP  1

typedef struct Compy_RtpReceiver Compy_RtpReceiver;

/**
 * Creates a new RTP receiver for demuxing incoming data.
 *
 * @param[in] rtcp The RTCP session for handling incoming RTCP. May be NULL.
 * @param[in] audio_receiver The audio callback for backchannel data. The
 * `.self` field may be NULL if no backchannel is configured.
 */
Compy_RtpReceiver *Compy_RtpReceiver_new(
    Compy_Rtcp *rtcp, Compy_AudioReceiver audio_receiver) COMPY_PRIV_MUST_USE;

/**
 * Feeds raw received data into the receiver for demuxing.
 *
 * @param[in] self The receiver.
 * @param[in] channel_type Either #COMPY_CHANNEL_RTCP or #COMPY_CHANNEL_RTP.
 * @param[in] data The raw packet data.
 * @param[in] len Length of @p data.
 *
 * @pre `self != NULL`
 * @pre `data != NULL`
 *
 * @return 0 on success, -1 on parse error.
 */
int Compy_RtpReceiver_feed(
    Compy_RtpReceiver *self, uint8_t channel_type, const uint8_t *data,
    size_t len);

/**
 * #Compy_RtpReceiver_feed with the packet's arrival instant, which
 * enables the reception statistics behind
 * #Compy_RtpReceiver_write_rr: interarrival jitter (RFC 3550 A.8) and
 * the DLSR field measured from snooped sender reports. Plain
 * #Compy_RtpReceiver_feed is this call with `arrival_us = 0`, which
 * records sequence accounting only.
 *
 * The application is the clock owner (Compy never samples one):
 * @p arrival_us must be a monotonic microsecond instant.
 */
int Compy_RtpReceiver_feed_at(
    Compy_RtpReceiver *self, uint8_t channel_type, const uint8_t *data,
    size_t len, int64_t arrival_us);

/**
 * Sets the RTP clock rate used to convert arrival instants into
 * timestamp units for jitter (RFC 3550 §6.4.1). Defaults to 8000.
 * On a multi-format media section, pick the offer's baseline format;
 * jitter is only exact while the sender uses that format, while every
 * other RR field is sequence-based and format-independent.
 */
void Compy_RtpReceiver_set_clock_rate(
    Compy_RtpReceiver *self, uint32_t clock_rate);

/**
 * Writes an RTCP receiver report (RFC 3550 §6.4.2) about the RTP
 * stream this receiver has been fed, followed by an SDES CNAME chunk
 * when @p cname != NULL (making the packet a compliant compound).
 *
 * Interval fields (fraction lost) cover the span since the previous
 * call. LSR/DLSR come from the last snooped sender report, or read 0
 * when none has been seen, as the RFC prescribes.
 *
 * @return The number of bytes written, 0 when no RTP has been seen
 * yet (nothing to report), or -1 when @p cap is too small.
 *
 * @pre `self != NULL`
 * @pre `buf != NULL`
 */
ssize_t Compy_RtpReceiver_write_rr(
    Compy_RtpReceiver *self, uint32_t reporter_ssrc, int64_t now_us,
    const char *cname, uint8_t *buf, size_t cap);

/**
 * Writes the leave compound (RFC 3550 §6.3.7): the same report
 * #Compy_RtpReceiver_write_rr produces, with a BYE for
 * @p reporter_ssrc appended as the compound's final packet. Send it
 * once, when the reporting participant leaves the session.
 *
 * @return As #Compy_RtpReceiver_write_rr — in particular 0 when no
 * RTP was ever seen, in which case no report was ever sent and no
 * BYE is owed.
 */
ssize_t Compy_RtpReceiver_write_bye(
    Compy_RtpReceiver *self, uint32_t reporter_ssrc, int64_t now_us,
    const char *cname, uint8_t *buf, size_t cap);

declImplExtern99(Compy_Droppable, Compy_RtpReceiver);
