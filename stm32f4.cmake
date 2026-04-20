# stm32f4.cmake - Cross compilation toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Bypass the executable test during CMake Configuration
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Set compilers to the ARM embedded GCC
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# Cortex-M4 flags
set(ARM_OPTIONS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(CMAKE_C_FLAGS "${ARM_OPTIONS} -ffunction-sections -fdata-sections" CACHE INTERNAL "C Compiler")
set(CMAKE_ASM_FLAGS "${ARM_OPTIONS} -x assembler-with-cpp" CACHE INTERNAL "ASM Compiler")
set(CMAKE_EXE_LINKER_FLAGS "${ARM_OPTIONS} -Wl,--gc-sections" CACHE INTERNAL "Linker flags")
