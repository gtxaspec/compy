/*
 * A simple RTSP server implementation using libevent [1].
 *
 * To obtain `audio.g711a` and `video.h264`:
 *
 * $ ffmpeg -i http://docs.evostream.com/sample_content/assets/bun33s.mp4 \
 *     -acodec pcm_mulaw -f mulaw -ar 8000 -ac 1 audio.g711a \
 *     -vcodec h264 -x264opts aud=1 video.h264
 *
 * [1] https://libevent.org/
 */

#include <compy.h>

#include "compy-libevent.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ENABLE_AUDIO
#define ENABLE_VIDEO

/* Media files loaded at startup via mmap */
static uint8_t *media_video = NULL;
static size_t media_video_len = 0;
static uint8_t *media_audio = NULL;
static size_t media_audio_len = 0;
static int media_video_fps = 30;

/* Authentication (NULL = disabled) */
static Compy_Auth *g_auth = NULL;

/* TLS/SRTP globals */
#ifdef COMPY_HAS_TLS
static Compy_TlsContext *g_tls_ctx = NULL;
static bool g_srtp_enabled = false;
static Compy_SrtpKeyMaterial g_srtp_key;
#endif

static int mmap_file(const char *path, uint8_t **out, size_t *out_len) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror(path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return -1;
    }

    *out_len = (size_t)st.st_size;
    *out = mmap(NULL, *out_len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (*out == MAP_FAILED) {
        perror("mmap");
        *out = NULL;
        return -1;
    }

    return 0;
}

#define SERVER_PORT 8554

#define AUDIO_PCMU_PAYLOAD_TYPE  0
#define AUDIO_SAMPLE_RATE        8000
#define AUDIO_SAMPLES_PER_PACKET 160
#define AUDIO_PACKETIZATION_TIME_US                                            \
    (1e6 / (AUDIO_SAMPLE_RATE / AUDIO_SAMPLES_PER_PACKET))

#define VIDEO_PAYLOAD_TYPE 96 // dynamic PT
#define VIDEO_SAMPLE_RATE  90000
#define VIDEO_FPS          30

#define RTCP_INTERVAL_SEC 5

#define BACKCHANNEL_PAYLOAD_TYPE 0 // PCMU
#define BACKCHANNEL_SAMPLE_RATE  8000

#define AUDIO_STREAM_ID       0
#define VIDEO_STREAM_ID       1
#define BACKCHANNEL_STREAM_ID 2

#define MAX_STREAMS 3

typedef struct {
    uint64_t session_id;
    Compy_RtpTransport *transport;
    Compy_Rtcp *rtcp;
    struct event *ev;
    struct event *rtcp_ev;
    Compy_Droppable ctx;
} Stream;

/* Backchannel audio receiver — logs received audio for demonstration */
typedef struct {
    size_t total_bytes;
} BackchannelReceiver;

static void BackchannelReceiver_on_audio(
    VSelf, uint8_t payload_type, uint32_t timestamp, uint32_t ssrc,
    U8Slice99 payload) {
    VSELF(BackchannelReceiver);
    (void)timestamp;
    (void)ssrc;

    self->total_bytes += payload.len;
    printf(
        "Backchannel: received %zu bytes (PT=%u, total=%zu)\n", payload.len,
        payload_type, self->total_bytes);
}

impl(Compy_AudioReceiver, BackchannelReceiver);

typedef struct {
    struct event_base *base;
    struct bufferevent *bev;
    struct sockaddr_storage addr;
    size_t addr_len;
    Stream streams[MAX_STREAMS];
    int streams_playing;
    bool backchannel_supported;
    Compy_Backchannel *backchannel;
    BackchannelReceiver backchannel_recv;
} Client;

declImpl(Compy_Controller, Client);

static void listener_cb(
    struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa,
    int socklen, void *ctx);
static void on_event_cb(struct bufferevent *bev, short events, void *ctx);
static void on_sigint_cb(evutil_socket_t sig, short events, void *ctx);

static int setup_transport(
    Client *self, Compy_Context *ctx, const Compy_Request *req,
    Compy_Transport *t, Compy_Transport *rtcp_t);
static int setup_tcp(
    Compy_Context *ctx, Compy_Transport *t, Compy_Transport *rtcp_t,
    Compy_TransportConfig config);
