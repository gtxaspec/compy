#include <compy/jpeg_transport.h>
#include <compy/transport.h>

#include <greatest.h>

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* --- Packet capture infrastructure --- */

#define MAX_TEST_PACKETS 64

typedef struct {
    uint8_t data[2048];
    ssize_t len;
} TestPacket;

typedef struct {
    TestPacket packets[MAX_TEST_PACKETS];
    int count;
} TestPacketCapture;

static void capture_init(TestPacketCapture *cap) {
    cap->count = 0;
}

static void capture_recv_all(TestPacketCapture *cap, int fd) {
    while (cap->count < MAX_TEST_PACKETS) {
        ssize_t n = recv(
            fd, cap->packets[cap->count].data, sizeof cap->packets[0].data,
            MSG_DONTWAIT);
        if (n <= 0) {
            break;
        }
        cap->packets[cap->count].len = n;
        cap->count++;
    }
}

/* --- RTP header helpers --- */

static bool packet_rtp_marker(const TestPacket *pkt) {
    return (pkt->data[1] & 0x80) != 0;
}

/* --- JPEG RTP header helpers (RFC 2435, starts at byte 12) --- */

static uint8_t packet_jpeg_type_specific(const TestPacket *pkt) {
    return pkt->data[12];
}

static uint32_t packet_jpeg_frag_offset(const TestPacket *pkt) {
    return ((uint32_t)pkt->data[13] << 16) | ((uint32_t)pkt->data[14] << 8) |
           pkt->data[15];
}

static uint8_t packet_jpeg_type(const TestPacket *pkt) {
    return pkt->data[16];
}

static uint8_t packet_jpeg_q(const TestPacket *pkt) {
    return pkt->data[17];
}

static uint8_t packet_jpeg_width(const TestPacket *pkt) {
    return pkt->data[18];
}

static uint8_t packet_jpeg_height(const TestPacket *pkt) {
    return pkt->data[19];
}

/* --- QT header helpers (first fragment, after JPEG header at 20) --- */

static uint8_t packet_qt_mbz(const TestPacket *pkt, size_t qt_off) {
    return pkt->data[qt_off];
}

static uint8_t packet_qt_precision(const TestPacket *pkt, size_t qt_off) {
    return pkt->data[qt_off + 1];
}

static uint16_t packet_qt_length(const TestPacket *pkt, size_t qt_off) {
    return ((uint16_t)pkt->data[qt_off + 2] << 8) | pkt->data[qt_off + 3];
}

/* --- Test JPEG frame builder --- */

static size_t build_test_jpeg(
    uint8_t *buf, size_t buf_size, uint16_t width, uint16_t height,
    uint8_t y_h_sampling, uint8_t y_v_sampling, const uint8_t *scan_data,
    size_t scan_len) {
    uint8_t *p = buf;

    (void)buf_size;

    /* SOI */
    *p++ = 0xFF;
    *p++ = 0xD8;

    /* DQT - luma (table 0) */
    *p++ = 0xFF;
    *p++ = 0xDB;
    *p++ = 0x00;
    *p++ = 0x43; /* length = 67 */
    *p++ = 0x00; /* Pq=0, Tq=0 */
    for (int i = 0; i < 64; i++) {
        *p++ = (uint8_t)(i + 1);
    }

    /* DQT - chroma (table 1) */
    *p++ = 0xFF;
    *p++ = 0xDB;
    *p++ = 0x00;
    *p++ = 0x43; /* length = 67 */
    *p++ = 0x01; /* Pq=0, Tq=1 */
    for (int i = 0; i < 64; i++) {
        *p++ = (uint8_t)(i + 65);
    }

    /* SOF0 - Baseline DCT */
    *p++ = 0xFF;
    *p++ = 0xC0;
    *p++ = 0x00;
    *p++ = 0x11; /* length = 17 */
    *p++ = 0x08; /* precision = 8 */
    *p++ = (uint8_t)(height >> 8);
    *p++ = (uint8_t)(height & 0xFF);
    *p++ = (uint8_t)(width >> 8);
    *p++ = (uint8_t)(width & 0xFF);
    *p++ = 0x03; /* 3 components */
    *p++ = 0x01; /* Y: id=1 */
    *p++ = (uint8_t)((y_h_sampling << 4) | y_v_sampling);
    *p++ = 0x00; /* qt=0 */
    *p++ = 0x02; /* Cb: id=2 */
    *p++ = 0x11; /* h=1, v=1 */
    *p++ = 0x01; /* qt=1 */
    *p++ = 0x03; /* Cr: id=3 */
    *p++ = 0x11; /* h=1, v=1 */
    *p++ = 0x01; /* qt=1 */

    /* SOS */
    *p++ = 0xFF;
    *p++ = 0xDA;
    *p++ = 0x00;
    *p++ = 0x0C; /* length = 12 */
    *p++ = 0x03; /* 3 components */
    *p++ = 0x01;
    *p++ = 0x00;
    *p++ = 0x02;
    *p++ = 0x11;
    *p++ = 0x03;
    *p++ = 0x11;
    *p++ = 0x00;
    *p++ = 0x3F;
    *p++ = 0x00;

    /* Scan data */
    memcpy(p, scan_data, scan_len);
    p += scan_len;

    /* EOI */
    *p++ = 0xFF;
    *p++ = 0xD9;

    return (size_t)(p - buf);
}

