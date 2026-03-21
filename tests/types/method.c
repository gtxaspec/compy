#include <compy/types/method.h>

#include "test_util.h"
#include <greatest.h>

DEF_TEST_PARSE(Compy_Method)

TEST parse_method(void) {
    TEST_PARSE("OPTIONS ", COMPY_METHOD_OPTIONS);
    TEST_PARSE("DESCRIBE ", COMPY_METHOD_DESCRIBE);
    TEST_PARSE("ANNOUNCE ", COMPY_METHOD_ANNOUNCE);
    TEST_PARSE("SETUP ", COMPY_METHOD_SETUP);
    TEST_PARSE("PLAY ", COMPY_METHOD_PLAY);
    TEST_PARSE("PAUSE ", COMPY_METHOD_PAUSE);
    TEST_PARSE("TEARDOWN ", COMPY_METHOD_TEARDOWN);
    TEST_PARSE("GET_PARAMETER ", COMPY_METHOD_GET_PARAMETER);
    TEST_PARSE("SET_PARAMETER ", COMPY_METHOD_SET_PARAMETER);
    TEST_PARSE("REDIRECT ", COMPY_METHOD_REDIRECT);
    TEST_PARSE("RECORD ", COMPY_METHOD_RECORD);

    Compy_Method result;

    ASSERT(Compy_ParseResult_is_failure(
        Compy_Method_parse(&result, CharSlice99_from_str("~123"))));
    ASSERT(Compy_ParseResult_is_failure(Compy_Method_parse(
        &result, CharSlice99_from_str("/ hello ~19r world"))));

    PASS();
}

SUITE(types_method) {
    RUN_TEST(parse_method);
}
