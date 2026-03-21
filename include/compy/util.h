/**
 * @file
 * @brief Utilitary stuff.
 */

#pragma once

#include <compy/option.h>
#include <compy/types/error.h>
#include <compy/types/header_map.h>

#include <stdbool.h>
#include <stdint.h>

#include <slice99.h>

/* Forward declaration to avoid circular include with context.h */
typedef struct Compy_Context Compy_Context;

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

    /**
     * The `server_port` parameter, if present.
     */
    Compy_PortPairOption server_port;
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
 * The ONVIF backchannel feature tag.
 *
 * @see <https://www.onvif.org/specs/stream/ONVIF-Streaming-Spec.pdf> Section
 * 5.3.1
 */
#define COMPY_REQUIRE_ONVIF_BACKCHANNEL                                        \
    (CharSlice99_from_str("www.onvif.org/ver20/backchannel"))

/**
 * Checks whether a `Require` header is present in @p headers and contains
 * the feature tag @p tag.
 *
 * The `Require` header value is compared as a whole against @p tag (not
 * comma-separated). For multiple tags, check each separately.
 *
 * @param[in] headers The request header map.
 * @param[in] tag The feature tag to look for.
 *
 * @return `true` if the `Require` header is present and matches @p tag.
 *
 * @pre `headers != NULL`
 */
bool compy_require_has_tag(
    const Compy_HeaderMap *restrict headers,
    CharSlice99 tag) COMPY_PRIV_MUST_USE;

/**
 * Responds with `551 Option not supported` and an `Unsupported` header
 * listing @p tag.
 *
 * Use this when a request contains a `Require` header with a feature tag
 * the server does not understand, per RFC 2326 Section 12.32.
 *
 * @param[in] ctx The request context.
 * @param[in] tag The unsupported feature tag value.
 *
 * @pre `ctx != NULL`
 */
void compy_respond_option_not_supported(
    Compy_Context *ctx, CharSlice99 tag);

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
