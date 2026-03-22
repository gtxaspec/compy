#include <compy/types/status_code.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_StatusCode)

TEST parse_status_code(void) {
    TEST_PARSE("100 ", COMPY_STATUS_CONTINUE);
    TEST_PARSE("200 ", COMPY_STATUS_OK);
    TEST_PARSE("303 ", COMPY_STATUS_SEE_OTHER);
    TEST_PARSE("404 ", COMPY_STATUS_NOT_FOUND);
    TEST_PARSE("551 ", COMPY_STATUS_OPTION_NOT_SUPPORTED);

    Compy_StatusCode result;

    ASSERT(Compy_ParseResult_is_failure(
        Compy_StatusCode_parse(&result, CharSlice99_from_str("blah"))));
    ASSERT(Compy_ParseResult_is_failure(
        Compy_StatusCode_parse(&result, CharSlice99_from_str("~ 2424 blah"))));

    PASS();
}

TEST serialize_status_code(void) {
    char buffer[20] = {0};

    const Compy_StatusCode status = COMPY_STATUS_NOT_FOUND;

    const ssize_t ret =
        Compy_StatusCode_serialize(&status, compy_string_writer(buffer));

    const char *expected = "404";

    ASSERT_EQ((ssize_t)strlen(expected), ret);
    ASSERT_STR_EQ(expected, buffer);

    PASS();
}

SUITE(types_status_code) {
    RUN_TEST(parse_status_code);
    RUN_TEST(serialize_status_code);
}
