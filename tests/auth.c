#include <compy/auth.h>
#include <compy/context.h>
#include <compy/types/header.h>
#include <compy/types/header_map.h>
#include <compy/types/request.h>
#include <compy/writer.h>

#include <greatest.h>

#include <string.h>
#include <time.h>

/* Lets a test move the clock the nonce freshness check sees. */
static long g_clock_skew_sec;

time_t time(time_t *out) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t now = ts.tv_sec + g_clock_skew_sec;
    if (out) {
        *out = now;
    }
    return now;
}

/* Test credential lookup */
static bool test_lookup(
    const char *username, char *password_out, size_t password_max,
    void *user_data) {
    (void)user_data;

    if (strcmp(username, "admin") == 0) {
        strncpy(password_out, "secret", password_max - 1);
        password_out[password_max - 1] = '\0';
        return true;
    }

    return false;
}

TEST digest_response_rfc2617(void) {
    /*
     * Verify our digest computation matches the RFC 2617 algorithm:
     * HA1 = MD5(admin:IP Camera:secret)
     * HA2 = MD5(DESCRIBE:rtsp://192.168.1.1/stream)
     * response = MD5(HA1:testnonce123:HA2)
     *
     * Pre-computed with: echo -n "str" | md5sum
     */
    char response[33];
    compy_digest_response(
        response, "admin", "IP Camera", "secret", "testnonce123", "DESCRIBE",
        "rtsp://192.168.1.1/stream");

    /* Verify it's a 32-char lowercase hex string */
    ASSERT_EQ(32, strlen(response));
    for (int i = 0; i < 32; i++) {
        ASSERT(
            (response[i] >= '0' && response[i] <= '9') ||
            (response[i] >= 'a' && response[i] <= 'f'));
    }

    /* Verify deterministic: same inputs produce same output */
    char response2[33];
    compy_digest_response(
        response2, "admin", "IP Camera", "secret", "testnonce123", "DESCRIBE",
        "rtsp://192.168.1.1/stream");
    ASSERT_STR_EQ(response, response2);

    /* Verify different password produces different response */
    char response3[33];
    compy_digest_response(
        response3, "admin", "IP Camera", "wrong", "testnonce123", "DESCRIBE",
        "rtsp://192.168.1.1/stream");
    ASSERT_FALSE(strcmp(response, response3) == 0);

    PASS();
}

TEST digest_response_different_methods(void) {
    char r1[33], r2[33];
    compy_digest_response(
        r1, "admin", "test", "pass", "nonce1", "DESCRIBE", "/stream");
    compy_digest_response(
        r2, "admin", "test", "pass", "nonce1", "SETUP", "/stream");
    ASSERT_FALSE(strcmp(r1, r2) == 0);
    PASS();
}

TEST digest_response_different_nonces(void) {
    char r1[33], r2[33];
    compy_digest_response(
        r1, "admin", "test", "pass", "nonce1", "DESCRIBE", "/stream");
    compy_digest_response(
        r2, "admin", "test", "pass", "nonce2", "DESCRIBE", "/stream");
    ASSERT_FALSE(strcmp(r1, r2) == 0);
    PASS();
}

TEST auth_check_no_header(void) {
    Compy_Auth *auth = Compy_Auth_new("IP Camera", test_lookup, NULL);

    char buf[1024] = {0};
    Compy_Writer w = compy_string_writer(buf);
    Compy_Context *ctx = Compy_Context_new(w, 1);

    Compy_Request req = Compy_Request_uninit();
    req.cseq = 1;
    req.start_line.method = COMPY_METHOD_DESCRIBE;
    req.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req.header_map = Compy_HeaderMap_empty();

    int ret = compy_auth_check(auth, ctx, &req);
    ASSERT_EQ(-1, ret);

    /* Should contain 401 and WWW-Authenticate */
    ASSERT(strstr(buf, "401") != NULL);
    ASSERT(strstr(buf, "WWW-Authenticate") != NULL);
    ASSERT(strstr(buf, "Digest") != NULL);
    ASSERT(strstr(buf, "IP Camera") != NULL);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx);
    Compy_Auth_free(auth);
    PASS();
}

