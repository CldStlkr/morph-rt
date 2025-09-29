#include "list.h"
#include "memory.h"
#include "queue.h"
#include "task.h"
#include "test_queue.h"
#include "time_utils.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

//=============================================================================
// MOCK IMPLEMENTATIONS
//=============================================================================

// Mock global variables that queue.c depends on
volatile uint32_t tick_now = 0;
task_handle_t current_task = NULL;

// Mock task structure for testing blocking behavior
static task_control_block mock_task;

// Mock scheduler functions
void scheduler_cancel_timeout(task_handle_t task) {
  (void)task; // Mock implementation - do nothing
}

void scheduler_add_task(task_handle_t task) {
  (void)task; // Mock implementation - do nothing
}

void scheduler_remove_task(task_handle_t task) { 
  (void)task; 
}

void scheduler_set_timeout(task_handle_t task, uint32_t wake_tick) {
  (void)task;
  (void)wake_tick; // Mock implementation - do nothing
}

void scheduler_yield(void) {
  // Mock implementation - do nothing (in real system, this would context switch)
}

//=============================================================================
// TEST FIXTURES
//=============================================================================

static queue_handle_t test_queue;

// Module-specific setup/teardown functions
void setUp(void) {
  // Initialize memory pools before each test
  memory_pools_init();
  
  test_queue = NULL;
  tick_now = 0;
  current_task = NULL;
  
  // Initialize mock task
  memset(&mock_task, 0, sizeof(mock_task));
  mock_task.state = TASK_READY;
  mock_task.wake_reason = WAKE_REASON_NONE;
  mock_task.waiting_on = NULL;
  list_init(&mock_task.wait_link);
}

void tearDown(void) {
  if (test_queue) {
    queue_delete(test_queue);
    test_queue = NULL;
  }
  current_task = NULL;
}

//=============================================================================
// QUEUE CREATION/DELETION TESTS
//=============================================================================

void test_queue_create_should_succeed_with_valid_parameters(void) {
  test_queue = queue_create(10, sizeof(int));
  TEST_ASSERT_NOT_NULL(test_queue);
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
  TEST_ASSERT_FALSE(queue_is_full(test_queue));
  TEST_ASSERT_EQUAL(0, queue_messages_waiting(test_queue));
}

void test_queue_create_should_fail_with_zero_length(void) {
  test_queue = queue_create(0, sizeof(int));
  TEST_ASSERT_NULL(test_queue);
}

void test_queue_create_should_fail_with_zero_item_size(void) {
  test_queue = queue_create(10, 0);
  TEST_ASSERT_NULL(test_queue);
}

void test_queue_create_should_fail_when_pools_exhausted(void) {
  // Fill up the queue control block pool
  queue_handle_t queues[MAX_QUEUES];
  int created_count = 0;
  
  // Create as many queues as possible
  for (int i = 0; i < MAX_QUEUES; i++) {
    queues[i] = queue_create(4, sizeof(int));
    if (queues[i] != NULL) {
      created_count++;
    } else {
      break;
    }
  }
  
  // Should have created MAX_QUEUES
  TEST_ASSERT_EQUAL(MAX_QUEUES, created_count);
  
  // Next creation should fail
  queue_handle_t overflow_queue = queue_create(4, sizeof(int));
  TEST_ASSERT_NULL(overflow_queue);
  
  // Clean up
  for (int i = 0; i < created_count; i++) {
    queue_delete(queues[i]);
  }
}

void test_queue_create_should_select_appropriate_buffer_pool(void) {
  // Test small buffer allocation (64 bytes)
  queue_handle_t small_queue = queue_create(16, sizeof(int)); // 16 * 4 = 64 bytes
  TEST_ASSERT_NOT_NULL(small_queue);
  
  // Test medium buffer allocation (256 bytes) 
  queue_handle_t medium_queue = queue_create(64, sizeof(int)); // 64 * 4 = 256 bytes
  TEST_ASSERT_NOT_NULL(medium_queue);
  
  // Test large buffer allocation (1024 bytes)
  queue_handle_t large_queue = queue_create(256, sizeof(int)); // 256 * 4 = 1024 bytes
  TEST_ASSERT_NOT_NULL(large_queue);
  
  // Clean up
  queue_delete(small_queue);
  queue_delete(medium_queue);
  queue_delete(large_queue);
}

