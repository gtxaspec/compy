cmake_minimum_required(VERSION 3.16)
project(examples LANGUAGES C)

add_subdirectory(.. build)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

find_package(LibEvent REQUIRED)

add_executable(server server.c compy-libevent.c)

target_link_libraries(server compy ${LIBEVENT_LIBRARIES})
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LIBEVENT_INCLUDE_DIR})
set_target_properties(server PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  target_compile_options(server PRIVATE -Wall -Wextra)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(server PRIVATE -Wall -Wextra -Wno-misleading-indentation)
endif()

