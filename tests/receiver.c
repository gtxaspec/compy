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

SUITE(receiver) {
    RUN_TEST(receiver_feed_audio);
    RUN_TEST(receiver_feed_rtp_too_short);
    RUN_TEST(receiver_feed_no_audio_receiver);
    RUN_TEST(receiver_feed_invalid_channel);
    RUN_TEST(rtp_header_roundtrip);
    RUN_TEST(backchannel_lifecycle);
}
