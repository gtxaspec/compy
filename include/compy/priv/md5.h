/**
 * @file
 * @brief Minimal MD5 implementation (RFC 1321).
 *
 * Public domain — derived from the RSA reference implementation.
 * This is a private header; do not include directly from application code.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} Compy_Md5;

void compy_md5_init(Compy_Md5 *ctx);
void compy_md5_update(Compy_Md5 *ctx, const uint8_t *data, size_t len);
void compy_md5_final(Compy_Md5 *ctx, uint8_t digest[16]);

/**
 * Computes MD5 of @p data and writes the result as a 32-character lowercase
 * hex string to @p out (plus null terminator).
 *
 * @param[in] data Input data.
 * @param[in] len Length of @p data.
 * @param[out] out Output buffer, must be at least 33 bytes.
 */
void compy_md5_hex(const uint8_t *data, size_t len, char out[33]);
