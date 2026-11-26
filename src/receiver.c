#include <compy/receiver.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

/*
 * Reception statistics per RFC 3550 appendix A. One source only: a
 * backchannel is a single sender, so an SSRC change is treated as the
 * source restarting rather than a second participant.
 */
typedef struct {
    bool active;
    uint32_t ssrc;

    /* A.1 sequence accounting */
    uint16_t max_seq;
    uint32_t cycles;
    uint32_t base_seq;
    uint32_t received;

    /* §6.4.1 interval state (previous write_rr call) */
    uint32_t expected_prior;
    uint32_t received_prior;

    /* A.8 jitter, kept scaled by 16 as in the reference code */
    uint32_t jitter;
    int64_t transit;
    bool have_transit;

    /* Last snooped sender report, for LSR/DLSR */
    uint32_t last_sr_lsr;
    int64_t last_sr_arrival_us;
} Compy_RecvStats;

struct Compy_RtpReceiver {
    Compy_Rtcp *rtcp;
    Compy_AudioReceiver audio_receiver;
    Compy_RecvStats stats;
    uint32_t clock_rate;
};

Compy_RtpReceiver *
Compy_RtpReceiver_new(Compy_Rtcp *rtcp, Compy_AudioReceiver audio_receiver) {
    Compy_RtpReceiver *self = malloc(sizeof *self);
    assert(self);

    self->rtcp = rtcp;
    self->audio_receiver = audio_receiver;
    memset(&self->stats, 0, sizeof self->stats);
    self->clock_rate = 8000;

    return self;
}

static void Compy_RtpReceiver_drop(VSelf) {
    VSELF(Compy_RtpReceiver);
    assert(self);
    free(self);
}

implExtern(Compy_Droppable, Compy_RtpReceiver);

void Compy_RtpReceiver_set_clock_rate(
    Compy_RtpReceiver *self, uint32_t clock_rate) {
    assert(self);
    if (clock_rate > 0) {
        self->clock_rate = clock_rate;
    }
}

static void
stats_init_source(Compy_RecvStats *st, uint32_t ssrc, uint16_t seq) {
    memset(st, 0, sizeof *st);
    st->active = true;
    st->ssrc = ssrc;
    st->base_seq = seq;
    st->max_seq = seq;
}

static void stats_note_rtp(
    Compy_RtpReceiver *self, uint32_t ssrc, uint16_t seq, uint32_t rtp_ts,
    int64_t arrival_us) {
    Compy_RecvStats *st = &self->stats;

    if (!st->active || st->ssrc != ssrc) {
        stats_init_source(st, ssrc, seq);
    }

    /* A.1, without the probation dance: the transports feeding this
     * are connected sockets or interleaved channels, so the source is
     * authenticated by the session rather than by packet count. */
    uint16_t udelta = (uint16_t)(seq - st->max_seq);
    if (udelta < 0x8000) {
        if (seq < st->max_seq) {
            st->cycles += 1u << 16;
        }
        st->max_seq = seq;
    } /* else: duplicate or reordered packet, count it but keep max_seq */
    st->received++;

    if (arrival_us > 0) {
        /* A.8, in RTP timestamp units at the configured clock */
        int64_t arrival_units =
            arrival_us * (int64_t)self->clock_rate / 1000000;
        int64_t transit = arrival_units - (int64_t)rtp_ts;
        if (st->have_transit) {
            int64_t d = transit - st->transit;
            if (d < 0) {
                d = -d;
            }
            st->jitter += (uint32_t)d - ((st->jitter + 8) >> 4);
        }
        st->transit = transit;
        st->have_transit = true;
    }
}

/* Walk a compound RTCP frame for a sender report and remember its NTP
 * middle 32 bits + arrival, which become the RR's LSR/DLSR. */
static void stats_snoop_rtcp(
    Compy_RtpReceiver *self, const uint8_t *data, size_t len,
    int64_t arrival_us) {
    size_t off = 0;
    while (off + 8 <= len) {
        if ((data[off] >> 6) != 2) {
            return;
        }
        size_t chunk =
            ((size_t)(((uint16_t)data[off + 2] << 8) | data[off + 3]) + 1) * 4;
        if (chunk == 0 || off + chunk > len) {
            return;
        }
        if (data[off + 1] == 200 && chunk >= 20) {
            /* NTP timestamp sits at bytes 8..15 of the SR; the middle
             * 32 bits are what the RR echoes back. */
            self->stats.last_sr_lsr = ((uint32_t)data[off + 10] << 24) |
                                      ((uint32_t)data[off + 11] << 16) |
                                      ((uint32_t)data[off + 12] << 8) |
                                      (uint32_t)data[off + 13];
            self->stats.last_sr_arrival_us = arrival_us;
        }
        off += chunk;
    }
}

