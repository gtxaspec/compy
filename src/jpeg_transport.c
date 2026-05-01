// See <https://datatracker.ietf.org/doc/html/rfc2435>

#include <compy/jpeg_transport.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <alloca.h>

#include <slice99.h>

// RFC 2435 Section 3.1: main JPEG RTP header is always 8 bytes.
#define JPEG_RTP_HDR_SIZE 8

// RFC 2435 Section 3.1.8: quantization table header (excluding table data).
#define QT_HDR_SIZE 4

// RFC 2435 Section 3.1.7: restart marker header.
#define RST_HDR_SIZE 4

// Maximum quantization table data: 4 tables x 128 bytes (16-bit).
#define MAX_QT_DATA 512

// RFC 2435 Section 3.1.6: Q=255 means custom tables, always in-band.
#define JPEG_Q_CUSTOM 255

// JPEG marker codes.
#define JPEG_SOI  0xD8
#define JPEG_EOI  0xD9
#define JPEG_SOF0 0xC0
#define JPEG_DQT  0xDB
#define JPEG_DRI  0xDD
#define JPEG_SOS  0xDA

typedef struct {
    uint8_t type;
    uint8_t width_8;
    uint8_t height_8;
    uint16_t restart_interval;
    uint8_t qt_data[MAX_QT_DATA];
    size_t qt_len;
    uint8_t qt_precision;
    size_t scan_offset;
    size_t scan_len;
} JpegFrameInfo;

static int parse_dqt(const uint8_t *data, size_t len, JpegFrameInfo *info);
static int parse_sof0(const uint8_t *data, size_t len, JpegFrameInfo *info);
static int parse_jpeg_frame(U8Slice99 frame, JpegFrameInfo *info);

static void
write_jpeg_rtp_header(uint8_t *buf, uint32_t offset, const JpegFrameInfo *info);
static void write_qt_header(uint8_t *buf, uint8_t precision, size_t qt_len);
static void write_restart_header(
    uint8_t *buf, uint16_t interval, bool is_first, bool is_last);

Compy_JpegTransportConfig Compy_JpegTransportConfig_default(void) {
    return (Compy_JpegTransportConfig){
        .max_fragment_size = COMPY_JPEG_DEFAULT_MAX_FRAGMENT_SIZE,
    };
}

struct Compy_JpegTransport {
    Compy_RtpTransport *transport;
    Compy_JpegTransportConfig config;
};

Compy_JpegTransport *Compy_JpegTransport_new(Compy_RtpTransport *t) {
    assert(t);

    return Compy_JpegTransport_new_with_config(
        t, Compy_JpegTransportConfig_default());
}

Compy_JpegTransport *Compy_JpegTransport_new_with_config(
    Compy_RtpTransport *t, Compy_JpegTransportConfig config) {
    assert(t);

    Compy_JpegTransport *self = malloc(sizeof *self);
    assert(self);

    self->transport = t;
    self->config = config;

    return self;
}

static void Compy_JpegTransport_drop(VSelf) {
    VSELF(Compy_JpegTransport);
    assert(self);

    VTABLE(Compy_RtpTransport, Compy_Droppable).drop(self->transport);

    free(self);
}

implExtern(Compy_Droppable, Compy_JpegTransport);

bool Compy_JpegTransport_is_full(Compy_JpegTransport *self) {
    return Compy_RtpTransport_is_full(self->transport);
}

