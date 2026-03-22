#include <compy/types/header_map.h>

#include <stdlib.h>

#define TEST_PARSE_INIT_TYPE(result) result = Compy_HeaderMap_empty()

#include "test_util.h"
#include <greatest.h>

#include <slice99.h>

DEF_TEST_PARSE(Compy_HeaderMap)

#define HEADER_MAP                                                             \
    Compy_HeaderMap_from_array({                                               \
        {COMPY_HEADER_CONTENT_LENGTH, CharSlice99_from_str("10")},             \
        {COMPY_HEADER_ACCEPT_LANGUAGE, CharSlice99_from_str("English")},       \
        {COMPY_HEADER_CONTENT_TYPE,                                            \
         CharSlice99_from_str("application/octet-stream")},                    \
    })

#define HEADER_MAP_STR                                                         \
    "Content-Length: 10\r\nAccept-Language: English\r\nContent-Type: "         \
    "application/octet-stream\r\n\r\n"

TEST parse_header_map(void) {
    TEST_PARSE(HEADER_MAP_STR, HEADER_MAP);

    Compy_HeaderMap result = Compy_HeaderMap_empty();

    ASSERT(Compy_ParseResult_is_failure(Compy_HeaderMap_parse(
        &result, CharSlice99_from_str("~29838\r\n\r\n"))));
    ASSERT(Compy_ParseResult_is_failure(Compy_HeaderMap_parse(
        &result,
        CharSlice99_from_str("Content-Length: 10\r\n38@@2: 10\r\n\r\n"))));

    PASS();
}

TEST serialize_header_map(void) {
    char buffer[500] = {0};
    const Compy_HeaderMap map = HEADER_MAP;

    const ssize_t ret =
        Compy_HeaderMap_serialize(&map, compy_string_writer(buffer));
    ASSERT_EQ((ssize_t)strlen(HEADER_MAP_STR), ret);
    ASSERT_STR_EQ(HEADER_MAP_STR, buffer);

    PASS();
}

TEST find(void) {
    const Compy_HeaderMap map = HEADER_MAP;

    CharSlice99 content_length;
    const bool content_length_is_found = Compy_HeaderMap_find(
        &map, COMPY_HEADER_CONTENT_LENGTH, &content_length);

    ASSERT(content_length_is_found);
    ASSERT(
        CharSlice99_primitive_eq(content_length, CharSlice99_from_str("10")));

    PASS();
}

TEST contains_key(void) {
    const Compy_HeaderMap map = HEADER_MAP;

    ASSERT(Compy_HeaderMap_contains_key(&map, COMPY_HEADER_CONTENT_LENGTH));
    ASSERT(!Compy_HeaderMap_contains_key(&map, COMPY_HEADER_ALLOW));

    PASS();
}

TEST append(void) {
    Compy_HeaderMap map = Compy_HeaderMap_empty(), expected = HEADER_MAP;

    for (size_t i = 0; i < HEADER_MAP.len; i++) {
        Compy_HeaderMap_append(&map, HEADER_MAP.headers[i]);
    }

    ASSERT(Compy_HeaderMap_eq(&map, &expected));
    PASS();
}

TEST is_full(void) {
    Compy_HeaderMap map = Compy_HeaderMap_empty();

    ASSERT(!Compy_HeaderMap_is_full(&map));

    map.len = COMPY_HEADER_MAP_CAPACITY;
    ASSERT(Compy_HeaderMap_is_full(&map));

    PASS();
}

TEST scanf_header(void) {
    const Compy_HeaderMap map = HEADER_MAP;

    size_t content_length = 0;
    int ret = compy_scanf_header(
        &map, COMPY_HEADER_CONTENT_LENGTH, "%zd", &content_length);
    ASSERT_EQ(1, ret);
    ASSERT_EQ(10, content_length);

    ret = compy_scanf_header(&map, COMPY_HEADER_ACCEPT_LANGUAGE, "%*d");
    ASSERT_EQ(0, ret);

    char auth[32] = {0};
    ret = compy_scanf_header(&map, COMPY_HEADER_WWW_AUTHENTICATE, "%s", auth);
    ASSERT_EQ(-1, ret);

    PASS();
}

SUITE(types_header_map) {
    RUN_TEST(parse_header_map);
    RUN_TEST(serialize_header_map);
    RUN_TEST(find);
    RUN_TEST(contains_key);
    RUN_TEST(append);
    RUN_TEST(is_full);
    RUN_TEST(scanf_header);
}
