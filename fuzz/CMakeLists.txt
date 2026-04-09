cmake_minimum_required(VERSION 3.16)
project(compy_fuzz LANGUAGES C)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Fuzz targets require Clang (for -fsanitize=fuzzer)")
endif()

# Instrument the library for coverage-guided fuzzing
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=fuzzer-no-link,address")

add_subdirectory(.. build)

set(FUZZ_FLAGS -fsanitize=fuzzer,address)

add_executable(fuzz_request fuzz_request_parse.c)
target_link_libraries(fuzz_request compy)
target_compile_options(fuzz_request PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_request PRIVATE ${FUZZ_FLAGS})
set_target_properties(fuzz_request PROPERTIES C_STANDARD 99)

add_executable(fuzz_response fuzz_response_parse.c)
target_link_libraries(fuzz_response compy)
target_compile_options(fuzz_response PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_response PRIVATE ${FUZZ_FLAGS})
set_target_properties(fuzz_response PROPERTIES C_STANDARD 99)
