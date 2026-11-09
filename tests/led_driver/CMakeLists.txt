cmake_minimum_required(VERSION 3.20.0)

# Let Zephyr find dts/bindings/ at the repo root. Must come before find_package,
# because devicetree is processed during find_package.
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_led_driver)

target_sources(app PRIVATE
    src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/pca9685_led.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers)