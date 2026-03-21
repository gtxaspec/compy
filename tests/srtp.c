#include <compy/transport.h>

#include <compy/priv/base64.h>
#include <compy/priv/crypto.h>

#include <greatest.h>

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>

/* --- SRTP key derivation + crypto attr tests --- */

TEST srtp_generate_key_nonzero(void) {
    Compy_SrtpKeyMaterial k1, k2;
    ASSERT_EQ(0, compy_srtp_generate_key(&k1));
    ASSERT_EQ(0, compy_srtp_generate_key(&k2));

    /* Two calls should produce different keys */
    ASSERT_FALSE(memcmp(&k1, &k2, sizeof k1) == 0);

    /* Key should not be all zeros */
    uint8_t zeros[16] = {0};
    ASSERT_FALSE(memcmp(k1.master_key, zeros, 16) == 0);

    PASS();
}

TEST srtp_crypto_attr_format(void) {
    Compy_SrtpKeyMaterial key = {
        .master_key =
            {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
             0x0C, 0x0D, 0x0E, 0x0F, 0x10},
        .master_salt =
            {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
             0x1C, 0x1D, 0x1E},
    };

    char buf[128];
    int ret = compy_srtp_format_crypto_attr(
        buf, sizeof buf, 1, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);
    ASSERT(ret > 0);

    /* Should start with "1 AES_CM_128_HMAC_SHA1_80 inline:" */
    ASSERT(strstr(buf, "1 AES_CM_128_HMAC_SHA1_80 inline:") == buf);

    PASS();
}

TEST srtp_crypto_attr_roundtrip(void) {
    Compy_SrtpKeyMaterial key_in;
    ASSERT_EQ(0, compy_srtp_generate_key(&key_in));

    char buf[128];
    int ret = compy_srtp_format_crypto_attr(
        buf, sizeof buf, 1, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key_in);
    ASSERT(ret > 0);

    Compy_SrtpSuite suite_out;
    Compy_SrtpKeyMaterial key_out;
    ASSERT_EQ(0, compy_srtp_parse_crypto_attr(buf, &suite_out, &key_out));

    ASSERT_EQ(Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, suite_out);
    ASSERT_MEM_EQ(key_in.master_key, key_out.master_key, 16);
    ASSERT_MEM_EQ(key_in.master_salt, key_out.master_salt, 14);

    PASS();
}

TEST srtp_crypto_attr_parse_invalid(void) {
    Compy_SrtpSuite suite;
    Compy_SrtpKeyMaterial key;

    ASSERT_EQ(-1, compy_srtp_parse_crypto_attr("garbage", &suite, &key));
    ASSERT_EQ(
        -1, compy_srtp_parse_crypto_attr(
                "1 UNKNOWN_SUITE inline:AAAA", &suite, &key));

    PASS();
}

TEST srtp_crypto_attr_32(void) {
    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    char buf[128];
    int ret = compy_srtp_format_crypto_attr(
        buf, sizeof buf, 2, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32, &key);
    ASSERT(ret > 0);
    ASSERT(strstr(buf, "AES_CM_128_HMAC_SHA1_32") != NULL);

    Compy_SrtpSuite suite;
    Compy_SrtpKeyMaterial key_out;
    ASSERT_EQ(0, compy_srtp_parse_crypto_attr(buf, &suite, &key_out));
    ASSERT_EQ(Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32, suite);

    PASS();
}

/* --- SRTP transport encryption tests --- */

