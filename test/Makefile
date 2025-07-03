CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -DUNIT_TEST
BUILD_DIR = build
UNITY_ROOT = Unity
UNITY_SRC = $(UNITY_ROOT)/src
INCLUDES = -I$(UNITY_SRC) -I../kernel/inc -Iinc

# Source files
UNITY_SOURCES = $(UNITY_SRC)/unity.c
KERNEL_SOURCES = ../kernel/src/circular_buffer.c
# ../kernel/src/task.c
TEST_SOURCES = src/test_circular_buffer.c src/test_runner.c
# src/test_task.c

# Target
TARGET = $(BUILD_DIR)/test_runner

# Default target
all: $(TARGET)

$(TARGET): $(TEST_SOURCES) $(UNITY_SOURCES) $(KERNEL_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean
