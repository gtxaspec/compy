#include <compy/types/rtsp_version.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_RtspVersion)

TEST parse_rtsp_version(void) {
    TEST_PARSE("RTSP/1.1 ", ((Compy_RtspVersion){1, 1}));
    TEST_PARSE("RTSP/0.0 ", ((Compy_RtspVersion){0, 0}));
    TEST_PARSE("RTSP/123.200 ", ((Compy_RtspVersion){123, 200}));
    TEST_PARSE("RTSP/0.200 ", ((Compy_RtspVersion){0, 200}));

    Compy_RtspVersion result;

    ASSERT(Compy_ParseResult_is_failure(
        Compy_RtspVersion_parse(&result, CharSlice99_from_str("192"))));
    ASSERT(Compy_ParseResult_is_failure(
        Compy_RtspVersion_parse(&result, CharSlice99_from_str(" ~ RTSP/"))));

    PASS();
}

TEST serialize_rtsp_version(void) {
    char buffer[20] = {0};

    const ssize_t ret = Compy_RtspVersion_serialize(
        &(Compy_RtspVersion){1, 0}, compy_string_writer(buffer));

    const char *expected = "RTSP/1.0";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_rtsp_version) {
    RUN_TEST(parse_rtsp_version);
    RUN_TEST(serialize_rtsp_version);
}
