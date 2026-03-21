#include <compy/rtcp.h>
#include <compy/transport.h>
#include <compy/writer.h>

#include <greatest.h>

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

SUITE(rtcp) {
    RUN_TEST(rtcp_send_sr);
    RUN_TEST(rtcp_send_bye);
    RUN_TEST(rtcp_handle_rr);
}