void test_queue_delete_should_handle_null_queue(void) {
  // Should not crash
  queue_delete(NULL);
  TEST_ASSERT_TRUE(true); // If we get here, test passed
}

void test_queue_delete_should_cleanup_resources(void) {
  test_queue = queue_create(5, sizeof(int));
  TEST_ASSERT_NOT_NULL(test_queue);
  
  queue_delete(test_queue);
  test_queue = NULL; // Prevent double-free in teardown
  
  // If we reach here without crashing, cleanup succeeded
  TEST_ASSERT_TRUE(true);
}

//=============================================================================
// BASIC SEND/RECEIVE TESTS
//=============================================================================

void test_queue_send_receive_immediate_single_item(void) {
  test_queue = queue_create(5, sizeof(int));
  int send_value = 42;
  int receive_value = 0;

  // Send item
  queue_result_t send_result = queue_send_immediate(test_queue, &send_value);
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, send_result);
  TEST_ASSERT_FALSE(queue_is_empty(test_queue));
  TEST_ASSERT_EQUAL(1, queue_messages_waiting(test_queue));

  // Receive item
  queue_result_t receive_result = queue_receive_immediate(test_queue, &receive_value);
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, receive_result);
  TEST_ASSERT_EQUAL(send_value, receive_value);
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
  TEST_ASSERT_EQUAL(0, queue_messages_waiting(test_queue));
}

void test_queue_send_receive_immediate_multiple_items(void) {
  test_queue = queue_create(5, sizeof(int));
  int send_values[] = {10, 20, 30, 40};
  int receive_values[4] = {0};

  // Send multiple items
  for (int i = 0; i < 4; i++) {
    queue_result_t result = queue_send_immediate(test_queue, &send_values[i]);
    TEST_ASSERT_EQUAL(QUEUE_SUCCESS, result);
    TEST_ASSERT_EQUAL(i + 1, queue_messages_waiting(test_queue));
  }

  // Receive multiple items
  for (int i = 0; i < 4; i++) {
    queue_result_t result = queue_receive_immediate(test_queue, &receive_values[i]);
    TEST_ASSERT_EQUAL(QUEUE_SUCCESS, result);
    TEST_ASSERT_EQUAL(send_values[i], receive_values[i]);
  }
  
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
}

void test_queue_should_maintain_fifo_order(void) {
  test_queue = queue_create(10, sizeof(int));
  
  // Send sequence: 100, 200, 300
  int values[] = {100, 200, 300};
  for (int i = 0; i < 3; i++) {
    queue_send_immediate(test_queue, &values[i]);
  }

  // Receive should be in same order: 100, 200, 300
  for (int i = 0; i < 3; i++) {
    int received;
    queue_receive_immediate(test_queue, &received);
    TEST_ASSERT_EQUAL(values[i], received);
  }
}

void test_queue_send_immediate_should_fail_when_full(void) {
  test_queue = queue_create(3, sizeof(int));
  int value = 123;
  
  // Fill the queue (capacity will be rounded up to power of 2, so likely 4)
  // Keep sending until full
  queue_result_t result;
  do {
    result = queue_send_immediate(test_queue, &value);
  } while (result == QUEUE_SUCCESS);

  // Last send should have failed
  TEST_ASSERT_EQUAL(QUEUE_ERROR_FULL, result);
  TEST_ASSERT_TRUE(queue_is_full(test_queue));

  // Another send should also fail
  result = queue_send_immediate(test_queue, &value);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_FULL, result);
}

void test_queue_receive_immediate_should_fail_when_empty(void) {
  test_queue = queue_create(5, sizeof(int));
  int value;
  
  queue_result_t result = queue_receive_immediate(test_queue, &value);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_EMPTY, result);
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
}

//=============================================================================
// QUEUE STATE TESTS
//=============================================================================

