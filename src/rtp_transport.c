#include <compy/rtp_transport.h>

#include <compy/types/rtp.h>

#include <assert.h>
#include <stdlib.h>

#include <alloca.h>
#include <arpa/inet.h>

struct Compy_RtpTransport {
    uint16_t seq_num;
    uint32_t ssrc;
    uint8_t payload_ty;
    uint32_t clock_rate;
    Compy_Transport transport;
    uint32_t packet_count;
    uint32_t octet_count;
    uint32_t last_rtp_timestamp;
};

static uint32_t compute_timestamp(Compy_RtpTimestamp ts, uint32_t clock_rate);

Compy_RtpTransport *Compy_RtpTransport_new(
    Compy_Transport t, uint8_t payload_ty, uint32_t clock_rate) {
    assert(t.self && t.vptr);

    Compy_RtpTransport *self = malloc(sizeof *self);
    assert(self);

    self->seq_num = 0;
    self->ssrc = (uint32_t)rand();
    self->payload_ty = payload_ty;
    self->clock_rate = clock_rate;
    self->transport = t;
    self->packet_count = 0;
    self->octet_count = 0;
    self->last_rtp_timestamp = 0;

    return self;
}

static void Compy_RtpTransport_drop(VSelf) {
    VSELF(Compy_RtpTransport);
    assert(self);

    VCALL_SUPER(self->transport, Compy_Droppable, drop);

    free(self);
}

implExtern(Compy_Droppable, Compy_RtpTransport);

int Compy_RtpTransport_send_packet(
    Compy_RtpTransport *self, Compy_RtpTimestamp ts, bool marker,
    U8Slice99 payload_header, U8Slice99 payload) {
    assert(self);

    const Compy_RtpHeader header = {
        .version = 2,
        .padding = false,
        .extension = false,
        .csrc_count = 0,
        .marker = marker,
        .payload_ty = self->payload_ty,
        .sequence_number = htons(self->seq_num),
        .timestamp = htobe32(compute_timestamp(ts, self->clock_rate)),
        .ssrc = self->ssrc,
        .csrc = NULL,
        .extension_profile = htons(0),
        .extension_payload_len = htons(0),
        .extension_payload = NULL,
    };

    const size_t rtp_header_size = Compy_RtpHeader_size(header);
    const U8Slice99 rtp_header = U8Slice99_new(
        Compy_RtpHeader_serialize(header, alloca(rtp_header_size)),
        rtp_header_size);

    const Compy_IoVecSlice bufs =
        (Compy_IoVecSlice)Slice99_typed_from_array((struct iovec[]){
            compy_slice_to_iovec(rtp_header),
            compy_slice_to_iovec(payload_header),
            compy_slice_to_iovec(payload),
        });

    const int ret = VCALL(self->transport, transmit, bufs);
    if (ret != -1) {
        self->seq_num++;
        self->packet_count++;
        self->octet_count += (uint32_t)(payload_header.len + payload.len);
        self->last_rtp_timestamp = compute_timestamp(ts, self->clock_rate);
    }

    return ret;
}

static uint32_t compute_timestamp(Compy_RtpTimestamp ts, uint32_t clock_rate) {
    match(ts) {
        of(Compy_RtpTimestamp_Raw, raw_ts) {
            return *raw_ts;
        }
        of(Compy_RtpTimestamp_SysClockUs, time_us) {
            const uint64_t us_rem = *time_us % 1000,
                           ms = (*time_us - us_rem) / 1000;
            uint32_t clock_rate_kHz = clock_rate / 1000;
            return ms * clock_rate_kHz +
                   (uint32_t)(us_rem * ((double)clock_rate_kHz / 1000.0));
        }
    }

    return 0;
}

bool Compy_RtpTransport_is_full(Compy_RtpTransport *self) {
    return VCALL(self->transport, is_full);
}

uint32_t Compy_RtpTransport_get_ssrc(const Compy_RtpTransport *self) {
    assert(self);
    return self->ssrc;
}

uint32_t Compy_RtpTransport_get_packet_count(const Compy_RtpTransport *self) {
    assert(self);
    return self->packet_count;
}

uint32_t Compy_RtpTransport_get_octet_count(const Compy_RtpTransport *self) {
    assert(self);
    return self->octet_count;
}

uint32_t
Compy_RtpTransport_get_last_rtp_timestamp(const Compy_RtpTransport *self) {
    assert(self);
    return self->last_rtp_timestamp;
}