static size_t build_test_jpeg_with_dri(
    uint8_t *buf, size_t buf_size, uint16_t width, uint16_t height,
    uint16_t restart_interval, const uint8_t *scan_data, size_t scan_len) {
    uint8_t *p = buf;

    (void)buf_size;

    /* SOI */
    *p++ = 0xFF;
    *p++ = 0xD8;

    /* DQT - luma */
    *p++ = 0xFF;
    *p++ = 0xDB;
    *p++ = 0x00;
    *p++ = 0x43;
    *p++ = 0x00;
    for (int i = 0; i < 64; i++) {
        *p++ = (uint8_t)(i + 1);
    }

    /* DQT - chroma */
    *p++ = 0xFF;
    *p++ = 0xDB;
    *p++ = 0x00;
    *p++ = 0x43;
    *p++ = 0x01;
    for (int i = 0; i < 64; i++) {
        *p++ = (uint8_t)(i + 65);
    }

    /* DRI */
    *p++ = 0xFF;
    *p++ = 0xDD;
    *p++ = 0x00;
    *p++ = 0x04; /* length = 4 */
    *p++ = (uint8_t)(restart_interval >> 8);
    *p++ = (uint8_t)(restart_interval & 0xFF);

    /* SOF0 */
    *p++ = 0xFF;
    *p++ = 0xC0;
    *p++ = 0x00;
    *p++ = 0x11;
    *p++ = 0x08;
    *p++ = (uint8_t)(height >> 8);
    *p++ = (uint8_t)(height & 0xFF);
    *p++ = (uint8_t)(width >> 8);
    *p++ = (uint8_t)(width & 0xFF);
    *p++ = 0x03;
    *p++ = 0x01;
    *p++ = 0x22; /* Y: h=2, v=2 */
    *p++ = 0x00;
    *p++ = 0x02;
    *p++ = 0x11;
    *p++ = 0x01;
    *p++ = 0x03;
    *p++ = 0x11;
    *p++ = 0x01;

    /* SOS */
    *p++ = 0xFF;
    *p++ = 0xDA;
    *p++ = 0x00;
    *p++ = 0x0C;
    *p++ = 0x03;
    *p++ = 0x01;
    *p++ = 0x00;
    *p++ = 0x02;
    *p++ = 0x11;
    *p++ = 0x03;
    *p++ = 0x11;
    *p++ = 0x00;
    *p++ = 0x3F;
    *p++ = 0x00;

    memcpy(p, scan_data, scan_len);
    p += scan_len;

    *p++ = 0xFF;
    *p++ = 0xD9;

    return (size_t)(p - buf);
}

