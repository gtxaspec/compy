#include <compy/rtcp.h>
#include <compy/transport.h>
#include <compy/writer.h>

#include <greatest.h>

#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

TEST rtcp_send_sr(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    srand(42);
    Compy_Transport rtp_t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(rtp_t, 96, 90000);

    int rtcp_fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, rtcp_fds) == 0);

    Compy_Transport rtcp_t = compy_transport_udp(rtcp_fds[0]);
    Compy_Rtcp *rtcp = Compy_Rtcp_new(rtp, rtcp_t, "test@camera");

    const int ret = Compy_Rtcp_send_sr(rtcp);
    ASSERT_EQ(0, ret);

    /* Read the SR packet from the other end */
    uint8_t buf[COMPY_RTCP_MAX_PACKET_SIZE];
    const ssize_t n = read(rtcp_fds[1], buf, sizeof buf);
    ASSERT(n > 0);

    /* Verify SR header */
    ASSERT_EQ(0x80, buf[0]); /* V=2 */
    ASSERT_EQ(COMPY_RTCP_SR, buf[1]);

    /* Verify SDES follows the SR (at offset 28) */
    ASSERT(n > 28);
    ASSERT_EQ(COMPY_RTCP_SDES, buf[29]);

    VCALL(DYN(Compy_Rtcp, Compy_Droppable, rtcp), drop);
    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    close(rtcp_fds[1]);

    PASS();
}

TEST rtcp_send_bye(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    srand(42);
    Compy_Transport rtp_t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(rtp_t, 96, 90000);

    int rtcp_fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, rtcp_fds) == 0);

    Compy_Transport rtcp_t = compy_transport_udp(rtcp_fds[0]);
    Compy_Rtcp *rtcp = Compy_Rtcp_new(rtp, rtcp_t, "test@camera");

    const int ret = Compy_Rtcp_send_bye(rtcp);
    ASSERT_EQ(0, ret);

    uint8_t buf[16];
    const ssize_t n = read(rtcp_fds[1], buf, sizeof buf);
    ASSERT_EQ(8, n);
    ASSERT_EQ(0x81, buf[0]); /* V=2, SC=1 */
    ASSERT_EQ(COMPY_RTCP_BYE, buf[1]);

    VCALL(DYN(Compy_Rtcp, Compy_Droppable, rtcp), drop);
    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    close(rtcp_fds[1]);

    PASS();
}

TEST rtcp_handle_rr(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    srand(42);
    Compy_Transport rtp_t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(rtp_t, 96, 90000);

    int rtcp_fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, rtcp_fds) == 0);

    Compy_Transport rtcp_t = compy_transport_udp(rtcp_fds[0]);
    Compy_Rtcp *rtcp = Compy_Rtcp_new(rtp, rtcp_t, "test@camera");

    /* No RR received yet */
    ASSERT_EQ(NULL, Compy_Rtcp_get_last_rr(rtcp));

    /* Build a fake RR packet: header (4) + reporter SSRC (4) + report block
     * (24) = 32 */
    uint8_t rr_packet[32];
    /* Header: V=2, RC=1, PT=201, length=7 */
    rr_packet[0] = 0x81;
    rr_packet[1] = COMPY_RTCP_RR;
    rr_packet[2] = 0x00;
    rr_packet[3] = 0x07;
    /* Reporter SSRC */
    rr_packet[4] = 0xAA;
    rr_packet[5] = 0xBB;
    rr_packet[6] = 0xCC;
    rr_packet[7] = 0xDD;
    /* Report block SSRC */
    rr_packet[8] = 0x12;
    rr_packet[9] = 0x34;
    rr_packet[10] = 0x56;
    rr_packet[11] = 0x78;
    /* fraction lost: 10, cumulative lost: 0x000005 */
    rr_packet[12] = 10;
    rr_packet[13] = 0x00;
    rr_packet[14] = 0x00;
    rr_packet[15] = 0x05;
    /* Rest: zeros */
    memset(&rr_packet[16], 0, 16);

    const int ret =
        Compy_Rtcp_handle_incoming(rtcp, rr_packet, sizeof rr_packet);
    ASSERT_EQ(0, ret);

    const Compy_RtcpReportBlock *rr = Compy_Rtcp_get_last_rr(rtcp);
    ASSERT(rr != NULL);
    ASSERT_EQ(0x12345678, rr->ssrc);
    ASSERT_EQ(10, rr->fraction_lost);
    ASSERT_EQ(5, rr->cumulative_lost);

    VCALL(DYN(Compy_Rtcp, Compy_Droppable, rtcp), drop);
    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    close(rtcp_fds[1]);

    PASS();
}

