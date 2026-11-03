cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_protocol)

target_sources(app PRIVATE
    src/test_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/packet.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)