int Compy_RtpReceiver_feed_at(
    Compy_RtpReceiver *self, uint8_t channel_type, const uint8_t *data,
    size_t len, int64_t arrival_us) {
    assert(self);
    assert(data);

    if (channel_type == COMPY_CHANNEL_RTCP) {
        stats_snoop_rtcp(self, data, len, arrival_us);
        if (self->rtcp) {
            return Compy_Rtcp_handle_incoming(self->rtcp, data, len);
        }
        return 0;
    }

    if (channel_type == COMPY_CHANNEL_RTP) {
        Compy_RtpHeader hdr;
        const int hdr_size = Compy_RtpHeader_deserialize(&hdr, data, len);
        if (hdr_size < 0) {
            return -1;
        }

        stats_note_rtp(
            self, ntohl(hdr.ssrc), ntohs(hdr.sequence_number),
            ntohl(hdr.timestamp), arrival_us);

        if (!self->audio_receiver.self) {
            return 0;
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

int Compy_RtpReceiver_feed(
    Compy_RtpReceiver *self, uint8_t channel_type, const uint8_t *data,
    size_t len) {
    return Compy_RtpReceiver_feed_at(self, channel_type, data, len, 0);
}

#define COMPY_RR_LEN 32

ssize_t Compy_RtpReceiver_write_rr(
    Compy_RtpReceiver *self, uint32_t reporter_ssrc, int64_t now_us,
    const char *cname, uint8_t *buf, size_t cap) {
    assert(self);
    assert(buf);

    Compy_RecvStats *st = &self->stats;
    if (!st->active) {
        return 0;
    }

    size_t sdes_len = 0;
    if (cname) {
        size_t cname_len = strlen(cname);
        if (cname_len > 255) {
            cname_len = 255;
        }
        /* header + ssrc + item type + item length + text + null,
         * padded to a word boundary */
        sdes_len = (4 + 4 + 2 + cname_len + 1 + 3) & ~(size_t)3;
    }
    if (cap < COMPY_RR_LEN + sdes_len) {
        return -1;
    }

    /* §6.4.1 fraction lost over the interval since the last report */
    uint32_t extended_max = st->cycles + st->max_seq;
    uint32_t expected = extended_max - st->base_seq + 1;
    int32_t lost = (int32_t)(expected - st->received);
    if (lost > 0x7FFFFF) {
        lost = 0x7FFFFF;
    }
    if (lost < -0x800000) {
        lost = -0x800000;
    }
    uint32_t expected_interval = expected - st->expected_prior;
    uint32_t received_interval = st->received - st->received_prior;
    st->expected_prior = expected;
    st->received_prior = st->received;
    int64_t lost_interval = (int64_t)expected_interval - received_interval;
    uint8_t fraction = 0;
    if (expected_interval > 0 && lost_interval > 0) {
        uint64_t f = ((uint64_t)lost_interval << 8) / expected_interval;
        fraction = f > 255 ? 255 : (uint8_t)f;
    }

    uint32_t dlsr = 0;
    if (st->last_sr_arrival_us > 0 && now_us > st->last_sr_arrival_us) {
        dlsr = (uint32_t)((now_us - st->last_sr_arrival_us) * 65536 / 1000000);
    }

    uint8_t *p = buf;
    *p++ = 0x81; /* V=2, RC=1 */
    *p++ = 201;  /* RR */
    *p++ = 0;
    *p++ = 7; /* length: 8 words - 1 */
    uint32_t v = htonl(reporter_ssrc);
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(st->ssrc);
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(((uint32_t)fraction << 24) | ((uint32_t)lost & 0xFFFFFF));
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(extended_max);
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(st->jitter >> 4);
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(st->last_sr_lsr);
    memcpy(p, &v, 4);
    p += 4;
    v = htonl(dlsr);
    memcpy(p, &v, 4);
    p += 4;

    if (cname) {
        size_t cname_len = strlen(cname);
        if (cname_len > 255) {
            cname_len = 255;
        }
        uint16_t words = (uint16_t)(sdes_len / 4 - 1);
        *p++ = 0x81; /* V=2, SC=1 */
        *p++ = 202;  /* SDES */
        *p++ = (uint8_t)(words >> 8);
        *p++ = (uint8_t)words;
        v = htonl(reporter_ssrc);
        memcpy(p, &v, 4);
        p += 4;
        *p++ = 1; /* CNAME */
        *p++ = (uint8_t)cname_len;
        memcpy(p, cname, cname_len);
        p += cname_len;
        *p++ = 0; /* item list terminator */
        while (((size_t)(p - buf)) % 4 != 0) {
            *p++ = 0;
        }
    }

    return (ssize_t)(p - buf);
}

ssize_t Compy_RtpReceiver_write_bye(
    Compy_RtpReceiver *self, uint32_t reporter_ssrc, int64_t now_us,
    const char *cname, uint8_t *buf, size_t cap) {
    /* BYE closes the compound (RFC 3550 §6.3.7), so reserve its eight
     * bytes before handing the rest of the buffer to the report. */
    ssize_t n = Compy_RtpReceiver_write_rr(
        self, reporter_ssrc, now_us, cname, buf, cap >= 8 ? cap - 8 : 0);
    if (n <= 0) {
        return n;
    }
    uint8_t *p = buf + n;
    *p++ = 0x81; /* V=2, SC=1 */
    *p++ = 203;  /* BYE */
    *p++ = 0;
    *p++ = 1; /* length: 2 words - 1 */
    uint32_t v = htonl(reporter_ssrc);
    memcpy(p, &v, 4);
    return n + 8;
}
