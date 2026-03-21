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

/*
 * Regression test for FU fragmentation bug: when a NALU's total size
 * (header + payload) exceeds max_packet_size but payload alone is smaller,
 * the old code produced a single FU with both S and E bits set.
 * The fix ensures we always get >= 2 fragments.
 */
TEST fu_fragmentation_no_single_fragment(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    srand(42);
    Compy_Transport t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    /* Set max to 10 bytes. H.264 NAL header is 1 byte, FU header is 2 bytes.
     * Create a payload of 9 bytes so total NALU = 1 + 9 = 10 >= max.
     * Without the fix: payload(9) < max(10), so packets_count=0 and
     * only the remainder block runs with is_first=true AND is_last=true.
     * With the fix: max adjusted to 10-2=8, so we get 2 fragments. */
    Compy_NalTransportConfig cfg = {
        .max_h264_nalu_size = 10, .max_h265_nalu_size = 10};
    Compy_NalTransport *nal = Compy_NalTransport_new_with_config(rtp, cfg);

    uint8_t payload[9] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    Compy_NalUnit nalu = {
        .header = Compy_NalHeader_H264(
            Compy_H264NalHeader_parse(0x65)), /* IDR slice */
        .payload = U8Slice99_new(payload, sizeof payload),
    };

    int ret __attribute__((unused)) =
        Compy_NalTransport_send_packet(nal, Compy_RtpTimestamp_Raw(0), nalu);

    /* Count packets received — should be >= 2 */
    int packet_count = 0;
    uint8_t buf[1500];
    while (1) {
        ssize_t n = recv(fds[1], buf, sizeof buf, MSG_DONTWAIT);
        if (n <= 0) {
            break;
        }
        packet_count++;
    }

    ASSERT(packet_count >= 2);

    VTABLE(Compy_NalTransport, Compy_Droppable).drop(nal);
    close(fds[1]);

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

    RUN_TEST(fu_fragmentation_no_single_fragment);
}