/* --- Test context --- */

typedef struct {
    int fds[2];
    Compy_JpegTransport *jpeg;
} JpegTestCtx;

static void jpeg_setup(JpegTestCtx *ctx, size_t max_frag) {
    int ok = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, ctx->fds);
    assert(ok == 0);

    srand(42);
    Compy_Transport t = compy_transport_udp(ctx->fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 26, 90000);

    Compy_JpegTransportConfig cfg = {.max_fragment_size = max_frag};
    ctx->jpeg = Compy_JpegTransport_new_with_config(rtp, cfg);
}

static void jpeg_teardown(JpegTestCtx *ctx) {
    VTABLE(Compy_JpegTransport, Compy_Droppable).drop(ctx->jpeg);
    close(ctx->fds[1]);
}

/* --- Tests --- */

TEST jpeg_single_fragment(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 1200);

    uint8_t scan[100];
    for (int i = 0; i < 100; i++) {
        scan[i] = (uint8_t)(i ^ 0xAA);
    }

    uint8_t jpeg_buf[2048];
    size_t jpeg_len = build_test_jpeg(
        jpeg_buf, sizeof jpeg_buf, 1920, 1080, 2, 2, scan, sizeof scan);

    int ret = Compy_JpegTransport_send_frame(
        ctx.jpeg, Compy_RtpTimestamp_Raw(12345),
        U8Slice99_new(jpeg_buf, jpeg_len));
    ASSERT_EQ(0, ret);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    /* Single packet */
    ASSERT_EQ(1, cap.count);

    /* Marker bit set on last (only) packet */
    ASSERT(packet_rtp_marker(&cap.packets[0]));

    /* JPEG RTP header fields (RFC 2435 Section 3.1) */
    ASSERT_EQ(0, packet_jpeg_type_specific(&cap.packets[0]));
    ASSERT_EQ(0, packet_jpeg_frag_offset(&cap.packets[0]));
    ASSERT_EQ(0, packet_jpeg_type(&cap.packets[0])); /* YUV420 */
    ASSERT_EQ(255, packet_jpeg_q(&cap.packets[0]));
    ASSERT_EQ(240, packet_jpeg_width(&cap.packets[0]));  /* 1920/8 */
    ASSERT_EQ(135, packet_jpeg_height(&cap.packets[0])); /* 1080/8 */

    /* QT header (at byte 20, no DRI) */
    const size_t qt_off = 20;
    ASSERT_EQ(0, packet_qt_mbz(&cap.packets[0], qt_off));
    ASSERT_EQ(0, packet_qt_precision(&cap.packets[0], qt_off));
    ASSERT_EQ(128, packet_qt_length(&cap.packets[0], qt_off));

    /* QT data: verify first and last bytes of luma table */
    ASSERT_EQ(1, cap.packets[0].data[qt_off + 4]);       /* luma[0] */
    ASSERT_EQ(64, cap.packets[0].data[qt_off + 4 + 63]); /* luma[63] */

    /* QT data: verify first byte of chroma table */
    ASSERT_EQ(65, cap.packets[0].data[qt_off + 4 + 64]); /* chroma[0] */

    /* Scan data starts after QT header + QT data */
    const size_t scan_off = qt_off + 4 + 128;
    ASSERT_EQ((ssize_t)(12 + 8 + 4 + 128 + 100), cap.packets[0].len);
    ASSERT_MEM_EQ(scan, cap.packets[0].data + scan_off, sizeof scan);

    jpeg_teardown(&ctx);
    PASS();
}

