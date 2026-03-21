#include <compy/util.h>

#include <inttypes.h>
#include <stdint.h>

#include <greatest.h>

TEST parse_transport_config(void) {
    Compy_TransportConfig config;
    memset(&config, '\0', sizeof config);

    int ret = compy_parse_transport(
        &config,
        CharSlice99_from_str(
            "RTP/AVP/UDP;unicast;client_port=3056-3057;interleaved=4-5"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(Compy_LowerTransport_UDP, config.lower);
    ASSERT(config.unicast);
    ASSERT(!config.multicast);

    match(config.interleaved) {
        of(Compy_ChannelPair_Some, val) {
            ASSERT_EQ(4, val->rtp_channel);
            ASSERT_EQ(5, val->rtcp_channel);
        }
        otherwise FAIL();
    }

    match(config.client_port) {
        of(Compy_PortPair_Some, val) {
            ASSERT_EQ(3056, val->rtp_port);
            ASSERT_EQ(3057, val->rtcp_port);
        }
        otherwise FAIL();
    }

    PASS();
}

TEST parse_transport_minimal(void) {
    Compy_TransportConfig config;
    memset(&config, '\0', sizeof config);

#define CHECK_REST                                                             \
    do {                                                                       \
        ASSERT(!config.unicast);                                               \
        ASSERT(!config.multicast);                                             \
        ASSERT(MATCHES(config.interleaved, Compy_ChannelPair_None));        \
        ASSERT(MATCHES(config.client_port, Compy_PortPair_None));           \
    } while (0)

    int ret =
        compy_parse_transport(&config, CharSlice99_from_str("RTP/AVP"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(Compy_LowerTransport_UDP, config.lower);
    CHECK_REST;

    ret =
        compy_parse_transport(&config, CharSlice99_from_str("RTP/AVP/TCP"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(Compy_LowerTransport_TCP, config.lower);
    CHECK_REST;

    ret =
        compy_parse_transport(&config, CharSlice99_from_str("RTP/AVP/UDP"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(Compy_LowerTransport_UDP, config.lower);
    CHECK_REST;

#undef CHECK_REST

    PASS();
}

TEST parse_transport_trailing_semicolon(void) {
    Compy_TransportConfig config;
    memset(&config, '\0', sizeof config);

    int ret = compy_parse_transport(
        &config,
        CharSlice99_from_str(
            "RTP/AVP/UDP;unicast;client_port=3056-3057;interleaved=4-5;"));
    ASSERT_EQ(0, ret);
    ASSERT_EQ(Compy_LowerTransport_UDP, config.lower);
    ASSERT(config.unicast);
    ASSERT(!config.multicast);

    match(config.interleaved) {
        of(Compy_ChannelPair_Some, val) {
            ASSERT_EQ(4, val->rtp_channel);
            ASSERT_EQ(5, val->rtcp_channel);
        }
        otherwise FAIL();
    }

    match(config.client_port) {
        of(Compy_PortPair_Some, val) {
            ASSERT_EQ(3056, val->rtp_port);
            ASSERT_EQ(3057, val->rtcp_port);
        }
        otherwise FAIL();
    }

    PASS();
}

TEST interleaved_header(void) {
    uint8_t channel_id = 123;
    uint16_t payload_len = 54321;

    const uint32_t binary =
        compy_interleaved_header(channel_id, payload_len);

    ASSERT_EQ(0xd4317b24, binary);

    PASS();
}

TEST parse_interleaved_header(void) {
    const uint8_t data[] = {'$', 42, 0b00110000, 0b00111001};
    uint8_t channel_id = 0;
    uint16_t payload_len = 0;

    compy_parse_interleaved_header(data, &channel_id, &payload_len);

    ASSERT_EQ_FMT(42, channel_id, "%" PRIu8);
    ASSERT_EQ_FMT(12345, payload_len, "%" PRIu16);

    PASS();
}

SUITE(util) {
    RUN_TEST(parse_transport_config);
    RUN_TEST(parse_transport_minimal);
    RUN_TEST(parse_transport_trailing_semicolon);
    RUN_TEST(interleaved_header);
    RUN_TEST(parse_interleaved_header);
}
