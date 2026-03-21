#include <compy/auth.h>

#include <compy/context.h>
#include <compy/types/header.h>
#include <compy/types/header_map.h>
#include <compy/types/request.h>
#include <compy/types/status_code.h>

#include <compy/priv/md5.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

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
    compy_md5_hex((const uint8_t *)buf, strlen(buf), ha1);

    snprintf(buf, sizeof buf, "%s:%s", method, uri);
    compy_md5_hex((const uint8_t *)buf, strlen(buf), ha2);

    snprintf(buf, sizeof buf, "%s:%s:%s", ha1, nonce, ha2);
    compy_md5_hex((const uint8_t *)buf, strlen(buf), out);
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