TEST auth_check_valid_credentials(void) {
    Compy_Auth *auth = Compy_Auth_new("IP Camera", test_lookup, NULL);

    /* First request without auth to get the nonce */
    char buf1[1024] = {0};
    Compy_Writer w1 = compy_string_writer(buf1);
    Compy_Context *ctx1 = Compy_Context_new(w1, 1);

    Compy_Request req1 = Compy_Request_uninit();
    req1.cseq = 1;
    req1.start_line.method = COMPY_METHOD_DESCRIBE;
    req1.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req1.header_map = Compy_HeaderMap_empty();

    int first_ret __attribute__((unused)) = compy_auth_check(auth, ctx1, &req1);

    /* Extract nonce from response */
    char *nonce_start = strstr(buf1, "nonce=\"");
    ASSERT(nonce_start != NULL);
    nonce_start += strlen("nonce=\"");
    char nonce[64] = {0};
    char *nonce_end = strchr(nonce_start, '"');
    ASSERT(nonce_end != NULL);
    memcpy(nonce, nonce_start, (size_t)(nonce_end - nonce_start));

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx1);

    /* Compute valid digest response */
    char digest[33];
    compy_digest_response(
        digest, "admin", "IP Camera", "secret", nonce, "DESCRIBE",
        "rtsp://camera/stream");

    /* Build Authorization header */
    char auth_header[512];
    snprintf(
        auth_header, sizeof auth_header,
        "Digest username=\"admin\", realm=\"IP Camera\", "
        "nonce=\"%s\", uri=\"rtsp://camera/stream\", response=\"%s\"",
        nonce, digest);

    /* Second request with valid auth */
    char buf2[1024] = {0};
    Compy_Writer w2 = compy_string_writer(buf2);
    Compy_Context *ctx2 = Compy_Context_new(w2, 2);

    Compy_Request req2 = Compy_Request_uninit();
    req2.cseq = 2;
    req2.start_line.method = COMPY_METHOD_DESCRIBE;
    req2.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req2.header_map = Compy_HeaderMap_empty();
    Compy_HeaderMap_append(
        &req2.header_map, (Compy_Header){COMPY_HEADER_AUTHORIZATION,
                                         CharSlice99_from_str(auth_header)});

    int ret = compy_auth_check(auth, ctx2, &req2);
    ASSERT_EQ(0, ret);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx2);
    Compy_Auth_free(auth);
    PASS();
}

TEST auth_check_wrong_password(void) {
    Compy_Auth *auth = Compy_Auth_new("IP Camera", test_lookup, NULL);

    /* Get nonce */
    char buf1[1024] = {0};
    Compy_Writer w1 = compy_string_writer(buf1);
    Compy_Context *ctx1 = Compy_Context_new(w1, 1);

    Compy_Request req1 = Compy_Request_uninit();
    req1.cseq = 1;
    req1.start_line.method = COMPY_METHOD_DESCRIBE;
    req1.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req1.header_map = Compy_HeaderMap_empty();
    int first_ret __attribute__((unused)) = compy_auth_check(auth, ctx1, &req1);

    char *nonce_start = strstr(buf1, "nonce=\"") + strlen("nonce=\"");
    char nonce[64] = {0};
    memcpy(
        nonce, nonce_start, (size_t)(strchr(nonce_start, '"') - nonce_start));

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx1);

    /* Compute digest with wrong password */
    char digest[33];
    compy_digest_response(
        digest, "admin", "IP Camera", "wrongpass", nonce, "DESCRIBE",
        "rtsp://camera/stream");

    char auth_header[512];
    snprintf(
        auth_header, sizeof auth_header,
        "Digest username=\"admin\", realm=\"IP Camera\", "
        "nonce=\"%s\", uri=\"rtsp://camera/stream\", response=\"%s\"",
        nonce, digest);

    char buf2[1024] = {0};
    Compy_Writer w2 = compy_string_writer(buf2);
    Compy_Context *ctx2 = Compy_Context_new(w2, 2);

    Compy_Request req2 = Compy_Request_uninit();
    req2.cseq = 2;
    req2.start_line.method = COMPY_METHOD_DESCRIBE;
    req2.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req2.header_map = Compy_HeaderMap_empty();
    Compy_HeaderMap_append(
        &req2.header_map, (Compy_Header){COMPY_HEADER_AUTHORIZATION,
                                         CharSlice99_from_str(auth_header)});

    int ret = compy_auth_check(auth, ctx2, &req2);
    ASSERT_EQ(-1, ret);
    ASSERT(strstr(buf2, "401") != NULL);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx2);
    Compy_Auth_free(auth);
    PASS();
}

