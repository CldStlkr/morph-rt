cmake_minimum_required(VERSION 3.16)
project(rtos_kernel C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -DSTM32F4 -DSTM32F407xx -D__ARM_ARCH=7 -g -O0)

# SystemView sources
set(SYSVIEW_DIR ${CMAKE_SOURCE_DIR}/third-party/SysView)
set(SYSVIEW_SOURCES
    ${SYSVIEW_DIR}/SEGGER_SYSVIEW.c
    ${SYSVIEW_DIR}/SEGGER_RTT.c
    ${SYSVIEW_DIR}/SEGGER_RTT_ASM_ARMv7M.S
    ${SYSVIEW_DIR}/SEGGER_SYSVIEW_Config_NoOS.c
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
    foreach(target uart_demo i2c blinky traffic_stop_ffi)
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