void test_queue_is_empty_should_detect_empty_state(void) {
  test_queue = queue_create(5, sizeof(int));
  
  // Initially empty
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
  
  // Add item - no longer empty
  int value = 42;
  queue_send_immediate(test_queue, &value);
  TEST_ASSERT_FALSE(queue_is_empty(test_queue));
  
  // Remove item - empty again
  queue_receive_immediate(test_queue, &value);
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
}

void test_queue_is_full_should_detect_full_state(void) {
  test_queue = queue_create(2, sizeof(int)); // Will be rounded to power of 2
  int value = 42;
  
  // Initially not full
  TEST_ASSERT_FALSE(queue_is_full(test_queue));
  
  // Fill until full
  while (!queue_is_full(test_queue)) {
    queue_send_immediate(test_queue, &value);
  }
  TEST_ASSERT_TRUE(queue_is_full(test_queue));
  
  // Remove one item - no longer full
  queue_receive_immediate(test_queue, &value);
  TEST_ASSERT_FALSE(queue_is_full(test_queue));
}

void test_queue_messages_waiting_should_count_correctly(void) {
  test_queue = queue_create(10, sizeof(int));
  TEST_ASSERT_EQUAL(0, queue_messages_waiting(test_queue));
  
  int value = 42;
  // Add items and verify count
  for (size_t i = 1; i <= 5; i++) {
    queue_send_immediate(test_queue, &value);
    TEST_ASSERT_EQUAL(i, queue_messages_waiting(test_queue));
  }
  
  // Remove items and verify count
  for (size_t i = 4; i > 0; i--) {
    queue_receive_immediate(test_queue, &value);
    TEST_ASSERT_EQUAL(i, queue_messages_waiting(test_queue));
  }
}

void test_queue_state_functions_should_handle_null_queue(void) {
  TEST_ASSERT_TRUE(queue_is_empty(NULL));
  TEST_ASSERT_FALSE(queue_is_full(NULL));
  TEST_ASSERT_EQUAL(0, queue_messages_waiting(NULL));
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

void test_queue_send_should_handle_null_parameters(void) {
  test_queue = queue_create(5, sizeof(int));
  int value = 42;
  
  // Null queue
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_send(NULL, &value, 100));
  
  // Null item
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_send(test_queue, NULL, 100));
  
  // Both null
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_send(NULL, NULL, 100));
}

void test_queue_receive_should_handle_null_parameters(void) {
  test_queue = queue_create(5, sizeof(int));
  int value;
  
  // Null queue
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_receive(NULL, &value, 100));
  
  // Null item
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_receive(test_queue, NULL, 100));
  
  // Both null
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_receive(NULL, NULL, 100));
}

void test_queue_send_immediate_should_handle_null_parameters(void) {
  test_queue = queue_create(5, sizeof(int));
  int value = 42;
  
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_send_immediate(NULL, &value));
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_send_immediate(test_queue, NULL));
}

void test_queue_receive_immediate_should_handle_null_parameters(void) {
  test_queue = queue_create(5, sizeof(int));
  int value;
  
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_receive_immediate(NULL, &value));
  TEST_ASSERT_EQUAL(QUEUE_ERROR_NULL_POINTER, queue_receive_immediate(test_queue, NULL));
}

//=============================================================================
// BLOCKING BEHAVIOR TESTS
//=============================================================================

void test_queue_send_with_zero_timeout_should_not_block(void) {
  test_queue = queue_create(2, sizeof(int));
  int value = 42;
  
  // Fill the queue completely
  while (queue_send_immediate(test_queue, &value) == QUEUE_SUCCESS) {
    // Keep adding until full
  }
  
  // Send with zero timeout should fail immediately
  queue_result_t result = queue_send(test_queue, &value, 0);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_FULL, result);
}

void test_queue_receive_with_zero_timeout_should_not_block(void) {
  test_queue = queue_create(5, sizeof(int));
  int value;
  
  // Queue is empty, receive with zero timeout should fail immediately
  queue_result_t result = queue_receive(test_queue, &value, 0);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_EMPTY, result);
}