static int setup_udp(
    const struct sockaddr *addr, Compy_Context *ctx, Compy_Transport *t,
    Compy_Transport *rtcp_t, Compy_TransportConfig config);

typedef struct {
    Compy_RtpTransport *transport;
    size_t i;
    struct event *ev;
    struct bufferevent *bev;
    int *streams_playing;
} AudioCtx;

static Compy_Droppable play_audio(
    struct event_base *base, struct bufferevent *bev, Compy_RtpTransport *t,
    struct event **ev, int *streams_playing);
static void send_audio_packet_cb(evutil_socket_t fd, short events, void *arg);

typedef struct {
    Compy_NalTransport *transport;
    Compy_NalStartCodeTester start_code_tester;
    uint32_t timestamp;
    U8Slice99 video;
    uint8_t *nalu_start;
    struct event *ev;
    struct bufferevent *bev;
    int *streams_playing;
} VideoCtx;

static Compy_Droppable play_video(
    struct event_base *base, struct bufferevent *bev, Compy_RtpTransport *t,
    struct event **ev, int *streams_playing);
static void send_video_packet_cb(evutil_socket_t fd, short events, void *arg);
static bool send_nalu(VideoCtx *ctx);

/* Auth credential lookup callback */
static bool auth_lookup(
    const char *username, char *password_out, size_t password_max,
    void *user_data) {
    const char *expected_creds = user_data; /* "user:pass" */
    const char *colon = strchr(expected_creds, ':');
    if (!colon)
        return false;

    size_t user_len = (size_t)(colon - expected_creds);
    if (strlen(username) != user_len)
        return false;
    if (strncmp(username, expected_creds, user_len) != 0)
        return false;

    strncpy(password_out, colon + 1, password_max - 1);
    password_out[password_max - 1] = '\0';
    return true;
}