TEST jpeg_multi_fragment(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 200);

    uint8_t scan[500];
    for (int i = 0; i < 500; i++) {
        scan[i] = (uint8_t)(i & 0xFF);
    }

    uint8_t jpeg_buf[2048];
    size_t jpeg_len = build_test_jpeg(
        jpeg_buf, sizeof jpeg_buf, 640, 480, 2, 2, scan, sizeof scan);

    int ret = Compy_JpegTransport_send_frame(
        ctx.jpeg, Compy_RtpTimestamp_Raw(0), U8Slice99_new(jpeg_buf, jpeg_len));
    ASSERT_EQ(0, ret);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    /* Must produce multiple fragments */
    ASSERT(cap.count >= 3);

    /* Marker only on last */
    for (int i = 0; i < cap.count - 1; i++) {
        ASSERT_FALSE(packet_rtp_marker(&cap.packets[i]));
    }
    ASSERT(packet_rtp_marker(&cap.packets[cap.count - 1]));

    /* All packets share the same type/Q/width/height */
    for (int i = 0; i < cap.count; i++) {
        ASSERT_EQ(0, packet_jpeg_type(&cap.packets[i]));
        ASSERT_EQ(255, packet_jpeg_q(&cap.packets[i]));
        ASSERT_EQ(80, packet_jpeg_width(&cap.packets[i]));  /* 640/8 */
        ASSERT_EQ(60, packet_jpeg_height(&cap.packets[i])); /* 480/8 */
    }

    /* Fragment offsets: first is 0, subsequent increase monotonically */
    ASSERT_EQ(0, packet_jpeg_frag_offset(&cap.packets[0]));
    for (int i = 1; i < cap.count; i++) {
        ASSERT(
            packet_jpeg_frag_offset(&cap.packets[i]) >
            packet_jpeg_frag_offset(&cap.packets[i - 1]));
    }

    /* QT header only on first packet (at byte 20) */
    ASSERT_EQ(128, packet_qt_length(&cap.packets[0], 20));
    /* Subsequent packets: byte 20 is scan data, not QT header */

    /* Reassemble scan data and verify */
    uint8_t reassembled[2048];
    size_t total = 0;
    for (int i = 0; i < cap.count; i++) {
        size_t hdr;
        if (i == 0) {
            /* RTP(12) + JPEG(8) + QT_HDR(4) + QT_DATA(128) */
            hdr = 12 + 8 + 4 + 128;
        } else {
            /* RTP(12) + JPEG(8) */
            hdr = 12 + 8;
        }
        size_t data_len = (size_t)cap.packets[i].len - hdr;
        memcpy(reassembled + total, cap.packets[i].data + hdr, data_len);
        total += data_len;
    }

    ASSERT_EQ(sizeof scan, total);
    ASSERT_MEM_EQ(scan, reassembled, sizeof scan);

    jpeg_teardown(&ctx);
    PASS();
}

TEST jpeg_yuv422_type(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 1200);

    uint8_t scan[50];
    for (int i = 0; i < 50; i++) {
        scan[i] = (uint8_t)i;
    }

    uint8_t jpeg_buf[2048];
    /* YUV 4:2:2: Y sampling h=2, v=1 */
    size_t jpeg_len = build_test_jpeg(
        jpeg_buf, sizeof jpeg_buf, 640, 480, 2, 1, scan, sizeof scan);

    int ret = Compy_JpegTransport_send_frame(
        ctx.jpeg, Compy_RtpTimestamp_Raw(0), U8Slice99_new(jpeg_buf, jpeg_len));
    ASSERT_EQ(0, ret);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT_EQ(1, cap.count);
    ASSERT_EQ(1, packet_jpeg_type(&cap.packets[0])); /* YUV422 */

    jpeg_teardown(&ctx);
    PASS();
}

