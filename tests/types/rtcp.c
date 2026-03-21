#include <compy/types/rtcp.h>

#include <greatest.h>

#include <arpa/inet.h>
#include <string.h>

TEST sr_serialize_basic(void) {
    const Compy_RtcpSenderInfo info = {
        .ssrc = 0x12345678,
        .ntp_timestamp_msw = 0xAABBCCDD,
        .ntp_timestamp_lsw = 0x11223344,
        .rtp_timestamp = 0x55667788,
        .sender_packet_count = 100,
        .sender_octet_count = 50000,
    };

    uint8_t buffer[COMPY_RTCP_MAX_PACKET_SIZE];
    const size_t len =
        Compy_RtcpSenderReport_serialize(info, "test", buffer);

    ASSERT(len > 0);

    /* SR header: V=2, P=0, RC=0, PT=200 */
    ASSERT_EQ(0x80, buffer[0]);
    ASSERT_EQ(COMPY_RTCP_SR, buffer[1]);

    /* SSRC */
    ASSERT_EQ(0x12, buffer[4]);
    ASSERT_EQ(0x34, buffer[5]);
    ASSERT_EQ(0x56, buffer[6]);
    ASSERT_EQ(0x78, buffer[7]);

    /* NTP MSW */
    ASSERT_EQ(0xAA, buffer[8]);
    ASSERT_EQ(0xBB, buffer[9]);
    ASSERT_EQ(0xCC, buffer[10]);
    ASSERT_EQ(0xDD, buffer[11]);

    /* After SR (28 bytes): SDES header */
    ASSERT_EQ(0x81, buffer[28]); /* V=2, SC=1 */
    ASSERT_EQ(COMPY_RTCP_SDES, buffer[29]);

    PASS();
}

TEST bye_serialize_basic(void) {
    uint8_t buffer[8];
    const size_t len = Compy_RtcpBye_serialize(0xDEADBEEF, buffer);

    ASSERT_EQ(8, len);
    ASSERT_EQ(0x81, buffer[0]); /* V=2, SC=1 */
    ASSERT_EQ(COMPY_RTCP_BYE, buffer[1]);
    /* length = 1 (in 32-bit words minus one) */
    ASSERT_EQ(0x00, buffer[2]);
    ASSERT_EQ(0x01, buffer[3]);
    /* SSRC */
    ASSERT_EQ(0xDE, buffer[4]);
    ASSERT_EQ(0xAD, buffer[5]);
    ASSERT_EQ(0xBE, buffer[6]);
    ASSERT_EQ(0xEF, buffer[7]);

    PASS();
}

TEST rtcp_header_deserialize_basic(void) {
    /* V=2, P=0, RC=1, PT=201 (RR), length=7 */
    const uint8_t data[] = {0x81, 0xC9, 0x00, 0x07};

    Compy_RtcpHeader hdr;
    const int ret = Compy_RtcpHeader_deserialize(&hdr, data, sizeof data);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, hdr.version);
    ASSERT_EQ(0, hdr.padding);
    ASSERT_EQ(1, hdr.count);
    ASSERT_EQ(COMPY_RTCP_RR, hdr.packet_type);
    ASSERT_EQ(7, hdr.length);

    PASS();
}

TEST rtcp_header_deserialize_too_short(void) {
    const uint8_t data[] = {0x80, 0xC8, 0x00};

    Compy_RtcpHeader hdr;
    ASSERT_EQ(-1, Compy_RtcpHeader_deserialize(&hdr, data, sizeof data));

    PASS();
}

TEST rtcp_header_deserialize_bad_version(void) {
    /* V=1 instead of 2 */
    const uint8_t data[] = {0x41, 0xC8, 0x00, 0x06};

    Compy_RtcpHeader hdr;
    ASSERT_EQ(-1, Compy_RtcpHeader_deserialize(&hdr, data, sizeof data));

    PASS();
}

TEST rr_deserialize_basic(void) {
    /* Hand-crafted RR report block (24 bytes) */
    uint8_t data[24];
    uint8_t *p = data;

    /* SSRC of source: 0xAABBCCDD */
    uint32_t ssrc = htonl(0xAABBCCDD);
    memcpy(p, &ssrc, 4);
    p += 4;

    /* fraction lost: 25, cumulative lost: 0x001234 (24-bit) */
    *p++ = 25;
    *p++ = 0x00;
    *p++ = 0x12;
    *p++ = 0x34;

    /* extended highest seq: 5000 */
    uint32_t seq = htonl(5000);
    memcpy(p, &seq, 4);
    p += 4;

    /* jitter: 100 */
    uint32_t jitter = htonl(100);
    memcpy(p, &jitter, 4);
    p += 4;

    /* last SR: 0x11112222 */
    uint32_t lsr = htonl(0x11112222);
    memcpy(p, &lsr, 4);
    p += 4;

    /* delay since last SR: 0x33334444 */
    uint32_t dlsr = htonl(0x33334444);
    memcpy(p, &dlsr, 4);

    Compy_RtcpReportBlock rr;
    const int ret = Compy_RtcpReportBlock_deserialize(&rr, data, sizeof data);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(0xAABBCCDD, rr.ssrc);
    ASSERT_EQ(25, rr.fraction_lost);
    ASSERT_EQ(0x001234, rr.cumulative_lost);
    ASSERT_EQ(5000, rr.extended_highest_seq);
    ASSERT_EQ(100, rr.interarrival_jitter);
    ASSERT_EQ(0x11112222, rr.last_sr);
    ASSERT_EQ(0x33334444, rr.delay_since_last_sr);

    PASS();
}

TEST rr_deserialize_too_short(void) {
    uint8_t data[20] = {0};

    Compy_RtcpReportBlock rr;
    ASSERT_EQ(-1, Compy_RtcpReportBlock_deserialize(&rr, data, sizeof data));

    PASS();
}

SUITE(types_rtcp) {
    RUN_TEST(sr_serialize_basic);
    RUN_TEST(bye_serialize_basic);
    RUN_TEST(rtcp_header_deserialize_basic);
    RUN_TEST(rtcp_header_deserialize_too_short);
    RUN_TEST(rtcp_header_deserialize_bad_version);
    RUN_TEST(rr_deserialize_basic);
    RUN_TEST(rr_deserialize_too_short);
}