static void print_usage(const char *prog) {
    fprintf(
        stderr,
        "Usage: %s [options]\n"
        "  -v <file.h264>   H.264 video file (Annex B)\n"
        "  -a <file.g711a>  G.711 mu-law audio file\n"
        "  -f <fps>         Video frame rate (default: 30)\n"
        "  -p <port>        Server port (default: %d)\n"
        "  -u <user:pass>   Enable Digest authentication\n"
#ifdef COMPY_HAS_TLS
        "  -t <cert.pem>    TLS certificate (enables RTSPS)\n"
        "  -k <key.pem>     TLS private key\n"
        "  -s               Enable SRTP/SRTCP encryption\n"
#endif
        ,
        prog, SERVER_PORT);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    const char *video_path = "media/bbb/bbb_sunflower_1080p_30fps_normal.h264";
    const char *audio_path = "media/bbb/bbb_sunflower_1080p_30fps_normal.g711a";
    int port = SERVER_PORT;
    const char *auth_creds = NULL;
#ifdef COMPY_HAS_TLS
    const char *tls_cert = NULL;
    const char *tls_key = NULL;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            video_path = argv[++i];
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            audio_path = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            media_video_fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            auth_creds = argv[++i];
#ifdef COMPY_HAS_TLS
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            tls_cert = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            tls_key = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            g_srtp_enabled = true;
#endif
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Set up authentication */
    if (auth_creds) {
        if (!strchr(auth_creds, ':')) {
            fprintf(stderr, "Auth format: user:pass\n");
            return EXIT_FAILURE;
        }
        g_auth = Compy_Auth_new("Compy", auth_lookup, (void *)auth_creds);
        printf(
            "Authentication enabled (user: %.*s)\n",
            (int)(strchr(auth_creds, ':') - auth_creds), auth_creds);
    }

#ifdef COMPY_HAS_TLS
    /* Set up TLS */
    if (tls_cert && tls_key) {
        g_tls_ctx = Compy_TlsContext_new(
            (Compy_TlsConfig){.cert_path = tls_cert, .key_path = tls_key});
        if (!g_tls_ctx) {
            fprintf(stderr, "Failed to load TLS cert/key\n");
            return EXIT_FAILURE;
        }
        printf("RTSPS enabled (cert: %s)\n", tls_cert);
    }

    /* Set up SRTP key material */
    if (g_srtp_enabled) {
        if (compy_srtp_generate_key(&g_srtp_key) != 0) {
            fprintf(stderr, "Failed to generate SRTP key\n");
            return EXIT_FAILURE;
        }
        printf("SRTP/SRTCP enabled (AES-128-CM + HMAC-SHA1-80)\n");
    }
#endif

    if (mmap_file(video_path, &media_video, &media_video_len) == -1) {
        fprintf(stderr, "Failed to load video: %s\n", video_path);
        return EXIT_FAILURE;
    }

    if (mmap_file(audio_path, &media_audio, &media_audio_len) == -1) {
        fprintf(stderr, "Failed to load audio: %s\n", audio_path);
        return EXIT_FAILURE;
    }

    printf(
        "Loaded video: %s (%zu bytes, %d fps)\n"
        "Loaded audio: %s (%zu bytes)\n",
        video_path, media_video_len, media_video_fps, audio_path,
        media_audio_len);

    struct event_base *base;
    if ((base = event_base_new()) == NULL) {
        fputs("event_base_new failed.\n", stderr);
        return EXIT_FAILURE;
    }

    /* Dual-stack IPv6 socket: accepts both IPv4 and IPv6 connections */
    int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    {
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
        opt = 0;
        setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt);
    }

    struct sockaddr_in6 sin6 = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port),
        .sin6_addr = in6addr_any,
    };

    if (bind(listen_fd, (struct sockaddr *)&sin6, sizeof sin6) == -1) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, 128) == -1) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    struct evconnlistener *listener;
    if ((listener = evconnlistener_new(
             base, listener_cb, (void *)base, LEV_OPT_CLOSE_ON_FREE, 0,
             listen_fd)) == NULL) {
        fputs("evconnlistener_new failed.\n", stderr);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    struct event *sigint_handler;
    if ((sigint_handler =
             evsignal_new(base, SIGINT, on_sigint_cb, (void *)base)) == NULL) {
        fputs("evsignal_new failed.\n", stderr);
        return EXIT_FAILURE;
    }

    if (event_add(sigint_handler, NULL) < 0) {
        fputs("event_add failed.\n", stderr);
        return EXIT_FAILURE;
    }

    printf("Server started on port %d.\n", port);

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_free(sigint_handler);
    event_base_free(base);

    if (g_auth) {
        Compy_Auth_free(g_auth);
    }

#ifdef COMPY_HAS_TLS
    if (g_tls_ctx) {
        Compy_TlsContext_free(g_tls_ctx);
    }
#endif

    puts("Done.");
    return EXIT_SUCCESS;
}

static void listener_cb(
    struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa,
    int socklen, void *arg) {
    (void)listener;
    (void)fd;
    (void)socklen;

    struct event_base *base = arg;

    struct bufferevent *bev;
    if ((bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE)) ==
        NULL) {
        fputs("bufferevent_socket_new failed.\n", stderr);
        event_base_loopbreak(base);
        return;
    }

    Client *client = calloc(1, sizeof *client);
    assert(client);
    client->base = base;
    client->bev = bev;
    memcpy(&client->addr, sa, socklen);
    client->addr_len = socklen;

    Compy_Controller controller = DYN(Client, Compy_Controller, client);
    void *ctx = compy_libevent_ctx(controller);

    bufferevent_setcb(bev, compy_libevent_cb, NULL, on_event_cb, ctx);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

static void on_event_cb(struct bufferevent *bev, short events, void *ctx) {
    if (events & BEV_EVENT_EOF) {
        puts("Connection closed.");
    } else if (events & BEV_EVENT_ERROR) {
        perror("Got an error on the connection");
    }

    bufferevent_free(bev);
    compy_libevent_ctx_free(ctx);
}

static void on_sigint_cb(evutil_socket_t sig, short events, void *ctx) {
    (void)sig;
    (void)events;

    struct event_base *base = ctx;

    puts("Caught an interrupt signal; exiting cleanly in two seconds.");

    struct timeval delay = {2, 0};
    event_base_loopexit(base, &delay);
}

static void send_rtcp_sr_cb(evutil_socket_t fd, short events, void *arg) {
    (void)fd;
    (void)events;

    Compy_Rtcp *rtcp = arg;
    if (Compy_Rtcp_send_sr(rtcp) == -1) {
        perror("Failed to send RTCP SR");
    }
}

