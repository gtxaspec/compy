/**
 * @file
 * @brief A request context.
 */

#pragma once

#include <compy/droppable.h>
#include <compy/priv/compiler_attrs.h>
#include <compy/types/message_body.h>
#include <compy/types/status_code.h>
#include <compy/writer.h>

#include <stdarg.h>
#include <stdint.h>

#include <interface99.h>
#include <slice99.h>

/**
 * An opaque type used to conveniently respond to RTSP clients.
 */
typedef struct Compy_Context Compy_Context;

/**
 * Creates a new Compy context.
 *
 * @param[in] w The writer to be provided with the response.
 * @param[in] cseq The sequence number for an RTSP request/response pair.
 *
 * @pre `w.self && w.vptr`
 */
Compy_Context *
Compy_Context_new(Compy_Writer w, uint32_t cseq) COMPY_PRIV_MUST_USE;

/**
 * Retrieves the writer specified in #Compy_Context_new.
 *
 * @pre `ctx != NULL`
 */
Compy_Writer
Compy_Context_get_writer(const Compy_Context *ctx) COMPY_PRIV_MUST_USE;

/**
 * Retrieves `cseq` specified in #Compy_Context_new.
 *
 * @pre `ctx != NULL`
 */
uint32_t
Compy_Context_get_cseq(const Compy_Context *ctx) COMPY_PRIV_MUST_USE;

/**
 * Retrieves the RTSP respond return value.
 *
 * If you have not responded yet, the result is 0.
 *
 * @pre `ctx != NULL`
 */
ssize_t
Compy_Context_get_ret(const Compy_Context *ctx) COMPY_PRIV_MUST_USE;

/**
 * Appends an RTSP header to the request context.
 *
 * @param[out] ctx The request context to modify.
 * @param[in] key The header key.
 * @param[in] fmt The `printf`-like format string (header value).
 * @param[in] list The variadic function arguments.
 *
 * @pre `ctx != NULL`
 * @pre @p ctx must contain strictly less than #COMPY_HEADER_MAP_CAPACITY
 * headers.
 * @pre `fmt != NULL`
 */
void compy_vheader(
    Compy_Context *ctx, CharSlice99 key, const char *restrict fmt,
    va_list list) COMPY_PRIV_GCC_ATTR(format(printf, 3, 0));

/**
 * The #compy_vheader twin.
 */
void compy_header(
    Compy_Context *ctx, CharSlice99 key, const char *restrict fmt, ...)
    COMPY_PRIV_GCC_ATTR(format(printf, 3, 4));

/**
 * Sets an RTSP body in the request context.
 *
 * @param[out] ctx The request context to modify.
 * @param[in] body The RTSP body.
 *
 * @pre `ctx != NULL`
 */
void compy_body(Compy_Context *ctx, Compy_MessageBody body);

/**
 * Writes an RTSP response to the underlying writer.
 *
 * The `CSeq` and `Content-Length` headers will be written automatically as
 * first headers.
 *
 * @param[out] ctx The request context to write the response to.
 * @param[in] code The RTSP status code.
 * @param[in] reason The RTSP reason phrase.
 *
 * @pre `ctx != NULL`
 * @pre @p reason is a null-terminated string.
 *
 * @return The number of bytes written or a negative value on error.
 */
ssize_t compy_respond(
    Compy_Context *ctx, Compy_StatusCode code, const char *reason);

/**
 * A shortcut for `compy_respond(ctx, COMPY_STATUS_OK, "OK")`.
 */
ssize_t compy_respond_ok(Compy_Context *ctx);

/**
 * A shortcut for `compy_respond(ctx, COMPY_STATUS_INTERNAL_SERVER_ERROR,
 * "Internal error")`.
 */
ssize_t compy_respond_internal_error(Compy_Context *ctx);

/**
 * Implements #Compy_Droppable_IFACE for #Compy_Context.
 *
 * See [Interface99](https://github.com/Hirrolot/interface99) for the macro
 * usage.
 */
declImplExtern99(Compy_Droppable, Compy_Context);