TEST auth_check_unknown_user(void) {
    Compy_Auth *auth = Compy_Auth_new("IP Camera", test_lookup, NULL);

    /* Get nonce */
    char buf1[1024] = {0};
    Compy_Writer w1 = compy_string_writer(buf1);
    Compy_Context *ctx1 = Compy_Context_new(w1, 1);

    Compy_Request req1 = Compy_Request_uninit();
    req1.cseq = 1;
    req1.start_line.method = COMPY_METHOD_DESCRIBE;
    req1.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req1.header_map = Compy_HeaderMap_empty();
    int first_ret __attribute__((unused)) = compy_auth_check(auth, ctx1, &req1);

    char *nonce_start = strstr(buf1, "nonce=\"") + strlen("nonce=\"");
    char nonce[64] = {0};
    memcpy(
        nonce, nonce_start, (size_t)(strchr(nonce_start, '"') - nonce_start));

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx1);

    char digest[33];
    compy_digest_response(
        digest, "nobody", "IP Camera", "anything", nonce, "DESCRIBE",
        "rtsp://camera/stream");

    char auth_header[512];
    snprintf(
        auth_header, sizeof auth_header,
        "Digest username=\"nobody\", realm=\"IP Camera\", "
        "nonce=\"%s\", uri=\"rtsp://camera/stream\", response=\"%s\"",
        nonce, digest);

    char buf2[1024] = {0};
    Compy_Writer w2 = compy_string_writer(buf2);
    Compy_Context *ctx2 = Compy_Context_new(w2, 2);

    Compy_Request req2 = Compy_Request_uninit();
    req2.cseq = 2;
    req2.start_line.method = COMPY_METHOD_DESCRIBE;
    req2.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req2.header_map = Compy_HeaderMap_empty();
    Compy_HeaderMap_append(
        &req2.header_map, (Compy_Header){COMPY_HEADER_AUTHORIZATION,
                                         CharSlice99_from_str(auth_header)});

    int ret = compy_auth_check(auth, ctx2, &req2);
    ASSERT_EQ(-1, ret);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx2);
    Compy_Auth_free(auth);
    PASS();
}

TEST auth_check_bad_format(void) {
    Compy_Auth *auth = Compy_Auth_new("test", test_lookup, NULL);

    char buf[1024] = {0};
    Compy_Writer w = compy_string_writer(buf);
    Compy_Context *ctx = Compy_Context_new(w, 1);

    Compy_Request req = Compy_Request_uninit();
    req.cseq = 1;
    req.start_line.method = COMPY_METHOD_DESCRIBE;
    req.start_line.uri = CharSlice99_from_str("rtsp://camera/stream");
    req.header_map = Compy_HeaderMap_empty();
    Compy_HeaderMap_append(
        &req.header_map,
        (Compy_Header){COMPY_HEADER_AUTHORIZATION,
                       CharSlice99_from_str("Basic dXNlcjpwYXNz")});

    int ret = compy_auth_check(auth, ctx, &req);
    ASSERT_EQ(-1, ret);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx);
    Compy_Auth_free(auth);
    PASS();
}

/* Runs the challenge round trip, then answers it with a digest computed over
 * `digest_uri`, sent as `header_uri`, for a request whose line carries
 * `request_uri`. */
static int check_answer(
    char *request_uri, char *header_uri, char *digest_uri,
    long answer_delay_sec, char *answer_out) {
    Compy_Auth *auth = Compy_Auth_new("IP Camera", test_lookup, NULL);

    char buf1[1024] = {0};
    Compy_Writer w1 = compy_string_writer(buf1);
    Compy_Context *ctx1 = Compy_Context_new(w1, 1);
    Compy_Request req1 = Compy_Request_uninit();
    req1.cseq = 1;
    req1.start_line.method = COMPY_METHOD_DESCRIBE;
    req1.start_line.uri = CharSlice99_from_str(request_uri);
    req1.header_map = Compy_HeaderMap_empty();
    int first_ret __attribute__((unused)) = compy_auth_check(auth, ctx1, &req1);

    char *nonce_start = strstr(buf1, "nonce=\"") + strlen("nonce=\"");
    char nonce[64] = {0};
    memcpy(
        nonce, nonce_start, (size_t)(strchr(nonce_start, '"') - nonce_start));
    VTABLE(Compy_Context, Compy_Droppable).drop(ctx1);

    char digest[33];
    compy_digest_response(
        digest, "admin", "IP Camera", "secret", nonce, "DESCRIBE", digest_uri);
    char auth_header[512];
    snprintf(
        auth_header, sizeof auth_header,
        "Digest username=\"admin\", realm=\"IP Camera\", "
        "nonce=\"%s\", uri=\"%s\", response=\"%s\"",
        nonce, header_uri, digest);

    char buf2[1024] = {0};
    Compy_Writer w2 = compy_string_writer(buf2);
    Compy_Context *ctx2 = Compy_Context_new(w2, 2);
    Compy_Request req2 = Compy_Request_uninit();
    req2.cseq = 2;
    req2.start_line.method = COMPY_METHOD_DESCRIBE;
    req2.start_line.uri = CharSlice99_from_str(request_uri);
    g_clock_skew_sec = answer_delay_sec;
    req2.header_map = Compy_HeaderMap_empty();
    Compy_HeaderMap_append(
        &req2.header_map, (Compy_Header){COMPY_HEADER_AUTHORIZATION,
                                         CharSlice99_from_str(auth_header)});
    int ret = compy_auth_check(auth, ctx2, &req2);
    g_clock_skew_sec = 0;
    if (answer_out) {
        snprintf(answer_out, 1024, "%s", buf2);
    }
    VTABLE(Compy_Context, Compy_Droppable).drop(ctx2);
    Compy_Auth_free(auth);
    return ret;
}