static void Client_drop(VSelf) {
    VSELF(Client);

    for (size_t i = 0; i < MAX_STREAMS; i++) {
        if (self->streams[i].rtcp) {
            int bye_ret __attribute__((unused)) =
                Compy_Rtcp_send_bye(self->streams[i].rtcp);
            if (self->streams[i].rtcp_ev) {
                event_free(self->streams[i].rtcp_ev);
            }
            VCALL(
                DYN(Compy_Rtcp, Compy_Droppable, self->streams[i].rtcp), drop);
        }
        if (self->streams[i].ctx.vptr != NULL) {
            VCALL(self->streams[i].ctx, drop);
        }
    }

    if (self->backchannel) {
        VCALL(DYN(Compy_Backchannel, Compy_Droppable, self->backchannel), drop);
    }

    free(self);
}

impl(Compy_Droppable, Client);

static void
Client_options(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    compy_header(
        ctx, COMPY_HEADER_PUBLIC,
        "DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER");
    compy_respond_ok(ctx);
}

static void
Client_describe(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    /* Check if client requests backchannel via ONVIF Require tag */
    self->backchannel_supported = compy_require_has_tag(
        &req->header_map, COMPY_REQUIRE_ONVIF_BACKCHANNEL);

    char sdp_buf[2048] = {0};
    Compy_Writer sdp = compy_string_writer(sdp_buf);
    ssize_t ret = 0;

    const char *ip_ver = self->addr.ss_family == AF_INET6 ? "IP6" : "IP4";
    const char *ip_any = self->addr.ss_family == AF_INET6 ? "::" : "0.0.0.0";

    // clang-format off
    COMPY_SDP_DESCRIBE(
        ret, sdp,
        (COMPY_SDP_VERSION, "0"),
        (COMPY_SDP_ORIGIN, "Compy 3855320066 3855320129 IN %s %s", ip_ver, ip_any),
        (COMPY_SDP_SESSION_NAME, "Compy example"),
        (COMPY_SDP_CONNECTION, "IN %s %s", ip_ver, ip_any),
        (COMPY_SDP_TIME, "0 0"));

#ifdef ENABLE_VIDEO
    COMPY_SDP_DESCRIBE(
        ret, sdp,
        (COMPY_SDP_MEDIA, "video 0 RTP/AVP %d", VIDEO_PAYLOAD_TYPE),
        (COMPY_SDP_ATTR, "control:video"),
        (COMPY_SDP_ATTR, "recvonly"),
        (COMPY_SDP_ATTR, "rtpmap:%d H264/%" PRIu32, VIDEO_PAYLOAD_TYPE, VIDEO_SAMPLE_RATE),
        (COMPY_SDP_ATTR, "fmtp:%d packetization-mode=1", VIDEO_PAYLOAD_TYPE),
        (COMPY_SDP_ATTR, "framerate:%d", media_video_fps));
#endif

#ifdef ENABLE_AUDIO
    COMPY_SDP_DESCRIBE(
        ret, sdp,
        (COMPY_SDP_MEDIA, "audio 0 RTP/AVP %d", AUDIO_PCMU_PAYLOAD_TYPE),
        (COMPY_SDP_ATTR, "control:audio"),
        (COMPY_SDP_ATTR, "recvonly"));
#endif

    if (self->backchannel_supported) {
        COMPY_SDP_DESCRIBE(
            ret, sdp,
            (COMPY_SDP_MEDIA, "audio 0 RTP/AVP %d", BACKCHANNEL_PAYLOAD_TYPE),
            (COMPY_SDP_ATTR, "control:audioback"),
            (COMPY_SDP_ATTR, "rtpmap:%d PCMU/%d", BACKCHANNEL_PAYLOAD_TYPE, BACKCHANNEL_SAMPLE_RATE),
            (COMPY_SDP_ATTR, "sendonly"));
    }

#ifdef COMPY_HAS_TLS
    if (g_srtp_enabled) {
        char crypto_attr[128];
        compy_srtp_format_crypto_attr(
            crypto_attr, sizeof crypto_attr, 1,
            Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &g_srtp_key);
        COMPY_SDP_DESCRIBE(
            ret, sdp,
            (COMPY_SDP_ATTR, "crypto:%s", crypto_attr));
    }
#endif
    // clang-format on

    assert(ret > 0);

    compy_header(ctx, COMPY_HEADER_CONTENT_TYPE, "application/sdp");
    compy_body(ctx, CharSlice99_from_str(sdp_buf));

    compy_respond_ok(ctx);
}

