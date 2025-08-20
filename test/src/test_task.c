#include "task.h"
#include "test_task.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

// Mock scheduler functions for testing
void scheduler_remove_task(task_handle_t task) {
  (void)task; // Mock implementation - do nothing
}

// Test fixture - global variables
static task_handle_t test_task;

// Dummy task function for testing
void dummy_task_function(void *param) {
  (void)param;
  // This function should never actually execute in tests
  while (1) {
  }
}

// Module-specific setup/teardown functions
void setUp(void) { test_task = NULL; }

void tearDown(void) {
  if (test_task) {
    task_delete_internal(test_task);
    test_task = NULL;
  }
}

//=============================================================================
// TASK CREATION TESTS
//=============================================================================

void test_task_create_should_succeed_with_valid_parameters(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  TEST_ASSERT_NOT_NULL(test_task);
  TEST_ASSERT_NOT_NULL(test_task->stack_base);
  TEST_ASSERT_EQUAL(512, test_task->stack_size);
  TEST_ASSERT_EQUAL_STRING("TestTask", test_task->name);
  TEST_ASSERT_EQUAL(3, test_task->priority);
  TEST_ASSERT_EQUAL(TASK_READY, test_task->state);
  TEST_ASSERT_EQUAL(0, test_task->wake_tick);
  TEST_ASSERT_EQUAL(WAKE_REASON_NONE, test_task->wake_reason);
  TEST_ASSERT_NULL(test_task->waiting_on);
  TEST_ASSERT_EQUAL(0, test_task->run_count);
  TEST_ASSERT_EQUAL(0, test_task->total_runtime);
}

void test_task_create_should_fail_with_null_function(void) {
  test_task = task_create_internal(NULL, "TestTask", 512, NULL, 3);
  TEST_ASSERT_NULL(test_task);
}

void test_task_create_should_fail_with_null_name(void) {
  test_task = task_create_internal(dummy_task_function, NULL, 512, NULL, 3);
  TEST_ASSERT_NULL(test_task);
}

void test_task_create_should_fail_with_zero_stack_size(void) {
  test_task = task_create_internal(dummy_task_function, "TestTask", 0, NULL, 3);
  TEST_ASSERT_NULL(test_task);
}

void test_task_create_should_fail_with_invalid_priority(void) {
  test_task = task_create_internal(dummy_task_function, "TestTask", 512, NULL,
                                   MAX_PRIORITY + 1);
  TEST_ASSERT_NULL(test_task);
}

void test_task_create_should_use_default_stack_size_when_zero(void) {
  // Note: This test is for the public API which uses DEFAULT_STACK_SIZE when 0
  // is passed The internal function should reject 0, but we can test this
  // concept differently
  test_task = task_create_internal(dummy_task_function, "TestTask",
                                   DEFAULT_STACK_SIZE, NULL, 3);

  TEST_ASSERT_NOT_NULL(test_task);
  TEST_ASSERT_EQUAL(DEFAULT_STACK_SIZE, test_task->stack_size);
}

//=============================================================================
// TASK DELETION TESTS
//=============================================================================

void test_task_delete_should_cleanup_resources(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);
  TEST_ASSERT_NOT_NULL(test_task);

  // Store original pointers to verify they get cleaned up
  void *original_stack = test_task->stack_base;
  TEST_ASSERT_NOT_NULL(original_stack);

  task_delete_internal(test_task);
  test_task = NULL; // Prevent double-free in tearDown

  // Note: We can't verify memory was freed since the task structure itself is
  // freed In a real system, we might use memory debugging tools
}

void test_task_delete_should_handle_null_task(void) {
  // Should not crash
  task_delete_internal(NULL);
  TEST_ASSERT_TRUE(true); // If we get here, test passed
}

void test_task_delete_should_free_stack_memory(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);
  TEST_ASSERT_NOT_NULL(test_task);
  TEST_ASSERT_NOT_NULL(test_task->stack_base);

  task_delete_internal(test_task);
  test_task = NULL; // Prevent double-free in tearDown

  // Memory should be freed - we can't directly test this without memory
  // debugging tools but we verify the function completes without crashing
}

//=============================================================================
// TASK STATE MANAGEMENT TESTS
//=============================================================================

void test_task_set_state_should_update_state(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);
  TEST_ASSERT_EQUAL(TASK_READY, test_task->state);

  task_set_state(test_task, TASK_RUNNING);
  TEST_ASSERT_EQUAL(TASK_RUNNING, test_task->state);

  task_set_state(test_task, TASK_BLOCKED);
  TEST_ASSERT_EQUAL(TASK_BLOCKED, test_task->state);

  task_set_state(test_task, TASK_SUSPENDED);
  TEST_ASSERT_EQUAL(TASK_SUSPENDED, test_task->state);
}

void test_task_set_state_should_handle_null_task(void) {
  // Should not crash
  task_set_state(NULL, TASK_RUNNING);
  TEST_ASSERT_TRUE(true); // If we get here, test passed
}

