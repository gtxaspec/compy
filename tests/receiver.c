#include <compy/backchannel.h>
#include <compy/receiver.h>
#include <compy/types/rtp.h>

#include <greatest.h>

#include <arpa/inet.h>
#include <string.h>

/* Test implementation of Compy_AudioReceiver */
typedef struct {
    uint8_t last_payload_type;
    uint32_t last_timestamp;
    uint32_t last_ssrc;
    size_t call_count;
    uint8_t last_payload[256];
    size_t last_payload_len;
} TestAudioReceiver;

static void TestAudioReceiver_on_audio(
    VSelf, uint8_t payload_type, uint32_t timestamp, uint32_t ssrc,
    U8Slice99 payload) {
    VSELF(TestAudioReceiver);
    self->last_payload_type = payload_type;
    self->last_timestamp = timestamp;
    self->last_ssrc = ssrc;
    self->call_count++;
    self->last_payload_len = payload.len < sizeof(self->last_payload)
                                 ? payload.len
                                 : sizeof(self->last_payload);
    memcpy(self->last_payload, payload.ptr, self->last_payload_len);
}

impl(Compy_AudioReceiver, TestAudioReceiver);

static void build_rtp_packet(
    uint8_t *buf, size_t *len, uint8_t pt, uint32_t ts, uint32_t ssrc,
    const uint8_t *payload, size_t payload_len) {
    Compy_RtpHeader hdr = {
        .version = 2,
        .padding = false,
        .extension = false,
        .csrc_count = 0,
        .marker = false,
        .payload_ty = pt,
        .sequence_number = htons(1),
        .timestamp = htonl(ts),
        .ssrc = htonl(ssrc),
        .csrc = NULL,
        .extension_profile = 0,
        .extension_payload_len = 0,
        .extension_payload = NULL,
    };

    const size_t hdr_size = Compy_RtpHeader_size(hdr);
    uint8_t *ret_buf __attribute__((unused)) =
        Compy_RtpHeader_serialize(hdr, buf);
    memcpy(buf + hdr_size, payload, payload_len);
    *len = hdr_size + payload_len;
}

TEST receiver_feed_audio(void) {
    TestAudioReceiver test_recv = {0};
    Compy_AudioReceiver audio =
        DYN(TestAudioReceiver, Compy_AudioReceiver, &test_recv);

    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, audio);

    const uint8_t audio_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t packet[256];
    size_t packet_len;
    build_rtp_packet(
        packet, &packet_len, 0, 8000, 0xABCD1234, audio_data,
        sizeof audio_data);

    const int ret =
        Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, packet, packet_len);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, test_recv.call_count);
    ASSERT_EQ(0, test_recv.last_payload_type);
    ASSERT_EQ(8000, test_recv.last_timestamp);
    ASSERT_EQ(0xABCD1234, test_recv.last_ssrc);
    ASSERT_EQ(sizeof audio_data, test_recv.last_payload_len);
    ASSERT_MEM_EQ(audio_data, test_recv.last_payload, sizeof audio_data);

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);

    PASS();
}

TEST receiver_feed_rtp_too_short(void) {
    TestAudioReceiver test_recv = {0};
    Compy_AudioReceiver audio =
        DYN(TestAudioReceiver, Compy_AudioReceiver, &test_recv);

    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, audio);

    const uint8_t short_data[] = {0x80, 0x00, 0x00, 0x01};
    const int ret = Compy_RtpReceiver_feed(
        recv, COMPY_CHANNEL_RTP, short_data, sizeof short_data);
    ASSERT_EQ(-1, ret);
    ASSERT_EQ(0, test_recv.call_count);

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);

    PASS();
}

TEST receiver_feed_no_audio_receiver(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    const uint8_t audio_data[] = {0x01};
    uint8_t packet[256];
    size_t packet_len;
    build_rtp_packet(packet, &packet_len, 0, 160, 1, audio_data, 1);

    const int ret =
        Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, packet, packet_len);
    ASSERT_EQ(0, ret);

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);

    PASS();
}

