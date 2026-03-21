/**
 * @file
 * @brief An RTSP status code.
 */

#pragma once

#include <compy/priv/compiler_attrs.h>
#include <compy/types/error.h>
#include <compy/writer.h>

#include <stdbool.h>
#include <stdint.h>

#include <slice99.h>

/**
 * An RTSP status code.
 */
typedef uint16_t Compy_StatusCode;

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
ssize_t Compy_StatusCode_serialize(
    const Compy_StatusCode *restrict self,
    Compy_Writer w) COMPY_PRIV_MUST_USE;

/**
 * Parses @p data to @p self.
 */
Compy_ParseResult Compy_StatusCode_parse(
    Compy_StatusCode *restrict self,
    CharSlice99 input) COMPY_PRIV_MUST_USE;

/**
 * Tests @p lhs and @p rhs for equality.
 *
 * @pre `lhs != NULL`
 * @pre `rhs != NULL`
 */
bool Compy_StatusCode_eq(
    const Compy_StatusCode *restrict lhs,
    const Compy_StatusCode *restrict rhs) COMPY_PRIV_MUST_USE;

/**
 * `Continue`.
 */
#define COMPY_STATUS_CONTINUE 100

/**
 * `OK`.
 */
#define COMPY_STATUS_OK 200

/**
 * `Created`.
 */
#define COMPY_STATUS_CREATED 201

/**
 * `Low on Storage Space`.
 */
#define COMPY_STATUS_LOW_ON_STORAGE_SPACE 250

/**
 * `Multiple Choices`.
 */
#define COMPY_STATUS_MULTIPLE_CHOICES 300

/**
 * `Moved Permanently`.
 */
#define COMPY_STATUS_MOVED_PERMANENTLY 301

/**
 * `Moved Temporarily`.
 */
#define COMPY_STATUS_MOVED_TEMPORARILY 302

/**
 * `See Other`.
 */
#define COMPY_STATUS_SEE_OTHER 303

/**
 * `Not Modified`.
 */
#define COMPY_STATUS_NOT_MODIFIED 304

/**
 * `Use Proxy`.
 */
#define COMPY_STATUS_USE_PROXY 305

/**
 * `Bad Request`.
 */
#define COMPY_STATUS_BAD_REQUEST 400

/**
 * `Unauthorized`.
 */
#define COMPY_STATUS_UNAUTHORIZED 401

/**
 * `Payment Required`.
 */
#define COMPY_STATUS_PAYMENT_REQUIRED 402

/**
 * `Forbidden`.
 */
#define COMPY_STATUS_FORBIDDEN 403

/**
 * `Not Found`.
 */
#define COMPY_STATUS_NOT_FOUND 404

/**
 * `Method Not Allowed`.
 */
#define COMPY_STATUS_METHOD_NOT_ALLOWED 405

/**
 * `Not Acceptable`.
 */
#define COMPY_STATUS_NOT_ACCEPTABLE 406

/**
 * `Proxy Authentication Required`.
 */
#define COMPY_STATUS_PROXY_AUTHENTICATION_REQUIRED 407

/**
 * `Request Time-out`.
 */
#define COMPY_STATUS_REQUEST_TIMEOUT 408

/**
 * `Gone`.
 */
#define COMPY_STATUS_GONE 410

/**
 * `Length Required`.
 */
#define COMPY_STATUS_LENGTH_REQUIRED 411

/**
 * `Precondition Failed`.
 */
#define COMPY_STATUS_PRECONDITION_FAILED 412

/**
 * `Request Entity Too Large`.
 */
#define COMPY_STATUS_REQUEST_ENTITY_TOO_LARGE 413

/**
 * `Request-URI Too Large`.
 */
#define COMPY_STATUS_REQUEST_URI_TOO_LARGE 414

/**
 * `Unsupported Media Type`.
 */
#define COMPY_STATUS_UNSUPPORTED_MEDIA_TYPE 415

/**
 * `Parameter Not Understood`.
 */
#define COMPY_STATUS_PARAMETER_NOT_UNDERSTOOD 451

/**
 * `Conference Not Found`.
 */
#define COMPY_STATUS_CONFERENCE_NOT_FOUND 452

/**
 * `Not Enough Bandwidth`.
 */
#define COMPY_STATUS_NOT_ENOUGH_BANDWIDTH 453

/**
 * `Session Not Found`.
 */
#define COMPY_STATUS_SESSION_NOT_FOUND 454

/**
 * `Method Not Valid in This State`.
 */
#define COMPY_STATUS_METHOD_NOT_VALID_IN_THIS_STATE 455

/**
 * `Header Field Not Valid for Resource`.
 */
#define COMPY_STATUS_HEADER_FIELD_NOT_VALID_FOR_RESOURCE 456

/**
 * `Invalid Range`.
 */
#define COMPY_STATUS_INVALID_RANGE 457

/**
 * `Parameter Is Read-Only`.
 */
#define COMPY_STATUS_PARAMETER_IS_READ_ONLY 458

/**
 * `Aggregate operation not allowed`.
 */
#define COMPY_STATUS_AGGREGATE_OPERATION_NOT_ALLOWED 459

/**
 * `Only aggregate operation allowed`.
 */
#define COMPY_STATUS_ONLY_AGGREGATE_OPERATION_ALLOWED 460

/**
 * `Unsupported transport`.
 */
#define COMPY_STATUS_UNSUPPORTED_TRANSPORT 461

/**
 * `Destination unreachable`.
 */
#define COMPY_STATUS_DESTINATION_UNREACHABLE 462

/**
 * `Internal Server Error`.
 */
#define COMPY_STATUS_INTERNAL_SERVER_ERROR 500

/**
 * `Not Implemented`.
 */
#define COMPY_STATUS_NOT_IMPLEMENTED 501

/**
 * `Bad Gateway`.
 */
#define COMPY_STATUS_BAD_GATEWAY 502

/**
 * `Service Unavailable`.
 */
#define COMPY_STATUS_SERVICE_UNAVAILABLE 503

/**
 * `Gateway Time-out`.
 */
#define COMPY_STATUS_GATEWAY_TIMEOUT 504

/**
 * `RTSP Version not supported`.
 */
#define COMPY_STATUS_RTSP_VERSION_NOT_SUPPORTED 505

/**
 * `Option not supported`.
 */
#define COMPY_STATUS_OPTION_NOT_SUPPORTED 551
