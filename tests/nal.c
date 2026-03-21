#include <compy/nal.h>
#include <compy/nal_transport.h>
#include <compy/transport.h>

#include <greatest.h>

#include <alloca.h>
#include <sys/socket.h>
#include <unistd.h>

static const Compy_H264NalHeader h264_header = {
    .forbidden_zero_bit = false,
    .ref_idc = 0b10,
    .unit_type = COMPY_H264_NAL_UNIT_SUBSET_SPS,
};

static const Compy_H265NalHeader h265_header = {
    .forbidden_zero_bit = false,
    .unit_type = COMPY_H265_NAL_UNIT_RASL_N,
    .nuh_layer_id = 0b110101,
    .nuh_temporal_id_plus1 = 0b101,
};

#define NAL_HEADER_TEST_GETTER(T, name, h264_value, h265_value)                \
    TEST header_##name##_h264(void) {                                          \
        const T result =                                                       \
            Compy_NalHeader_##name(Compy_NalHeader_H264(h264_header));         \
        ASSERT_EQ(h264_value, result);                                         \
        PASS();                                                                \
    }                                                                          \
                                                                               \
    TEST header_##name##_h265(void) {                                          \
        const T result =                                                       \
            Compy_NalHeader_##name(Compy_NalHeader_H265(h265_header));         \
        ASSERT_EQ(h265_value, result);                                         \
        PASS();                                                                \
    }

NAL_HEADER_TEST_GETTER(
    uint8_t, unit_type, h264_header.unit_type, h265_header.unit_type)
NAL_HEADER_TEST_GETTER(
    size_t, size, COMPY_H264_NAL_HEADER_SIZE, COMPY_H265_NAL_HEADER_SIZE)
NAL_HEADER_TEST_GETTER(
    size_t, fu_size, COMPY_H264_FU_HEADER_SIZE, COMPY_H265_FU_HEADER_SIZE)

#undef NAL_HEADER_TEST_GETTER

TEST header_serialize_h264(void) {
    uint8_t buffer[COMPY_H264_NAL_HEADER_SIZE] = {0};
    Compy_NalHeader_serialize(Compy_NalHeader_H264(h264_header), buffer);
    ASSERT_EQ(0b01001111, buffer[0]);

    PASS();
}

TEST header_serialize_h265(void) {
    uint8_t buffer[COMPY_H265_NAL_HEADER_SIZE] = {0};
    Compy_NalHeader_serialize(Compy_NalHeader_H265(h265_header), buffer);
    ASSERT_EQ(0b00010001, buffer[0]);
    ASSERT_EQ(0b10101101, buffer[1]);

    PASS();
}

TEST header_serialize_fu_h264(void) {
    uint8_t buffer[COMPY_H264_FU_HEADER_SIZE] = {0};
    const bool is_first_fragment = true, is_last_fragment = false;

    Compy_NalHeader_write_fu_header(
        Compy_NalHeader_H264(h264_header), buffer, is_first_fragment,
        is_last_fragment);

    ASSERT_EQ(0b01011100, buffer[0]);
    ASSERT_EQ(0b10001111, buffer[1]);

    PASS();
}

TEST header_serialize_fu_h265(void) {
    uint8_t buffer[COMPY_H265_FU_HEADER_SIZE] = {0};
    const bool is_first_fragment = true, is_last_fragment = false;

    Compy_NalHeader_write_fu_header(
        Compy_NalHeader_H265(h265_header), buffer, is_first_fragment,
        is_last_fragment);

    ASSERT_EQ(0b01100011, buffer[0]);
    ASSERT_EQ(0b10101101, buffer[1]);
    ASSERT_EQ(0b10001000, buffer[2]);

    PASS();
}

TEST determine_start_code(void) {
    ASSERT_EQ(NULL, compy_determine_start_code(U8Slice99_empty()));

#define CHECK(expected, ...)                                                   \
    ASSERT_EQ(                                                                 \
        expected,                                                              \
        compy_determine_start_code(                                            \
            (U8Slice99)Slice99_typed_from_array((uint8_t[]){__VA_ARGS__})))

    CHECK(NULL, 0x00);
    CHECK(NULL, 0x00, 0x00);
    CHECK(compy_test_start_code_3b, 0x00, 0x00, 0x01);
    CHECK(compy_test_start_code_3b, 0x00, 0x00, 0x01, 0xAB);
    CHECK(compy_test_start_code_3b, 0x00, 0x00, 0x01, 0xAB, 0x8C);

    CHECK(compy_test_start_code_4b, 0x00, 0x00, 0x00, 0x01);
    CHECK(compy_test_start_code_4b, 0x00, 0x00, 0x00, 0x01, 0xAB);
    CHECK(compy_test_start_code_4b, 0x00, 0x00, 0x00, 0x01, 0xAB, 0x8C);

#undef CHECK

    PASS();
}