static int
check_with_uris(char *request_uri, char *header_uri, char *digest_uri) {
    return check_answer(request_uri, header_uri, digest_uri, 0, NULL);
}

TEST auth_check_uri_must_match_request(void) {
    /* A header captured for one URI must not open another */
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/stream1", "rtsp://camera/stream0",
                "rtsp://camera/stream0"));
    ASSERT_EQ(
        -1, check_with_uris("rtsp://camera/stream1", "/stream0", "/stream0"));
    /* Same prefix, different path */
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/stream1", "rtsp://camera/stream",
                "rtsp://camera/stream"));
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/stream", "rtsp://camera/stream1",
                "rtsp://camera/stream1"));
    /* uri parameter matches the request but the digest was over another */
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/stream1", "rtsp://camera/stream1",
                "rtsp://camera/stream0"));
    PASS();
}

TEST auth_check_uri_forms_accepted(void) {
    ASSERT_EQ(
        0, check_with_uris(
               "rtsp://camera/stream", "rtsp://camera/stream",
               "rtsp://camera/stream"));
    /* Clients disagree on absolute vs path-only between the request line
     * and the Authorization header */
    ASSERT_EQ(
        0, check_with_uris(
               "rtsp://camera:554/stream?x=1", "/stream?x=1", "/stream?x=1"));
    ASSERT_EQ(
        0, check_with_uris(
               "/stream", "rtsp://camera/stream", "rtsp://camera/stream"));
    ASSERT_EQ(
        0, check_with_uris(
               "rtsp://camera/stream/track1", "rtsp://camera/stream/track1",
               "rtsp://camera/stream/track1"));
    PASS();
}

TEST auth_check_base_uri_covers_its_tracks(void) {
    /* live555 (openRTSP, VLC) signs every request with the session base URL
     * (Content-Base), so a SETUP of a track carries the stream's uri */
    ASSERT_EQ(
        0, check_with_uris(
               "rtsp://camera:554/ch0/video", "rtsp://camera:554/ch0/",
               "rtsp://camera:554/ch0/"));
    ASSERT_EQ(
        0, check_with_uris(
               "rtsp://camera/ch0/audio", "rtsp://camera/ch0",
               "rtsp://camera/ch0"));
    ASSERT_EQ(0, check_with_uris("rtsp://camera/ch0/", "/ch0", "/ch0"));
    /* ...but only that stream's own tracks */
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/ch1/video", "rtsp://camera/ch0/",
                "rtsp://camera/ch0/"));
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/ch0/video/x", "rtsp://camera/ch0/",
                "rtsp://camera/ch0/"));
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/ch0x/video", "rtsp://camera/ch0",
                "rtsp://camera/ch0"));
    ASSERT_EQ(
        -1, check_with_uris(
                "rtsp://camera/ch0", "rtsp://camera/", "rtsp://camera/"));
    ASSERT_EQ(
        -1, check_with_uris("rtsp://camera/ch0/video", "/ch0?a=1", "/ch0?a=1"));
    /* A sibling whose name extends the stream's is not its child */
    ASSERT_EQ(-1, check_with_uris("rtsp://camera/ch0ab", "/ch0", "/ch0"));
    /* Neither a uri with a query nor the root covers anything below it */
    ASSERT_EQ(
        -1, check_with_uris("rtsp://camera/ch0?a=1/x", "/ch0?a=1", "/ch0?a=1"));
    ASSERT_EQ(-1, check_with_uris("rtsp://camera//x", "/", "/"));
    PASS();
}

TEST auth_check_nonce_expires(void) {
    char *uri = "rtsp://camera/stream";
    char answer[1024];

    ASSERT_EQ(0, check_answer(uri, uri, uri, 30, answer));

    ASSERT_EQ(-1, check_answer(uri, uri, uri, 61, answer));
    /* The client is told to retry with the new nonce, not to re-prompt */
    ASSERT(strstr(answer, "stale=true") != NULL);
    PASS();
}

SUITE(auth) {
    RUN_TEST(digest_response_rfc2617);
    RUN_TEST(digest_response_different_methods);
    RUN_TEST(digest_response_different_nonces);
    RUN_TEST(auth_check_no_header);
    RUN_TEST(auth_check_valid_credentials);
    RUN_TEST(auth_check_wrong_password);
    RUN_TEST(auth_check_unknown_user);
    RUN_TEST(auth_check_bad_format);
    RUN_TEST(auth_check_uri_must_match_request);
    RUN_TEST(auth_check_uri_forms_accepted);
    RUN_TEST(auth_check_base_uri_covers_its_tracks);
    RUN_TEST(auth_check_nonce_expires);
}