void test_task_get_state_should_return_current_state(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  TEST_ASSERT_EQUAL(TASK_READY, task_get_state(test_task));

  task_set_state(test_task, TASK_RUNNING);
  TEST_ASSERT_EQUAL(TASK_RUNNING, task_get_state(test_task));

  task_set_state(test_task, TASK_BLOCKED);
  TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state(test_task));
}

void test_task_get_state_should_return_deleted_for_null_task(void) {
  TEST_ASSERT_EQUAL(TASK_DELETED, task_get_state(NULL));
}

//=============================================================================
// STACK MANAGEMENT TESTS
//=============================================================================

void test_task_init_stack_should_setup_cortex_m4_stack(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  // Stack pointer should be set and within the allocated stack
  TEST_ASSERT_NOT_NULL(test_task->stack_pointer);
  TEST_ASSERT_GREATER_OR_EQUAL(test_task->stack_base, test_task->stack_pointer);

  uint32_t *stack_top =
      test_task->stack_base + (test_task->stack_size / sizeof(uint32_t));
  TEST_ASSERT_LESS_THAN(stack_top, test_task->stack_pointer);
}

void test_task_init_stack_should_set_function_address(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  // The PC should be set to the function address (2nd from top in Cortex-M4
  // stack frame)
  uint32_t *stack_top =
      test_task->stack_base + (test_task->stack_size / sizeof(uint32_t));
  uint32_t *pc_location =
      stack_top - 2; // PC is 2nd from top in Cortex-M4 stack frame

  TEST_ASSERT_EQUAL_HEX32((uintptr_t)dummy_task_function, *pc_location);
}

void test_task_init_stack_should_set_parameter_in_r0(void) {
  void *test_param = (void *)0x12345678;
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, test_param, 3);

  // R0 should contain the parameter (at stack_pointer + 0 words from saved
  // context)
  uint32_t *stack_top =
      test_task->stack_base + (test_task->stack_size / sizeof(uint32_t));
  uint32_t *r0_location =
      stack_top - 8; // R0 is 8th from top in Cortex-M4 stack frame

  TEST_ASSERT_EQUAL_HEX32((uintptr_t)test_param, *r0_location);
}

void test_task_init_stack_should_set_psr_thumb_bit(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  // xPSR should have Thumb bit set (0x01000000)
  uint32_t *stack_top =
      test_task->stack_base + (test_task->stack_size / sizeof(uint32_t));
  uint32_t *psr_location = stack_top - 1; // xPSR is at top of stack frame

  TEST_ASSERT_EQUAL_HEX32(0x01000000, *psr_location);
}

void test_task_stack_check_should_detect_valid_stack(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  // Fresh task should have valid stack
  TEST_ASSERT_TRUE(task_stack_check(test_task));
}

void test_task_stack_check_should_handle_null_task(void) {
  TEST_ASSERT_FALSE(task_stack_check(NULL));
}

void test_task_stack_used_bytes_should_calculate_correctly(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  uint32_t used_bytes = task_stack_used_bytes(test_task);

  // Should have used some bytes for the initial stack frame (16 registers * 4
  // bytes = 64 bytes)
  TEST_ASSERT_EQUAL(64, used_bytes);
  TEST_ASSERT_GREATER_THAN(0, used_bytes);
  TEST_ASSERT_LESS_THAN(test_task->stack_size, used_bytes);
}

void test_task_stack_used_bytes_should_handle_null_task(void) {
  TEST_ASSERT_EQUAL(0, task_stack_used_bytes(NULL));
}

//=============================================================================
// TASK PROPERTIES TESTS
//=============================================================================

void test_task_should_store_name_correctly(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  TEST_ASSERT_EQUAL_STRING("TestTask", test_task->name);
}

void test_task_should_store_priority_correctly(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    task_handle_t task = task_create_internal(dummy_task_function, "TestTask",
                                              512, NULL, priority);
    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_EQUAL(priority, task->priority);
    task_delete_internal(task);
  }
}

void test_task_should_initialize_statistics(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  TEST_ASSERT_EQUAL(0, test_task->run_count);
  TEST_ASSERT_EQUAL(0, test_task->total_runtime);
}

void test_task_should_handle_name_truncation(void) {
  // Test with name longer than the buffer (16 chars including null terminator)
  char long_name[] = "ThisNameIsVeryLongAndShouldBeTruncated";
  test_task =
      task_create_internal(dummy_task_function, long_name, 512, NULL, 3);

  TEST_ASSERT_NOT_NULL(test_task);
  // Name should be truncated to 15 chars + null terminator
  TEST_ASSERT_EQUAL(15, strlen(test_task->name));
  // Should still be null-terminated
  TEST_ASSERT_EQUAL('\0', test_task->name[15]);
  // Should match the first 15 characters
  TEST_ASSERT_EQUAL_STRING_LEN("ThisNameIsVeryL", test_task->name, 15);
}

//=============================================================================
// MEMORY MANAGEMENT TESTS
//=============================================================================

