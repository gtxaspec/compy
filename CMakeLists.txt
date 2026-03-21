cmake_minimum_required(VERSION 3.16)
project(compy LANGUAGES C)

# Fix the warnings about `DOWNLOAD_EXTRACT_TIMESTAMP` in newer CMake versions.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

option(COMPY_SHARED "Build a shared library" OFF)
option(COMPY_FULL_MACRO_EXPANSION "Show full macro expansion backtraces" OFF)

include(FetchContent)

FetchContent_Declare(
  slice99
  URL https://github.com/Hirrolot/slice99/archive/refs/tags/v0.7.8.tar.gz)

FetchContent_Declare(
  datatype99
  URL https://github.com/Hirrolot/datatype99/archive/refs/tags/v1.6.5.tar.gz)

FetchContent_Declare(
  interface99
  URL https://github.com/Hirrolot/interface99/archive/refs/tags/v1.0.2.tar.gz)

FetchContent_MakeAvailable(slice99 datatype99 interface99)

set(COMPY_SOURCES
    include/compy/types/error.h
    include/compy/types/header_map.h
    include/compy/types/header.h
    include/compy/types/message_body.h
    include/compy/types/method.h
    include/compy/types/reason_phrase.h
    include/compy/types/request_line.h
    include/compy/types/request_uri.h
    include/compy/types/request.h
    include/compy/types/response_line.h
    include/compy/types/response.h
    include/compy/types/rtsp_version.h
    include/compy/types/status_code.h
    include/compy/types/sdp.h
    include/compy/types/rtp.h
    include/compy/auth.h
    include/compy/nal/h264.h
    include/compy/nal/h265.h
    include/compy/nal.h
    include/compy/writer.h
    include/compy/util.h
    include/compy/transport.h
    include/compy/rtp_transport.h
    include/compy/nal_transport.h
    include/compy/droppable.h
    include/compy/controller.h
    include/compy/io_vec.h
    include/compy/context.h
    include/compy/option.h
    include/compy/types/rtcp.h
    include/compy/rtcp.h
    include/compy/receiver.h
    include/compy/backchannel.h
    src/types/error.c
    src/types/header_map.c
    src/types/header.c
    src/types/message_body.c
    src/types/method.c
    src/types/parsing.c
    src/types/parsing.h
    src/types/reason_phrase.c
    src/types/request_line.c
    src/types/request_uri.c
    src/types/request.c
    src/types/response_line.c
    src/types/response.c
    src/types/rtsp_version.c
    src/types/status_code.c
    src/types/sdp.c
    src/types/rtp.c
    src/nal/h264.c
    src/nal/h265.c
    src/nal.c
    src/writer.c
    src/writer/fd.c
    src/writer/file.c
    src/writer/string.c
    src/util.c
    src/transport/tcp.c
    src/transport/udp.c
    src/rtp_transport.c
    src/nal_transport.c
    src/io_vec.c
    src/controller.c
    src/context.c
    src/types/rtcp.c
    src/rtcp.c
    src/receiver.c
    src/backchannel.c
    src/auth.c
    src/md5.c
    include/compy/priv/md5.h
    src/macros.h
)

if(COMPY_SHARED)
  add_library(${PROJECT_NAME} SHARED ${COMPY_SOURCES})
else()
  add_library(${PROJECT_NAME} STATIC ${COMPY_SOURCES})
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-misleading-indentation)
endif()

if(NOT COMPY_FULL_MACRO_EXPANSION)
  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PUBLIC -fmacro-backtrace-limit=1)
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PUBLIC -ftrack-macro-expansion=0)
  endif()
endif()

# Precompile headers that use Datatype99/Interface99.
target_precompile_headers(
  ${PROJECT_NAME} PUBLIC
  include/compy/types/error.h
  include/compy/nal/h264.h
  include/compy/nal/h265.h
  include/compy/nal.h
  include/compy/writer.h
  include/compy/transport.h
  include/compy/droppable.h
  include/compy/controller.h
  include/compy/rtp_transport.h
  include/compy/receiver.h
  include/compy/rtcp.h
  include/compy/util.h)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC slice99 metalang99 datatype99 interface99)

set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