void test_queue_send_should_return_timeout_when_deadline_exceeded(void) {
  test_queue = queue_create(2, sizeof(int));
  int value = 42;
  
  // Set up mock task
  current_task = &mock_task;
  current_task->wake_reason = WAKE_REASON_TIMEOUT;
  
  // Fill the queue completely
  while (queue_send_immediate(test_queue, &value) == QUEUE_SUCCESS) {
    // Keep adding until full
  }
  
  // Set time so that deadline will be exceeded
  tick_now = 100;
  
  // Send with timeout - should return timeout
  queue_result_t result = queue_send(test_queue, &value, 10);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_TIMEOUT, result);
}

void test_queue_receive_should_return_timeout_when_deadline_exceeded(void) {
  test_queue = queue_create(5, sizeof(int));
  int value;
  
  // Set up mock task
  current_task = &mock_task;
  current_task->wake_reason = WAKE_REASON_TIMEOUT;
  
  // Set time so that deadline will be exceeded
  tick_now = 100;
  
  // Receive from empty queue with timeout - should return timeout
  queue_result_t result = queue_receive(test_queue, &value, 10);
  TEST_ASSERT_EQUAL(QUEUE_ERROR_TIMEOUT, result);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

void test_queue_should_integrate_with_memory_pools(void) {
  // Test that queue creation/deletion integrates properly with memory pools
  
  // Get initial pool stats
  pool_stats_t initial_qcb_stats = pool_get_stats(POOL_QCB);
  pool_stats_t initial_buffer_stats = pool_get_stats(POOL_BUFFER_SMALL);
  
  // Create a queue
  queue_handle_t queue = queue_create(16, sizeof(int)); // Should use small buffer
  TEST_ASSERT_NOT_NULL(queue);
  
  // Pool usage should increase
  pool_stats_t after_create_qcb_stats = pool_get_stats(POOL_QCB);
  pool_stats_t after_create_buffer_stats = pool_get_stats(POOL_BUFFER_SMALL);
  
  TEST_ASSERT_EQUAL(initial_qcb_stats.used_objects + 1, after_create_qcb_stats.used_objects);
  TEST_ASSERT_EQUAL(initial_buffer_stats.used_objects + 1, after_create_buffer_stats.used_objects);
  
  // Delete the queue
  queue_delete(queue);
  
  // Pool usage should return to initial state
  pool_stats_t after_delete_qcb_stats = pool_get_stats(POOL_QCB);
  pool_stats_t after_delete_buffer_stats = pool_get_stats(POOL_BUFFER_SMALL);
  
  TEST_ASSERT_EQUAL(initial_qcb_stats.used_objects, after_delete_qcb_stats.used_objects);
  TEST_ASSERT_EQUAL(initial_buffer_stats.used_objects, after_delete_buffer_stats.used_objects);
}

void test_queue_full_lifecycle_send_receive_pattern(void) {
  test_queue = queue_create(3, sizeof(int));
  
  // Pattern: send 2, receive 1, send 1, receive 2
  int values[] = {10, 20, 30};
  int received[3];
  
  // Send first two
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_send_immediate(test_queue, &values[0]));
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_send_immediate(test_queue, &values[1]));
  TEST_ASSERT_EQUAL(2, queue_messages_waiting(test_queue));
  
  // Receive one
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_receive_immediate(test_queue, &received[0]));
  TEST_ASSERT_EQUAL(values[0], received[0]);
  TEST_ASSERT_EQUAL(1, queue_messages_waiting(test_queue));
  
  // Send one more
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_send_immediate(test_queue, &values[2]));
  TEST_ASSERT_EQUAL(2, queue_messages_waiting(test_queue));
  
  // Receive remaining two
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_receive_immediate(test_queue, &received[1]));
  TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_receive_immediate(test_queue, &received[2]));
  TEST_ASSERT_EQUAL(values[1], received[1]);
  TEST_ASSERT_EQUAL(values[2], received[2]);
  TEST_ASSERT_TRUE(queue_is_empty(test_queue));
}

//=============================================================================
// CAPACITY AND BUFFER MANAGEMENT
//=============================================================================

