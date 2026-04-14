cmake_minimum_required(VERSION 3.20)
project(pebble-adventure C)

set(CMAKE_C_STANDARD 11)

# Pebble SDK include paths
set(PEBBLE_SDK_PATH "$ENV{HOME}/.pebble-sdk/SDKs/4.9.148/sdk-core/pebble")

include_directories(
    ${PEBBLE_SDK_PATH}/emery/include
    ${CMAKE_SOURCE_DIR}/build/emery
    ${CMAKE_SOURCE_DIR}/build/include
    ${CMAKE_SOURCE_DIR}/src/c
)

# App sources (not actually compiled — this is for IDE indexing only)
file(GLOB_RECURSE APP_SOURCES "src/c/*.c" "src/c/*.h")
file(GLOB_RECURSE WORKER_SOURCES "worker_src/c/*.c" "worker_src/c/*.h")

add_library(pebble_adventure ${APP_SOURCES} ${WORKER_SOURCES})
