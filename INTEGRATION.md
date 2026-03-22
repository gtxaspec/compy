# Compy Integration Guide for Raptor

Technical reference for building an RTSP server application on top of compy.

## Library Architecture

Compy is a protocol library, not a server. It provides:
- RTSP message parsing and serialization
- RTP/RTCP packet construction and parsing
- H.264/H.265 NAL unit fragmentation
- SRTP/SRTCP encryption and decryption
- Digest authentication
- TLS wrapping

The application provides:
- Event loop (epoll, libevent, etc.)
- Socket management (accept, read, write)
- Encoder integration (V4L2/ISP)
- Session management (timeouts, multiple clients)
- Business logic (who can connect, which stream)

## Core Data Flow

### Send path (camera → client)

```
Encoder (V4L2) → raw H.264 NALUs
    ↓
Compy_NalTransport_send_packet(transport, timestamp, nalu)
    ↓ (fragments if > max_packet_size)
Compy_RtpTransport_send_packet(transport, ts, marker, header, payload)
    ↓ (builds RTP header, 3-element iovec)
Compy_Transport.transmit(iovec)
    ↓
[SrtpTransport: encrypt + HMAC tag]  ← optional
    ↓
[TcpTransport: $+channel+len framing] or [UdpTransport: sendmsg()]
    ↓
[TlsWriter: TLS encrypt]  ← optional (RTSPS only)
    ↓
Network
```

### Receive path (client → camera)

```
Network
    ↓
[compy_tls_read(): TLS decrypt]  ← optional
    ↓
RTSP request: Compy_Request_parse() → compy_dispatch()
    ↓
Interleaved RTP: compy_parse_interleaved_header() → channel routing
    ↓
[compy_srtp_recv_unprotect(): verify HMAC + decrypt]  ← optional
    ↓
Compy_RtpReceiver_feed(receiver, channel_type, data, len)
    ↓
COMPY_CHANNEL_RTCP → Compy_Rtcp_handle_incoming()
COMPY_CHANNEL_RTP  → Compy_AudioReceiver.on_audio() callback
```

## Step-by-Step Integration

### 1. Initialize

```c
#include <compy.h>

// Required: seed PRNG (used for SSRC generation)
srand(time(NULL));
```

### 2. Implement the Controller interface

The controller handles RTSP method dispatch. Implement all methods:

```c
typedef struct {
    // your per-client state
    Stream streams[3];    // video, audio, backchannel
    int streams_playing;
    Compy_Auth *auth;     // optional
    // ...
} MyClient;

// Required method implementations:
static void MyClient_options(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_describe(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_setup(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_play(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_pause_method(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_teardown(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_get_parameter(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_unknown(VSelf, Compy_Context *ctx, const Compy_Request *req);
static Compy_ControlFlow MyClient_before(VSelf, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_after(VSelf, ssize_t ret, Compy_Context *ctx, const Compy_Request *req);
static void MyClient_drop(VSelf);

// Register interface implementations:
impl(Compy_Controller, MyClient);
impl(Compy_Droppable, MyClient);

// Create controller instance:
Compy_Controller ctrl = DYN(MyClient, Compy_Controller, client_ptr);
```

### 3. Parse and dispatch requests

When data arrives on the RTSP TCP connection:

```c
Compy_Request req = Compy_Request_uninit();
Compy_ParseResult result = Compy_Request_parse(&req, input_slice);

if (Compy_ParseResult_is_complete(result)) {
    Compy_Writer writer = compy_fd_writer(&client_fd);
    // or: compy_tls_writer(tls_conn) for RTSPS
    compy_dispatch(writer, controller, &req);
}
```

### 4. Handle DESCRIBE

Generate SDP describing available media:

```c
char sdp[2048] = {0};
Compy_Writer w = compy_string_writer(sdp);
ssize_t ret = 0;

COMPY_SDP_DESCRIBE(ret, w,
    (COMPY_SDP_VERSION, "0"),
    (COMPY_SDP_ORIGIN, "Raptor 1 1 IN IP4 0.0.0.0"),
    (COMPY_SDP_SESSION_NAME, "Live"),
    (COMPY_SDP_CONNECTION, "IN IP4 0.0.0.0"),
    (COMPY_SDP_TIME, "0 0"));

// Video
COMPY_SDP_DESCRIBE(ret, w,
    (COMPY_SDP_MEDIA, "video 0 RTP/AVP 96"),
    (COMPY_SDP_ATTR, "control:video"),
    (COMPY_SDP_ATTR, "recvonly"),
    (COMPY_SDP_ATTR, "rtpmap:96 H264/90000"));

// Audio
COMPY_SDP_DESCRIBE(ret, w,
    (COMPY_SDP_MEDIA, "audio 0 RTP/AVP 0"),
    (COMPY_SDP_ATTR, "control:audio"),
    (COMPY_SDP_ATTR, "recvonly"));

// Backchannel (if ONVIF requested)
if (compy_require_has_tag(&req->header_map, COMPY_REQUIRE_ONVIF_BACKCHANNEL)) {
    COMPY_SDP_DESCRIBE(ret, w,
        (COMPY_SDP_MEDIA, "audio 0 RTP/AVP 0"),
        (COMPY_SDP_ATTR, "control:audioback"),
        (COMPY_SDP_ATTR, "rtpmap:0 PCMU/8000"),
        (COMPY_SDP_ATTR, "sendonly"));
}

// SRTP crypto (if enabled)
#ifdef COMPY_HAS_TLS
char crypto[128];
compy_srtp_format_crypto_attr(crypto, sizeof crypto, 1,
    Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &srtp_key);
COMPY_SDP_DESCRIBE(ret, w, (COMPY_SDP_ATTR, "crypto:%s", crypto));
#endif

compy_header(ctx, COMPY_HEADER_CONTENT_TYPE, "application/sdp");
compy_body(ctx, CharSlice99_from_str(sdp));
compy_respond_ok(ctx);
```