TEST test_start_code_3b(void) {
    ASSERT_EQ(0, compy_test_start_code_3b(U8Slice99_empty()));

#define CHECK(expected, ...)                                                   \
    ASSERT_EQ(                                                                 \
        expected,                                                              \
        compy_test_start_code_3b(                                              \
            (U8Slice99)Slice99_typed_from_array((uint8_t[]){__VA_ARGS__})))

    CHECK(0, 0x00);
    CHECK(0, 0x00, 0x00);
    CHECK(3, 0x00, 0x00, 0x01);
    CHECK(3, 0x00, 0x00, 0x01, 0xAB);
    CHECK(3, 0x00, 0x00, 0x01, 0xAB, 0x8C);

#undef CHECK

    PASS();
}

TEST test_start_code_4b(void) {
    ASSERT_EQ(0, compy_test_start_code_4b(U8Slice99_empty()));

#define CHECK(expected, ...)                                                   \
    ASSERT_EQ(                                                                 \
        expected,                                                              \
        compy_test_start_code_4b(                                              \
            (U8Slice99)Slice99_typed_from_array((uint8_t[]){__VA_ARGS__})))

    CHECK(0, 0x00);
    CHECK(0, 0x00, 0x00);
    CHECK(4, 0x00, 0x00, 0x00, 0x01);
    CHECK(4, 0x00, 0x00, 0x00, 0x01, 0xAB);
    CHECK(4, 0x00, 0x00, 0x00, 0x01, 0xAB, 0x8C);

#undef CHECK

    PASS();
}

/* --- NAL fragmentation test infrastructure --- */

#define MAX_TEST_PACKETS 64

typedef struct {
    uint8_t data[1500];
    ssize_t len;
} TestPacket;

typedef struct {
    TestPacket packets[MAX_TEST_PACKETS];
    int count;
} TestPacketCapture;

static void capture_init(TestPacketCapture *cap) {
    cap->count = 0;
}

static void capture_recv_all(TestPacketCapture *cap, int fd) {
    while (cap->count < MAX_TEST_PACKETS) {
        ssize_t n = recv(
            fd, cap->packets[cap->count].data, sizeof cap->packets[0].data,
            MSG_DONTWAIT);
        if (n <= 0) {
            break;
        }
        cap->packets[cap->count].len = n;
        cap->count++;
    }
}

/* Parse RTP marker bit from a captured packet (byte 1, bit 7) */
static bool packet_rtp_marker(const TestPacket *pkt) {
    return (pkt->data[1] & 0x80) != 0;
}

/* Get the FU header byte from an H.264 FU-A packet.
 * RTP header is 12 bytes, FU indicator is byte 12, FU header is byte 13. */
static uint8_t packet_h264_fu_header(const TestPacket *pkt) {
    return pkt->data[13];
}

/* Get the FU header byte from an H.265 FU packet.
 * RTP header is 12 bytes, payload header is bytes 12-13, FU header is byte 14.
 */
static uint8_t packet_h265_fu_header(const TestPacket *pkt) {
    return pkt->data[14];
}

static bool fu_start_bit(uint8_t fu_hdr) {
    return (fu_hdr & 0x80) != 0;
}

static bool fu_end_bit(uint8_t fu_hdr) {
    return (fu_hdr & 0x40) != 0;
}

/* Helper: set up a socketpair + NalTransport with given max sizes */
typedef struct {
    int fds[2];
    Compy_NalTransport *nal;
} FragTestCtx;

static void frag_setup(FragTestCtx *ctx, size_t max_h264, size_t max_h265) {
    int ok = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, ctx->fds);
    assert(ok == 0);

    srand(42);
    Compy_Transport t = compy_transport_udp(ctx->fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    Compy_NalTransportConfig cfg = {
        .max_h264_nalu_size = max_h264, .max_h265_nalu_size = max_h265};
    ctx->nal = Compy_NalTransport_new_with_config(rtp, cfg);
}

