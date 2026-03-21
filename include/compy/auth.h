/**
 * @file
 * @brief RFC 2617 Digest authentication for RTSP.
 *
 * Provides server-side Digest authentication as required by ONVIF and
 * most NVR/VMS systems. The application supplies a credential lookup
 * function; the library handles nonce generation, digest validation,
 * and 401 challenge responses.
 *
 * Typical usage in a Controller's `before()` hook:
 * @code
 * if (compy_auth_check(auth, ctx, req) != 0) {
 *     return Compy_ControlFlow_Break;  // 401 already sent
 * }
 * return Compy_ControlFlow_Continue;
 * @endcode
 */

#pragma once

#include <compy/types/header_map.h>

#include <stdbool.h>
#include <stddef.h>

#include <slice99.h>

#include <compy/priv/compiler_attrs.h>

/* Forward declaration — full definition in context.h */
typedef struct Compy_Context Compy_Context;

#include <compy/types/request.h>

/**
 * Credential lookup callback.
 *
 * Given a username, writes the corresponding password into @p password_out
 * (up to @p password_max bytes including null terminator).
 *
 * @param[in] username The username to look up.
 * @param[out] password_out Buffer to write the password into.
 * @param[in] password_max Size of @p password_out.
 * @param[in] user_data Application-provided context pointer.
 *
 * @return `true` if the user exists and @p password_out was written,
 * `false` if the user is not found.
 */
typedef bool (*Compy_CredentialLookup)(
    const char *username, char *password_out, size_t password_max,
    void *user_data);

/**
 * Digest authentication context.
 */
typedef struct Compy_Auth Compy_Auth;

/**
 * Creates a new Digest authentication context.
 *
 * @param[in] realm The authentication realm (e.g., "IP Camera").
 * @param[in] lookup The credential lookup callback.
 * @param[in] user_data Opaque pointer passed to @p lookup.
 *
 * @pre `realm != NULL`
 * @pre `lookup != NULL`
 */
Compy_Auth *Compy_Auth_new(
    const char *realm, Compy_CredentialLookup lookup,
    void *user_data) COMPY_PRIV_MUST_USE;

/**
 * Frees an authentication context.
 *
 * @pre `self != NULL`
 */
void Compy_Auth_free(Compy_Auth *self);

/**
 * Validates the request's Authorization header.
 *
 * If no Authorization header is present or the credentials are invalid,
 * sends a `401 Unauthorized` response with a `WWW-Authenticate` challenge
 * header and returns -1. The caller should then return
 * #Compy_ControlFlow_Break from the `before()` hook.
 *
 * If the credentials are valid, returns 0 and the caller should return
 * #Compy_ControlFlow_Continue.
 *
 * @param[in] self The auth context.
 * @param[in] ctx The RTSP request context (used to send 401 if needed).
 * @param[in] req The incoming RTSP request.
 *
 * @pre `self != NULL`
 * @pre `ctx != NULL`
 * @pre `req != NULL`
 *
 * @return 0 if authenticated, -1 if not (401 already sent).
 */
int compy_auth_check(
    Compy_Auth *self, Compy_Context *ctx,
    const Compy_Request *req) COMPY_PRIV_MUST_USE;

/**
 * Computes the MD5 Digest response hash per RFC 2617 Section 3.2.2.
 *
 * `response = MD5(MD5(username:realm:password):nonce:MD5(method:uri))`
 *
 * @param[out] out Output buffer, must be at least 33 bytes (32 hex + null).
 * @param[in] username The username.
 * @param[in] realm The authentication realm.
 * @param[in] password The password.
 * @param[in] nonce The server-generated nonce.
 * @param[in] method The RTSP method (e.g., "DESCRIBE").
 * @param[in] uri The request URI.
 */
void compy_digest_response(
    char out[restrict 33], const char *username, const char *realm,
    const char *password, const char *nonce, const char *method,
    const char *uri);