TEST srtp_transport_encrypts(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtp = compy_transport_srtp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Build a minimal RTP packet: 12-byte header + 20-byte payload */
    uint8_t rtp_packet[32];
    memset(rtp_packet, 0, sizeof rtp_packet);
    rtp_packet[0] = 0x80; /* V=2, P=0, X=0, CC=0 */
    rtp_packet[1] = 96;   /* PT=96 */
    rtp_packet[2] = 0;    /* Seq MSB */
    rtp_packet[3] = 1;    /* Seq LSB */
    /* Timestamp: bytes 4-7 = 0 */
    /* SSRC: bytes 8-11 */
    rtp_packet[8] = 0xDE;
    rtp_packet[9] = 0xAD;
    rtp_packet[10] = 0xBE;
    rtp_packet[11] = 0xEF;
    /* Payload: 20 bytes of 0xAA */
    memset(rtp_packet + 12, 0xAA, 20);

    struct iovec bufs[] = {
        {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);

    int ret = VCALL(srtp, transmit, slices);
    ASSERT_EQ(0, ret);

    /* Read encrypted packet */
    uint8_t recv_buf[256];
    ssize_t n = recv(fds[1], recv_buf, sizeof recv_buf, MSG_DONTWAIT);
    ASSERT(n > 0);

    /* Encrypted packet should be larger (auth tag appended) */
    ASSERT_EQ((ssize_t)(sizeof rtp_packet + 10), n); /* +10 for HMAC-SHA1-80 */

    /* RTP header (first 12 bytes) should be unchanged */
    ASSERT_MEM_EQ(rtp_packet, recv_buf, 12);

    /* Payload should be encrypted (different from original) */
    uint8_t original_payload[20];
    memset(original_payload, 0xAA, 20);
    ASSERT_FALSE(memcmp(recv_buf + 12, original_payload, 20) == 0);

    VCALL_SUPER(srtp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

TEST srtp_transport_different_keys_different_output(void) {
    Compy_SrtpKeyMaterial key1, key2;
    ASSERT_EQ(0, compy_srtp_generate_key(&key1));
    ASSERT_EQ(0, compy_srtp_generate_key(&key2));

    /* Same plaintext RTP packet */
    uint8_t rtp_packet[32];
    memset(rtp_packet, 0, sizeof rtp_packet);
    rtp_packet[0] = 0x80;
    rtp_packet[1] = 96;
    rtp_packet[3] = 1;
    rtp_packet[8] = 0x12;
    rtp_packet[9] = 0x34;
    rtp_packet[10] = 0x56;
    rtp_packet[11] = 0x78;
    memset(rtp_packet + 12, 0xBB, 20);

    uint8_t out1[256], out2[256];
    ssize_t n1, n2;

    /* Encrypt with key1 */
    {
        int fds[2];
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds);
        Compy_Transport udp = compy_transport_udp(fds[0]);
        Compy_Transport srtp = compy_transport_srtp(
            udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key1);

        struct iovec iov1[] = {
            {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet}};
        Compy_IoVecSlice s1 = Slice99_typed_from_array(iov1);
        int ret __attribute__((unused)) = VCALL(srtp, transmit, s1);
        n1 = recv(fds[1], out1, sizeof out1, MSG_DONTWAIT);
        VCALL_SUPER(srtp, Compy_Droppable, drop);
        close(fds[1]);
    }

    /* Encrypt with key2 */
    {
        int fds[2];
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds);
        Compy_Transport udp = compy_transport_udp(fds[0]);
        Compy_Transport srtp = compy_transport_srtp(
            udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key2);

        struct iovec iov2[] = {
            {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet}};
        Compy_IoVecSlice s2 = Slice99_typed_from_array(iov2);
        int ret __attribute__((unused)) = VCALL(srtp, transmit, s2);
        n2 = recv(fds[1], out2, sizeof out2, MSG_DONTWAIT);
        VCALL_SUPER(srtp, Compy_Droppable, drop);
        close(fds[1]);
    }

    ASSERT_EQ(n1, n2);
    /* Encrypted payloads should differ (different keys) */
    ASSERT_FALSE(memcmp(out1 + 12, out2 + 12, 20) == 0);

    PASS();
}

TEST srtcp_transport_encrypts(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtcp = compy_transport_srtcp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Build a minimal RTCP SR packet: header(4) + SSRC(4) + sender info(20)
     * = 28 bytes */
    uint8_t rtcp_packet[28];
    memset(rtcp_packet, 0, sizeof rtcp_packet);
    rtcp_packet[0] = 0x80; /* V=2, P=0, RC=0 */
    rtcp_packet[1] = 200;  /* PT=SR */
    rtcp_packet[2] = 0;    /* Length MSB */
    rtcp_packet[3] = 6;    /* Length LSB (6 32-bit words after header) */
    /* SSRC */
    rtcp_packet[4] = 0xDE;
    rtcp_packet[5] = 0xAD;
    rtcp_packet[6] = 0xBE;
    rtcp_packet[7] = 0xEF;
    /* Sender info: 20 bytes of 0xCC */
    memset(rtcp_packet + 8, 0xCC, 20);

    struct iovec bufs[] = {
        {.iov_base = rtcp_packet, .iov_len = sizeof rtcp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);

    int ret = VCALL(srtcp, transmit, slices);
    ASSERT_EQ(0, ret);

    uint8_t recv_buf[256];
    ssize_t n = recv(fds[1], recv_buf, sizeof recv_buf, MSG_DONTWAIT);
    ASSERT(n > 0);

    /* SRTCP packet = original(28) + SRTCP index(4) + auth tag(10) = 42 */
    ASSERT_EQ(42, (int)n);

    /* First 8 bytes (header + SSRC) should be unencrypted */
    ASSERT_MEM_EQ(rtcp_packet, recv_buf, 8);

    /* Sender info should be encrypted (different from original) */
    uint8_t original_info[20];
    memset(original_info, 0xCC, 20);
    ASSERT_FALSE(memcmp(recv_buf + 8, original_info, 20) == 0);

    /* SRTCP index should have E-flag set (bit 31) */
    uint32_t srtcp_idx;
    memcpy(&srtcp_idx, recv_buf + 28, 4);
    srtcp_idx = ntohl(srtcp_idx);
    ASSERT(srtcp_idx & 0x80000000);              /* E-flag set */
    ASSERT_EQ(0, (int)(srtcp_idx & 0x7FFFFFFF)); /* index = 0 */

    VCALL_SUPER(srtcp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

TEST srtcp_index_increments(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtcp = compy_transport_srtcp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    uint8_t rtcp_packet[28];
    memset(rtcp_packet, 0, sizeof rtcp_packet);
    rtcp_packet[0] = 0x80;
    rtcp_packet[1] = 200;
    rtcp_packet[3] = 6;
    rtcp_packet[4] = 0x12;
    rtcp_packet[5] = 0x34;
    rtcp_packet[6] = 0x56;
    rtcp_packet[7] = 0x78;

    /* Send two packets */
    for (int pkt = 0; pkt < 2; pkt++) {
        struct iovec bufs[] = {
            {.iov_base = rtcp_packet, .iov_len = sizeof rtcp_packet},
        };
        Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);
        int ret __attribute__((unused)) = VCALL(srtcp, transmit, slices);
    }

    /* Read both and check SRTCP indices */
    uint8_t buf1[256], buf2[256];
    ssize_t n1 = recv(fds[1], buf1, sizeof buf1, MSG_DONTWAIT);
    ssize_t n2 = recv(fds[1], buf2, sizeof buf2, MSG_DONTWAIT);
    ASSERT(n1 > 0);
    ASSERT(n2 > 0);

    uint32_t idx1, idx2;
    memcpy(&idx1, buf1 + 28, 4);
    memcpy(&idx2, buf2 + 28, 4);
    idx1 = ntohl(idx1) & 0x7FFFFFFF;
    idx2 = ntohl(idx2) & 0x7FFFFFFF;

    ASSERT_EQ(0, (int)idx1);
    ASSERT_EQ(1, (int)idx2);

    VCALL_SUPER(srtcp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

/* --- SRTP/SRTCP decrypt tests --- */

TEST srtp_encrypt_decrypt_roundtrip(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtp = compy_transport_srtp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    Compy_SrtpRecvCtx *recv_ctx =
        compy_srtp_recv_new(Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Build RTP packet */
    uint8_t rtp_packet[32];
    memset(rtp_packet, 0, sizeof rtp_packet);
    rtp_packet[0] = 0x80;
    rtp_packet[1] = 96;
    rtp_packet[3] = 1;
    rtp_packet[8] = 0xDE;
    rtp_packet[9] = 0xAD;
    rtp_packet[10] = 0xBE;
    rtp_packet[11] = 0xEF;
    memset(rtp_packet + 12, 0x42, 20);

    struct iovec bufs[] = {
        {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);
    int ret __attribute__((unused)) = VCALL(srtp, transmit, slices);

    /* Receive encrypted packet */
    uint8_t encrypted[256];
    ssize_t n = recv(fds[1], encrypted, sizeof encrypted, MSG_DONTWAIT);
    ASSERT(n > 0);

    /* Decrypt */
    size_t dec_len = (size_t)n;
    ASSERT_EQ(0, compy_srtp_recv_unprotect(recv_ctx, encrypted, &dec_len));

    /* Should match original */
    ASSERT_EQ(sizeof rtp_packet, dec_len);
    ASSERT_MEM_EQ(rtp_packet, encrypted, sizeof rtp_packet);

    compy_srtp_recv_free(recv_ctx);
    VCALL_SUPER(srtp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

TEST srtp_decrypt_bad_auth_fails(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtp = compy_transport_srtp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Use a different key for receive — auth should fail */
    Compy_SrtpKeyMaterial wrong_key;
    ASSERT_EQ(0, compy_srtp_generate_key(&wrong_key));
    Compy_SrtpRecvCtx *recv_ctx = compy_srtp_recv_new(
        Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &wrong_key);

    uint8_t rtp_packet[32];
    memset(rtp_packet, 0, sizeof rtp_packet);
    rtp_packet[0] = 0x80;
    rtp_packet[1] = 96;
    rtp_packet[3] = 1;
    rtp_packet[8] = 0x12;
    rtp_packet[9] = 0x34;
    rtp_packet[10] = 0x56;
    rtp_packet[11] = 0x78;
    memset(rtp_packet + 12, 0xAA, 20);

    struct iovec bufs[] = {
        {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);
    int ret __attribute__((unused)) = VCALL(srtp, transmit, slices);

    uint8_t encrypted[256];
    ssize_t n = recv(fds[1], encrypted, sizeof encrypted, MSG_DONTWAIT);
    ASSERT(n > 0);

    size_t dec_len = (size_t)n;
    ASSERT_EQ(-1, compy_srtp_recv_unprotect(recv_ctx, encrypted, &dec_len));

    compy_srtp_recv_free(recv_ctx);
    VCALL_SUPER(srtp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

TEST srtcp_encrypt_decrypt_roundtrip(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtcp = compy_transport_srtcp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    Compy_SrtpRecvCtx *recv_ctx =
        compy_srtp_recv_new(Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Build RTCP SR packet */
    uint8_t rtcp_packet[28];
    memset(rtcp_packet, 0, sizeof rtcp_packet);
    rtcp_packet[0] = 0x80;
    rtcp_packet[1] = 200;
    rtcp_packet[3] = 6;
    rtcp_packet[4] = 0xDE;
    rtcp_packet[5] = 0xAD;
    rtcp_packet[6] = 0xBE;
    rtcp_packet[7] = 0xEF;
    memset(rtcp_packet + 8, 0x55, 20);

    struct iovec bufs[] = {
        {.iov_base = rtcp_packet, .iov_len = sizeof rtcp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);
    int ret __attribute__((unused)) = VCALL(srtcp, transmit, slices);

    uint8_t encrypted[256];
    ssize_t n = recv(fds[1], encrypted, sizeof encrypted, MSG_DONTWAIT);
    ASSERT(n > 0);

    size_t dec_len = (size_t)n;
    ASSERT_EQ(0, compy_srtcp_recv_unprotect(recv_ctx, encrypted, &dec_len));

    /* Should match original RTCP packet */
    ASSERT_EQ(sizeof rtcp_packet, dec_len);
    ASSERT_MEM_EQ(rtcp_packet, encrypted, sizeof rtcp_packet);

    compy_srtp_recv_free(recv_ctx);
    VCALL_SUPER(srtcp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

TEST srtp_replay_rejected(void) {
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

    Compy_SrtpKeyMaterial key;
    ASSERT_EQ(0, compy_srtp_generate_key(&key));

    Compy_Transport udp = compy_transport_udp(fds[0]);
    Compy_Transport srtp = compy_transport_srtp(
        udp, Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    Compy_SrtpRecvCtx *recv_ctx =
        compy_srtp_recv_new(Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80, &key);

    /* Send one packet */
    uint8_t rtp_packet[32];
    memset(rtp_packet, 0, sizeof rtp_packet);
    rtp_packet[0] = 0x80;
    rtp_packet[1] = 96;
    rtp_packet[3] = 1; /* seq = 1 */
    rtp_packet[8] = 0xAA;
    rtp_packet[9] = 0xBB;
    rtp_packet[10] = 0xCC;
    rtp_packet[11] = 0xDD;
    memset(rtp_packet + 12, 0x77, 20);

    struct iovec bufs[] = {
        {.iov_base = rtp_packet, .iov_len = sizeof rtp_packet},
    };
    Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);
    int ret __attribute__((unused)) = VCALL(srtp, transmit, slices);

    /* Receive and decrypt first copy — should succeed */
    uint8_t encrypted[256];
    ssize_t n = recv(fds[1], encrypted, sizeof encrypted, MSG_DONTWAIT);
    ASSERT(n > 0);

    /* Save a copy for replay */
    uint8_t replay[256];
    memcpy(replay, encrypted, (size_t)n);

    size_t dec_len = (size_t)n;
    ASSERT_EQ(0, compy_srtp_recv_unprotect(recv_ctx, encrypted, &dec_len));

    /* Try to decrypt the same packet again — should be rejected as replay */
    size_t replay_len = (size_t)n;
    ASSERT_EQ(-1, compy_srtp_recv_unprotect(recv_ctx, replay, &replay_len));

    compy_srtp_recv_free(recv_ctx);
    VCALL_SUPER(srtp, Compy_Droppable, drop);
    close(fds[1]);
    PASS();
}

SUITE(srtp) {
    RUN_TEST(srtp_generate_key_nonzero);
    RUN_TEST(srtp_crypto_attr_format);
    RUN_TEST(srtp_crypto_attr_roundtrip);
    RUN_TEST(srtp_crypto_attr_parse_invalid);
    RUN_TEST(srtp_crypto_attr_32);
    RUN_TEST(srtp_transport_encrypts);
    RUN_TEST(srtp_transport_different_keys_different_output);
    RUN_TEST(srtcp_transport_encrypts);
    RUN_TEST(srtcp_index_increments);
    RUN_TEST(srtp_encrypt_decrypt_roundtrip);
    RUN_TEST(srtp_decrypt_bad_auth_fails);
    RUN_TEST(srtcp_encrypt_decrypt_roundtrip);
    RUN_TEST(srtp_replay_rejected);
}