static void Client_setup(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    Compy_Transport transport, rtcp_transport;
    if (setup_transport(self, ctx, req, &transport, &rtcp_transport) == -1) {
        return;
    }

    size_t stream_id;
    if (CharSlice99_primitive_ends_with(
            req->start_line.uri, CharSlice99_from_str("/audioback"))) {
        stream_id = BACKCHANNEL_STREAM_ID;
    } else if (CharSlice99_primitive_ends_with(
                   req->start_line.uri, CharSlice99_from_str("/audio"))) {
        stream_id = AUDIO_STREAM_ID;
    } else {
        stream_id = VIDEO_STREAM_ID;
    }
    Stream *stream = &self->streams[stream_id];

    const bool aggregate_control_requested =
        Compy_HeaderMap_contains_key(&req->header_map, COMPY_HEADER_SESSION);
    if (aggregate_control_requested) {
        uint64_t session_id;
        if (compy_scanf_header(
                &req->header_map, COMPY_HEADER_SESSION, "%" SCNu64,
                &session_id) != 1) {
            compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "Malformed `Session'");
            return;
        }

        stream->session_id = session_id;
    } else {
        {
            uint64_t sid;
            FILE *f = fopen("/dev/urandom", "r");
            assert(f);
            assert(fread(&sid, sizeof sid, 1, f) == 1);
            fclose(f);
            stream->session_id = sid;
        }
    }

    if (BACKCHANNEL_STREAM_ID == stream_id) {
        /* Backchannel: no outbound RTP transport needed, create receiver */
        stream->transport = NULL;
        self->backchannel_recv = (BackchannelReceiver){.total_bytes = 0};
        self->backchannel = Compy_Backchannel_new(
            Compy_BackchannelConfig_default(),
            DYN(BackchannelReceiver, Compy_AudioReceiver,
                &self->backchannel_recv));
        /* RTCP transport not used for backchannel in this example */
        VCALL_SUPER(rtcp_transport, Compy_Droppable, drop);
        VCALL_SUPER(transport, Compy_Droppable, drop);
    } else if (AUDIO_STREAM_ID == stream_id) {
        stream->transport = Compy_RtpTransport_new(
            transport, AUDIO_PCMU_PAYLOAD_TYPE, AUDIO_SAMPLE_RATE);
        stream->rtcp =
            Compy_Rtcp_new(stream->transport, rtcp_transport, "compy@camera");
    } else {
        stream->transport = Compy_RtpTransport_new(
            transport, VIDEO_PAYLOAD_TYPE, VIDEO_SAMPLE_RATE);
        stream->rtcp =
            Compy_Rtcp_new(stream->transport, rtcp_transport, "compy@camera");
    }

    compy_header(ctx, COMPY_HEADER_SESSION, "%" PRIu64, stream->session_id);

    compy_respond_ok(ctx);
}

static void Client_play(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    uint64_t session_id;
    if (compy_scanf_header(
            &req->header_map, COMPY_HEADER_SESSION, "%" SCNu64, &session_id) !=
        1) {
        compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "Malformed `Session'");
        return;
    }

    bool played = false;
    for (size_t i = 0; i < MAX_STREAMS; i++) {
        if (self->streams[i].session_id == session_id) {
            if (AUDIO_STREAM_ID == i) {
                self->streams[i].ctx = play_audio(
                    self->base, self->bev, self->streams[i].transport,
                    &self->streams[i].ev, &self->streams_playing);
            } else {
                self->streams[i].ctx = play_video(
                    self->base, self->bev, self->streams[i].transport,
                    &self->streams[i].ev, &self->streams_playing);
            }

            /* Start RTCP SR timer */
            if (self->streams[i].rtcp && self->streams[i].rtcp_ev == NULL) {
                self->streams[i].rtcp_ev = event_new(
                    self->base, -1, EV_PERSIST | EV_TIMEOUT, send_rtcp_sr_cb,
                    self->streams[i].rtcp);
                assert(self->streams[i].rtcp_ev);
                event_add(
                    self->streams[i].rtcp_ev,
                    &(const struct timeval){.tv_sec = RTCP_INTERVAL_SEC,
                                            .tv_usec = 0});
            }

            played = true;
        }
    }

    if (!played) {
        compy_respond(
            ctx, COMPY_STATUS_SESSION_NOT_FOUND, "Invalid Session ID");
        return;
    }

    compy_header(ctx, COMPY_HEADER_RANGE, "npt=now-");
    compy_respond_ok(ctx);
}

