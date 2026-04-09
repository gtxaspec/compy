#include <compy/types/request.h>

#define TEST_PARSE_INIT_TYPE(result) result = Compy_Request_uninit()

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_Request)

TEST parse_request(void) {
    const Compy_Request expected = {
        .start_line =
            {
                .method = COMPY_METHOD_DESCRIBE,
                .uri = CharSlice99_from_str("http://example.com"),
                .version = {.major = 1, .minor = 1},
            },
        .header_map = Compy_HeaderMap_from_array({
            {COMPY_HEADER_C_SEQ, CharSlice99_from_str("123")},
            {COMPY_HEADER_CONTENT_LENGTH, CharSlice99_from_str("10")},
            {COMPY_HEADER_ACCEPT_LANGUAGE, CharSlice99_from_str("English")},
            {COMPY_HEADER_CONTENT_TYPE,
             CharSlice99_from_str("application/octet-stream")},
        }),
        .body = CharSlice99_from_str("0123456789"),
        .cseq = 123,
    };

    TEST_PARSE(
        "DESCRIBE http://example.com RTSP/1.1\r\n"
        "CSeq: 123\r\n"
        "Content-Length: 10\r\n"
        "Accept-Language: English\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n0123456789",
        expected);

    PASS();
}

TEST serialize_request(void) {
    char buffer[500] = {0};

    const Compy_Request request = {
        .start_line =
            {
                .method = COMPY_METHOD_DESCRIBE,
                .uri = CharSlice99_from_str("http://example.com"),
                .version = {1, 0},
            },
        .header_map = Compy_HeaderMap_from_array({
            {COMPY_HEADER_CONTENT_TYPE,
             CharSlice99_from_str("application/octet-stream")},
        }),
        .body = CharSlice99_from_str("1234567890"),
        .cseq = 456,
    };

    const ssize_t ret =
        Compy_Request_serialize(&request, compy_string_writer(buffer));

    const char *expected =
        "DESCRIBE http://example.com RTSP/1.0\r\n"
        "CSeq: 456\r\n"
        "Content-Length: 10\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n1234567890";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

TEST parse_request_with_interleaved_prefix(void) {
    /*
     * During RTSPS streaming, the client sends RTCP RR as interleaved $
     * frames mixed with RTSP text. The parser must skip past binary frames
     * to find the RTSP request. This is exactly what happens when a client
     * sends a keepalive while receiving video.
     */

    /* Build: interleaved frame ($ ch=1, len=8, 8 bytes payload)
     *        + RTSP GET_PARAMETER request */
    const char interleaved_then_request[] =
        "$\x01\x00\x08"    /* $ channel=1 length=8 */
        "\x81\xc9\x00\x01" /* RTCP RR header (fake) */
        "\xAA\xBB\xCC\xDD" /* RTCP payload */
        "GET_PARAMETER rtsp://camera/stream RTSP/1.0\r\n"
        "CSeq: 5\r\n"
        "\r\n";

    Compy_Request result = Compy_Request_uninit();
    Compy_ParseResult ret = Compy_Request_parse(
        &result, CharSlice99_new(
                     (char *)interleaved_then_request,
                     sizeof interleaved_then_request - 1));

    ASSERT(Compy_ParseResult_is_complete(ret));
    ASSERT_EQ(5, result.cseq);

    const Compy_Method get_param = COMPY_METHOD_GET_PARAMETER;
    ASSERT(Compy_Method_eq(&result.start_line.method, &get_param));

    PASS();
}

TEST parse_request_with_multiple_interleaved(void) {
    /* Two interleaved frames before an RTSP request */
    const char input[] =
        "$\x00\x00\x04"    /* $ channel=0 length=4 */
        "\x11\x22\x33\x44" /* RTP data */
        "$\x01\x00\x04"    /* $ channel=1 length=4 */
        "\x55\x66\x77\x88" /* RTCP data */
        "OPTIONS rtsp://camera RTSP/1.0\r\n"
        "CSeq: 10\r\n"
        "\r\n";

    Compy_Request result = Compy_Request_uninit();
    Compy_ParseResult ret = Compy_Request_parse(
        &result, CharSlice99_new((char *)input, sizeof input - 1));

    ASSERT(Compy_ParseResult_is_complete(ret));
    ASSERT_EQ(10, result.cseq);

    const Compy_Method options = COMPY_METHOD_OPTIONS;
    ASSERT(Compy_Method_eq(&result.start_line.method, &options));

    PASS();
}

SUITE(types_request) {
    RUN_TEST(parse_request);
    RUN_TEST(serialize_request);
    RUN_TEST(parse_request_with_interleaved_prefix);
    RUN_TEST(parse_request_with_multiple_interleaved);
}
