/**
 * @file
 * @brief Minimal libevent integration for compy.
 *
 * Provides a bufferevent-backed Compy_Writer and a read callback that
 * parses incoming RTSP requests and dispatches them to a Compy_Controller.
 */

#pragma once

#include <compy.h>

#include <event2/bufferevent.h>

/**
 * Creates a Compy_Writer backed by a libevent bufferevent.
 *
 * Supports lock/unlock for TCP interleaved RTP and tracks buffer fill
 * level for backpressure detection.
 *
 * @pre `bev != NULL`
 */
Compy_Writer compy_bev_writer(struct bufferevent *bev);

/**
 * Creates a libevent callback context for dispatching RTSP requests.
 *
 * @param[in] controller The controller to handle incoming requests.
 *
 * @return An opaque context pointer to pass as the bufferevent callback arg.
 */
void *compy_libevent_ctx(Compy_Controller controller);

/**
 * Libevent read callback. Set this as the bufferevent read callback.
 */
void compy_libevent_cb(struct bufferevent *bev, void *arg);

/**
 * Frees the context created by compy_libevent_ctx.
 */
void compy_libevent_ctx_free(void *ctx);