static void
Client_teardown(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    uint64_t session_id;
    if (compy_scanf_header(
            &req->header_map, COMPY_HEADER_SESSION, "%" SCNu64, &session_id) !=
        1) {
        compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "Malformed `Session'");
        return;
    }

    bool teardowned = false;
    for (size_t i = 0; i < MAX_STREAMS; i++) {
        if (self->streams[i].session_id == session_id) {
            event_del(self->streams[i].ev);
            teardowned = true;
        }
    }

    if (!teardowned) {
        compy_respond(
            ctx, COMPY_STATUS_SESSION_NOT_FOUND, "Invalid Session ID");
        return;
    }

    compy_respond_ok(ctx);
}

static void
Client_pause_method(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    compy_respond_ok(ctx);
}

static void
Client_get_parameter(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    /* Keepalive — just respond 200 OK */
    compy_respond_ok(ctx);
}

static void
Client_unknown(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    compy_respond(ctx, COMPY_STATUS_NOT_IMPLEMENTED, "Not Implemented");
}

static Compy_ControlFlow
Client_before(VSelf, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;

    printf(
        "%s %s CSeq=%" PRIu32 ".\n",
        CharSlice99_alloca_c_str(req->start_line.method),
        CharSlice99_alloca_c_str(req->start_line.uri), req->cseq);

    /* Digest authentication check */
    if (g_auth && compy_auth_check(g_auth, ctx, req) != 0) {
        return Compy_ControlFlow_Break;
    }

    return Compy_ControlFlow_Continue;
}

static void
Client_after(VSelf, ssize_t ret, Compy_Context *ctx, const Compy_Request *req) {
    VSELF(Client);

    (void)self;
    (void)ctx;
    (void)req;

    if (ret < 0) {
        perror("Failed to respond");
    }
}

impl(Compy_Controller, Client);

static int setup_transport(
    Client *self, Compy_Context *ctx, const Compy_Request *req,
    Compy_Transport *t, Compy_Transport *rtcp_t) {
    CharSlice99 transport_val;
    const bool transport_found = Compy_HeaderMap_find(
        &req->header_map, COMPY_HEADER_TRANSPORT, &transport_val);
    if (!transport_found) {
        compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "`Transport' not present");
        return -1;
    }

    Compy_TransportConfig config;
    if (compy_parse_transport(&config, transport_val) == -1) {
        compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "Malformed `Transport'");
        return -1;
    }

    switch (config.lower) {
    case Compy_LowerTransport_TCP:
        if (setup_tcp(ctx, t, rtcp_t, config) == -1) {
            compy_respond_internal_error(ctx);
            return -1;
        }
        break;
    case Compy_LowerTransport_UDP:
        if (setup_udp(
                (const struct sockaddr *)&self->addr, ctx, t, rtcp_t, config) ==
            -1) {
            compy_respond_internal_error(ctx);
            return -1;
        }
        break;
    }

    return 0;
}

static int setup_tcp(
    Compy_Context *ctx, Compy_Transport *t, Compy_Transport *rtcp_t,
    Compy_TransportConfig config) {
    ifLet(config.interleaved, Compy_ChannelPair_Some, interleaved) {
        *t = compy_transport_tcp(
            Compy_Context_get_writer(ctx), interleaved->rtp_channel, 0);
        *rtcp_t = compy_transport_tcp(
            Compy_Context_get_writer(ctx), interleaved->rtcp_channel, 0);

        compy_header(
            ctx, COMPY_HEADER_TRANSPORT,
            "RTP/AVP/TCP;unicast;interleaved=%" PRIu8 "-%" PRIu8,
            interleaved->rtp_channel, interleaved->rtcp_channel);
        return 0;
    }

    compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "`interleaved' not found");
    return -1;
}

