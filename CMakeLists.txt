cmake_minimum_required(VERSION 3.16)
project(rtos_kernel C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

include_directories(
    kernel/inc
    kernel/inc/cmsis
    kernel/inc/stm32
    port/arm-cortex-m4
)

add_compile_options(-Wall -Wextra -DSTM32F4 -DSTM32F407xx -D__ARM_ARCH=7 -g -O0)

file(GLOB KERNEL_SOURCES
    "kernel/src/*.c"
    "port/arm-cortex-m4/*.s"
)

add_library(rtos_kernel ${KERNEL_SOURCES})


add_executable(i2c examples/i2c.c)
target_link_libraries(i2c rtos_kernel)

add_executable(blinky examples/blinky.c)
target_link_libraries(blinky rtos_kernel)

add_custom_target(
    traffic_light_rust ALL
    COMMAND cargo build --manifest-path ${CMAKE_SOURCE_DIR}/examples/traffic-light/Cargo.toml --target thumbv7em-none-eabihf --release
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples/traffic-light
)

add_executable(traffic_stop_ffi examples/traffic_stop_ffi.c)
add_dependencies(traffic_stop_ffi traffic_light_rust)
target_link_libraries(traffic_stop_ffi PRIVATE rtos_kernel ${CMAKE_SOURCE_DIR}/examples/traffic-light/target/thumbv7em-none-eabihf/release/libtraffic_light.a)


add_executable(uart_demo examples/uart.c)
target_link_libraries(uart_demo rtos_kernel)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_link_options(uart_demo PRIVATE
        -T ${CMAKE_SOURCE_DIR}/stm32f4.ld
        --specs=nano.specs
        --specs=nosys.specs
    )
    add_custom_command(TARGET uart_demo POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:uart_demo> ${CMAKE_BINARY_DIR}/uart_demo.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:uart_demo>
    )


    target_link_options(i2c PRIVATE
        -T ${CMAKE_SOURCE_DIR}/stm32f4.ld
        --specs=nano.specs
        --specs=nosys.specs
    )
    add_custom_command(TARGET i2c POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:i2c> ${CMAKE_BINARY_DIR}/i2c.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:i2c>
    )

    target_link_options(blinky PRIVATE
        -T ${CMAKE_SOURCE_DIR}/stm32f4.ld
        --specs=nano.specs
        --specs=nosys.specs
    )
    add_custom_command(TARGET blinky POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:blinky> ${CMAKE_BINARY_DIR}/blinky.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:blinky>
    )
    
    target_link_options(traffic_stop_ffi PRIVATE
        -T ${CMAKE_SOURCE_DIR}/stm32f4.ld
        --specs=nano.specs
        --specs=nosys.specs
    )
    add_custom_command(TARGET traffic_stop_ffi POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:traffic_stop_ffi> ${CMAKE_BINARY_DIR}/traffic_stop_ffi.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:traffic_stop_ffi>
    )
endif()
