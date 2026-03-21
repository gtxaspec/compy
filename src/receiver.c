#include <compy/receiver.h>

#include <assert.h>
#include <stdlib.h>

#include <arpa/inet.h>

struct Compy_RtpReceiver {
    Compy_Rtcp *rtcp;
    Compy_AudioReceiver audio_receiver;
};

Compy_RtpReceiver *
Compy_RtpReceiver_new(Compy_Rtcp *rtcp, Compy_AudioReceiver audio_receiver) {
    Compy_RtpReceiver *self = malloc(sizeof *self);
    assert(self);

    self->rtcp = rtcp;
    self->audio_receiver = audio_receiver;

    return self;
}

static void Compy_RtpReceiver_drop(VSelf) {
    VSELF(Compy_RtpReceiver);
    assert(self);
    free(self);
}

implExtern(Compy_Droppable, Compy_RtpReceiver);

int Compy_RtpReceiver_feed(
    Compy_RtpReceiver *self, uint8_t channel_type, const uint8_t *data,
    size_t len) {
    assert(self);
    assert(data);

    if (channel_type == COMPY_CHANNEL_RTCP) {
        if (self->rtcp) {
            return Compy_Rtcp_handle_incoming(self->rtcp, data, len);
        }
        return 0;
    }

    if (channel_type == COMPY_CHANNEL_RTP) {
        if (!self->audio_receiver.self) {
            return 0;
        }

        Compy_RtpHeader hdr;
        const int hdr_size = Compy_RtpHeader_deserialize(&hdr, data, len);
        if (hdr_size < 0) {
            return -1;
        }

        const U8Slice99 payload =
            U8Slice99_new((uint8_t *)data + hdr_size, len - hdr_size);

        VCALL(
            self->audio_receiver, on_audio, hdr.payload_ty,
            ntohl(hdr.timestamp), ntohl(hdr.ssrc), payload);
        return 0;
    }

    return -1;
}