TEST receiver_feed_invalid_channel(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    const uint8_t data[] = {0x00};
    const int ret = Compy_RtpReceiver_feed(recv, 99, data, sizeof data);
    ASSERT_EQ(-1, ret);

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);

    PASS();
}

TEST rtp_header_roundtrip(void) {
    Compy_RtpHeader orig = {
        .version = 2,
        .padding = false,
        .extension = false,
        .csrc_count = 0,
        .marker = true,
        .payload_ty = 96,
        .sequence_number = htons(42),
        .timestamp = htonl(90000),
        .ssrc = htonl(0xDEADBEEF),
        .csrc = NULL,
        .extension_profile = 0,
        .extension_payload_len = 0,
        .extension_payload = NULL,
    };

    uint8_t buf[64];
    const size_t size = Compy_RtpHeader_size(orig);
    uint8_t *ret_buf __attribute__((unused)) =
        Compy_RtpHeader_serialize(orig, buf);

    Compy_RtpHeader parsed;
    const int hdr_size = Compy_RtpHeader_deserialize(&parsed, buf, size);

    ASSERT_EQ((int)size, hdr_size);
    ASSERT_EQ(orig.version, parsed.version);
    ASSERT_EQ(orig.padding, parsed.padding);
    ASSERT_EQ(orig.extension, parsed.extension);
    ASSERT_EQ(orig.csrc_count, parsed.csrc_count);
    ASSERT_EQ(orig.marker, parsed.marker);
    ASSERT_EQ(orig.payload_ty, parsed.payload_ty);
    ASSERT_EQ(orig.sequence_number, parsed.sequence_number);
    ASSERT_EQ(orig.timestamp, parsed.timestamp);
    ASSERT_EQ(orig.ssrc, parsed.ssrc);

    PASS();
}

TEST backchannel_lifecycle(void) {
    TestAudioReceiver test_recv = {0};
    Compy_AudioReceiver audio =
        DYN(TestAudioReceiver, Compy_AudioReceiver, &test_recv);

    Compy_BackchannelConfig cfg = Compy_BackchannelConfig_default();
    ASSERT_EQ(0, cfg.payload_type);
    ASSERT_EQ(8000, cfg.clock_rate);

    Compy_Backchannel *bc = Compy_Backchannel_new(cfg, audio);
    ASSERT(bc != NULL);

    Compy_BackchannelConfig got = Compy_Backchannel_get_config(bc);
    ASSERT_EQ(cfg.payload_type, got.payload_type);
    ASSERT_EQ(cfg.clock_rate, got.clock_rate);

    Compy_RtpReceiver *recv = Compy_Backchannel_get_receiver(bc);
    ASSERT(recv != NULL);

    /* Feed audio through the backchannel receiver */
    const uint8_t audio_data[] = {0xAA, 0xBB};
    uint8_t packet[256];
    size_t packet_len;
    build_rtp_packet(packet, &packet_len, 0, 160, 42, audio_data, 2);

    int ret =
        Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, packet, packet_len);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, test_recv.call_count);
    ASSERT_EQ(0, test_recv.last_payload_type);

    VCALL(DYN(Compy_Backchannel, Compy_Droppable, bc), drop);
    PASS();
}

static void build_rtp_packet_seq(
    uint8_t *buf, size_t *len, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    Compy_RtpHeader hdr = {
        .version = 2,
        .payload_ty = 0,
        .sequence_number = htons(seq),
        .timestamp = htonl(ts),
        .ssrc = htonl(ssrc),
    };
    const size_t hdr_size = Compy_RtpHeader_size(hdr);
    uint8_t *ret_buf __attribute__((unused)) =
        Compy_RtpHeader_serialize(hdr, buf);
    buf[hdr_size] = 0xFF;
    *len = hdr_size + 1;
}

