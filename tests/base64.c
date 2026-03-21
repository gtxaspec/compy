#include <compy/priv/base64.h>

#include <greatest.h>

#include <string.h>

/* RFC 4648 Section 10 test vectors */
TEST encode_rfc4648_vectors(void) {
    char out[64];

    ASSERT_EQ(0, compy_base64_encode("", 0, out, sizeof out));
    ASSERT_STR_EQ("", out);

    ASSERT_EQ(4, compy_base64_encode("f", 1, out, sizeof out));
    ASSERT_STR_EQ("Zg==", out);

    ASSERT_EQ(4, compy_base64_encode("fo", 2, out, sizeof out));
    ASSERT_STR_EQ("Zm8=", out);

    ASSERT_EQ(4, compy_base64_encode("foo", 3, out, sizeof out));
    ASSERT_STR_EQ("Zm9v", out);

    ASSERT_EQ(8, compy_base64_encode("foob", 4, out, sizeof out));
    ASSERT_STR_EQ("Zm9vYg==", out);

    ASSERT_EQ(8, compy_base64_encode("fooba", 5, out, sizeof out));
    ASSERT_STR_EQ("Zm9vYmE=", out);

    ASSERT_EQ(8, compy_base64_encode("foobar", 6, out, sizeof out));
    ASSERT_STR_EQ("Zm9vYmFy", out);

    PASS();
}

TEST decode_rfc4648_vectors(void) {
    char out[64];

    ASSERT_EQ(0, compy_base64_decode("", 0, out, sizeof out));

    ASSERT_EQ(1, compy_base64_decode("Zg==", 4, out, sizeof out));
    ASSERT_MEM_EQ("f", out, 1);

    ASSERT_EQ(2, compy_base64_decode("Zm8=", 4, out, sizeof out));
    ASSERT_MEM_EQ("fo", out, 2);

    ASSERT_EQ(3, compy_base64_decode("Zm9v", 4, out, sizeof out));
    ASSERT_MEM_EQ("foo", out, 3);

    ASSERT_EQ(4, compy_base64_decode("Zm9vYg==", 8, out, sizeof out));
    ASSERT_MEM_EQ("foob", out, 4);

    ASSERT_EQ(5, compy_base64_decode("Zm9vYmE=", 8, out, sizeof out));
    ASSERT_MEM_EQ("fooba", out, 5);

    ASSERT_EQ(6, compy_base64_decode("Zm9vYmFy", 8, out, sizeof out));
    ASSERT_MEM_EQ("foobar", out, 6);

    PASS();
}

TEST encode_decode_roundtrip(void) {
    /* Binary data with all byte values */
    uint8_t data[256];
    for (int i = 0; i < 256; i++)
        data[i] = (uint8_t)i;

    char encoded[512];
    ssize_t enc_len =
        compy_base64_encode(data, sizeof data, encoded, sizeof encoded);
    ASSERT(enc_len > 0);

    uint8_t decoded[256];
    ssize_t dec_len =
        compy_base64_decode(encoded, (size_t)enc_len, decoded, sizeof decoded);
    ASSERT_EQ((ssize_t)sizeof data, dec_len);
    ASSERT_MEM_EQ(data, decoded, sizeof data);

    PASS();
}

TEST encode_buffer_too_small(void) {
    char out[4]; /* needs 5 for "Zg==" + null */
    ASSERT_EQ(-1, compy_base64_encode("f", 1, out, sizeof out));
    PASS();
}

TEST decode_invalid_length(void) {
    char out[64];
    /* Not a multiple of 4 */
    ASSERT_EQ(-1, compy_base64_decode("Zg=", 3, out, sizeof out));
    ASSERT_EQ(-1, compy_base64_decode("Z", 1, out, sizeof out));
    PASS();
}

TEST decode_invalid_chars(void) {
    char out[64];
    ASSERT_EQ(-1, compy_base64_decode("Zg!!", 4, out, sizeof out));
    ASSERT_EQ(-1, compy_base64_decode("@@@@", 4, out, sizeof out));
    PASS();
}

TEST decode_buffer_too_small(void) {
    char out[1]; /* needs 3 for "foo" */
    ASSERT_EQ(-1, compy_base64_decode("Zm9v", 4, out, sizeof out));
    PASS();
}

TEST encode_srtp_key_material(void) {
    /* 30 bytes (16-byte key + 14-byte salt) — typical SRTP keying material */
    uint8_t key[30] = {0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0,
                       0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39,
                       0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB,
                       0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6};
    char encoded[64];
    ssize_t enc_len =
        compy_base64_encode(key, sizeof key, encoded, sizeof encoded);
    ASSERT_EQ(40, enc_len); /* 30 bytes → 40 base64 chars */

    uint8_t decoded[30];
    ssize_t dec_len =
        compy_base64_decode(encoded, (size_t)enc_len, decoded, sizeof decoded);
    ASSERT_EQ(30, dec_len);
    ASSERT_MEM_EQ(key, decoded, sizeof key);

    PASS();
}

SUITE(base64) {
    RUN_TEST(encode_rfc4648_vectors);
    RUN_TEST(decode_rfc4648_vectors);
    RUN_TEST(encode_decode_roundtrip);
    RUN_TEST(encode_buffer_too_small);
    RUN_TEST(decode_invalid_length);
    RUN_TEST(decode_invalid_chars);
    RUN_TEST(decode_buffer_too_small);
    RUN_TEST(encode_srtp_key_material);
}
