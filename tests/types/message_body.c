#include <compy/types/message_body.h>

#include <greatest.h>

TEST parse_message_body(void) {
    const CharSlice99 input =
        CharSlice99_from_str(" 012345 ~ abc(^*%  D#NIN#3   ");
    const size_t content_length = input.len;

    Compy_MessageBody result;
    Compy_ParseResult ret;

    ret = Compy_MessageBody_parse(&result, input, content_length + 100);
    ASSERT(Compy_ParseResult_is_partial(ret));
    ret = Compy_MessageBody_parse(&result, input, content_length + 3);
    ASSERT(Compy_ParseResult_is_partial(ret));

    ret = Compy_MessageBody_parse(&result, input, content_length);
    ASSERT(Compy_ParseResult_is_complete(ret));

    PASS();
}

TEST empty(void) {
    ASSERT(CharSlice99_primitive_eq(
        Compy_MessageBody_empty(), CharSlice99_empty()));
    PASS();
}

SUITE(types_message_body) {
    RUN_TEST(parse_message_body);
    RUN_TEST(empty);
}
