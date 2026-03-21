/**
 * @file
 * @brief RTCP session management (RFC 3550 Section 6).
 *
 * Provides Sender Report generation, BYE signaling, and incoming Receiver
 * Report parsing. The application is responsible for calling
 * #Compy_Rtcp_send_sr periodically (recommended interval: 5 seconds).
 */

#pragma once

#include <compy/droppable.h>
#include <compy/rtp_transport.h>
#include <compy/transport.h>
#include <compy/types/rtcp.h>

#include <compy/priv/compiler_attrs.h>

typedef struct Compy_Rtcp Compy_Rtcp;

/**
 * Creates a new RTCP session context.
 *
 * @param[in] rtp The RTP transport to read statistics from (not owned).
 * @param[in] rtcp_transport The transport for sending RTCP packets
 * (port+1 or channel+1).
 * @param[in] cname The CNAME identifier for SDES (e.g., "camera@192.168.1.1").
 *
 * @pre `rtp != NULL`
 * @pre `rtcp_transport.self && rtcp_transport.vptr`
 * @pre `cname != NULL`
 */
Compy_Rtcp *Compy_Rtcp_new(
    Compy_RtpTransport *rtp, Compy_Transport rtcp_transport,
    const char *cname) COMPY_PRIV_MUST_USE;

/**
 * Generates and sends a compound SR + SDES packet.
 *
 * Reads current statistics from the associated RTP transport and sends
 * a Sender Report followed by an SDES chunk containing the CNAME.
 *
 * @pre `self != NULL`
 *
 * @return -1 on I/O error, 0 on success.
 */
int Compy_Rtcp_send_sr(Compy_Rtcp *self) COMPY_PRIV_MUST_USE;

/**
 * Generates and sends a BYE packet.
 *
 * @pre `self != NULL`
 *
 * @return -1 on I/O error, 0 on success.
 */
int Compy_Rtcp_send_bye(Compy_Rtcp *self) COMPY_PRIV_MUST_USE;

/**
 * Processes an incoming RTCP packet.
 *
 * Currently handles Receiver Report (RR) packets, storing the first
 * report block for later retrieval via #Compy_Rtcp_get_last_rr.
 *
 * @param[in] self The RTCP session.
 * @param[in] data The raw RTCP packet data.
 * @param[in] len Length of @p data.
 *
 * @pre `self != NULL`
 * @pre `data != NULL`
 *
 * @return 0 on success, -1 on parse error.
 */
int Compy_Rtcp_handle_incoming(
    Compy_Rtcp *self, const uint8_t *data, size_t len);

/**
 * Returns the last received Receiver Report block, or NULL if none received.
 *
 * @pre `self != NULL`
 */
const Compy_RtcpReportBlock *
Compy_Rtcp_get_last_rr(const Compy_Rtcp *self) COMPY_PRIV_MUST_USE;

declImplExtern99(Compy_Droppable, Compy_Rtcp);