int Compy_JpegTransport_send_frame(
    Compy_JpegTransport *self, Compy_RtpTimestamp ts, U8Slice99 jpeg_frame) {
    assert(self);

    JpegFrameInfo info;
    memset(&info, 0, sizeof info);

    if (parse_jpeg_frame(jpeg_frame, &info) != 0) {
        return -1;
    }

    // RFC 2435 fragment offset is 24-bit; scan data beyond 16MB cannot
    // be addressed.
    if (info.scan_len == 0 || info.scan_len > 0xFFFFFF) {
        return -1;
    }

    const bool has_dri = info.restart_interval > 0;
    const size_t base_hdr = JPEG_RTP_HDR_SIZE + (has_dri ? RST_HDR_SIZE : 0);
    const size_t qt_total = QT_HDR_SIZE + info.qt_len;
    const size_t first_hdr = base_hdr + qt_total;
    const size_t max_frag = self->config.max_fragment_size;

    if (first_hdr >= max_frag) {
        return -1;
    }

    uint8_t *phdr = alloca(first_hdr);
    size_t offset = 0;
    bool first = true;

    while (offset < info.scan_len) {
        const size_t hdr_size = first ? first_hdr : base_hdr;
        const size_t avail = max_frag - hdr_size;
        const size_t remaining = info.scan_len - offset;
        const size_t chunk = remaining < avail ? remaining : avail;
        const bool last = (offset + chunk >= info.scan_len);

        uint8_t *p = phdr;

        write_jpeg_rtp_header(p, (uint32_t)offset, &info);
        p += JPEG_RTP_HDR_SIZE;

        if (has_dri) {
            write_restart_header(p, info.restart_interval, first, last);
            p += RST_HDR_SIZE;
        }

        if (first) {
            write_qt_header(p, info.qt_precision, info.qt_len);
            p += QT_HDR_SIZE;
            memcpy(p, info.qt_data, info.qt_len);
        }

        if (Compy_RtpTransport_send_packet(
                self->transport, ts, last, U8Slice99_new(phdr, hdr_size),
                U8Slice99_new(
                    jpeg_frame.ptr + info.scan_offset + offset, chunk)) != 0) {
            return -1;
        }

        offset += chunk;
        first = false;
    }

    return 0;
}

// See <https://datatracker.ietf.org/doc/html/rfc2435#section-3.1>
static void write_jpeg_rtp_header(
    uint8_t *buf, uint32_t offset, const JpegFrameInfo *info) {
    buf[0] = 0;
    buf[1] = (uint8_t)((offset >> 16) & 0xFF);
    buf[2] = (uint8_t)((offset >> 8) & 0xFF);
    buf[3] = (uint8_t)(offset & 0xFF);
    buf[4] = info->type;
    buf[5] = JPEG_Q_CUSTOM;
    buf[6] = info->width_8;
    buf[7] = info->height_8;
}

// See <https://datatracker.ietf.org/doc/html/rfc2435#section-3.1.8>
static void write_qt_header(uint8_t *buf, uint8_t precision, size_t qt_len) {
    buf[0] = 0;
    buf[1] = precision;
    buf[2] = (uint8_t)((qt_len >> 8) & 0xFF);
    buf[3] = (uint8_t)(qt_len & 0xFF);
}

// See <https://datatracker.ietf.org/doc/html/rfc2435#section-3.1.7>
static void write_restart_header(
    uint8_t *buf, uint16_t interval, bool is_first, bool is_last) {
    buf[0] = (uint8_t)(interval >> 8);
    buf[1] = (uint8_t)(interval & 0xFF);
    // F=first, L=last, restart count=0x3FFF (unknown, per RFC 2435)
    uint16_t fl_count = 0x3FFF;
    if (is_first) {
        fl_count |= 0x8000;
    }
    if (is_last) {
        fl_count |= 0x4000;
    }
    buf[2] = (uint8_t)(fl_count >> 8);
    buf[3] = (uint8_t)(fl_count & 0xFF);
}

static int parse_dqt(const uint8_t *data, size_t len, JpegFrameInfo *info) {
    const uint8_t *p = data;
    const uint8_t *end = data + len;

    while (p < end) {
        uint8_t pq = (*p >> 4) & 0x0F;
        uint8_t tq = *p & 0x0F;
        p++;

        size_t table_size = (pq == 0) ? 64 : 128;
        if ((size_t)(end - p) < table_size) {
            return -1;
        }
        if (info->qt_len + table_size > sizeof info->qt_data) {
            return -1;
        }

        memcpy(info->qt_data + info->qt_len, p, table_size);
        info->qt_len += table_size;

        if (pq != 0 && tq < 8) {
            info->qt_precision |= (uint8_t)(1 << tq);
        }

        p += table_size;
    }

    return 0;
}

