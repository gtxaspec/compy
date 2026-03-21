/**
 * @file
 * @brief Possible parsing errors.
 */

#pragma once

#include <compy/writer.h>

#include <stdbool.h>
#include <stddef.h>

#include <datatype99.h>
#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/**
 * Types of data that can be failed to parse.
 */
typedef enum {
    /**
     * An integer (`-34`, `0`, `123`).
     */
    Compy_ParseType_Int,

    /**
     * An identifier (`abc`).
     */
    Compy_ParseType_Ident,

    /**
     * A header name (`Content-Length`, `Authorization`).
     */
    Compy_ParseType_HeaderName,
} Compy_ParseType;

/**
 * Returns a string representation of @p self.
 */
const char *
Compy_ParseType_str(Compy_ParseType self) COMPY_PRIV_MUST_USE;

/**
 * An error that might occur during parsing.
 *
 * ## Variants
 *
 *  - `ContentLength` -- An invalid value of the `Content-Length` header was
 * specified. Arguments:
 *    1. The value of this header.
 *  - `StrMismatch` -- Two given strings are uneqal. Arguments:
 *    1. Expected string.
 *    2. Actual string.
 *  - `TypeMismatch` -- Failed to parse an item. Arguments:
 *    1. A type of item failed to parse.
 *    2. The erroneous string.
 *  - `HeaderMapOverflow` -- An attempt to add a header to a full header map.
 *  - `MissingCSeq` -- Missing the `CSeq` header.
 *  - `InvalidCSeq` -- Failed to parse the `CSeq` header.
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */

// clang-format off
datatype99(
    Compy_ParseError,
    (Compy_ParseError_ContentLength, CharSlice99),
    (Compy_ParseError_StrMismatch, CharSlice99, CharSlice99),
    (Compy_ParseError_TypeMismatch, Compy_ParseType, CharSlice99),
    (Compy_ParseError_HeaderMapOverflow),
    (Compy_ParseError_MissingCSeq),
    (Compy_ParseError_InvalidCSeq, CharSlice99)
);
// clang-format on

/**
 * Prints @p self into @p w.
 *
 * @param[in] self The error to print.
 * @param[in] w The writer to be provided with data.
 *
 * @return The number of bytes written or a negative value on error.
 *
 * @pre `w.self && w.vptr`
 */
int Compy_ParseError_print(Compy_ParseError self, Compy_Writer w)
    COMPY_PRIV_MUST_USE;

/**
 * A status of successful parsing.
 *
 * ## Variants
 *
 *  - `Complete` -- The parsing has completed. Arguments:
 *    1. A number of consumed bytes from the beginning of input.
 *  - `Partial` -- Need more data to continue parsing.
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */

// clang-format off
datatype99(
    Compy_ParseStatus,
    (Compy_ParseStatus_Complete, size_t),
    (Compy_ParseStatus_Partial)
);
// clang-format on

/**
 * Returns whether @p self is complete.
 */
bool Compy_ParseStatus_is_complete(Compy_ParseStatus self)
    COMPY_PRIV_MUST_USE;

/**
 * Returns whether @p self is partial.
 */
bool Compy_ParseStatus_is_partial(Compy_ParseStatus self)
    COMPY_PRIV_MUST_USE;

/**
 * A result of parsing (either success or failure).
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */

// clang-format off
datatype99(
    Compy_ParseResult,
    (Compy_ParseResult_Success, Compy_ParseStatus),
    (Compy_ParseResult_Failure, Compy_ParseError)
);
// clang-format on

/**
 * Creates a **successful** and **partial** parse result.
 */
Compy_ParseResult Compy_ParseResult_partial(void) COMPY_PRIV_MUST_USE;

/**
 * Creates a **successful** and **complete** parse result with the byte offset
 * @p offset (from the beginning of input).
 */
Compy_ParseResult
Compy_ParseResult_complete(size_t offset) COMPY_PRIV_MUST_USE;

/**
 * Returns whether @p self is successful.
 */
bool Compy_ParseResult_is_success(Compy_ParseResult self)
    COMPY_PRIV_MUST_USE;

/**
 * Returns whether @p self is a failure.
 */
bool Compy_ParseResult_is_failure(Compy_ParseResult self)
    COMPY_PRIV_MUST_USE;

/**
 * Returns whether @p self is both **successful** and **partial**.
 */
bool Compy_ParseResult_is_partial(Compy_ParseResult self)
    COMPY_PRIV_MUST_USE;

/**
 * The same as #Compy_ParseResult_is_partial but for a complete result.
 */
bool Compy_ParseResult_is_complete(Compy_ParseResult self)
    COMPY_PRIV_MUST_USE;
