cmake_minimum_required(VERSION 3.0.2)
project(tests LANGUAGES C)

# Fix the warnings about `DOWNLOAD_EXTRACT_TIMESTAMP` in newer CMake versions.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

add_subdirectory(.. build)

include(FetchContent)

FetchContent_Declare(
  greatest
  URL https://github.com/silentbicycle/greatest/archive/refs/tags/v1.5.0.tar.gz)

FetchContent_MakeAvailable(greatest)
FetchContent_GetProperties(greatest)

add_executable(
  tests
  main.c
  types/header_map.c
  types/header.c
  types/message_body.c
  types/method.c
  types/reason_phrase.c
  types/request.c
  types/request_line.c
  types/response.c
  types/response_line.c
  types/request_uri.c
  types/rtsp_version.c
  types/status_code.c
  types/sdp.c
  types/test_util.h
  nal/h264.c
  nal/h265.c
  nal.c
  util.c
  writer.c
  io_vec.c
  controller.c
  context.c
  transport.c
  types/rtcp.c
  rtcp.c
  receiver.c
  auth.c
  base64.c)

option(COMPY_TSAN "Build tests with ThreadSanitizer instead of AddressSanitizer" OFF)

if(COMPY_TSAN)
  set(SANITIZER_FLAGS -fsanitize=thread)
else()
  set(SANITIZER_FLAGS -fsanitize=address)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  target_compile_options(tests PRIVATE -Wall -Wextra ${SANITIZER_FLAGS})
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(tests PRIVATE -Wall -Wextra ${SANITIZER_FLAGS} -Wno-misleading-indentation)
endif()

target_link_options(tests PRIVATE ${SANITIZER_FLAGS})

target_link_libraries(tests compy)
target_include_directories(tests PRIVATE ${greatest_SOURCE_DIR})

if(COMPY_TLS_OPENSSL OR COMPY_TLS_WOLFSSL OR COMPY_TLS_MBEDTLS)
  target_sources(tests PRIVATE srtp.c tls.c)
  # TLS I/O tests use mbedTLS as the client regardless of compy's backend
  if(NOT COMPY_TLS_MBEDTLS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MBEDTLS_TEST REQUIRED mbedtls mbedx509 mbedcrypto)
    target_include_directories(tests PRIVATE ${MBEDTLS_TEST_INCLUDE_DIRS})
    target_link_directories(tests PRIVATE ${MBEDTLS_TEST_LIBRARY_DIRS})
    target_link_libraries(tests ${MBEDTLS_TEST_LIBRARIES})
  endif()
endif()

set_target_properties(tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
