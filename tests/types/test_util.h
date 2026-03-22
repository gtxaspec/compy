#pragma once

#include <compy/types/error.h>

#include <stddef.h>

#include <greatest.h>
#include <slice99.h>

#define TEST_PARSE(input, expected) CHECK_CALL(test_parse(input, expected))

#ifndef TEST_PARSE_INIT_TYPE
#define TEST_PARSE_INIT_TYPE(_result)
#endif

#ifndef TEST_PARSE_DEINIT_TYPE
#define TEST_PARSE_DEINIT_TYPE(_result)
#endif

#define DEF_TEST_PARSE(T)                                                      \
    static enum greatest_test_res test_parse(const char *str, T expected) {    \
        T result;                                                              \
        TEST_PARSE_INIT_TYPE(result);                                          \
        const CharSlice99 input = CharSlice99_from_str((char *)str);           \
        Compy_ParseResult ret;                                                 \
                                                                               \
        for (size_t i = 1; i < input.len; i++) {                               \
            ret = T##_parse(&result, CharSlice99_sub(input, 0, i));            \
            ASSERT(Compy_ParseResult_is_partial(ret));                         \
        }                                                                      \
                                                                               \
        ret = T##_parse(&result, input);                                       \
        match(ret) {                                                           \
            of(Compy_ParseResult_Success, status) {                            \
                ASSERT(Compy_ParseStatus_is_complete(*status));                \
                /* ASSERT_EQ(input.len, status->offset); */                    \
                ASSERT(T##_eq(&result, &expected));                            \
            }                                                                  \
            of(Compy_ParseResult_Failure, error) {                             \
                const int err_bytes =                                          \
                    Compy_ParseError_print(*error, compy_file_writer(stderr)); \
                ASSERT(err_bytes >= 0);                                        \
                FAILm("Parsing failed");                                       \
            }                                                                  \
        }                                                                      \
                                                                               \
        TEST_PARSE_DEINIT_TYPE(result);                                        \
        PASS();                                                                \
    }
