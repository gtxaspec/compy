#include <compy/types/header.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_Header)

TEST parse_header(void) {
    TEST_PARSE(
        "User-Agent: LibVLC/3.0.8 (LIVE555 Streaming Media v2018.02.18)\r\n",
        ((Compy_Header){
            COMPY_HEADER_USER_AGENT,
            CharSlice99_from_str(
                "LibVLC/3.0.8 (LIVE555 Streaming Media v2018.02.18)"),
        }));

    Compy_Header result;

    ASSERT(Compy_ParseResult_is_failure(
        Compy_Header_parse(&result, CharSlice99_from_str("~@~"))));

    PASS();
}

TEST serialize_header(void) {
    char buffer[200] = {0};

    const Compy_Header header = {
        COMPY_HEADER_CONTENT_LENGTH,
        CharSlice99_from_str("123"),
    };

    const ssize_t ret =
        Compy_Header_serialize(&header, compy_string_writer(buffer));

    const char *expected = "Content-Length: 123\r\n";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_header) {
    RUN_TEST(parse_header);
    RUN_TEST(serialize_header);
}