static void frag_teardown(FragTestCtx *ctx) {
    VTABLE(Compy_NalTransport, Compy_Droppable).drop(ctx->nal);
    close(ctx->fds[1]);
}

/* --- Test: small NALU fits in one packet, no fragmentation --- */
TEST fu_h264_small_nalu_no_fragmentation(void) {
    FragTestCtx ctx;
    frag_setup(&ctx, 1200, 4096);

    /* 5-byte payload + 1-byte header = 6, well under 1200 */
    uint8_t payload[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    Compy_NalUnit nalu = {
        .header =
            Compy_NalHeader_H264(Compy_H264NalHeader_parse(0x65)), /* IDR */
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT_EQ(1, cap.count);
    /* Single packet should have marker bit set (IDR slice) */
    ASSERT(packet_rtp_marker(&cap.packets[0]));

    frag_teardown(&ctx);
    PASS();
}

/* --- Test: boundary NALU triggers fragmentation (regression for S+E bug) ---
 */
TEST fu_h264_boundary_nalu_fragments(void) {
    FragTestCtx ctx;
    /* max=10. H.264 header=1, so payload of 9 → total 10 ≥ max → fragment.
     * FU header=2, effective max=10-2=8, so 9 bytes → 2 fragments. */
    frag_setup(&ctx, 10, 10);

    uint8_t payload[9];
    for (int i = 0; i < 9; i++)
        payload[i] = (uint8_t)(i + 1);

    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H264(Compy_H264NalHeader_parse(0x65)),
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    /* Must be >= 2 fragments (the old bug produced 1 with S+E both set) */
    ASSERT(cap.count >= 2);

    /* No packet should have both S and E set */
    for (int i = 0; i < cap.count; i++) {
        uint8_t fu = packet_h264_fu_header(&cap.packets[i]);
        ASSERT_FALSE(fu_start_bit(fu) && fu_end_bit(fu));
    }

    frag_teardown(&ctx);
    PASS();
}

/* --- Test: large NALU produces correct S/E bit pattern --- */
TEST fu_h264_large_nalu_se_bits(void) {
    FragTestCtx ctx;
    /* max=20, FU overhead=2, effective=18. Payload=100 → ceil(100/18)=6
     * fragments */
    frag_setup(&ctx, 20, 20);

    uint8_t payload[100];
    for (int i = 0; i < 100; i++)
        payload[i] = (uint8_t)i;

    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H264(Compy_H264NalHeader_parse(0x65)),
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT(cap.count >= 3);

    /* First fragment: S=1, E=0 */
    uint8_t first = packet_h264_fu_header(&cap.packets[0]);
    ASSERT(fu_start_bit(first));
    ASSERT_FALSE(fu_end_bit(first));
    ASSERT_FALSE(packet_rtp_marker(&cap.packets[0]));

    /* Middle fragments: S=0, E=0 */
    for (int i = 1; i < cap.count - 1; i++) {
        uint8_t mid = packet_h264_fu_header(&cap.packets[i]);
        ASSERT_FALSE(fu_start_bit(mid));
        ASSERT_FALSE(fu_end_bit(mid));
        ASSERT_FALSE(packet_rtp_marker(&cap.packets[i]));
    }

    /* Last fragment: S=0, E=1, marker=1 */
    uint8_t last = packet_h264_fu_header(&cap.packets[cap.count - 1]);
    ASSERT_FALSE(fu_start_bit(last));
    ASSERT(fu_end_bit(last));
    ASSERT(packet_rtp_marker(&cap.packets[cap.count - 1]));

    frag_teardown(&ctx);
    PASS();
}

/* --- Test: reassemble fragmented payload matches original --- */
TEST fu_h264_payload_integrity(void) {
    FragTestCtx ctx;
    frag_setup(&ctx, 30, 30);

    uint8_t payload[80];
    for (int i = 0; i < 80; i++)
        payload[i] = (uint8_t)(i ^ 0xAA);

    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H264(Compy_H264NalHeader_parse(0x41)),
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT(cap.count >= 2);

    /* Reassemble: each fragment is RTP(12) + FU-indicator(1) + FU-header(1) +
     * data */
    uint8_t reassembled[256];
    size_t total = 0;
    for (int i = 0; i < cap.count; i++) {
        size_t rtp_hdr = 12;
        size_t fu_hdr = 2; /* H.264 FU-A: indicator + header */
        size_t data_offset = rtp_hdr + fu_hdr;
        size_t data_len = (size_t)cap.packets[i].len - data_offset;
        memcpy(
            reassembled + total, cap.packets[i].data + data_offset, data_len);
        total += data_len;
    }

    ASSERT_EQ(sizeof payload, total);
    ASSERT_MEM_EQ(payload, reassembled, sizeof payload);

    frag_teardown(&ctx);
    PASS();
}

/* --- Test: H.265 fragmentation with 3-byte FU header --- */
TEST fu_h265_fragmentation_se_bits(void) {
    FragTestCtx ctx;
    /* H.265: NAL header=2, FU header=3. max=20, effective=20-3=17. */
    frag_setup(&ctx, 1200, 20);

    uint8_t payload[50];
    for (int i = 0; i < 50; i++)
        payload[i] = (uint8_t)(i + 0x10);

    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H265((Compy_H265NalHeader){
            .forbidden_zero_bit = false,
            .unit_type = 1, /* TRAIL_R */
            .nuh_layer_id = 0,
            .nuh_temporal_id_plus1 = 1,
        }),
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT(cap.count >= 2);

    /* Verify S/E bits (H.265 FU header at offset 14) */
    uint8_t first = packet_h265_fu_header(&cap.packets[0]);
    ASSERT(fu_start_bit(first));
    ASSERT_FALSE(fu_end_bit(first));

    uint8_t last = packet_h265_fu_header(&cap.packets[cap.count - 1]);
    ASSERT_FALSE(fu_start_bit(last));
    ASSERT(fu_end_bit(last));
    ASSERT(packet_rtp_marker(&cap.packets[cap.count - 1]));

    /* No packet has both S and E */
    for (int i = 0; i < cap.count; i++) {
        uint8_t fu = packet_h265_fu_header(&cap.packets[i]);
        ASSERT_FALSE(fu_start_bit(fu) && fu_end_bit(fu));
    }

    frag_teardown(&ctx);
    PASS();
}

/* --- Test: H.265 payload integrity after reassembly --- */
TEST fu_h265_payload_integrity(void) {
    FragTestCtx ctx;
    frag_setup(&ctx, 1200, 25);

    uint8_t payload[60];
    for (int i = 0; i < 60; i++)
        payload[i] = (uint8_t)(i ^ 0x55);

    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H265((Compy_H265NalHeader){
            .forbidden_zero_bit = false,
            .unit_type = 19, /* IDR_W_RADL */
            .nuh_layer_id = 0,
            .nuh_temporal_id_plus1 = 1,
        }),
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) = Compy_NalTransport_send_packet(
        ctx.nal, Compy_RtpTimestamp_Raw(0), nalu);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT(cap.count >= 2);

    /* Reassemble: RTP(12) + payload-hdr(2) + FU-header(1) + data */
    uint8_t reassembled[256];
    size_t total = 0;
    for (int i = 0; i < cap.count; i++) {
        size_t data_offset = 12 + 3; /* RTP + H.265 FU header */
        size_t data_len = (size_t)cap.packets[i].len - data_offset;
        memcpy(
            reassembled + total, cap.packets[i].data + data_offset, data_len);
        total += data_len;
    }

    ASSERT_EQ(sizeof payload, total);
    ASSERT_MEM_EQ(payload, reassembled, sizeof payload);

    frag_teardown(&ctx);
    PASS();
}

SUITE(nal) {
    RUN_TEST(header_unit_type_h264);
    RUN_TEST(header_unit_type_h265);
    RUN_TEST(header_size_h264);
    RUN_TEST(header_size_h265);
    RUN_TEST(header_fu_size_h264);
    RUN_TEST(header_fu_size_h265);

    RUN_TEST(header_serialize_h264);
    RUN_TEST(header_serialize_h265);
    RUN_TEST(header_serialize_fu_h264);
    RUN_TEST(header_serialize_fu_h265);

    RUN_TEST(determine_start_code);
    RUN_TEST(test_start_code_3b);
    RUN_TEST(test_start_code_4b);

    RUN_TEST(fu_h264_small_nalu_no_fragmentation);
    RUN_TEST(fu_h264_boundary_nalu_fragments);
    RUN_TEST(fu_h264_large_nalu_se_bits);
    RUN_TEST(fu_h264_payload_integrity);
    RUN_TEST(fu_h265_fragmentation_se_bits);
    RUN_TEST(fu_h265_payload_integrity);
}