static int parse_sof0(const uint8_t *data, size_t len, JpegFrameInfo *info) {
    // SOF0 data: precision(1) + height(2) + width(2) + nf(1) + components
    if (len < 6) {
        return -1;
    }

    uint16_t height = ((uint16_t)data[1] << 8) | data[2];
    uint16_t width = ((uint16_t)data[3] << 8) | data[4];
    uint8_t nf = data[5];

    if (nf < 1 || nf > 4 || len < 6 + (size_t)nf * 3) {
        return -1;
    }

    // RFC 2435 Section 3.1.5-3.1.6: dimensions in 8-pixel units, 0 if >2040.
    info->width_8 = (width > 2040) ? 0 : (uint8_t)(width / 8);
    info->height_8 = (height > 2040) ? 0 : (uint8_t)(height / 8);

    if (nf >= 3) {
        // RFC 2435 Section 4.1: type 0 = YUV 4:2:2 (Y: h=2, v=1)
        // RFC 2435 Section 4.2: type 1 = YUV 4:2:0 (Y: h=2, v=2)
        uint8_t y_h = (data[7] >> 4) & 0x0F;
        uint8_t y_v = data[7] & 0x0F;

        if (y_h == 2 && y_v == 2) {
            info->type = 1;
        } else {
            info->type = 0;
        }
    } else {
        info->type = 0;
    }

    return 0;
}

static int parse_jpeg_frame(U8Slice99 frame, JpegFrameInfo *info) {
    const uint8_t *p = frame.ptr;
    const uint8_t *end = frame.ptr + frame.len;
    bool found_sof = false;
    bool found_sos = false;

    if (frame.len < 4 || p[0] != 0xFF || p[1] != JPEG_SOI) {
        return -1;
    }
    p += 2;

    while (p < end - 1) {
        if (*p != 0xFF) {
            return -1;
        }
        p++;

        while (p < end && *p == 0xFF) {
            p++;
        }
        if (p >= end) {
            return -1;
        }

        uint8_t marker = *p++;

        if (marker == JPEG_EOI || marker == 0x00 || marker == 0x01) {
            continue;
        }
        if (marker >= 0xD0 && marker <= 0xD7) {
            continue;
        }

        if (end - p < 2) {
            return -1;
        }
        uint16_t seg_len = ((uint16_t)p[0] << 8) | p[1];
        if (seg_len < 2 || p + seg_len > end) {
            return -1;
        }

        const uint8_t *seg_data = p + 2;
        size_t seg_data_len = (size_t)(seg_len - 2);

        if (marker == JPEG_SOS) {
            size_t scan_start = (size_t)((p + seg_len) - frame.ptr);
            if (scan_start >= frame.len - 1) {
                return -1;
            }
            if (frame.ptr[frame.len - 2] != 0xFF ||
                frame.ptr[frame.len - 1] != JPEG_EOI) {
                return -1;
            }
            info->scan_offset = scan_start;
            info->scan_len = frame.len - 2 - scan_start;
            found_sos = true;
            break;
        }

        if (marker == JPEG_DQT) {
            if (parse_dqt(seg_data, seg_data_len, info) != 0) {
                return -1;
            }
        } else if (marker == JPEG_SOF0) {
            if (parse_sof0(seg_data, seg_data_len, info) != 0) {
                return -1;
            }
            found_sof = true;
        } else if (marker == JPEG_DRI) {
            if (seg_data_len < 2) {
                return -1;
            }
            info->restart_interval = ((uint16_t)seg_data[0] << 8) | seg_data[1];
        }

        p += seg_len;
    }

    if (!found_sof || !found_sos || info->qt_len == 0) {
        return -1;
    }

    // RFC 2435 Section 3.1.4: types 64-127 indicate restart markers.
    if (info->restart_interval > 0) {
        info->type += 64;
    }

    return 0;
}