### 5. Handle SETUP

Parse Transport header, create transports:

```c
CharSlice99 transport_val;
Compy_HeaderMap_find(&req->header_map, COMPY_HEADER_TRANSPORT, &transport_val);

Compy_TransportConfig config;
compy_parse_transport(&config, transport_val);

Compy_Transport rtp_t, rtcp_t;

if (config.lower == Compy_LowerTransport_TCP) {
    // TCP interleaved
    rtp_t = compy_transport_tcp(writer, interleaved.rtp_channel, 0);
    rtcp_t = compy_transport_tcp(writer, interleaved.rtcp_channel, 0);
} else {
    // UDP
    int fd = compy_dgram_socket(AF_INET, client_ip, client_port.rtp_port);
    int rtcp_fd = compy_dgram_socket(AF_INET, client_ip, client_port.rtcp_port);
    rtp_t = compy_transport_udp(fd);
    rtcp_t = compy_transport_udp(rtcp_fd);
}

// Wrap with SRTP/SRTCP if enabled
#ifdef COMPY_HAS_TLS
if (srtp_enabled) {
    rtp_t = compy_transport_srtp(rtp_t, suite, &key);
    rtcp_t = compy_transport_srtcp(rtcp_t, suite, &key);
}
#endif

// Create RTP transport for this stream
Compy_RtpTransport *rtp = Compy_RtpTransport_new(rtp_t, payload_type, clock_rate);

// Create RTCP session
Compy_Rtcp *rtcp = Compy_Rtcp_new(rtp, rtcp_t, "raptor@camera");

// Respond with Transport header including server_port
compy_header(ctx, COMPY_HEADER_TRANSPORT,
    "RTP/AVP/UDP;unicast;client_port=%u-%u;server_port=%u-%u",
    client_rtp, client_rtcp, server_rtp, server_rtcp);
compy_header(ctx, COMPY_HEADER_SESSION, "%" PRIu64 ";timeout=60", session_id);
compy_respond_ok(ctx);
```

### 6. Send video frames

When the encoder produces a frame:

```c
// For H.264: parse NALUs from encoder output
Compy_NalTransport *nal = Compy_NalTransport_new(rtp_transport);

// For each NALU in the frame:
Compy_NalUnit nalu = {
    .header = Compy_NalHeader_H264(Compy_H264NalHeader_parse(nalu_data[0])),
    .payload = U8Slice99_new(nalu_data + 1, nalu_len - 1),
};

Compy_NalTransport_send_packet(nal, Compy_RtpTimestamp_Raw(pts), nalu);
// Compy handles fragmentation automatically
```

### 7. Send audio

```c
// G.711 PCMU at 8kHz, 160 samples per packet (20ms)
Compy_RtpTransport_send_packet(
    audio_rtp, Compy_RtpTimestamp_Raw(sample_count),
    false, U8Slice99_empty(),
    U8Slice99_new(pcmu_data, 160));
```

### 8. RTCP Sender Reports

Call periodically (every 5 seconds):

```c
Compy_Rtcp_send_sr(rtcp);  // reads stats from RtpTransport automatically
```

On session teardown:

```c
Compy_Rtcp_send_bye(rtcp);
```

### 9. Authentication

```c
// Credential lookup callback
static bool lookup(const char *user, char *pass, size_t max, void *ctx) {
    // check user database
    if (strcmp(user, "admin") == 0) {
        strncpy(pass, "password", max - 1);
        return true;
    }
    return false;
}

Compy_Auth *auth = Compy_Auth_new("Raptor Camera", lookup, NULL);

// In before() hook:
if (compy_auth_check(auth, ctx, req) != 0) {
    return Compy_ControlFlow_Break;  // 401 sent automatically
}
```

### 10. RTSPS (TLS)

```c
#ifdef COMPY_HAS_TLS
Compy_TlsContext *tls = Compy_TlsContext_new(
    (Compy_TlsConfig){.cert_path = "cert.pem", .key_path = "key.pem"});

// On new connection:
Compy_TlsConn *conn = Compy_TlsConn_accept(tls, client_fd);
Compy_Writer writer = compy_tls_writer(conn);

// Read RTSP requests:
ssize_t n = compy_tls_read(conn, buf, sizeof buf);
#endif
```

