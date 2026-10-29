# Oldest CMake version this file is written for
cmake_minimum_required(VERSION 3.20.0)

# Pull in Zephyr's build system (kernel, drivers, devicetree, Kconfig).
# HINTS uses ZEPHYR_BASE if it's set; otherwise CMake finds Zephyr
# through the registration from "west zephyr-export".
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Name of your project
project(light_hub)

# Add your source files to "app", the target Zephyr creates for your code
target_sources(app PRIVATE
    src/main.c
    src/bulb_mgr.c
    src/parser.c
    src/uart_rx.c
)