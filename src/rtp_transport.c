#include <compy/rtp_transport.h>

#include <compy/types/rtp.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alloca.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

struct Compy_RtpTransport {
    uint16_t seq_num;
    uint32_t ssrc;
    uint8_t payload_ty;
    uint32_t clock_rate;
    Compy_Transport transport;
    uint32_t packet_count;
    uint32_t octet_count;
    uint32_t last_rtp_timestamp;
    uint64_t last_send_time_us; /* CLOCK_MONOTONIC µs at last send */

    /* Optional one-byte header extension (RFC 8285) */
    uint8_t ext_data[12]; /* extension payload (max 12 bytes padded) */
    uint16_t ext_len;     /* 0 = no extension */
};

static uint32_t compute_timestamp(Compy_RtpTimestamp ts, uint32_t clock_rate);

Compy_RtpTransport *Compy_RtpTransport_new(
    Compy_Transport t, uint8_t payload_ty, uint32_t clock_rate) {
    assert(t.self && t.vptr);

    Compy_RtpTransport *self = malloc(sizeof *self);
    assert(self);

    self->seq_num = 0;
    /* Use OS entropy for SSRC to avoid collisions (RFC 3550 §8.1) */
    uint32_t ssrc = 0;
    bool got_random = false;
#if defined(__linux__) && defined(SYS_getrandom)
    if (syscall(SYS_getrandom, &ssrc, sizeof ssrc, 0) == sizeof ssrc)
        got_random = true;
#endif
    if (!got_random) {
        FILE *f = fopen("/dev/urandom", "r");
        if (f) {
            got_random = fread(&ssrc, 1, sizeof ssrc, f) == sizeof ssrc;
            fclose(f);
        }
    }
    self->ssrc = got_random ? ssrc : (uint32_t)rand();
    self->payload_ty = payload_ty;
    self->clock_rate = clock_rate;
    self->transport = t;
    self->packet_count = 0;
    self->octet_count = 0;
    self->last_rtp_timestamp = 0;
    self->ext_len = 0;
    memset(self->ext_data, 0, sizeof(self->ext_data));

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
        .extension = self->ext_len > 0,
        .csrc_count = 0,
        .marker = marker,
        .payload_ty = self->payload_ty,
        .sequence_number = htons(self->seq_num),
        .timestamp = htobe32(compute_timestamp(ts, self->clock_rate)),
        .ssrc = htonl(self->ssrc),
        .csrc = NULL,
        .extension_profile = htons(0xBEDE), /* RFC 8285 one-byte header */
        .extension_payload_len =
            self->ext_len > 0 ? (uint16_t)((self->ext_len + 3) / 4) : 0,
        .extension_payload = self->ext_len > 0 ? self->ext_data : NULL,
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
        struct timespec mts;
        clock_gettime(CLOCK_MONOTONIC, &mts);
        self->last_send_time_us =
            (uint64_t)mts.tv_sec * 1000000 + (uint64_t)mts.tv_nsec / 1000;
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

void Compy_RtpTransport_set_ssrc(Compy_RtpTransport *self, uint32_t ssrc) {
    assert(self);
    self->ssrc = ssrc;
}

void Compy_RtpTransport_set_extension(
    Compy_RtpTransport *self, uint8_t id, const uint8_t *value, uint8_t len) {
    assert(self);
    assert(id >= 1 && id <= 14);
    assert(len >= 1 && len <= 8);

    /* RFC 8285 one-byte header element: [ID(4) | L(4)] [value(L+1 bytes)]
     * Padded to 4-byte boundary with zero bytes. */
    uint8_t pos = 0;
    self->ext_data[pos++] = (uint8_t)((id << 4) | (len - 1));
    for (uint8_t i = 0; i < len; i++)
        self->ext_data[pos++] = value[i];
    /* Pad to 4-byte boundary */
    while (pos % 4 != 0)
        self->ext_data[pos++] = 0;
    self->ext_len = pos;
}

uint16_t Compy_RtpTransport_get_seq(const Compy_RtpTransport *self) {
    assert(self);
    return self->seq_num;
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

uint32_t
Compy_RtpTransport_get_rtp_timestamp_now(const Compy_RtpTransport *self) {
    assert(self);
    if (self->last_send_time_us == 0)
        return self->last_rtp_timestamp;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_us =
        (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    uint64_t elapsed_us = now_us - self->last_send_time_us;
    return self->last_rtp_timestamp +
           (uint32_t)(elapsed_us * self->clock_rate / 1000000);
}