### 11. Backchannel (receive audio)

```c
// Implement AudioReceiver interface
typedef struct { /* state */ } MyAudioRecv;

static void MyAudioRecv_on_audio(
    VSelf, uint8_t pt, uint32_t ts, uint32_t ssrc, U8Slice99 payload) {
    VSELF(MyAudioRecv);
    // write payload to audio codec device
    write(audio_fd, payload.ptr, payload.len);
}

impl(Compy_AudioReceiver, MyAudioRecv);

// Create backchannel
Compy_Backchannel *bc = Compy_Backchannel_new(
    Compy_BackchannelConfig_default(),
    DYN(MyAudioRecv, Compy_AudioReceiver, &recv));

// Feed incoming data
Compy_RtpReceiver *receiver = Compy_Backchannel_get_receiver(bc);
Compy_RtpReceiver_feed(receiver, COMPY_CHANNEL_RTP, data, len);
```

## Key Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `COMPY_DEFAULT_PORT` | 554 | Standard RTSP port |
| `COMPY_DEFAULT_RTSPS_PORT` | 322 | Standard RTSPS port |
| `COMPY_MAX_H264_NALU_SIZE` | 1200 | Default FU-A fragmentation threshold |
| `COMPY_MAX_H265_NALU_SIZE` | 4096 | Default FU fragmentation threshold |
| `COMPY_HEADER_MAP_CAPACITY` | 32 | Maximum headers per request/response |
| `COMPY_REQUIRE_ONVIF_BACKCHANNEL` | `"www.onvif.org/ver20/backchannel"` | ONVIF backchannel feature tag |

## Status Codes

Common codes used in responses:

| Code | Constant | Usage |
|------|----------|-------|
| 200 | `COMPY_STATUS_OK` | Success |
| 400 | `COMPY_STATUS_BAD_REQUEST` | Malformed request |
| 401 | `COMPY_STATUS_UNAUTHORIZED` | Auth required |
| 454 | `COMPY_STATUS_SESSION_NOT_FOUND` | Invalid session ID |
| 455 | `COMPY_STATUS_METHOD_NOT_VALID_IN_THIS_STATE` | Wrong state |
| 501 | `COMPY_STATUS_NOT_IMPLEMENTED` | Unknown method |
| 551 | `COMPY_STATUS_OPTION_NOT_SUPPORTED` | Unknown Require tag |

## Memory Management

- All opaque types use `_new()` to allocate and `_drop()` or `_free()` to release
- Transports implement `Compy_Droppable` — call `VCALL_SUPER(t, Compy_Droppable, drop)`
- `Compy_RtpTransport_drop()` also drops its inner transport
- `Compy_NalTransport_drop()` also drops its inner RTP transport
- SRTP/SRTCP transports wipe keys with volatile zeroing before free
- Auth context wipes nonces on free

## Thread Safety

- Each `Compy_Writer` has `lock()`/`unlock()` for TCP interleaved channels
- `Compy_RtpTransport` is NOT thread-safe — one sender per transport
- RTCP send (`Compy_Rtcp_send_sr`) and RTP send should not race on the same transport
- Multiple clients should have independent transport instances

## Build Configuration

```cmake
# Without TLS (smallest binary, ~40KB on MIPS):
target_link_libraries(raptor compy)

# With TLS/SRTP:
# Set ONE of: -DCOMPY_TLS_OPENSSL, -DCOMPY_TLS_WOLFSSL, -DCOMPY_TLS_MBEDTLS, -DCOMPY_TLS_BEARSSL
# Then check COMPY_HAS_TLS in your code
```

## File Reference

| Header | Purpose |
|--------|---------|
| `compy.h` | Umbrella include (use this) |
| `compy/controller.h` | Controller interface (implement this) |
| `compy/context.h` | Request context, response helpers |
| `compy/rtp_transport.h` | RTP packet sending |
| `compy/nal_transport.h` | H.264/H.265 NAL fragmentation |
| `compy/transport.h` | TCP/UDP/SRTP/SRTCP transports |
| `compy/writer.h` | Writer interface (fd, TLS, string) |
| `compy/rtcp.h` | RTCP session (SR, BYE, RR handling) |
| `compy/receiver.h` | Receive path (AudioReceiver, RtpReceiver) |
| `compy/backchannel.h` | Backchannel session |
| `compy/auth.h` | Digest authentication |
| `compy/tls.h` | TLS context, connection, writer |
| `compy/util.h` | Transport parsing, interleaved headers, Require tag |
| `compy/types/rtp.h` | RTP header struct, serialize/deserialize |
| `compy/types/rtcp.h` | RTCP packet types |
| `compy/types/sdp.h` | SDP generation macros |
| `compy/types/request.h` | RTSP request parsing |
| `compy/types/header.h` | Header constants (all RFC 2326 headers) |
