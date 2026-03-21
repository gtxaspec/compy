#include <compy/nal_transport.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <alloca.h>

#include <slice99.h>

static int send_fragmentized_nal_data(
    Compy_RtpTransport *t, Compy_RtpTimestamp ts, size_t max_packet_size,
    Compy_NalUnit nalu);
static int send_fu(
    Compy_RtpTransport *t, Compy_RtpTimestamp ts, Compy_NalUnit fu,
    bool is_first_fragment, bool is_last_fragment);

Compy_NalTransportConfig Compy_NalTransportConfig_default(void) {
    return (Compy_NalTransportConfig){
        .max_h264_nalu_size = COMPY_MAX_H264_NALU_SIZE,
        .max_h265_nalu_size = COMPY_MAX_H265_NALU_SIZE,
    };
}

struct Compy_NalTransport {
    Compy_RtpTransport *transport;
    Compy_NalTransportConfig config;
};

Compy_NalTransport *Compy_NalTransport_new(Compy_RtpTransport *t) {
    assert(t);

    return Compy_NalTransport_new_with_config(
        t, Compy_NalTransportConfig_default());
}

Compy_NalTransport *Compy_NalTransport_new_with_config(
    Compy_RtpTransport *t, Compy_NalTransportConfig config) {
    assert(t);

    Compy_NalTransport *self = malloc(sizeof *self);
    assert(self);

    self->transport = t;
    self->config = config;

    return self;
}

static void Compy_NalTransport_drop(VSelf) {
    VSELF(Compy_NalTransport);
    assert(self);

    VTABLE(Compy_RtpTransport, Compy_Droppable).drop(self->transport);

    free(self);
}

implExtern(Compy_Droppable, Compy_NalTransport);

bool Compy_NalTransport_is_full(Compy_NalTransport *self) {
    return Compy_RtpTransport_is_full(self->transport);
}

int Compy_NalTransport_send_packet(
    Compy_NalTransport *self, Compy_RtpTimestamp ts,
    Compy_NalUnit nalu) {
    assert(self);

    const size_t max_packet_size = MATCHES(nalu.header, Compy_NalHeader_H264)
                                       ? self->config.max_h264_nalu_size
                                       : self->config.max_h265_nalu_size,
                 nalu_size =
                     Compy_NalHeader_size(nalu.header) + nalu.payload.len;

    if (nalu_size < max_packet_size) {
        const bool marker =
            Compy_NalHeader_is_coded_slice_idr(nalu.header) ||
            Compy_NalHeader_is_coded_slice_non_idr(nalu.header);

        const size_t header_buf_size = Compy_NalHeader_size(nalu.header);
        uint8_t *header_buf = alloca(header_buf_size);
        Compy_NalHeader_serialize(nalu.header, header_buf);

        return Compy_RtpTransport_send_packet(
            self->transport, ts, marker,
            U8Slice99_new(header_buf, header_buf_size), nalu.payload);
    }

    return send_fragmentized_nal_data(
        self->transport, ts, max_packet_size, nalu);
}

// See <https://tools.ietf.org/html/rfc6184#section-5.8> (H.264),
// <https://tools.ietf.org/html/rfc7798#section-4.4.3> (H.265).
static int send_fragmentized_nal_data(
    Compy_RtpTransport *t, Compy_RtpTimestamp ts, size_t max_packet_size,
    Compy_NalUnit nalu) {
    const size_t rem = nalu.payload.len % max_packet_size,
                 packets_count = (nalu.payload.len - rem) / max_packet_size;

    for (size_t packet_idx = 0; packet_idx < packets_count; packet_idx++) {
        const bool is_first_fragment = 0 == packet_idx,
                   is_last_fragment =
                       0 == rem && (packets_count - 1 == packet_idx);

        const U8Slice99 fu_data = U8Slice99_sub(
            nalu.payload, packet_idx * max_packet_size,
            is_last_fragment ? nalu.payload.len
                             : (packet_idx + 1) * max_packet_size);
        const Compy_NalUnit fu = {nalu.header, fu_data};

        if (send_fu(t, ts, fu, is_first_fragment, is_last_fragment) == -1) {
            return -1;
        }
    }

    if (rem != 0) {
        const U8Slice99 fu_data =
            U8Slice99_advance(nalu.payload, packets_count * max_packet_size);
        const Compy_NalUnit fu = {nalu.header, fu_data};
        const bool is_first_fragment = 0 == packets_count,
                   is_last_fragment = true;

        if (send_fu(t, ts, fu, is_first_fragment, is_last_fragment) == -1) {
            return -1;
        }
    }

    return 0;
}

static int send_fu(
    Compy_RtpTransport *t, Compy_RtpTimestamp ts, Compy_NalUnit fu,
    bool is_first_fragment, bool is_last_fragment) {
    const size_t fu_header_size = Compy_NalHeader_fu_size(fu.header);
    U8Slice99 fu_header = U8Slice99_new(alloca(fu_header_size), fu_header_size);

    Compy_NalHeader_write_fu_header(
        fu.header, fu_header.ptr, is_first_fragment, is_last_fragment);

    const bool marker = is_last_fragment;

    return Compy_RtpTransport_send_packet(
        t, ts, marker, fu_header, fu.payload);
}