TEST rtp_stats_tracking(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    srand(42);
    Compy_Transport t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    ASSERT_EQ(0, Compy_RtpTransport_get_packet_count(rtp));
    ASSERT_EQ(0, Compy_RtpTransport_get_octet_count(rtp));
    ASSERT(Compy_RtpTransport_get_ssrc(rtp) != 0);

    /* Send a packet with 10-byte payload */
    uint8_t payload[10] = {0};
    int ret __attribute__((unused)) = Compy_RtpTransport_send_packet(
        rtp, Compy_RtpTimestamp_Raw(100), false, U8Slice99_empty(),
        U8Slice99_new(payload, sizeof payload));

    ASSERT_EQ(1, Compy_RtpTransport_get_packet_count(rtp));
    ASSERT_EQ(10, Compy_RtpTransport_get_octet_count(rtp));
    ASSERT_EQ(100, Compy_RtpTransport_get_last_rtp_timestamp(rtp));

    /* Send another with 20-byte payload */
    uint8_t payload2[20] = {0};
    ret = Compy_RtpTransport_send_packet(
        rtp, Compy_RtpTimestamp_Raw(200), true, U8Slice99_empty(),
        U8Slice99_new(payload2, sizeof payload2));

    ASSERT_EQ(2, Compy_RtpTransport_get_packet_count(rtp));
    ASSERT_EQ(30, Compy_RtpTransport_get_octet_count(rtp));
    ASSERT_EQ(200, Compy_RtpTransport_get_last_rtp_timestamp(rtp));

    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    PASS();
}

TEST rtp_ssrc_nonzero_entropy(void) {
    /* Verify SSRC uses OS entropy (not zero from unseeded rand) */
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_Transport t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    uint32_t ssrc = Compy_RtpTransport_get_ssrc(rtp);
    ASSERT(ssrc != 0);

    /* Create a second — SSRCs should differ */
    int fds2[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds2) == 0);
    Compy_Transport t2 = compy_transport_udp(fds2[0]);
    Compy_RtpTransport *rtp2 = Compy_RtpTransport_new(t2, 96, 90000);
    uint32_t ssrc2 = Compy_RtpTransport_get_ssrc(rtp2);
    ASSERT(ssrc != ssrc2);

    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp2), drop);
    close(fds2[1]);
    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    PASS();
}

TEST rtp_seq_increments(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_Transport t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    ASSERT_EQ(0, Compy_RtpTransport_get_seq(rtp));

    uint8_t payload[4] = {0};
    for (int i = 0; i < 5; i++) {
        int ret __attribute__((unused)) = Compy_RtpTransport_send_packet(
            rtp, Compy_RtpTimestamp_Raw((uint32_t)(i * 100)), false,
            U8Slice99_empty(), U8Slice99_new(payload, sizeof payload));
    }

    ASSERT_EQ(5, Compy_RtpTransport_get_seq(rtp));
    ASSERT_EQ(5, Compy_RtpTransport_get_packet_count(rtp));
    ASSERT_EQ(20, Compy_RtpTransport_get_octet_count(rtp));

    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    PASS();
}

