#include <compy/types/sdp.h>

#include <greatest.h>

#define AUDIO_PORT             123
#define AUDIO_RTP_PAYLOAD_TYPE 456
#define SERVER_IP_ADDR         "0.0.0.0"

TEST serialize_sdp_line(void) {
    char buffer[20] = {0};

    const Compy_SdpLine sdp = {
        .ty = COMPY_SDP_ATTR,
        .value = CharSlice99_from_str("abc"),
    };

    const ssize_t ret =
        Compy_SdpLine_serialize(&sdp, compy_string_writer(buffer));

    const char *expected = "a=abc\r\n";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

TEST sdp_printf(void) {
    char buffer[20] = {0};

    const char *expected = "a=abc 123 @ def\r\n";

    const ssize_t ret = compy_sdp_printf(
        compy_string_writer(buffer), COMPY_SDP_ATTR, "abc %d @ %s", 123,
        "def");
    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

TEST sdp_describe(void) {
    char buffer[256] = {0};
    const Compy_Writer sdp = compy_string_writer(buffer);
    ssize_t ret = 0;

    // clang-format off
    COMPY_SDP_DESCRIBE(
        ret, sdp,
        (COMPY_SDP_VERSION, "0"),
        (COMPY_SDP_ORIGIN, "Compy 3855320066 3855320129 IN IP4 0.0.0.0"),
        (COMPY_SDP_SESSION_NAME, "Compy test"),
        (COMPY_SDP_CONNECTION, "IN IP4 %s", SERVER_IP_ADDR),
        (COMPY_SDP_TIME, "0 0"),
        (COMPY_SDP_MEDIA, "audio %d RTP/AVP %d", AUDIO_PORT, AUDIO_RTP_PAYLOAD_TYPE),
        (COMPY_SDP_ATTR, "control:audio"));
    // clang-format on

    const char *expected =
        "v=0\r\n"
        "o=Compy 3855320066 3855320129 IN IP4 0.0.0.0\r\n"
        "s=Compy test\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "t=0 0\r\n"
        "m=audio 123 RTP/AVP 456\r\n"
        "a=control:audio\r\n";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_sdp) {
    RUN_TEST(serialize_sdp_line);
    RUN_TEST(sdp_printf);
    RUN_TEST(sdp_describe);
}
