#include <compy/backchannel.h>

#include <assert.h>
#include <stdlib.h>

struct Compy_Backchannel {
    Compy_BackchannelConfig config;
    Compy_RtpReceiver *receiver;
};

Compy_BackchannelConfig Compy_BackchannelConfig_default(void) {
    return (Compy_BackchannelConfig){
        .payload_type = 0, /* PCMU */
        .clock_rate = 8000,
    };
}

Compy_Backchannel *Compy_Backchannel_new(
    Compy_BackchannelConfig config, Compy_AudioReceiver audio_receiver) {
    assert(audio_receiver.self && audio_receiver.vptr);

    Compy_Backchannel *self = malloc(sizeof *self);
    assert(self);

    self->config = config;
    self->receiver = Compy_RtpReceiver_new(NULL, audio_receiver);

    return self;
}

static void Compy_Backchannel_drop(VSelf) {
    VSELF(Compy_Backchannel);
    assert(self);

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, self->receiver), drop);
    free(self);
}

implExtern(Compy_Droppable, Compy_Backchannel);

Compy_RtpReceiver *Compy_Backchannel_get_receiver(Compy_Backchannel *self) {
    assert(self);
    return self->receiver;
}

Compy_BackchannelConfig
Compy_Backchannel_get_config(const Compy_Backchannel *self) {
    assert(self);
    return self->config;
}
