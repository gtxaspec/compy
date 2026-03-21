/**
 * @file
 * @brief RTSP backchannel (two-way audio) support.
 *
 * Provides the receive-side session state for backchannel audio. The
 * application advertises backchannel support in SDP with `a=recvonly` on a
 * separate audio media line, handles SETUP for the backchannel URI, and feeds
 * incoming data to the receiver returned by #Compy_Backchannel_get_receiver.
 */

#pragma once

#include <compy/droppable.h>
#include <compy/receiver.h>

#include <stdint.h>

#include <compy/priv/compiler_attrs.h>

/**
 * Backchannel audio configuration.
 */
typedef struct {
    uint8_t payload_type;
    uint32_t clock_rate;
} Compy_BackchannelConfig;

/**
 * Returns the default backchannel configuration (PCMU, 8000 Hz).
 */
Compy_BackchannelConfig
Compy_BackchannelConfig_default(void) COMPY_PRIV_MUST_USE;

typedef struct Compy_Backchannel Compy_Backchannel;

/**
 * Creates a new backchannel session.
 *
 * @param[in] config The audio configuration.
 * @param[in] audio_receiver The application callback for received audio.
 *
 * @pre `audio_receiver.self && audio_receiver.vptr`
 */
Compy_Backchannel *Compy_Backchannel_new(
    Compy_BackchannelConfig config,
    Compy_AudioReceiver audio_receiver) COMPY_PRIV_MUST_USE;

/**
 * Returns the underlying receiver for feeding incoming data.
 *
 * The application should call #Compy_RtpReceiver_feed on the returned
 * receiver when backchannel RTP data arrives.
 *
 * @pre `self != NULL`
 */
Compy_RtpReceiver *
Compy_Backchannel_get_receiver(Compy_Backchannel *self) COMPY_PRIV_MUST_USE;

/**
 * Returns the backchannel configuration (for SDP generation).
 *
 * @pre `self != NULL`
 */
Compy_BackchannelConfig
Compy_Backchannel_get_config(const Compy_Backchannel *self) COMPY_PRIV_MUST_USE;

declImplExtern99(Compy_Droppable, Compy_Backchannel);
