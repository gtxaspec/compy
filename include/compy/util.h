/**
 * @file
 * @brief Utilitary stuff.
 */

#pragma once

#include <compy/option.h>
#include <compy/types/error.h>

#include <stdint.h>

#include <slice99.h>

/**
 * Carriage-return + new-line represented as a data slice.
 */
#define COMPY_CRLF (CharSlice99_from_str("\r\n"))

/**
 * The default RTSP port.
 */
#define COMPY_DEFAULT_PORT 554

/**
 * An RTSP lower transport.
 */
typedef enum {
    /**
     * TCP.
     */
    Compy_LowerTransport_TCP,

    /**
     * UDP.
     */
    Compy_LowerTransport_UDP,
} Compy_LowerTransport;

/**
 * Converts @p self to a string representation (`"TCP"` for
 * #Compy_LowerTransport_TCP and `"UDP"` for #Compy_LowerTransport_UDP).
 */
const char *Compy_LowerTransport_str(Compy_LowerTransport self);

/**
 * An RTP/RTCP port pair specified as a range, e.g., `client_port=3456-3457`.
 */
typedef struct {
    /**
     * The RTP port.
     */
    uint16_t rtp_port;

    /**
     * The RTCP port.
     */
    uint16_t rtcp_port;
} Compy_PortPair;

/**
 * Defines `Compy_PortPairOption`.
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */
COMPY_DEF_OPTION(Compy_PortPair);

/**
 * An RTP/RTCP channel pair specified as a range, e.g., `interleaved=4-5`.
 */
typedef struct {
    /**
     * The RTP channel identifier.
     */
    uint8_t rtp_channel;

    /**
     * The RTCP channel identifier.
     */
    uint8_t rtcp_channel;
} Compy_ChannelPair;

/**
 * Defines `Compy_ChannelPairOption`.
 *
 * See [Datatype99](https://github.com/Hirrolot/datatype99) for the macro usage.
 */
COMPY_DEF_OPTION(Compy_ChannelPair);

/**
 * The RTSP transport configuration.
 *
 * @see <https://datatracker.ietf.org/doc/html/rfc2326#section-12.39>
 */
typedef struct {
    /**
     * The lower level transport (TCP or UDP).
     */
    Compy_LowerTransport lower;

    /**
     * True if the `unicast` parameter is present.
     */
    bool unicast;

    /**
     * True if the `multicast` parameter is present.
     */
    bool multicast;

    /**
     * The `interleaved` parameter, if present.
     */
    Compy_ChannelPairOption interleaved;

    /**
     * The `client_port` parameter, if present.
     */
    Compy_PortPairOption client_port;
} Compy_TransportConfig;

/**
 * Parses the
 * [`Transport`](https://datatracker.ietf.org/doc/html/rfc2326#section-12.39)
 * header.
 *
 * @param[out] config The result of parsing. It remains unchanged on failure.
 * @param[in] header_value The value of the `Transport` header.
 *
 * @return 0 on success, -1 on failure.
 *
 * @pre `config != NULL`
 */
int compy_parse_transport(
    Compy_TransportConfig *restrict config,
    CharSlice99 header_value) COMPY_PRIV_MUST_USE;

/**
 * Returns a four-octet interleaved binary data header.
 *
 * @param[in] channel_id The one-byte channel identifier.
 * @param[in] payload_len The length of the encapsulated binary data
 * (network byte order).
 *
 * @see <https://datatracker.ietf.org/doc/html/rfc2326#section-10.12>
 */
uint32_t compy_interleaved_header(uint8_t channel_id, uint16_t payload_len)
    COMPY_PRIV_MUST_USE;

/**
 * Parses an four-octet interleaved binary data header @p data.
 *
 * @param[in] data The header to parse.
 * @param[out] channel_id The one-byte channel identifier.
 * @param[out] payload_len The length of the encapsulated binary data
 * (host byte order).
 *
 * @pre `channel_id != NULL`
 * @pre `payload_len != NULL`
 *
 * @see <https://datatracker.ietf.org/doc/html/rfc2326#section-10.12>
 */
void compy_parse_interleaved_header(
    const uint8_t data[restrict static 4], uint8_t *restrict channel_id,
    uint16_t *restrict payload_len);
