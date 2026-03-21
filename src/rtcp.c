#include <compy/rtcp.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Seconds between 1900-01-01 and 1970-01-01 (RFC 868) */
#define NTP_EPOCH_OFFSET 2208988800ULL

struct Compy_Rtcp {
    Compy_RtpTransport *rtp;
    Compy_Transport rtcp_transport;
    char cname[256];
    Compy_RtcpReportBlock last_rr;
    bool has_rr;
};

Compy_Rtcp *Compy_Rtcp_new(
    Compy_RtpTransport *rtp, Compy_Transport rtcp_transport,
    const char *cname) {
    assert(rtp);
    assert(rtcp_transport.self && rtcp_transport.vptr);
    assert(cname);

    Compy_Rtcp *self = malloc(sizeof *self);
    assert(self);

    self->rtp = rtp;
    self->rtcp_transport = rtcp_transport;
    strncpy(self->cname, cname, sizeof(self->cname) - 1);
    self->cname[sizeof(self->cname) - 1] = '\0';
    memset(&self->last_rr, 0, sizeof(self->last_rr));
    self->has_rr = false;

    return self;
}

static void Compy_Rtcp_drop(VSelf) {
    VSELF(Compy_Rtcp);
    assert(self);

    VCALL_SUPER(self->rtcp_transport, Compy_Droppable, drop);
    free(self);
}

implExtern(Compy_Droppable, Compy_Rtcp);

static void get_ntp_timestamp(uint32_t *msw, uint32_t *lsw) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    *msw = (uint32_t)(ts.tv_sec + NTP_EPOCH_OFFSET);
    *lsw = (uint32_t)(((uint64_t)ts.tv_nsec << 32) / 1000000000ULL);
}

int Compy_Rtcp_send_sr(Compy_Rtcp *self) {
    assert(self);

    uint32_t ntp_msw, ntp_lsw;
    get_ntp_timestamp(&ntp_msw, &ntp_lsw);

    const Compy_RtcpSenderInfo info = {
        .ssrc = Compy_RtpTransport_get_ssrc(self->rtp),
        .ntp_timestamp_msw = ntp_msw,
        .ntp_timestamp_lsw = ntp_lsw,
        .rtp_timestamp = Compy_RtpTransport_get_last_rtp_timestamp(self->rtp),
        .sender_packet_count = Compy_RtpTransport_get_packet_count(self->rtp),
        .sender_octet_count = Compy_RtpTransport_get_octet_count(self->rtp),
    };

    uint8_t buffer[COMPY_RTCP_MAX_PACKET_SIZE];
    const size_t len =
        Compy_RtcpSenderReport_serialize(info, self->cname, buffer);

    const Compy_IoVecSlice bufs =
        (Compy_IoVecSlice)Slice99_typed_from_array((struct iovec[]){
            {.iov_base = buffer, .iov_len = len},
        });

    return VCALL(self->rtcp_transport, transmit, bufs);
}

int Compy_Rtcp_send_bye(Compy_Rtcp *self) {
    assert(self);

    uint8_t buffer[8];
    const size_t len = Compy_RtcpBye_serialize(
        Compy_RtpTransport_get_ssrc(self->rtp), buffer);

    const Compy_IoVecSlice bufs =
        (Compy_IoVecSlice)Slice99_typed_from_array((struct iovec[]){
            {.iov_base = buffer, .iov_len = len},
        });

    return VCALL(self->rtcp_transport, transmit, bufs);
}

int Compy_Rtcp_handle_incoming(
    Compy_Rtcp *self, const uint8_t *data, size_t len) {
    assert(self);
    assert(data);

    Compy_RtcpHeader hdr;
    if (Compy_RtcpHeader_deserialize(&hdr, data, len) != 0) {
        return -1;
    }

    if (hdr.packet_type == COMPY_RTCP_RR && hdr.count > 0) {
        /* Skip RTCP header (4 bytes) + reporter SSRC (4 bytes) */
        const size_t offset = 8;
        if (len < offset + 24) {
            return -1;
        }

        if (Compy_RtcpReportBlock_deserialize(
                &self->last_rr, data + offset, len - offset) != 0) {
            return -1;
        }

        self->has_rr = true;
    }

    return 0;
}

const Compy_RtcpReportBlock *
Compy_Rtcp_get_last_rr(const Compy_Rtcp *self) {
    assert(self);
    return self->has_rr ? &self->last_rr : NULL;
}