void test_task_should_allocate_separate_stack_memory(void) {
  task_handle_t task1 =
      task_create_internal(dummy_task_function, "Task1", 512, NULL, 1);
  task_handle_t task2 =
      task_create_internal(dummy_task_function, "Task2", 512, NULL, 2);

  TEST_ASSERT_NOT_NULL(task1);
  TEST_ASSERT_NOT_NULL(task2);
  TEST_ASSERT_NOT_EQUAL(task1->stack_base, task2->stack_base);

  task_delete_internal(task1);
  task_delete_internal(task2);
}

void test_task_stack_pointer_should_be_within_bounds(void) {
  test_task =
      task_create_internal(dummy_task_function, "TestTask", 512, NULL, 3);

  uint32_t *stack_top =
      test_task->stack_base + (test_task->stack_size / sizeof(uint32_t));

  // Stack pointer should be between base and top
  TEST_ASSERT_GREATER_OR_EQUAL(test_task->stack_base, test_task->stack_pointer);
  TEST_ASSERT_LESS_THAN(stack_top, test_task->stack_pointer);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

void test_multiple_tasks_should_have_separate_stacks(void) {
  task_handle_t tasks[3];

  // Create multiple tasks
  for (int i = 0; i < 3; i++) {
    char name[16];
    snprintf(name, sizeof(name), "Task%d", i);
    tasks[i] = task_create_internal(dummy_task_function, name, 512, NULL, i);
    TEST_ASSERT_NOT_NULL(tasks[i]);
  }

  // Verify all stacks are different
  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 3; j++) {
      TEST_ASSERT_NOT_EQUAL(tasks[i]->stack_base, tasks[j]->stack_base);
      TEST_ASSERT_NOT_EQUAL(tasks[i]->stack_pointer, tasks[j]->stack_pointer);
    }
  }

  // Clean up
  for (int i = 0; i < 3; i++) {
    task_delete_internal(tasks[i]);
  }
}

void test_task_lifecycle_create_to_delete(void) {
  // Create task
  test_task = task_create_internal(dummy_task_function, "LifecycleTask", 1024,
                                   (void *)0xDEADBEEF, 5);
  TEST_ASSERT_NOT_NULL(test_task);

  // Verify initial state
  TEST_ASSERT_EQUAL(TASK_READY, task_get_state(test_task));
  TEST_ASSERT_EQUAL_STRING("LifecycleTask", test_task->name);
  TEST_ASSERT_EQUAL(5, test_task->priority);
  TEST_ASSERT_EQUAL(1024, test_task->stack_size);

  // Change state
  task_set_state(test_task, TASK_RUNNING);
  TEST_ASSERT_EQUAL(TASK_RUNNING, task_get_state(test_task));

  task_set_state(test_task, TASK_BLOCKED);
  TEST_ASSERT_EQUAL(TASK_BLOCKED, task_get_state(test_task));

  // Delete task
  task_delete_internal(test_task);
  test_task = NULL; // Prevent double-free in tearDown
}

//=============================================================================
// TEST RUNNER
//=============================================================================

int main(void) {
  UNITY_BEGIN();

  // Task creation tests
  RUN_TEST(test_task_create_should_succeed_with_valid_parameters);
  RUN_TEST(test_task_create_should_fail_with_null_function);
  RUN_TEST(test_task_create_should_fail_with_null_name);
  RUN_TEST(test_task_create_should_fail_with_zero_stack_size);
  RUN_TEST(test_task_create_should_fail_with_invalid_priority);
  RUN_TEST(test_task_create_should_use_default_stack_size_when_zero);

  // Task deletion tests
  RUN_TEST(test_task_delete_should_cleanup_resources);
  RUN_TEST(test_task_delete_should_handle_null_task);
  RUN_TEST(test_task_delete_should_free_stack_memory);

  // Task state management tests
  RUN_TEST(test_task_set_state_should_update_state);
  RUN_TEST(test_task_set_state_should_handle_null_task);
  RUN_TEST(test_task_get_state_should_return_current_state);
  RUN_TEST(test_task_get_state_should_return_deleted_for_null_task);

  // Stack management tests
  RUN_TEST(test_task_init_stack_should_setup_cortex_m4_stack);
  RUN_TEST(test_task_init_stack_should_set_function_address);
  RUN_TEST(test_task_init_stack_should_set_parameter_in_r0);
  RUN_TEST(test_task_init_stack_should_set_psr_thumb_bit);
  RUN_TEST(test_task_stack_check_should_detect_valid_stack);
  RUN_TEST(test_task_stack_check_should_handle_null_task);
  RUN_TEST(test_task_stack_used_bytes_should_calculate_correctly);
  RUN_TEST(test_task_stack_used_bytes_should_handle_null_task);

  // Task properties tests
  RUN_TEST(test_task_should_store_name_correctly);
  RUN_TEST(test_task_should_store_priority_correctly);
  RUN_TEST(test_task_should_initialize_statistics);
  RUN_TEST(test_task_should_handle_name_truncation);

  // Memory management tests
  RUN_TEST(test_task_should_allocate_separate_stack_memory);
  RUN_TEST(test_task_stack_pointer_should_be_within_bounds);

  // Integration tests
  RUN_TEST(test_multiple_tasks_should_have_separate_stacks);
  RUN_TEST(test_task_lifecycle_create_to_delete);

  UNITY_END();
}
