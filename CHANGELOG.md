# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.2.0 - 2026-03-21

### Added

 - **RTCP support** (RFC 3550 Section 6): Sender Reports, Receiver Reports, SDES, BYE packets.
 - **Backchannel support**: unified receive path for two-way audio per ONVIF Streaming Spec Section 5.3.
 - **Digest authentication** (RFC 2617): self-contained MD5, nonce management, constant-time comparison.
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
 - **mmap-based media loading** in example server with CLI flags (`-v`, `-a`, `-f`, `-p`).
 - 106 tests, 1108 assertions under Address Sanitizer.

### Fixed

 - **FU fragmentation bug**: NALUs at the boundary of `max_packet_size` produced a single fragment with both S and E bits set, violating RFC 6184 Section 5.8. Fixed by subtracting FU header overhead before splitting.
 - **501 Not Implemented** for unknown methods (was 405, per RFC 2326 Section 11).
 - **Session IDs** now generated from `/dev/urandom` instead of weak `rand()` (RFC 2326 Section 3.4).
 - **Controller doc comments** incorrectly said "OPTIONS" for every method handler.
 - **CMake dependencies** switched from unreachable short git hashes to URL-based tarball fetching.

### Changed

 - Renamed project from smolrtsp to compy throughout.
 - MD5 split into separate reusable module (`src/md5.c`, `include/compy/priv/md5.h`).

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
