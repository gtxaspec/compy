#pragma once

#include <compy/types/error.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <datatype99.h>
#include <slice99.h>

#define MATCH(parse_expr)                                                      \
    do {                                                                       \
        const Compy_ParseResult parse_res_var = parse_expr;                    \
                                                                               \
        match(parse_res_var) {                                                 \
            of(Compy_ParseResult_Success, status) {                            \
                match(*status) {                                               \
                    of(Compy_ParseStatus_Complete, offset) input =             \
                        CharSlice99_advance(input, *offset);                   \
                    otherwise return parse_res_var;                            \
                }                                                              \
            }                                                                  \
            of(Compy_ParseResult_Failure, err) {                               \
                return Compy_ParseResult_Failure(*err);                        \
            }                                                                  \
        }                                                                      \
    } while (0)

/**
 * Consume @p input until @p matcher returns false.
 */
Compy_ParseResult compy_match_until(
    CharSlice99 input, bool (*matcher)(char c, void *ctx), void *ctx);

Compy_ParseResult
compy_match_until_str(CharSlice99 input, const char *restrict str);

Compy_ParseResult compy_match_until_crlf(CharSlice99 input);
Compy_ParseResult compy_match_until_double_crlf(CharSlice99 input);
Compy_ParseResult compy_match_char(CharSlice99 input, char c);
Compy_ParseResult compy_match_str(CharSlice99 input, const char *restrict str);
Compy_ParseResult compy_match_whitespaces(CharSlice99 input);
Compy_ParseResult compy_match_non_whitespaces(CharSlice99 input);
Compy_ParseResult compy_match_numeric(CharSlice99 input);
Compy_ParseResult compy_match_ident(CharSlice99 input);
Compy_ParseResult compy_match_header_name(CharSlice99 input);
