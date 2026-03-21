#include <compy/priv/base64.h>

#include <stdint.h>
#include <string.h>

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t b64_decode_table[256] = {
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,  ['E'] = 4,  ['F'] = 5,
    ['G'] = 6,  ['H'] = 7,  ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17,
    ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
    ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
    ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
    ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
    ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53,
    ['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
};

static int b64_valid_char(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

ssize_t
compy_base64_encode(const void *data, size_t len, char *out, size_t out_max) {
    const uint8_t *src = data;
    size_t needed = ((len + 2) / 3) * 4 + 1;

    if (out_max < needed) {
        return -1;
    }

    size_t oi = 0;
    size_t i;

    for (i = 0; i + 2 < len; i += 3) {
        uint32_t triplet =
            ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | src[i + 2];
        out[oi++] = b64_table[(triplet >> 18) & 0x3F];
        out[oi++] = b64_table[(triplet >> 12) & 0x3F];
        out[oi++] = b64_table[(triplet >> 6) & 0x3F];
        out[oi++] = b64_table[triplet & 0x3F];
    }

    if (i < len) {
        uint32_t triplet = (uint32_t)src[i] << 16;
        if (i + 1 < len) {
            triplet |= (uint32_t)src[i + 1] << 8;
        }

        out[oi++] = b64_table[(triplet >> 18) & 0x3F];
        out[oi++] = b64_table[(triplet >> 12) & 0x3F];
        out[oi++] = (i + 1 < len) ? b64_table[(triplet >> 6) & 0x3F] : '=';
        out[oi++] = '=';
    }

    out[oi] = '\0';
    return (ssize_t)oi;
}

ssize_t
compy_base64_decode(const char *b64, size_t len, void *out, size_t out_max) {
    if (len == 0) {
        return 0;
    }

    if (len % 4 != 0) {
        return -1;
    }

    /* Validate all characters */
    for (size_t i = 0; i < len; i++) {
        if (!b64_valid_char((uint8_t)b64[i])) {
            return -1;
        }
    }

    size_t pad = 0;
    if (b64[len - 1] == '=')
        pad++;
    if (len > 1 && b64[len - 2] == '=')
        pad++;

    size_t decoded_len = (len / 4) * 3 - pad;
    if (out_max < decoded_len) {
        return -1;
    }

    uint8_t *dst = out;
    size_t oi = 0;

    for (size_t i = 0; i < len; i += 4) {
        uint32_t sextet_a = b64_decode_table[(uint8_t)b64[i]];
        uint32_t sextet_b = b64_decode_table[(uint8_t)b64[i + 1]];
        uint32_t sextet_c = b64_decode_table[(uint8_t)b64[i + 2]];
        uint32_t sextet_d = b64_decode_table[(uint8_t)b64[i + 3]];

        uint32_t triple =
            (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        if (oi < decoded_len)
            dst[oi++] = (uint8_t)((triple >> 16) & 0xFF);
        if (oi < decoded_len)
            dst[oi++] = (uint8_t)((triple >> 8) & 0xFF);
        if (oi < decoded_len)
            dst[oi++] = (uint8_t)(triple & 0xFF);
    }

    return (ssize_t)decoded_len;
}