TEST rtcp_sr_reflects_rtp_stats(void) {
    /* RSD sends RTP packets then periodic SR — verify SR has correct stats */
    int rtp_fds[2], rtcp_fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, rtp_fds) == 0);
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, rtcp_fds) == 0);

    Compy_Transport rtp_t = compy_transport_udp(rtp_fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(rtp_t, 96, 90000);

    /* Send 3 RTP packets with 100-byte payloads */
    uint8_t payload[100];
    memset(payload, 0xAA, sizeof payload);
    for (int i = 0; i < 3; i++) {
        int ret __attribute__((unused)) = Compy_RtpTransport_send_packet(
            rtp, Compy_RtpTimestamp_Raw((uint32_t)(i * 3000)), false,
            U8Slice99_empty(), U8Slice99_new(payload, sizeof payload));
        /* Drain the RTP socket */
        uint8_t drain[256];
        recv(rtp_fds[1], drain, sizeof drain, MSG_DONTWAIT);
    }

    Compy_Transport rtcp_t = compy_transport_udp(rtcp_fds[0]);
    Compy_Rtcp *rtcp = Compy_Rtcp_new(rtp, rtcp_t, "raptor@camera");

    ASSERT_EQ(0, Compy_Rtcp_send_sr(rtcp));

    uint8_t sr_buf[COMPY_RTCP_MAX_PACKET_SIZE];
    ssize_t n = read(rtcp_fds[1], sr_buf, sizeof sr_buf);
    ASSERT(n > 28);

    /* Verify SSRC in SR matches RTP SSRC */
    uint32_t sr_ssrc;
    memcpy(&sr_ssrc, sr_buf + 4, 4);
    sr_ssrc = ntohl(sr_ssrc);
    ASSERT_EQ(Compy_RtpTransport_get_ssrc(rtp), sr_ssrc);

    /* Verify packet count (bytes 20-23 of SR) */
    uint32_t pkt_count;
    memcpy(&pkt_count, sr_buf + 20, 4);
    pkt_count = ntohl(pkt_count);
    ASSERT_EQ(3, pkt_count);

    /* Verify octet count (bytes 24-27 of SR) */
    uint32_t octet_count;
    memcpy(&octet_count, sr_buf + 24, 4);
    octet_count = ntohl(octet_count);
    ASSERT_EQ(300, octet_count);

    VCALL(DYN(Compy_Rtcp, Compy_Droppable, rtcp), drop);
    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(rtp_fds[1]);
    close(rtcp_fds[1]);
    PASS();
}

TEST rtp_extension_wire_format(void) {
    /* RWD uses RFC 8285 one-byte header extensions for WebRTC */
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_Transport t = compy_transport_udp(fds[0]);
    Compy_RtpTransport *rtp = Compy_RtpTransport_new(t, 96, 90000);

    /* Set a one-byte extension: ID=1, 2-byte value */
    uint8_t ext_val[2] = {0xAB, 0xCD};
    Compy_RtpTransport_set_extension(rtp, 1, ext_val, 2);

    uint8_t payload[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    int ret __attribute__((unused)) = Compy_RtpTransport_send_packet(
        rtp, Compy_RtpTimestamp_Raw(1000), true, U8Slice99_empty(),
        U8Slice99_new(payload, sizeof payload));

    uint8_t buf[256];
    ssize_t n = recv(fds[1], buf, sizeof buf, MSG_DONTWAIT);
    ASSERT(n > 0);

    /* Byte 0: V=2, P=0, X=1, CC=0 → 0x90 */
    ASSERT_EQ(0x90, buf[0]);

    /* Extension profile at bytes 12-13: 0xBEDE (RFC 8285 one-byte) */
    ASSERT_EQ(0xBE, buf[12]);
    ASSERT_EQ(0xDE, buf[13]);

    /* Extension length at bytes 14-15: 1 (one 32-bit word) */
    ASSERT_EQ(0x00, buf[14]);
    ASSERT_EQ(0x01, buf[15]);

    /* Extension data at byte 16: ID=1, L=1 (len-1) → 0x11 */
    ASSERT_EQ(0x11, buf[16]);
    /* Extension value at bytes 17-18 */
    ASSERT_EQ(0xAB, buf[17]);
    ASSERT_EQ(0xCD, buf[18]);

    /* Payload starts at byte 20 (12 header + 4 ext hdr + 4 ext data) */
    ASSERT_MEM_EQ(payload, buf + 20, 8);

    VCALL(DYN(Compy_RtpTransport, Compy_Droppable, rtp), drop);
    close(fds[1]);
    PASS();
}

SUITE(rtcp) {
    RUN_TEST(rtcp_send_sr);
    RUN_TEST(rtcp_send_bye);
    RUN_TEST(rtcp_handle_rr);
    RUN_TEST(rtp_stats_tracking);
    RUN_TEST(rtp_ssrc_nonzero_entropy);
    RUN_TEST(rtp_seq_increments);
    RUN_TEST(rtcp_sr_reflects_rtp_stats);
    RUN_TEST(rtp_extension_wire_format);
}
