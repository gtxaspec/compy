/**
 * @file
 * @brief <a href="https://datatracker.ietf.org/doc/html/rfc4566">RFC
 * 4566</a>-compliant SDP implementation.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/writer.h>

#include <slice99.h>

/**
 * An SDP type (one character).
 */
typedef char Compy_SdpType;

/**
 * An SDP line.
 */
typedef struct {
    /**
     * The type of this line.
     */
    Compy_SdpType ty;

    /**
     * The value of this line.
     */
    CharSlice99 value;
} Compy_SdpLine;

/**
 * Serialises @p self into @p w.
 *
 * @param[in] self The instance to be serialised.
 * @param[in] w The writer to be provided with serialised data.
 *
 * @return The number of bytes written or a negative value on error.
 *
 * @pre `self != NULL`
 * @pre `w.self && w.vptr`
 */
ssize_t Compy_SdpLine_serialize(
    const Compy_SdpLine *restrict self, Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Printfs a single SDP line to @p w.
 *
 * @param[out] w The writer to be provided with SDP data.
 * @param[in] ty The type of the SDP line.
 * @param[in] fmt The `printf`-like format string.
 *
 * @return The number of bytes written or a negative value on error.
 *
 * @pre `w.self && w.vptr`
 * @pre `fmt != NULL`
 */
ssize_t compy_sdp_printf(
    Compy_Writer w, Compy_SdpType ty, const char *restrict fmt,
    ...) COMPY_PRIV_MUST_USE COMPY_PRIV_GCC_ATTR(format(printf, 3, 4));

/**
 * Writes a sequence of SDP lines to @p w.
 *
 * @param[out] ret The variable name of type `ssize_t` that will be incremented
 * with a number of bytes written to @p w.
 * @param[out] w The variable name of type `Compy_Writer` to be provided with
 * SDP data.
 *
 * The rest of arguments represent a non-empty sequence of comma-separated
 * tuples `(ty, fmt, ...)`, where
 *  - `ty` is an SDP line type (such as #COMPY_SDP_VERSION),
 *  - `fmt` is a `printf`-like format string,
 *  - and `...` are the `fmt` parameters (may be omitted).
 *
 * Under the hood, #COMPY_SDP_DESCRIBE just generates a #compy_sdp_printf
 * function invocation for each variadic argument.
 *
 * ## Examples
 *
 * @code
 * char buffer[256] = {0};
 * const Compy_Writer sdp = compy_string_writer(buffer);
 * ssize_t ret = 0;
 *
 * COMPY_SDP_DESCRIBE(
 *     ret, sdp,
 *     (COMPY_SDP_VERSION, "0"),
 *     (COMPY_SDP_ORIGIN, "Compy 3855320066 3855320129 IN IP4 0.0.0.0"),
 *     (COMPY_SDP_SESSION_NAME, "Compy test"),
 *     (COMPY_SDP_CONNECTION, "IN IP4 %s", SERVER_IP_ADDR),
 *     (COMPY_SDP_TIME, "0 0"),
 *     (COMPY_SDP_MEDIA, "audio %d RTP/AVP %d", AUDIO_PORT,
 * AUDIO_RTP_PAYLOAD_TYPE),
 *     (COMPY_SDP_ATTR, "control:audio"));
 *
 * const char *expected =
 *     "v=0\r\n"
 *     "o=Compy 3855320066 3855320129 IN IP4 0.0.0.0\r\n"
 *     "s=Compy test\r\n"
 *     "c=IN IP4 0.0.0.0\r\n"
 *     "t=0 0\r\n"
 *     "m=audio 123 RTP/AVP 456\r\n"
 *     "a=control:audio\r\n";
 *
 * assert((ssize_t)strlen(expected) == ret);
 * assert(strcmp(expected, buffer) == 0);
 * @endcode
 */
#define COMPY_SDP_DESCRIBE(ret, w, ...)                                        \
    COMPY_PRIV_SDP_DESCRIBE(ret, w, __VA_ARGS__)

#ifndef DOXYGEN_IGNORE

#include <metalang99.h>

#define COMPY_PRIV_SDP_DESCRIBE(ret, w, ...)                                   \
    do {                                                                       \
        ssize_t compy_priv_sdp_ret = 0;                                        \
        ML99_EVAL(ML99_variadicsForEach(                                       \
            ML99_compose(                                                      \
                ML99_appl(v(COMPY_PRIV_genSdpPrintf), v(ret, w)),              \
                v(ML99_untuple)),                                              \
            v(__VA_ARGS__)))                                                   \
    } while (0)

#define COMPY_PRIV_genSdpPrintf_IMPL(ret, w, ty, ...)                          \
    v({                                                                        \
        compy_priv_sdp_ret = compy_sdp_printf(w, ty, __VA_ARGS__);             \
        if (compy_priv_sdp_ret < 0) {                                          \
            break;                                                             \
        }                                                                      \
        ret += compy_priv_sdp_ret;                                             \
    })

#define COMPY_PRIV_genSdpPrintf_ARITY 2

#endif // DOXYGEN_IGNORE

/**
 * Protocol Version (`v=`).
 */
#define COMPY_SDP_VERSION 'v'

/**
 * Origin (`o=`).
 */
#define COMPY_SDP_ORIGIN 'o'

/**
 * Session Name (`s=`).
 */
#define COMPY_SDP_SESSION_NAME 's'

/**
 * Session Information (`i=`).
 */
#define COMPY_SDP_INFO 'i'

/**
 * URI (`u=`).
 */
#define COMPY_SDP_URI 'u'

/**
 * Email Address (`e=`).
 */
#define COMPY_SDP_EMAIL 'e'

/**
 * Phone Number (`p=`).
 */
#define COMPY_SDP_PHONE 'p'

/**
 * Connection Data (`c=`).
 */
#define COMPY_SDP_CONNECTION 'c'

/**
 * Bandwidth (`b=`).
 */
#define COMPY_SDP_BANDWIDTH 'b'

/**
 * Timing (`t=`).
 */
#define COMPY_SDP_TIME 't'

/**
 * Repeat Times (`r=`).
 */
#define COMPY_SDP_REPEAT 'r'

/**
 * Time Zones (`z=`).
 */
#define COMPY_SDP_TIME_ZONES 'z'

/**
 * Encryption Keys (`k=`).
 */
#define COMPY_SDP_ENCRYPTION_KEYS 'k'

/**
 * Attributes (`a=`).
 */
#define COMPY_SDP_ATTR 'a'

/**
 * Media Descriptions (`m=`).
 */
#define COMPY_SDP_MEDIA 'm'
