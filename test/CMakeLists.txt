# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(UnitTests VERSION 1.0)

# Set the C standard to C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -DUNIT_TEST")

# Define the source directories
set(UNITY_ROOT Unity)
set(UNITY_SRC ${UNITY_ROOT}/src)
set(KERNEL_DIR ../kernel/src)
set(SOURCE_DIR src)

# Include directories for Unity, kernel, and tests
include_directories(${UNITY_SRC} ../kernel/inc inc)

# List of Unity source files
set(UNITY_SOURCES ${UNITY_SRC}/unity.c)

# Kernel source files
set(MEMORY_SOURCES ${KERNEL_DIR}/memory.c)
set(TASK_SOURCES ${KERNEL_DIR}/task.c ${MEMORY_SOURCES})
set(QUEUE_SOURCES ${KERNEL_DIR}/queue.c ${KERNEL_DIR}/task.c ${MEMORY_SOURCES})
set(SEMAPHORE_SOURCES ${KERNEL_DIR}/semaphore.c ${MEMORY_SOURCES} ${TASK_SOURCES})
set(MUTEX_SOURCES ${KERNEL_DIR}/mutex.c ${MEMORY_SOURCES} ${TASK_SOURCES})

# Test executables (use relative paths)
set(TEST_CB test_circular_buffer)
set(TEST_MEMORY test_memory)
set(TEST_TASK test_task)
set(TEST_QUEUE test_queue)
set(TEST_SEMAPHORE test_semaphore)
set(TEST_MUTEX test_mutex)

# Add the test executables
add_executable(${TEST_CB} ${SOURCE_DIR}/test_circular_buffer.c ${UNITY_SOURCES})
add_executable(${TEST_MEMORY} ${SOURCE_DIR}/test_memory.c ${MEMORY_SOURCES} ${UNITY_SOURCES})
add_executable(${TEST_TASK} ${SOURCE_DIR}/test_task.c ${TASK_SOURCES} ${UNITY_SOURCES})
add_executable(${TEST_QUEUE} ${SOURCE_DIR}/test_queue.c ${QUEUE_SOURCES} ${UNITY_SOURCES})
add_executable(${TEST_SEMAPHORE} ${SOURCE_DIR}/test_semaphore.c ${SEMAPHORE_SOURCES} ${MEMORY_SOURCES} ${UNITY_SOURCES})
add_executable(${TEST_MUTEX} ${SOURCE_DIR}/test_mutex.c ${MUTEX_SOURCES} ${MEMORY_SOURCES} ${UNITY_SOURCES})

# Enable testing
enable_testing()

# Add the test targets
add_test(NAME test_cb COMMAND ${TEST_CB})
add_test(NAME test_memory COMMAND ${TEST_MEMORY})
add_test(NAME test_task COMMAND ${TEST_TASK})
add_test(NAME test_queue COMMAND ${TEST_QUEUE})
add_test(NAME test_semaphore COMMAND ${TEST_SEMAPHORE})
add_test(NAME test_mutex COMMAND ${TEST_MUTEX})

# Convenience targets for running specific tests
add_custom_target(cb COMMAND ${TEST_CB})
add_custom_target(memory COMMAND ${TEST_MEMORY})
add_custom_target(task COMMAND ${TEST_TASK})
add_custom_target(queue COMMAND ${TEST_QUEUE})
add_custom_target(semaphore COMMAND ${TEST_SEMAPHORE})
add_custom_target(mutex COMMAND ${TEST_MUTEX})

# Default target to run all tests
add_custom_target(test_all
    COMMAND ${TEST_CB}
    COMMAND ${TEST_MEMORY}
    COMMAND ${TEST_TASK}
    COMMAND ${TEST_QUEUE}
    COMMAND ${TEST_SEMAPHORE}
    COMMAND ${TEST_MUTEX}
    COMMENT "Running all tests"
)

# Clean up the build directory (renamed to avoid conflict with built-in CMake clean target)
set(CLEAN_CMD "rm -rf ${CMAKE_BINARY_DIR}/CMakeFiles")
add_custom_target(my_clean
    COMMAND ${CLEAN_CMD}
    COMMENT "Cleaning the build directory"
)
