#include "parsing.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

Compy_ParseResult compy_match_until(
    CharSlice99 input, bool (*matcher)(char c, void *ctx), void *ctx) {
    assert(matcher);

    size_t offset = 0;

    while (!CharSlice99_is_empty(input)) {
        if (!matcher(*(char *)input.ptr, ctx)) {
            return Compy_ParseResult_complete(offset);
        }

        input = CharSlice99_advance(input, 1);
        offset++;
    }

    return Compy_ParseResult_partial();
}

static bool whitespace_matcher(char c, void *ctx) {
    (void)ctx;
    return c == ' ';
}

static bool non_whitespace_matcher(char c, void *ctx) {
    (void)ctx;
    return !whitespace_matcher(c, NULL);
}

static bool numeric_matcher(char c, void *ctx) {
    (void)ctx;
    return isdigit(c);
}

static bool ident_matcher(char c, void *ctx) {
    (void)ctx;
    return isalnum(c) || c == '_';
}

static bool header_name_char_matcher(char c, void *ctx) {
    (void)ctx;
    return isalpha(c) || c == '-';
}

static bool char_matcher(char c, void *ctx) {
    const char expected = *(const char *)ctx;
    return c == expected;
}

Compy_ParseResult
compy_match_until_str(CharSlice99 input, const char *restrict str) {
    assert(str);

    const size_t str_len = strlen(str);
    assert(str_len > 0);

    size_t offset = 0;

    for (size_t i = 0; i < input.len; i++) {
        if (input.len - i < str_len) {
            return Compy_ParseResult_partial();
        }

        if (memcmp(input.ptr + i, str, str_len) == 0) {
            return Compy_ParseResult_complete(offset + str_len);
        }

        offset++;
    }

    return Compy_ParseResult_partial();
}

Compy_ParseResult compy_match_until_crlf(CharSlice99 input) {
    return compy_match_until_str(input, "\r\n");
}

Compy_ParseResult compy_match_until_double_crlf(CharSlice99 input) {
    return compy_match_until_str(input, "\r\n\r\n");
}

Compy_ParseResult compy_match_char(CharSlice99 input, char c) {
    return compy_match_until(input, char_matcher, &c);
}

Compy_ParseResult
compy_match_str(CharSlice99 input, const char *restrict str) {
    assert(str);

    const size_t str_len = strlen(str);

    size_t offset = 0;

    const size_t min_len = input.len < str_len ? input.len : str_len;

    const bool are_coinciding = memcmp(input.ptr, str, min_len) == 0;
    if (!are_coinciding) {
        CharSlice99 expected = CharSlice99_from_str((char *)str),
                    actual = CharSlice99_sub(input, 0, min_len);
        return Compy_ParseResult_Failure(
            Compy_ParseError_StrMismatch(expected, actual));
    }

    if (input.len < str_len) {
        return Compy_ParseResult_partial();
    }

    input = CharSlice99_advance(input, str_len);
    offset += str_len;
    return Compy_ParseResult_complete(offset);
}

Compy_ParseResult compy_match_whitespaces(CharSlice99 input) {
    return compy_match_until(input, whitespace_matcher, NULL);
}

Compy_ParseResult compy_match_non_whitespaces(CharSlice99 input) {
    return compy_match_until(input, non_whitespace_matcher, NULL);
}

// TODO: refactor the following functions.

Compy_ParseResult compy_match_numeric(CharSlice99 input) {
    if (!numeric_matcher(input.ptr[0], NULL)) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_TypeMismatch(Compy_ParseType_Int, input));
    }

    return compy_match_until(input, numeric_matcher, NULL);
}

Compy_ParseResult compy_match_ident(CharSlice99 input) {
    if (!ident_matcher(input.ptr[0], NULL)) {
        return Compy_ParseResult_Failure(
            Compy_ParseError_TypeMismatch(Compy_ParseType_Ident, input));
    }

    return compy_match_until(input, ident_matcher, NULL);
}

Compy_ParseResult compy_match_header_name(CharSlice99 input) {
    if (!header_name_char_matcher(input.ptr[0], NULL)) {
        return Compy_ParseResult_Failure(Compy_ParseError_TypeMismatch(
            Compy_ParseType_HeaderName, input));
    }

    return compy_match_until(input, header_name_char_matcher, NULL);
}