static int setup_udp(
    const struct sockaddr *addr, Compy_Context *ctx, Compy_Transport *t,
    Compy_Transport *rtcp_t, Compy_TransportConfig config) {

    ifLet(config.client_port, Compy_PortPair_Some, client_port) {
        int fd;
        if ((fd = compy_dgram_socket(
                 addr->sa_family, compy_sockaddr_ip(addr),
                 client_port->rtp_port)) == -1) {
            return -1;
        }

        int rtcp_fd;
        if ((rtcp_fd = compy_dgram_socket(
                 addr->sa_family, compy_sockaddr_ip(addr),
                 client_port->rtcp_port)) == -1) {
            close(fd);
            return -1;
        }

        /* Determine the local ports */
        struct sockaddr_storage local_addr;
        socklen_t local_len = sizeof local_addr;
        uint16_t server_rtp_port = 0, server_rtcp_port = 0;
        if (getsockname(fd, (struct sockaddr *)&local_addr, &local_len) == 0) {
            if (local_addr.ss_family == AF_INET) {
                server_rtp_port =
                    ntohs(((struct sockaddr_in *)&local_addr)->sin_port);
            } else {
                server_rtp_port =
                    ntohs(((struct sockaddr_in6 *)&local_addr)->sin6_port);
            }
        }
        local_len = sizeof local_addr;
        if (getsockname(rtcp_fd, (struct sockaddr *)&local_addr, &local_len) ==
            0) {
            if (local_addr.ss_family == AF_INET) {
                server_rtcp_port =
                    ntohs(((struct sockaddr_in *)&local_addr)->sin_port);
            } else {
                server_rtcp_port =
                    ntohs(((struct sockaddr_in6 *)&local_addr)->sin6_port);
            }
        }

        *t = compy_transport_udp(fd);
        *rtcp_t = compy_transport_udp(rtcp_fd);

#ifdef COMPY_HAS_TLS
        if (g_srtp_enabled) {
            *t = compy_transport_srtp(
                *t, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &g_srtp_key);
            *rtcp_t = compy_transport_srtcp(
                *rtcp_t, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &g_srtp_key);
        }
#endif

        compy_header(
            ctx, COMPY_HEADER_TRANSPORT,
            "RTP/AVP/UDP;unicast;client_port=%" PRIu16 "-%" PRIu16
            ";server_port=%" PRIu16 "-%" PRIu16,
            client_port->rtp_port, client_port->rtcp_port, server_rtp_port,
            server_rtcp_port);
        return 0;
    }

    compy_respond(ctx, COMPY_STATUS_BAD_REQUEST, "`client_port' not found");
    return -1;
}

static void AudioCtx_drop(VSelf) {
    VSELF(AudioCtx);

    event_free(self->ev);
    VTABLE(Compy_RtpTransport, Compy_Droppable).drop(self->transport);
    free(self);
}

impl(Compy_Droppable, AudioCtx);

static Compy_Droppable play_audio(
    struct event_base *base, struct bufferevent *bev, Compy_RtpTransport *t,
    struct event **ev, int *streams_playing) {
    AudioCtx *ctx = malloc(sizeof *ctx);
    assert(ctx);
    *ctx = (AudioCtx){
        .transport = t,
        .i = 0,
        .ev = NULL,
        .streams_playing = streams_playing,
        .bev = bev,
    };

    ctx->ev = event_new(
        base, -1, EV_PERSIST | EV_TIMEOUT, send_audio_packet_cb, (void *)ctx);
    assert(ctx->ev);

    event_add(
        ctx->ev, &(const struct timeval){
                     .tv_sec = 0,
                     .tv_usec = AUDIO_PACKETIZATION_TIME_US,
                 });
    *ev = ctx->ev;
    (*streams_playing)++;

    return DYN(AudioCtx, Compy_Droppable, ctx);
}

