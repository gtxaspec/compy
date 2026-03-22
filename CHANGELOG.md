# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.2.0 - 2026-03-21

### Added

 - **RTCP support** (RFC 3550 Section 6): Sender Reports, Receiver Reports, SDES, BYE packets.
 - **Backchannel support**: unified receive path for two-way audio per ONVIF Streaming Spec Section 5.3.
 - **Digest authentication** (RFC 2617): self-contained MD5, nonce management, constant-time comparison, fresh nonce per challenge, volatile password wipe, `getrandom()` with `/dev/urandom` fallback.
 - **RTSPS** (RTSP over TLS): `Compy_TlsContext`, `Compy_TlsConn`, `compy_tls_writer()`, `compy_tls_read()`.
 - **SRTP** (RFC 3711): AES-128-CM + HMAC-SHA1-80/32 encryption, key derivation, SDP crypto attribute format/parse, `compy_transport_srtp()`.
 - **SRTCP** (RFC 3711 Section 3.4): encrypted RTCP with E-flag, SRTCP index, independent key derivation, `compy_transport_srtcp()`.
 - **SRTP/SRTCP receive-side decrypt**: `Compy_SrtpRecvCtx` with `compy_srtp_recv_unprotect()` and `compy_srtcp_recv_unprotect()`, constant-time auth verification, 64-bit replay window (RFC 3711 Section 3.3.2), proper ROC estimation (RFC 3711 Section 3.3.1), monotonic SRTCP index validation.
 - **Four TLS backends** (compile-time selectable): OpenSSL, wolfSSL, mbedTLS (3.6.x and 4.0 with PSA Crypto), BearSSL (SRTP crypto only).
 - **Crypto ops abstraction** (`include/compy/priv/crypto.h`): function pointer structs for TLS and SRTP operations.
 - **Base64** (RFC 4648): self-contained encode/decode in `include/compy/priv/base64.h`.
 - **PAUSE and GET_PARAMETER** methods in controller dispatch.
 - **RTP statistics tracking**: packet count, octet count, timestamp accessors on `Compy_RtpTransport`.
 - **RTP header deserialization** for the receive path.
 - **`Compy_AudioReceiver` interface** for backchannel audio callbacks.
 - **`Compy_RtpReceiver`** demuxer for incoming RTCP and backchannel RTP data.
 - **`Compy_Backchannel`** session type with default PCMU/8000 config.
 - **`compy_require_has_tag()`** and **`compy_respond_option_not_supported()`** for RTSP Require header handling.
 - **`compy_recv_dgram_socket()`** for binding UDP receive sockets.
 - **`server_port`** field in `Compy_TransportConfig` and Transport response header (RFC 2326 Section 12.39).
 - **ONVIF backchannel feature tag** constant (`COMPY_REQUIRE_ONVIF_BACKCHANNEL`).
 - **Libevent bridge** (`examples/compy-libevent.c`) replacing external dependency.
 - **mmap-based media loading** in example server with CLI flags.
 - **Example server** demonstrates all features: auth (`-u`), SRTP (`-s`), TLS (`-t`, `-k`), backchannel, RTCP.
 - 134 tests, 1241 assertions under Address Sanitizer across 6 TLS configurations.

### Fixed

 - **FU fragmentation bug**: NALUs at the boundary of `max_packet_size` produced a single fragment with both S and E bits set, violating RFC 6184 Section 5.8. Fixed by subtracting FU header overhead before splitting.
 - **501 Not Implemented** for unknown methods (was 405, per RFC 2326 Section 11).
 - **Session IDs** now generated from `/dev/urandom` instead of weak `rand()` (RFC 2326 Section 3.4).
 - **Controller doc comments** incorrectly said "OPTIONS" for every method handler.
 - **CMake dependencies** switched from unreachable short git hashes to URL-based tarball fetching.
 - **SRTP ROC rollover**: rollover counter now increments when RTP sequence number wraps past 0xFFFF.
 - **wolfSSL TLS 1.3**: use `wolfSSLv23_server_method()` instead of `wolfTLSv1_2_server_method()`.

### Changed

 - Renamed project from smolrtsp to compy throughout.
 - MD5 split into separate reusable module (`src/md5.c`, `include/compy/priv/md5.h`).
 - Key material wiped with volatile zeroing on free (SRTP, SRTCP, auth).

## 0.1.3 - 2023-03-12

### Fixed

 - Fix the `DOWNLOAD_EXTRACT_TIMESTAMP` CMake warning (see [datatype99/issues/15](https://github.com/Hirrolot/datatype99/issues/15)).

## 0.1.2 - 2022-07-27

### Fixed

 - Suppress a compilation warning for an unused variable in `compy_vheader`.
 - Overflow while computing an RTP timestamp.

## 0.1.1 - 2022-03-31

### Fixed

 - Mark the following functions with `__attribute__((warn_unused_result))` (when available):
   - `Compy_ParseError_print`.
   - `Compy_MessageBody_empty`.
   - `Compy_Request_uninit`.
   - `Compy_Response_uninit`.
   - `Compy_NalTransportConfig_default`.
   - `compy_determine_start_code`.
   - `compy_dgram_socket`.

## 0.1.0 - 2022-03-30

### Added

 - This awesome library.
