#include <compy/types/response_line.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_ResponseLine)

TEST parse_response_line(void) {
    const Compy_ResponseLine expected = {
        .version = {.major = 1, .minor = 1},
        .code = COMPY_STATUS_OK,
        .reason = CharSlice99_from_str("OK"),
    };

    TEST_PARSE("RTSP/1.1 200 OK\r\n", expected);

    Compy_ResponseLine result;

    ASSERT(Compy_ParseResult_is_failure(Compy_ResponseLine_parse(
        &result, CharSlice99_from_str("ABRACADABRA/1.1 200 OK\r\n"))));
    ASSERT(Compy_ParseResult_is_failure(Compy_ResponseLine_parse(
        &result, CharSlice99_from_str("RTSP/42 200 OK\r\n"))));
    ASSERT(Compy_ParseResult_is_failure(Compy_ResponseLine_parse(
        &result, CharSlice99_from_str("RTSP/1.1 ~~~ OK\r\n"))));

    PASS();
}

TEST serialize_response_line(void) {
    char buffer[100] = {0};

    const Compy_ResponseLine line = {
        .version = (Compy_RtspVersion){1, 0},
        .code = COMPY_STATUS_OK,
        .reason = CharSlice99_from_str("OK"),
    };

    const ssize_t ret =
        Compy_ResponseLine_serialize(&line, compy_string_writer(buffer));

    const char *expected = "RTSP/1.0 200 OK\r\n";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_response_line) {
    RUN_TEST(parse_response_line);
    RUN_TEST(serialize_response_line);
}
