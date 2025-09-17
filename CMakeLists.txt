cmake_minimum_required(VERSION 3.16)
project(rtos_kernel C)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C standard
set(CMAKE_C_STANDARD 11)

# Include directories (this is what clangd needs)
include_directories(
    kernel/inc
    port/arm-cortex-m4
)

# Basic compiler flags
set(CMAKE_C_FLAGS "-Wall -Wextra -DSTM32F4 -D__ARM_ARCH=7")

# All kernel sources
file(GLOB KERNEL_SOURCES
    "kernel/src/*.c"
    "port/arm-cortex-m4/*.s"
)

# Create a library target (for clangd to understand the build)
add_library(rtos_kernel ${KERNEL_SOURCES})

# Examples (so clangd knows about them too)
add_executable(blinky examples/blinky.c)
target_link_libraries(blinky rtos_kernel)

add_executable(traffic_stop examples/traffic_stop.c)
target_link_libraries(traffic_stop rtos_kernel)
