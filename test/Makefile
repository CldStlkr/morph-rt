CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -DUNIT_TEST
BUILD_DIR = build
UNITY_ROOT = Unity
UNITY_SRC = $(UNITY_ROOT)/src
INCLUDES = -I$(UNITY_SRC) -I../kernel/inc -Iinc

# Unity framework
UNITY_SOURCES = $(UNITY_SRC)/unity.c

# Kernel source paths
KERNEL_DIR = ../kernel/src
CB_SOURCES = $(KERNEL_DIR)/circular_buffer.c
TASK_SOURCES = $(KERNEL_DIR)/task.c  
QUEUE_SOURCES = $(KERNEL_DIR)/queue.c $(KERNEL_DIR)/circular_buffer.c $(KERNEL_DIR)/task.c

# Test executables
TEST_CB = $(BUILD_DIR)/test_circular_buffer
TEST_TASK = $(BUILD_DIR)/test_task
TEST_QUEUE = $(BUILD_DIR)/test_queue

# Individual test targets
test_cb: $(TEST_CB)
	./$(TEST_CB)

test_task: $(TEST_TASK)
	./$(TEST_TASK)

test_queue: $(TEST_QUEUE)
	./$(TEST_QUEUE)

# Combined target
test_all: test_cb test_task test_queue
	@echo "All tests passed! ✅"

# Build individual test executables
$(TEST_CB): src/test_circular_buffer.c $(CB_SOURCES) $(UNITY_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_TASK): src/test_task.c $(TASK_SOURCES) $(UNITY_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_QUEUE): src/test_queue.c $(QUEUE_SOURCES) $(UNITY_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Convenience targets
cb: test_cb
task: test_task  
queue: test_queue

# Default target
all: test_all

.PHONY: all test_cb test_task test_queue test_all clean cb task queue