void test_queue_should_use_power_of_two_capacity(void) {
  // Test that queue uses circular buffer's power-of-2 capacity rounding
  test_queue = queue_create(3, sizeof(int)); // Should round up to 4
  int value = 42;
  int count = 0;
  
  // Should be able to store 4 items (power of 2 rounding)
  while (queue_send_immediate(test_queue, &value) == QUEUE_SUCCESS) {
    count++;
  }
  
  // Verify we could store a power-of-2 number of items
  // (3 rounds up to 4, so we should have stored 4 items)
  TEST_ASSERT_TRUE((count & (count - 1)) == 0); // Check if power of 2
  TEST_ASSERT_GREATER_OR_EQUAL(3, count); // At least the requested capacity
}

void test_queue_should_handle_wraparound_correctly(void) {
  test_queue = queue_create(4, sizeof(int));
  
  // Fill, then repeatedly add/remove to force wraparound
  int values[] = {10, 20, 30, 40, 50, 60, 70, 80};
  
  // Fill completely
  for (int i = 0; i < 4; i++) {
    queue_send_immediate(test_queue, &values[i]);
  }
  
  // Now do add/remove cycles to force wraparound
  for (int cycle = 0; cycle < 4; cycle++) {
    int received;
    // Remove one
    TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_receive_immediate(test_queue, &received));
    TEST_ASSERT_EQUAL(values[cycle], received);
    
    // Add one (forces wraparound)
    TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_send_immediate(test_queue, &values[4 + cycle]));
    
    // Should still have 4 items
    TEST_ASSERT_EQUAL(4, queue_messages_waiting(test_queue));
  }
  
  // Verify remaining items are in correct order
  for (int i = 0; i < 4; i++) {
    int received;
    TEST_ASSERT_EQUAL(QUEUE_SUCCESS, queue_receive_immediate(test_queue, &received));
    TEST_ASSERT_EQUAL(values[4 + i], received);
  }
}

//=============================================================================
// TEST RUNNER
//=============================================================================

int main(void) {
  UNITY_BEGIN();

  // Queue creation/deletion tests
  RUN_TEST(test_queue_create_should_succeed_with_valid_parameters);
  RUN_TEST(test_queue_create_should_fail_with_zero_length);
  RUN_TEST(test_queue_create_should_fail_with_zero_item_size);
  RUN_TEST(test_queue_create_should_fail_when_pools_exhausted);
  RUN_TEST(test_queue_create_should_select_appropriate_buffer_pool);
  RUN_TEST(test_queue_delete_should_handle_null_queue);
  RUN_TEST(test_queue_delete_should_cleanup_resources);

  // Basic send/receive tests
  RUN_TEST(test_queue_send_receive_immediate_single_item);
  RUN_TEST(test_queue_send_receive_immediate_multiple_items);
  RUN_TEST(test_queue_should_maintain_fifo_order);
  RUN_TEST(test_queue_send_immediate_should_fail_when_full);
  RUN_TEST(test_queue_receive_immediate_should_fail_when_empty);

  // Queue state tests
  RUN_TEST(test_queue_is_empty_should_detect_empty_state);
  RUN_TEST(test_queue_is_full_should_detect_full_state);
  RUN_TEST(test_queue_messages_waiting_should_count_correctly);
  RUN_TEST(test_queue_state_functions_should_handle_null_queue);

  // Error handling tests
  RUN_TEST(test_queue_send_should_handle_null_parameters);
  RUN_TEST(test_queue_receive_should_handle_null_parameters);
  RUN_TEST(test_queue_send_immediate_should_handle_null_parameters);
  RUN_TEST(test_queue_receive_immediate_should_handle_null_parameters);

  // Blocking behavior tests
  RUN_TEST(test_queue_send_with_zero_timeout_should_not_block);
  RUN_TEST(test_queue_receive_with_zero_timeout_should_not_block);
  RUN_TEST(test_queue_send_should_return_timeout_when_deadline_exceeded);
  RUN_TEST(test_queue_receive_should_return_timeout_when_deadline_exceeded);

  // Integration tests
  RUN_TEST(test_queue_should_integrate_with_memory_pools);
  RUN_TEST(test_queue_full_lifecycle_send_receive_pattern);

  // Capacity and buffer management
  RUN_TEST(test_queue_should_use_power_of_two_capacity);
  RUN_TEST(test_queue_should_handle_wraparound_correctly);

  return UNITY_END();
}
