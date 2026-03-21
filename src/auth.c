#include <compy/auth.h>

#include <compy/context.h>
#include <compy/types/header.h>
#include <compy/types/header_map.h>
#include <compy/types/request.h>
#include <compy/types/status_code.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

/*
 * Minimal MD5 implementation (RFC 1321).
 * Public domain — derived from the RSA reference implementation.
 */

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} MD5_CTX;

static void md5_init(MD5_CTX *ctx);
static void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len);
static void md5_final(MD5_CTX *ctx, uint8_t digest[16]);

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROT(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

#define STEP(f, a, b, c, d, x, t, s)                                           \
    do {                                                                       \
        (a) += f((b), (c), (d)) + (x) + (t);                                   \
        (a) = ROT((a), (s));                                                   \
        (a) += (b);                                                            \
    } while (0)

static const uint8_t md5_padding[64] = {0x80};

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for (int i = 0; i < 16; i++) {
        x[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }

    STEP(F, a, b, c, d, x[0], 0xd76aa478, 7);
    STEP(F, d, a, b, c, x[1], 0xe8c7b756, 12);
    STEP(F, c, d, a, b, x[2], 0x242070db, 17);
    STEP(F, b, c, d, a, x[3], 0xc1bdceee, 22);
    STEP(F, a, b, c, d, x[4], 0xf57c0faf, 7);
    STEP(F, d, a, b, c, x[5], 0x4787c62a, 12);
    STEP(F, c, d, a, b, x[6], 0xa8304613, 17);
    STEP(F, b, c, d, a, x[7], 0xfd469501, 22);
    STEP(F, a, b, c, d, x[8], 0x698098d8, 7);
    STEP(F, d, a, b, c, x[9], 0x8b44f7af, 12);
    STEP(F, c, d, a, b, x[10], 0xffff5bb1, 17);
    STEP(F, b, c, d, a, x[11], 0x895cd7be, 22);
    STEP(F, a, b, c, d, x[12], 0x6b901122, 7);
    STEP(F, d, a, b, c, x[13], 0xfd987193, 12);
    STEP(F, c, d, a, b, x[14], 0xa679438e, 17);
    STEP(F, b, c, d, a, x[15], 0x49b40821, 22);

    STEP(G, a, b, c, d, x[1], 0xf61e2562, 5);
    STEP(G, d, a, b, c, x[6], 0xc040b340, 9);
    STEP(G, c, d, a, b, x[11], 0x265e5a51, 14);
    STEP(G, b, c, d, a, x[0], 0xe9b6c7aa, 20);
    STEP(G, a, b, c, d, x[5], 0xd62f105d, 5);
    STEP(G, d, a, b, c, x[10], 0x02441453, 9);
    STEP(G, c, d, a, b, x[15], 0xd8a1e681, 14);
    STEP(G, b, c, d, a, x[4], 0xe7d3fbc8, 20);
    STEP(G, a, b, c, d, x[9], 0x21e1cde6, 5);
    STEP(G, d, a, b, c, x[14], 0xc33707d6, 9);
    STEP(G, c, d, a, b, x[3], 0xf4d50d87, 14);
    STEP(G, b, c, d, a, x[8], 0x455a14ed, 20);
    STEP(G, a, b, c, d, x[13], 0xa9e3e905, 5);
    STEP(G, d, a, b, c, x[2], 0xfcefa3f8, 9);
    STEP(G, c, d, a, b, x[7], 0x676f02d9, 14);
    STEP(G, b, c, d, a, x[12], 0x8d2a4c8a, 20);

    STEP(H, a, b, c, d, x[5], 0xfffa3942, 4);
    STEP(H, d, a, b, c, x[8], 0x8771f681, 11);
    STEP(H, c, d, a, b, x[11], 0x6d9d6122, 16);
    STEP(H, b, c, d, a, x[14], 0xfde5380c, 23);
    STEP(H, a, b, c, d, x[1], 0xa4beea44, 4);
    STEP(H, d, a, b, c, x[4], 0x4bdecfa9, 11);
    STEP(H, c, d, a, b, x[7], 0xf6bb4b60, 16);
    STEP(H, b, c, d, a, x[10], 0xbebfbc70, 23);
    STEP(H, a, b, c, d, x[13], 0x289b7ec6, 4);
    STEP(H, d, a, b, c, x[0], 0xeaa127fa, 11);
    STEP(H, c, d, a, b, x[3], 0xd4ef3085, 16);
    STEP(H, b, c, d, a, x[6], 0x04881d05, 23);
    STEP(H, a, b, c, d, x[9], 0xd9d4d039, 4);
    STEP(H, d, a, b, c, x[12], 0xe6db99e5, 11);
    STEP(H, c, d, a, b, x[15], 0x1fa27cf8, 16);
    STEP(H, b, c, d, a, x[2], 0xc4ac5665, 23);

    STEP(I, a, b, c, d, x[0], 0xf4292244, 6);
    STEP(I, d, a, b, c, x[7], 0x432aff97, 10);
    STEP(I, c, d, a, b, x[14], 0xab9423a7, 15);
    STEP(I, b, c, d, a, x[5], 0xfc93a039, 21);
    STEP(I, a, b, c, d, x[12], 0x655b59c3, 6);
    STEP(I, d, a, b, c, x[3], 0x8f0ccc92, 10);
    STEP(I, c, d, a, b, x[10], 0xffeff47d, 15);
    STEP(I, b, c, d, a, x[1], 0x85845dd1, 21);
    STEP(I, a, b, c, d, x[8], 0x6fa87e4f, 6);
    STEP(I, d, a, b, c, x[15], 0xfe2ce6e0, 10);
    STEP(I, c, d, a, b, x[6], 0xa3014314, 15);
    STEP(I, b, c, d, a, x[13], 0x4e0811a1, 21);
    STEP(I, a, b, c, d, x[4], 0xf7537e82, 6);
    STEP(I, d, a, b, c, x[11], 0xbd3af235, 10);
    STEP(I, c, d, a, b, x[2], 0x2ad7d2bb, 15);
    STEP(I, b, c, d, a, x[9], 0xeb86d391, 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void md5_init(MD5_CTX *ctx) {
    ctx->count = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len) {
    size_t index = (size_t)(ctx->count & 0x3F);
    ctx->count += len;

    size_t i = 0;
    if (index) {
        size_t part = 64 - index;
        if (len >= part) {
            memcpy(ctx->buffer + index, data, part);
            md5_transform(ctx->state, ctx->buffer);
            i = part;
        } else {
            memcpy(ctx->buffer + index, data, len);
            return;
        }
    }

    for (; i + 64 <= len; i += 64) {
        md5_transform(ctx->state, data + i);
    }

    if (i < len) {
        memcpy(ctx->buffer, data + i, len - i);
    }
}

static void md5_final(MD5_CTX *ctx, uint8_t digest[16]) {
    uint8_t bits[8];
    uint64_t bit_count = ctx->count * 8;
    for (int i = 0; i < 8; i++) {
        bits[i] = (uint8_t)(bit_count >> (i * 8));
    }

    size_t index = (size_t)(ctx->count & 0x3F);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    md5_update(ctx, md5_padding, pad_len);
    md5_update(ctx, bits, 8);

    for (int i = 0; i < 4; i++) {
        digest[i * 4] = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

/* --- End MD5 --- */

static void md5_hex(const uint8_t *data, size_t len, char out[33]) {
    MD5_CTX ctx;
    uint8_t digest[16];
    md5_init(&ctx);
    md5_update(&ctx, data, len);
    md5_final(&ctx, digest);

    for (int i = 0; i < 16; i++) {
        sprintf(out + i * 2, "%02x", digest[i]);
    }
    out[32] = '\0';
}

/* --- Digest auth --- */

#define NONCE_LEN 32
#define MAX_REALM 128

struct Compy_Auth {
    char realm[MAX_REALM];
    char nonce[NONCE_LEN + 1];
    Compy_CredentialLookup lookup;
    void *user_data;
};

static int generate_nonce(char out[NONCE_LEN + 1]) {
    uint8_t raw[NONCE_LEN / 2];

#if defined(__linux__) && defined(SYS_getrandom)
    if (syscall(SYS_getrandom, raw, sizeof raw, 0) == (long)sizeof raw) {
        goto format;
    }
#endif

    FILE *f = fopen("/dev/urandom", "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(raw, 1, sizeof raw, f);
    fclose(f);
    if (n != sizeof raw) {
        return -1;
    }

#if defined(__linux__) && defined(SYS_getrandom)
format:
#endif
    for (size_t i = 0; i < sizeof raw; i++) {
        sprintf(out + i * 2, "%02x", raw[i]);
    }
    out[NONCE_LEN] = '\0';
    return 0;
}

static void explicit_bzero_fallback(void *buf, size_t len) {
    volatile uint8_t *p = buf;
    while (len--) {
        *p++ = 0;
    }
}

Compy_Auth *Compy_Auth_new(
    const char *realm, Compy_CredentialLookup lookup, void *user_data) {
    assert(realm);
    assert(lookup);

    Compy_Auth *self = malloc(sizeof *self);
    assert(self);

    strncpy(self->realm, realm, sizeof(self->realm) - 1);
    self->realm[sizeof(self->realm) - 1] = '\0';
    self->lookup = lookup;
    self->user_data = user_data;

    if (generate_nonce(self->nonce) != 0) {
        free(self);
        return NULL;
    }

    return self;
}

void Compy_Auth_free(Compy_Auth *self) {
    assert(self);
    free(self);
}

void compy_digest_response(
    char out[restrict 33], const char *username, const char *realm,
    const char *password, const char *nonce, const char *method,
    const char *uri) {
    /*
     * RFC 2617 Section 3.2.2:
     * HA1 = MD5(username:realm:password)
     * HA2 = MD5(method:uri)
     * response = MD5(HA1:nonce:HA2)
     */
    char ha1[33], ha2[33];
    char buf[1024];

    snprintf(buf, sizeof buf, "%s:%s:%s", username, realm, password);
    md5_hex((const uint8_t *)buf, strlen(buf), ha1);

    snprintf(buf, sizeof buf, "%s:%s", method, uri);
    md5_hex((const uint8_t *)buf, strlen(buf), ha2);

    snprintf(buf, sizeof buf, "%s:%s:%s", ha1, nonce, ha2);
    md5_hex((const uint8_t *)buf, strlen(buf), out);
}

/**
 * Parses a single key="value" or key=value parameter from a Digest
 * Authorization header. Advances *p past the consumed parameter.
 */
static int parse_digest_param(
    const char **p, char *key, size_t key_max, char *value, size_t value_max) {
    const char *s = *p;

    /* Skip whitespace and commas */
    while (*s == ' ' || *s == ',' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return -1;
    }

    /* Read key */
    size_t ki = 0;
    while (*s && *s != '=' && ki < key_max - 1) {
        key[ki++] = *s++;
    }
    key[ki] = '\0';

    if (*s != '=') {
        return -1;
    }
    s++;

    /* Read value (optionally quoted) */
    bool quoted = (*s == '"');
    if (quoted) {
        s++;
    }

    size_t vi = 0;
    while (*s && vi < value_max - 1) {
        if (quoted && *s == '"') {
            break;
        }
        if (!quoted && (*s == ',' || *s == ' ')) {
            break;
        }
        value[vi++] = *s++;
    }
    value[vi] = '\0';

    if (quoted && *s == '"') {
        s++;
    }

    *p = s;
    return 0;
}

int compy_auth_check(
    Compy_Auth *self, Compy_Context *ctx, const Compy_Request *req) {
    assert(self);
    assert(ctx);
    assert(req);

    CharSlice99 auth_value;
    if (!Compy_HeaderMap_find(
            &req->header_map, COMPY_HEADER_AUTHORIZATION, &auth_value)) {
        goto challenge;
    }

    /* Must start with "Digest " */
    const char *digest_prefix = "Digest ";
    const size_t prefix_len = strlen(digest_prefix);
    if (auth_value.len < prefix_len ||
        strncasecmp(auth_value.ptr, digest_prefix, prefix_len) != 0) {
        goto challenge;
    }

    /* Parse Digest parameters */
    char username[256] = {0};
    char realm[MAX_REALM] = {0};
    char nonce[NONCE_LEN + 1] = {0};
    char uri[512] = {0};
    char response[33] = {0};

    /* Null-terminate for parsing */
    char auth_str[1024];
    size_t copy_len = auth_value.len - prefix_len < sizeof(auth_str) - 1
                          ? auth_value.len - prefix_len
                          : sizeof(auth_str) - 1;
    memcpy(auth_str, auth_value.ptr + prefix_len, copy_len);
    auth_str[copy_len] = '\0';

    const char *p = auth_str;
    char key[64], value[512];
    while (parse_digest_param(&p, key, sizeof key, value, sizeof value) == 0) {
        if (strcmp(key, "username") == 0) {
            strncpy(username, value, sizeof(username) - 1);
        } else if (strcmp(key, "realm") == 0) {
            strncpy(realm, value, sizeof(realm) - 1);
        } else if (strcmp(key, "nonce") == 0) {
            strncpy(nonce, value, sizeof(nonce) - 1);
        } else if (strcmp(key, "uri") == 0) {
            strncpy(uri, value, sizeof(uri) - 1);
        } else if (strcmp(key, "response") == 0) {
            strncpy(response, value, sizeof(response) - 1);
        }
    }

    /* Validate required fields */
    if (username[0] == '\0' || nonce[0] == '\0' || uri[0] == '\0' ||
        response[0] == '\0') {
        goto challenge;
    }

    /* Verify realm matches */
    if (strcmp(realm, self->realm) != 0) {
        goto challenge;
    }

    /* Verify nonce matches */
    if (strcmp(nonce, self->nonce) != 0) {
        goto challenge;
    }

    /* Look up password */
    char password[256];
    if (!self->lookup(username, password, sizeof password, self->user_data)) {
        explicit_bzero_fallback(password, sizeof password);
        goto challenge;
    }

    /* Extract method as C string */
    char method[32];
    size_t method_len = req->start_line.method.len < sizeof(method) - 1
                            ? req->start_line.method.len
                            : sizeof(method) - 1;
    memcpy(method, req->start_line.method.ptr, method_len);
    method[method_len] = '\0';

    /* Compute expected response */
    char expected[33];
    compy_digest_response(
        expected, username, self->realm, password, nonce, method, uri);

    /* Wipe password from stack immediately */
    explicit_bzero_fallback(password, sizeof password);

    /* Constant-time comparison to prevent timing attacks */
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) {
        diff |= (uint8_t)(expected[i] ^ response[i]);
    }

    if (diff != 0) {
        goto challenge;
    }

    return 0;

challenge:
    /* Fresh nonce on every challenge to prevent replay attacks */
    generate_nonce(self->nonce);
    compy_header(
        ctx, COMPY_HEADER_WWW_AUTHENTICATE, "Digest realm=\"%s\", nonce=\"%s\"",
        self->realm, self->nonce);
    compy_respond(ctx, COMPY_STATUS_UNAUTHORIZED, "Unauthorized");
    return -1;
}
