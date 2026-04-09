#include <compy/transport.h>

#include <greatest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>

#define DATA_0 "abc"
#define DATA_1 "defghi"

static enum greatest_test_res test_transport(
    Compy_Transport t, int read_fd, size_t len,
    const char expected[restrict static len]) {
    struct iovec bufs[] = {
        {.iov_base = DATA_0, .iov_len = sizeof((char[]){DATA_0}) - 1},
        {.iov_base = DATA_1, .iov_len = sizeof((char[]){DATA_1}) - 1},
    };

    const Compy_IoVecSlice slices = Slice99_typed_from_array(bufs);

    const ssize_t ret = VCALL(t, transmit, slices);
    ASSERT_EQ(0, ret);

    char *buffer = malloc(len);
    read(read_fd, buffer, len);
    ASSERT_MEM_EQ(expected, buffer, len);
    free(buffer);

    VCALL_SUPER(t, Compy_Droppable, drop);

    PASS();
}

TEST check_tcp(void) {
    int fds[2];
    const bool socketpair_ok = socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0;
    ASSERT(socketpair_ok);

    const uint8_t chn_id = 123;

    Compy_Transport tcp =
        compy_transport_tcp(compy_fd_writer(&fds[0]), chn_id, 0);

    const char total_len = strlen(DATA_0) + strlen(DATA_1);
    const char expected[] = {'$', chn_id, 0,   total_len, 'a', 'b', 'c',
                             'd', 'e',    'f', 'g',       'h', 'i'};

    CHECK_CALL(test_transport(tcp, fds[1], sizeof expected, expected));

    close(fds[0]);
    close(fds[1]);
    PASS();
}

TEST check_udp(void) {
    int fds[2];
    const bool socketpair_ok = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0;
    ASSERT(socketpair_ok);

    Compy_Transport udp = compy_transport_udp(fds[0]);

    char expected[] = {DATA_0 DATA_1};

    CHECK_CALL(test_transport(udp, fds[1], strlen(expected), expected));

    close(fds[0]);
    close(fds[1]);
    PASS();
}

TEST sockaddr_get_ipv4(void) {
    struct sockaddr_storage addr;
    memset(&addr, '\0', sizeof addr);
    addr.ss_family = AF_INET;

    ASSERT_EQ(
        (void *)&((struct sockaddr_in *)&addr)->sin_addr,
        compy_sockaddr_ip((const struct sockaddr *)&addr));

    PASS();
}

TEST sockaddr_get_ipv6(void) {
    struct sockaddr_storage addr;
    memset(&addr, '\0', sizeof addr);
    addr.ss_family = AF_INET6;

    ASSERT_EQ(
        (void *)&((struct sockaddr_in6 *)&addr)->sin6_addr,
        compy_sockaddr_ip((const struct sockaddr *)&addr));

    PASS();
}

TEST sockaddr_get_unknown(void) {
    struct sockaddr_storage addr;
    memset(&addr, '\0', sizeof addr);
    addr.ss_family = AF_UNIX;

    ASSERT_EQ(NULL, compy_sockaddr_ip((const struct sockaddr *)&addr));

    PASS();
}

TEST check_tcp_multi_channel(void) {
    /* RSD uses channel 0/1 for video, 2/3 for audio on same connection */
    int fds[2];
    ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    Compy_Writer w = compy_fd_writer(&fds[0]);

    Compy_Transport video = compy_transport_tcp(w, 0, 0);
    Compy_Transport audio = compy_transport_tcp(w, 2, 0);

    /* Send video frame */
    uint8_t vdata[] = {0xAA, 0xBB, 0xCC};
    struct iovec vbufs[] = {{.iov_base = vdata, .iov_len = sizeof vdata}};
    Compy_IoVecSlice vslice = Slice99_typed_from_array(vbufs);
    ASSERT_EQ(0, VCALL(video, transmit, vslice));

    /* Send audio frame */
    uint8_t adata[] = {0x11, 0x22};
    struct iovec abufs[] = {{.iov_base = adata, .iov_len = sizeof adata}};
    Compy_IoVecSlice aslice = Slice99_typed_from_array(abufs);
    ASSERT_EQ(0, VCALL(audio, transmit, aslice));

    /* Read and verify both frames */
    uint8_t buf[32];
    ssize_t n = read(fds[1], buf, sizeof buf);
    ASSERT(n >= 11); /* 4+3 + 4+2 = 13 bytes total */

    /* Video frame: $ 0x00 0x0003 0xAA 0xBB 0xCC */
    ASSERT_EQ('$', buf[0]);
    ASSERT_EQ(0, buf[1]); /* channel 0 */
    ASSERT_MEM_EQ(vdata, buf + 4, 3);

    /* Audio frame: $ 0x02 0x0002 0x11 0x22 */
    ASSERT_EQ('$', buf[7]);
    ASSERT_EQ(2, buf[8]); /* channel 2 */
    ASSERT_MEM_EQ(adata, buf + 11, 2);

    /* Don't drop video/audio — they share the writer, just free manually */
    free(video.self);
    free(audio.self);
    close(fds[0]);
    close(fds[1]);
    PASS();
}

SUITE(transport) {
    RUN_TEST(check_tcp);
    RUN_TEST(check_udp);
    RUN_TEST(sockaddr_get_ipv4);
    RUN_TEST(sockaddr_get_ipv6);
    RUN_TEST(sockaddr_get_unknown);
    RUN_TEST(check_tcp_multi_channel);
}
