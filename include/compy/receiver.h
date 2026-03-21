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
    Compy_Rtcp *rtcp,
    Compy_AudioReceiver audio_receiver) COMPY_PRIV_MUST_USE;

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

declImplExtern99(Compy_Droppable, Compy_RtpReceiver);
