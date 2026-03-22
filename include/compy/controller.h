/**
 * @file
 * @brief An RTSP requests controller.
 */

#pragma once

#include <compy/context.h>
#include <compy/droppable.h>
#include <compy/types/request.h>
#include <compy/writer.h>

#include <stddef.h>

#include <interface99.h>

/**
 * Whether to stop or continue some processing.
 */
typedef enum {
    /**
     * Break processing.
     */
    Compy_ControlFlow_Break,

    /**
     * Continue processing.
     */
    Compy_ControlFlow_Continue,
} Compy_ControlFlow;

/**
 * A controller that handles incoming RTSP requests.
 *
 * All RTSP command handlers accept the following parameters:
 *  - `ctx` -- a request context used to respond to your RTSP client.
 *  - `req` -- a fully parsed request object.
 */
#define Compy_Controller_IFACE                                                 \
                                                                               \
    /*                                                                         \
     * Handles `OPTIONS` as defined in                                         \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.1>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, options, VSelf99, Compy_Context *ctx, const Compy_Request *req)  \
                                                                               \
    /*                                                                         \
     * Handles `DESCRIBE` as defined in                                        \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.2>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, describe, VSelf99, Compy_Context *ctx, const Compy_Request *req) \
                                                                               \
    /*                                                                         \
     * Handles `SETUP` as defined in                                           \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.4>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, setup, VSelf99, Compy_Context *ctx, const Compy_Request *req)    \
                                                                               \
    /*                                                                         \
     * Handles `PLAY` as defined in                                            \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.5>.           \
     */                                                                        \
    vfunc99(void, play, VSelf99, Compy_Context *ctx, const Compy_Request *req) \
                                                                               \
    /*                                                                         \
     * Handles `PAUSE` as defined in                                           \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.6>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, pause_method, VSelf99, Compy_Context *ctx,                       \
        const Compy_Request *req)                                              \
                                                                               \
    /*                                                                         \
     * Handles `TEARDOWN` as defined in                                        \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.7>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, teardown, VSelf99, Compy_Context *ctx, const Compy_Request *req) \
                                                                               \
    /*                                                                         \
     * Handles `GET_PARAMETER` as defined in                                   \
     * <https://datatracker.ietf.org/doc/html/rfc2326#section-10.8>.           \
     */                                                                        \
    vfunc99(                                                                   \
        void, get_parameter, VSelf99, Compy_Context *ctx,                      \
        const Compy_Request *req)                                              \
                                                                               \
    /*                                                                         \
     * Handles a command that is not one of the above.                         \
     */                                                                        \
    vfunc99(                                                                   \
        void, unknown, VSelf99, Compy_Context *ctx, const Compy_Request *req)  \
                                                                               \
    /*                                                                         \
     * A method that is invoked _before_ the actual request handling.          \
     */                                                                        \
    vfunc99(                                                                   \
        Compy_ControlFlow, before, VSelf99, Compy_Context *ctx,                \
        const Compy_Request *req)                                              \
                                                                               \
    /*                                                                         \
     * A method that is invoked _after_ request handling.                      \
     */                                                                        \
    vfunc99(                                                                   \
        void, after, VSelf99, ssize_t ret, Compy_Context *ctx,                 \
        const Compy_Request *req)

/**
 * The superinterfaces of #Compy_Controller_IFACE.
 */
#define Compy_Controller_EXTENDS (Compy_Droppable)

/**
 * Defines the `Compy_Controller` interface.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
interface99(Compy_Controller);

/**
 * Dispatches an incoming request to @p controller.
 *
 * The algorithm is as follows:
 *
 *  1. Setup a request context #Compy_Context. Later you can use it to
 * configure an RTSP response via #compy_header, #compy_body, and similar.
 *  2. Invoke the `before` method of @p controller. Here you should do some
 * preliminary stuff like logging a request or setting up initial response
 * headers via #compy_header. If `before` returns
 * #Compy_ControlFlow_Break, jump to step #4.
 *  3. Invoke a corresponding command handler of @p controller. Here you should
 * handle the request and respond to your client via
 * #compy_respond/#compy_respond_ok or similar.
 *  4. Invoke the `after` method of @p controller. Here you automatically
 * receive the return value of `compy_respond_*` (invoked during one of the
 * previous steps). If it is <0, it means that something bad happened so that
 * the handler has not been able to respond properly.
 *  5. Drop the request context.
 *
 * @param[out] conn The writer to send RTSP responses.
 * @param[in] controller The controller to handle the incoming request @p req.
 * @param[in] req The fully parsed RTSP request object.
 *
 * @pre `conn.self && conn.vptr`
 * @pre `controller.self && controller.vptr`
 * @pre `req != NULL`
 */
void compy_dispatch(
    Compy_Writer conn, Compy_Controller controller,
    const Compy_Request *restrict req);
