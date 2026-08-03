#include <compy/rtcp.h>

#include <assert.h>
#include <pthread.h>
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

static void ntp_anchor_ref(void);
static void ntp_anchor_unref(void);

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

    ntp_anchor_ref();

    return self;
}

static void Compy_Rtcp_drop(VSelf) {
    VSELF(Compy_Rtcp);
    assert(self);

    VCALL_SUPER(self->rtcp_transport, Compy_Droppable, drop);
    free(self);
    ntp_anchor_unref();
}

implExtern(Compy_Droppable, Compy_Rtcp);

/*
 * NTP timestamp derived from CLOCK_MONOTONIC so the NTP-RTP relationship
 * in Sender Reports is consistent (RTP timestamps also come from a
 * monotonic source). A one-time offset converts monotonic time to the
 * NTP epoch. Using CLOCK_REALTIME directly would drift relative to the
 * RTP clock, causing clients to recalibrate PTS on each SR.
 */
static pthread_mutex_t g_ntp_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_ntp_armed;
static int64_t g_mono_to_rt_ns;
static int g_rtcp_instances; /* guarded by g_ntp_lock */

static void ntp_anchor_arm_locked(void) {
    struct timespec rt, mt;
    clock_gettime(CLOCK_REALTIME, &rt);
    clock_gettime(CLOCK_MONOTONIC, &mt);
    /*
     * Full-precision offset. Computing it from tv_sec alone discarded
     * both fractional parts, biasing every SR NTP timestamp by the
     * fraction of the boot-time realtime/monotonic offset -- a constant
     * anywhere in (-1s, +1s), identical across daemon restarts, only
     * changing on reboot. Invisible to receivers (both tracks share
     * it, so A/V stays aligned) until something maps SR NTP against a
     * real wall clock, which is how a latency measurement read
     * -519 ms on a camera whose clock was 0.6 ms off.
     */
    int64_t rt_ns = (int64_t)rt.tv_sec * 1000000000LL + rt.tv_nsec;
    int64_t mt_ns = (int64_t)mt.tv_sec * 1000000000LL + mt.tv_nsec;
    g_mono_to_rt_ns = rt_ns - mt_ns;
    g_ntp_armed = true;
}

static void get_ntp_timestamp(uint32_t *msw, uint32_t *lsw) {
    pthread_mutex_lock(&g_ntp_lock);
    if (!g_ntp_armed)
        ntp_anchor_arm_locked();
    int64_t offset_ns = g_mono_to_rt_ns;
    pthread_mutex_unlock(&g_ntp_lock);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    int64_t now_ns = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec + offset_ns;

    *msw = (uint32_t)(now_ns / 1000000000LL + (int64_t)NTP_EPOCH_OFFSET);
    *lsw = (uint32_t)(((uint64_t)(now_ns % 1000000000LL) << 32) / 1000000000ULL);
}

/*
 * The anchor holds for as long as any RTCP instance exists, so no
 * receiver ever sees the NTP-RTP relationship recalibrate mid-session.
 * When the last instance drops it is disarmed, and the next session
 * anchors afresh. That closes the boot-order gap: on cameras that
 * stream to an auto-reconnecting client before NTP first syncs, the
 * anchor used to freeze the boot clock for the daemon's lifetime; now
 * one reconnect after sync is enough to carry correct wallclock.
 */
static void ntp_anchor_ref(void) {
    pthread_mutex_lock(&g_ntp_lock);
    g_rtcp_instances++;
    pthread_mutex_unlock(&g_ntp_lock);
}

static void ntp_anchor_unref(void) {
    pthread_mutex_lock(&g_ntp_lock);
    if (--g_rtcp_instances == 0)
        g_ntp_armed = false;
    pthread_mutex_unlock(&g_ntp_lock);
}

int Compy_Rtcp_send_sr(Compy_Rtcp *self) {
    assert(self);

    uint32_t ntp_msw, ntp_lsw;
    get_ntp_timestamp(&ntp_msw, &ntp_lsw);

    const Compy_RtcpSenderInfo info = {
        .ssrc = Compy_RtpTransport_get_ssrc(self->rtp),
        .ntp_timestamp_msw = ntp_msw,
        .ntp_timestamp_lsw = ntp_lsw,
        .rtp_timestamp = Compy_RtpTransport_get_rtp_timestamp_now(self->rtp),
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
    const size_t len =
        Compy_RtcpBye_serialize(Compy_RtpTransport_get_ssrc(self->rtp), buffer);

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

const Compy_RtcpReportBlock *Compy_Rtcp_get_last_rr(const Compy_Rtcp *self) {
    assert(self);
    return self->has_rr ? &self->last_rr : NULL;
}
