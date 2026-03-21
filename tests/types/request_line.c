#include <compy/types/request_line.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_RequestLine)

TEST parse_request_line(void) {
    const Compy_RequestLine expected = {
        .method = COMPY_METHOD_DESCRIBE,
        .uri = CharSlice99_from_str("http://example.com"),
        .version = {.major = 1, .minor = 1},
    };

    TEST_PARSE("DESCRIBE http://example.com RTSP/1.1\r\n", expected);

    Compy_RequestLine result;

    ASSERT(Compy_ParseResult_is_failure(Compy_RequestLine_parse(
        &result, CharSlice99_from_str("!!! http://example.com RTSP/1.1\r\n"))));
    ASSERT(Compy_ParseResult_is_failure(Compy_RequestLine_parse(
        &result, CharSlice99_from_str(
                     "DESCRIBE http://example.com ABRACADABRA/1.1\r\n"))));

    PASS();
}

TEST serialize_request_line(void) {
    char buffer[100] = {0};

    const Compy_RequestLine line = {
        .method = COMPY_METHOD_DESCRIBE,
        .uri = CharSlice99_from_str("http://example.com"),
        .version = (Compy_RtspVersion){1, 0},
    };

    const ssize_t ret =
        Compy_RequestLine_serialize(&line, compy_string_writer(buffer));

    const char *expected = "DESCRIBE http://example.com RTSP/1.0\r\n";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_request_line) {
    RUN_TEST(parse_request_line);
    RUN_TEST(serialize_request_line);
}
