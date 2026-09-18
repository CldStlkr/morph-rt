cmake_minimum_required(VERSION 3.16)
project(rtos_kernel C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -DSTM32F4 -DSTM32F407xx -D__ARM_ARCH=7 -g -O0)

# RTT sources (SystemView removed)
set(SYSVIEW_DIR ${CMAKE_SOURCE_DIR}/third-party/SysView)
set(SYSVIEW_SOURCES
    ${SYSVIEW_DIR}/SEGGER_RTT.c
    ${SYSVIEW_DIR}/SEGGER_RTT_ASM_ARMv7M.S
)

file(GLOB KERNEL_SOURCES
    "kernel/src/*.c"
    "port/arm-cortex-m4/*.s"
)

add_library(rtos_kernel OBJECT ${KERNEL_SOURCES} ${SYSVIEW_SOURCES})

target_include_directories(rtos_kernel PUBLIC
    kernel/inc
    kernel/inc/cmsis
    kernel/inc/stm32
    port/arm-cortex-m4
    ${SYSVIEW_DIR}
)

add_executable(i2c examples/i2c.c)
target_link_libraries(i2c rtos_kernel)

# Driver library: I2C1, CS43L22, I2S3/DMA
add_library(audio_drivers OBJECT
    drivers/i2c/i2c.c
    drivers/cs43l22/cs43l22.c
    drivers/i2s/i2s.c
)
target_include_directories(audio_drivers PUBLIC
    drivers/i2c
    drivers/cs43l22
    drivers/i2s
    kernel/inc
    kernel/inc/cmsis
    kernel/inc/stm32
    ${SYSVIEW_DIR}
)

add_executable(audio examples/audio.c)
target_link_libraries(audio rtos_kernel audio_drivers)
target_include_directories(audio PRIVATE
    drivers/i2c
    drivers/cs43l22
    drivers/i2s
)

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

add_executable(stress_n4 examples/stress_N4.c examples/latency.c)
target_link_libraries(stress_n4 rtos_kernel)
add_executable(stress_n8 examples/stress_N8.c examples/latency.c)
target_link_libraries(stress_n8 rtos_kernel)
add_executable(stress_n16 examples/stress_N16.c examples/latency.c)
target_link_libraries(stress_n16 rtos_kernel)
add_executable(stress_n32 examples/stress_N32.c examples/latency.c)
target_link_libraries(stress_n32 rtos_kernel)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    foreach(target uart_demo i2c blinky audio traffic_stop_ffi stress_n4 stress_n8 stress_n16 stress_n32)
        target_link_options(${target} PRIVATE
            -T ${CMAKE_SOURCE_DIR}/stm32f4.ld
            --specs=nano.specs
            --specs=nosys.specs
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${CMAKE_BINARY_DIR}/${target}.bin
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>
        )
    endforeach()
endif()
