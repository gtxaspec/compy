#include <compy/types/rtcp.h>

#include <assert.h>
#include <string.h>

#include <arpa/inet.h>

#define RTCP_VERSION         2
#define RTCP_HEADER_SIZE     4
#define SR_SENDER_INFO_SIZE  24
#define RR_REPORT_BLOCK_SIZE 24

static uint8_t *write_u32(uint8_t *p, uint32_t val) {
    val = htonl(val);
    memcpy(p, &val, 4);
    return p + 4;
}

static uint8_t *write_u16(uint8_t *p, uint16_t val) {
    val = htons(val);
    memcpy(p, &val, 2);
    return p + 2;
}

static uint32_t read_u32(const uint8_t *p) {
    uint32_t val;
    memcpy(&val, p, 4);
    return ntohl(val);
}

static uint16_t read_u16(const uint8_t *p) {
    uint16_t val;
    memcpy(&val, p, 2);
    return ntohs(val);
}

size_t Compy_RtcpSenderReport_serialize(
    Compy_RtcpSenderInfo info, const char *cname, uint8_t buffer[restrict]) {
    assert(cname);
    assert(buffer);

    uint8_t *p = buffer;

    /*
     * SR packet (RFC 3550 Section 6.4.1):
     *  0                   1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |V=2|P|    RC   |   PT=SR=200   |             length            |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                         SSRC of sender                        |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |              NTP timestamp, most significant word             |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |             NTP timestamp, least significant word             |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                         RTP timestamp                         |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                     sender's packet count                     |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                      sender's octet count                     |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    /* SR header: V=2, P=0, RC=0, PT=200 */
    *p++ = (RTCP_VERSION << 6);
    *p++ = COMPY_RTCP_SR;
    /* length in 32-bit words minus one: (28 - 4) / 4 = 6 */
    p = write_u16(p, (SR_SENDER_INFO_SIZE + 4) / 4 - 1);

    p = write_u32(p, info.ssrc);
    p = write_u32(p, info.ntp_timestamp_msw);
    p = write_u32(p, info.ntp_timestamp_lsw);
    p = write_u32(p, info.rtp_timestamp);
    p = write_u32(p, info.sender_packet_count);
    p = write_u32(p, info.sender_octet_count);

    /*
     * SDES packet (RFC 3550 Section 6.5):
     * Contains a single chunk with one CNAME item.
     */

    const size_t cname_len = strlen(cname);
    /* SDES chunk: SSRC(4) + type(1) + length(1) + cname + null terminator,
     * padded to 4-byte boundary */
    const size_t sdes_chunk_size = 4 + 2 + cname_len + 1;
    const size_t sdes_chunk_padded = (sdes_chunk_size + 3) & ~(size_t)3;
    const size_t sdes_total = RTCP_HEADER_SIZE + sdes_chunk_padded;

    /* SDES header: V=2, P=0, SC=1, PT=202 */
    *p++ = (RTCP_VERSION << 6) | 1;
    *p++ = COMPY_RTCP_SDES;
    p = write_u16(p, (uint16_t)(sdes_total / 4 - 1));

    p = write_u32(p, info.ssrc);

    /* CNAME item */
    *p++ = COMPY_RTCP_SDES_CNAME;
    *p++ = (uint8_t)cname_len;
    memcpy(p, cname, cname_len);
    p += cname_len;

    /* Null terminator + padding to 4-byte boundary */
    const size_t pad = sdes_chunk_padded - sdes_chunk_size;
    memset(p, 0, pad + 1);
    p += pad + 1;

    return (size_t)(p - buffer);
}

size_t Compy_RtcpBye_serialize(uint32_t ssrc, uint8_t buffer[restrict]) {
    assert(buffer);

    uint8_t *p = buffer;

    /* BYE header: V=2, P=0, SC=1, PT=203, length=1 */
    *p++ = (RTCP_VERSION << 6) | 1;
    *p++ = COMPY_RTCP_BYE;
    p = write_u16(p, 1);
    p = write_u32(p, ssrc);

    return (size_t)(p - buffer);
}

int Compy_RtcpHeader_deserialize(
    Compy_RtcpHeader *restrict self, const uint8_t *data, size_t len) {
    assert(self);
    assert(data);

    if (len < RTCP_HEADER_SIZE) {
        return -1;
    }

    self->version = (data[0] >> 6) & 0x03;
    if (self->version != RTCP_VERSION) {
        return -1;
    }

    self->padding = (data[0] >> 5) & 0x01;
    self->count = data[0] & 0x1F;
    self->packet_type = data[1];
    self->length = read_u16(&data[2]);

    return 0;
}

int Compy_RtcpReportBlock_deserialize(
    Compy_RtcpReportBlock *restrict self, const uint8_t *data, size_t len) {
    assert(self);
    assert(data);

    if (len < RR_REPORT_BLOCK_SIZE) {
        return -1;
    }

    self->ssrc = read_u32(&data[0]);
    self->fraction_lost = data[4];
    self->cumulative_lost =
        ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | data[7];
    self->extended_highest_seq = read_u32(&data[8]);
    self->interarrival_jitter = read_u32(&data[12]);
    self->last_sr = read_u32(&data[16]);
    self->delay_since_last_sr = read_u32(&data[20]);

    return 0;
}