TEST jpeg_with_dri(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 1200);

    uint8_t scan[50];
    for (int i = 0; i < 50; i++) {
        scan[i] = (uint8_t)i;
    }

    uint8_t jpeg_buf[2048];
    size_t jpeg_len = build_test_jpeg_with_dri(
        jpeg_buf, sizeof jpeg_buf, 640, 480, 10, scan, sizeof scan);

    int ret = Compy_JpegTransport_send_frame(
        ctx.jpeg, Compy_RtpTimestamp_Raw(0), U8Slice99_new(jpeg_buf, jpeg_len));
    ASSERT_EQ(0, ret);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT_EQ(1, cap.count);

    /* Type = 0 + 64 = 64 (YUV420 with DRI) */
    ASSERT_EQ(64, packet_jpeg_type(&cap.packets[0]));

    /* Restart marker header at byte 20, before QT header */
    uint16_t ri =
        ((uint16_t)cap.packets[0].data[20] << 8) | cap.packets[0].data[21];
    ASSERT_EQ(10, ri);

    /* F=1, L=1 (single fragment), count=0x3FFF */
    uint16_t fl =
        ((uint16_t)cap.packets[0].data[22] << 8) | cap.packets[0].data[23];
    ASSERT_EQ(0xFFFF, fl); /* F|L|0x3FFF = 0x8000|0x4000|0x3FFF */

    /* QT header follows restart header at byte 24 */
    ASSERT_EQ(0, packet_qt_mbz(&cap.packets[0], 24));
    ASSERT_EQ(128, packet_qt_length(&cap.packets[0], 24));

    jpeg_teardown(&ctx);
    PASS();
}

TEST jpeg_invalid_input(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 1200);

    /* Empty input */
    ASSERT_EQ(
        -1, Compy_JpegTransport_send_frame(
                ctx.jpeg, Compy_RtpTimestamp_Raw(0), U8Slice99_empty()));

    /* Too short */
    uint8_t tiny[] = {0xFF, 0xD8};
    ASSERT_EQ(
        -1, Compy_JpegTransport_send_frame(
                ctx.jpeg, Compy_RtpTimestamp_Raw(0),
                U8Slice99_new(tiny, sizeof tiny)));

    /* No SOI */
    uint8_t no_soi[] = {0x00, 0x00, 0xFF, 0xD9};
    ASSERT_EQ(
        -1, Compy_JpegTransport_send_frame(
                ctx.jpeg, Compy_RtpTimestamp_Raw(0),
                U8Slice99_new(no_soi, sizeof no_soi)));

    /* SOI but no SOS/SOF/DQT */
    uint8_t soi_eoi[] = {0xFF, 0xD8, 0xFF, 0xD9};
    ASSERT_EQ(
        -1, Compy_JpegTransport_send_frame(
                ctx.jpeg, Compy_RtpTimestamp_Raw(0),
                U8Slice99_new(soi_eoi, sizeof soi_eoi)));

    jpeg_teardown(&ctx);
    PASS();
}

TEST jpeg_width_height_encoding(void) {
    JpegTestCtx ctx;
    jpeg_setup(&ctx, 1200);

    uint8_t scan[10];
    memset(scan, 0x42, sizeof scan);

    /* 320x240 */
    uint8_t jpeg_buf[2048];
    size_t jpeg_len = build_test_jpeg(
        jpeg_buf, sizeof jpeg_buf, 320, 240, 2, 2, scan, sizeof scan);

    int ret = Compy_JpegTransport_send_frame(
        ctx.jpeg, Compy_RtpTimestamp_Raw(0), U8Slice99_new(jpeg_buf, jpeg_len));
    ASSERT_EQ(0, ret);

    TestPacketCapture cap;
    capture_init(&cap);
    capture_recv_all(&cap, ctx.fds[1]);

    ASSERT_EQ(1, cap.count);
    ASSERT_EQ(40, packet_jpeg_width(&cap.packets[0]));  /* 320/8 */
    ASSERT_EQ(30, packet_jpeg_height(&cap.packets[0])); /* 240/8 */

    jpeg_teardown(&ctx);
    PASS();
}

SUITE(jpeg_transport) {
    RUN_TEST(jpeg_single_fragment);
    RUN_TEST(jpeg_multi_fragment);
    RUN_TEST(jpeg_yuv422_type);
    RUN_TEST(jpeg_with_dri);
    RUN_TEST(jpeg_invalid_input);
    RUN_TEST(jpeg_width_height_encoding);
}