static void send_audio_packet_cb(evutil_socket_t fd, short events, void *arg) {
    (void)fd;
    (void)events;

    AudioCtx *ctx = arg;

    if (ctx->i * AUDIO_SAMPLES_PER_PACKET >= media_audio_len) {
        event_del(ctx->ev);
        (*ctx->streams_playing)--;
        if (0 == *ctx->streams_playing) {
            bufferevent_trigger_event(ctx->bev, BEV_EVENT_EOF, 0);
        }
        return;
    }

    const Compy_RtpTimestamp ts =
        Compy_RtpTimestamp_Raw(ctx->i * AUDIO_SAMPLES_PER_PACKET);
    const bool marker = false;
    const size_t samples_count =
        media_audio_len <
                ctx->i * AUDIO_SAMPLES_PER_PACKET + AUDIO_SAMPLES_PER_PACKET
            ? media_audio_len % AUDIO_SAMPLES_PER_PACKET
            : AUDIO_SAMPLES_PER_PACKET;
    const U8Slice99 header = U8Slice99_empty(),
                    payload = U8Slice99_new(
                        media_audio + ctx->i * AUDIO_SAMPLES_PER_PACKET,
                        samples_count);

    if (Compy_RtpTransport_send_packet(
            ctx->transport, ts, marker, header, payload) == -1) {
        perror("Failed to send RTP/PCMU");
    }

    ctx->i++;
}

static void VideoCtx_drop(VSelf) {
    VSELF(VideoCtx);

    event_free(self->ev);
    VTABLE(Compy_NalTransport, Compy_Droppable).drop(self->transport);
    free(self);
}

impl(Compy_Droppable, VideoCtx);

static Compy_Droppable play_video(
    struct event_base *base, struct bufferevent *bev, Compy_RtpTransport *t,
    struct event **ev, int *streams_playing) {
    U8Slice99 video = U8Slice99_new(media_video, media_video_len);

    Compy_NalStartCodeTester start_code_tester;
    if ((start_code_tester = compy_determine_start_code(video)) == NULL) {
        fputs("Invalid video file.\n", stderr);
        abort();
    }

    VideoCtx *ctx = malloc(sizeof *ctx);
    assert(ctx);
    *ctx = (VideoCtx){
        .transport = Compy_NalTransport_new(t),
        .start_code_tester = start_code_tester,
        .timestamp = 0,
        .video = video,
        .nalu_start = NULL,
        .ev = NULL,
        .bev = bev,
        .streams_playing = streams_playing,
    };

    ctx->ev = event_new(
        base, -1, EV_PERSIST | EV_TIMEOUT, send_video_packet_cb, (void *)ctx);
    assert(ctx->ev);

    event_add(
        ctx->ev, &(const struct timeval){
                     .tv_sec = 0,
                     .tv_usec = 1e6 / media_video_fps,
                 });
    *ev = ctx->ev;
    (*streams_playing)++;

    return DYN(VideoCtx, Compy_Droppable, ctx);
}

static void send_video_packet_cb(evutil_socket_t fd, short events, void *arg) {
    (void)fd;
    (void)events;

    VideoCtx *ctx = arg;

again:
    if (U8Slice99_is_empty(ctx->video)) {
        send_nalu(ctx);
        event_del(ctx->ev);
        (*ctx->streams_playing)--;
        if (0 == *ctx->streams_playing) {
            bufferevent_trigger_event(ctx->bev, BEV_EVENT_EOF, 0);
        }
        return;
    }

    const size_t start_code_len = ctx->start_code_tester(ctx->video);
    if (0 == start_code_len) {
        ctx->video = U8Slice99_advance(ctx->video, 1);
        goto again;
    }

    bool au_found = false;
    if (NULL != ctx->nalu_start) {
        au_found = send_nalu(ctx);
    }

    ctx->video = U8Slice99_advance(ctx->video, start_code_len);
    ctx->nalu_start = ctx->video.ptr;

    if (!au_found) {
        goto again;
    }
}

static bool send_nalu(VideoCtx *ctx) {
    const Compy_NalUnit nalu = {
        .header =
            Compy_NalHeader_H264(Compy_H264NalHeader_parse(ctx->nalu_start[0])),
        .payload = U8Slice99_from_ptrdiff(ctx->nalu_start + 1, ctx->video.ptr),
    };

    bool au_found = false;

    if (Compy_NalHeader_unit_type(nalu.header) == COMPY_H264_NAL_UNIT_AUD) {
        ctx->timestamp += VIDEO_SAMPLE_RATE / media_video_fps;
        au_found = true;
    }

    if (Compy_NalTransport_send_packet(
            ctx->transport, Compy_RtpTimestamp_Raw(ctx->timestamp), nalu) ==
        -1) {
        perror("Failed to send RTP/NAL");
    }

    return au_found;
}