TEST receiver_rr_empty_without_traffic(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    uint8_t rr[64];
    ASSERT_EQ(
        0, Compy_RtpReceiver_write_rr(recv, 1, 1000, NULL, rr, sizeof rr));

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

TEST receiver_rr_reports_loss(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    uint8_t pkt[64];
    size_t len;
    const uint16_t seqs[] = {1, 2, 4, 5}; /* 3 lost */
    for (size_t i = 0; i < 4; i++) {
        build_rtp_packet_seq(pkt, &len, seqs[i], 160 * seqs[i], 0xC0FFEE);
        ASSERT_EQ(0, Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, pkt, len));
    }

    uint8_t rr[64];
    ssize_t n =
        Compy_RtpReceiver_write_rr(recv, 0x1234, 0, NULL, rr, sizeof rr);
    ASSERT_EQ(32, (int)n);
    ASSERT_EQ(0x81, rr[0]); /* V=2, RC=1 */
    ASSERT_EQ(201, rr[1]);  /* RR */
    ASSERT_EQ(7, rr[3]);    /* length in words - 1 */

    uint32_t reporter, source, loss_word, ext_max, lsr, dlsr;
    memcpy(&reporter, rr + 4, 4);
    memcpy(&source, rr + 8, 4);
    memcpy(&loss_word, rr + 12, 4);
    memcpy(&ext_max, rr + 16, 4);
    memcpy(&lsr, rr + 24, 4);
    memcpy(&dlsr, rr + 28, 4);
    ASSERT_EQ(0x1234, ntohl(reporter));
    ASSERT_EQ(0xC0FFEE, ntohl(source));
    ASSERT_EQ(1, ntohl(loss_word) & 0xFFFFFF);         /* cumulative lost */
    ASSERT_EQ(256 / 5, (int)(ntohl(loss_word) >> 24)); /* 1 of 5 expected */
    ASSERT_EQ(5, ntohl(ext_max));
    ASSERT_EQ(0, ntohl(lsr)); /* no SR seen */
    ASSERT_EQ(0, ntohl(dlsr));

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

TEST receiver_rr_lsr_dlsr_from_snooped_sr(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    uint8_t pkt[64];
    size_t len;
    build_rtp_packet_seq(pkt, &len, 7, 700, 0xC0FFEE);
    ASSERT_EQ(
        0,
        Compy_RtpReceiver_feed_at(recv, COMPY_CHANNEL_RTP, pkt, len, 1000000));

    /* Minimal SR: header + ssrc + NTP + RTP ts + counts (28 bytes). */
    uint8_t sr[28] = {0x80, 200, 0, 6};
    const uint8_t ntp[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    memcpy(sr + 8, ntp, 8);
    ASSERT_EQ(
        0, Compy_RtpReceiver_feed_at(
               recv, COMPY_CHANNEL_RTCP, sr, sizeof sr, 2000000));

    uint8_t rr[64];
    /* One second after the SR arrived. */
    ssize_t n =
        Compy_RtpReceiver_write_rr(recv, 1, 3000000, NULL, rr, sizeof rr);
    ASSERT_EQ(32, (int)n);
    uint32_t lsr, dlsr;
    memcpy(&lsr, rr + 24, 4);
    memcpy(&dlsr, rr + 28, 4);
    ASSERT_EQ(0xCCDD1122, ntohl(lsr)); /* middle 32 of the NTP stamp */
    ASSERT_EQ(65536, ntohl(dlsr));     /* 1 s in 1/65536 units */

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

TEST receiver_rr_jitter_zero_on_perfect_cadence(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);
    Compy_RtpReceiver_set_clock_rate(recv, 8000);

    uint8_t pkt[64];
    size_t len;
    for (uint16_t i = 1; i <= 20; i++) {
        build_rtp_packet_seq(pkt, &len, i, 160u * i, 0xC0FFEE);
        /* Arrival marches in exact 20 ms steps: transit is constant. */
        ASSERT_EQ(
            0, Compy_RtpReceiver_feed_at(
                   recv, COMPY_CHANNEL_RTP, pkt, len,
                   1000000 + 20000 * (int64_t)i));
    }

    uint8_t rr[64];
    ASSERT_EQ(
        32,
        (int)Compy_RtpReceiver_write_rr(recv, 1, 2000000, NULL, rr, sizeof rr));
    uint32_t jitter;
    memcpy(&jitter, rr + 20, 4);
    ASSERT_EQ(0, ntohl(jitter));

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

TEST receiver_rr_compound_with_cname(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    uint8_t pkt[64];
    size_t len;
    build_rtp_packet_seq(pkt, &len, 1, 160, 0xC0FFEE);
    ASSERT_EQ(0, Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, pkt, len));

    uint8_t rr[96];
    ssize_t n =
        Compy_RtpReceiver_write_rr(recv, 1, 0, "cam@compy", rr, sizeof rr);
    ASSERT(n > 32);
    ASSERT_EQ(0, n % 4);     /* compound stays word-aligned */
    ASSERT_EQ(0x81, rr[32]); /* SDES, one chunk */
    ASSERT_EQ(202, rr[33]);
    ASSERT_EQ(1, rr[40]); /* CNAME item */
    ASSERT_EQ(9, rr[41]);
    ASSERT_MEM_EQ("cam@compy", rr + 42, 9);

    /* Too-small buffer refuses instead of truncating. */
    ASSERT_EQ(
        -1, (int)Compy_RtpReceiver_write_rr(recv, 1, 0, "cam@compy", rr, 40));

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

TEST receiver_bye_closes_the_compound(void) {
    Compy_AudioReceiver no_audio = {.self = NULL, .vptr = NULL};
    Compy_RtpReceiver *recv = Compy_RtpReceiver_new(NULL, no_audio);

    uint8_t rr[128];
    /* No traffic: no report was ever sent, so no BYE is owed. */
    ASSERT_EQ(
        0, (int)Compy_RtpReceiver_write_bye(recv, 7, 0, NULL, rr, sizeof rr));

    uint8_t pkt[64];
    size_t len;
    build_rtp_packet_seq(pkt, &len, 1, 160, 0xC0FFEE);
    ASSERT_EQ(0, Compy_RtpReceiver_feed(recv, COMPY_CHANNEL_RTP, pkt, len));

    /* Bare report + BYE. */
    ssize_t n = Compy_RtpReceiver_write_bye(recv, 7, 0, NULL, rr, sizeof rr);
    ASSERT_EQ(40, (int)n);
    ASSERT_EQ(0x81, rr[32]);
    ASSERT_EQ(203, rr[33]);
    ASSERT_EQ(1, rr[35]); /* one SSRC, length 1 */
    uint32_t ssrc;
    memcpy(&ssrc, rr + 36, 4);
    ASSERT_EQ(7, ntohl(ssrc));

    /* Full compound keeps BYE last, after the SDES chunk. */
    n = Compy_RtpReceiver_write_bye(recv, 7, 0, "cam@compy", rr, sizeof rr);
    ASSERT(n > 40);
    ASSERT_EQ(0, n % 4);
    ASSERT_EQ(202, rr[33]);        /* SDES follows the RR */
    ASSERT_EQ(203, rr[n - 8 + 1]); /* BYE closes the compound */

    /* A buffer that fits the report but not the BYE refuses whole. */
    ASSERT_EQ(-1, (int)Compy_RtpReceiver_write_bye(recv, 7, 0, NULL, rr, 36));

    VCALL(DYN(Compy_RtpReceiver, Compy_Droppable, recv), drop);
    PASS();
}

SUITE(receiver) {
    RUN_TEST(receiver_feed_audio);
    RUN_TEST(receiver_feed_rtp_too_short);
    RUN_TEST(receiver_feed_no_audio_receiver);
    RUN_TEST(receiver_feed_invalid_channel);
    RUN_TEST(rtp_header_roundtrip);
    RUN_TEST(backchannel_lifecycle);
    RUN_TEST(receiver_rr_empty_without_traffic);
    RUN_TEST(receiver_rr_reports_loss);
    RUN_TEST(receiver_rr_lsr_dlsr_from_snooped_sr);
    RUN_TEST(receiver_rr_jitter_zero_on_perfect_cadence);
    RUN_TEST(receiver_rr_compound_with_cname);
    RUN_TEST(receiver_bye_closes_the_compound);
}
