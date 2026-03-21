/**
 * @file
 * @brief Base64 encoding and decoding (RFC 4648).
 *
 * Self-contained implementation with no external dependencies.
 * This is a private header; do not include directly from application code.
 */

#pragma once

#include <stddef.h>

#include <unistd.h>

#include <compy/priv/compiler_attrs.h>

/**
 * Encodes binary data to base64.
 *
 * @param[in] data Input binary data.
 * @param[in] len Length of @p data.
 * @param[out] out Output buffer for the base64 string (null-terminated).
 * @param[in] out_max Size of @p out. Must be at least
 *            `((len + 2) / 3) * 4 + 1` bytes.
 *
 * @pre `out != NULL`
 *
 * @return Number of characters written (excluding null terminator),
 *         or -1 if @p out_max is too small.
 */
ssize_t compy_base64_encode(
    const void *data, size_t len, char *out,
    size_t out_max) COMPY_PRIV_MUST_USE;

/**
 * Decodes a base64 string to binary.
 *
 * @param[in] b64 The base64-encoded string.
 * @param[in] len Length of @p b64 (excluding any null terminator).
 * @param[out] out Output buffer for decoded binary data.
 * @param[in] out_max Size of @p out. Must be at least
 *            `(len / 4) * 3` bytes.
 *
 * @pre `out != NULL`
 *
 * @return Number of bytes decoded, or -1 on invalid input or if
 *         @p out_max is too small.
 */
ssize_t compy_base64_decode(
    const char *b64, size_t len, void *out, size_t out_max) COMPY_PRIV_MUST_USE;
