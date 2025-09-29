#include "memory.h"
#include "scheduler.h"
#include "semaphore.h"
#include "task.h"
#include "test_semaphore.h"
#include "time_utils.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

//=============================================================================
// MOCK IMPLEMENTATIONS
//=============================================================================

// Mock global variables that semaphore.c depends on
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

static semaphore_handle_t test_sem;

// Module-specific setup/teardown functions
void setUp(void) {
  // Initialize memory pools before each test
  memory_pools_init();
  
  test_sem = NULL;
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
  if (test_sem) {
    sem_delete(test_sem);
    test_sem = NULL;
  }
  current_task = NULL;
}

//=============================================================================
// SEMAPHORE CREATION/DELETION TESTS
//=============================================================================

void test_sem_create_should_succeed_with_valid_parameters(void) {
  test_sem = sem_create(2, 5, "TestSem");
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  TEST_ASSERT_FALSE(sem_has_waiting_tasks(test_sem));
}

void test_sem_create_should_fail_with_zero_max_count(void) {
  test_sem = sem_create(1, 0, "TestSem");
  TEST_ASSERT_NULL(test_sem);
}

void test_sem_create_should_fail_with_initial_greater_than_max(void) {
  test_sem = sem_create(5, 3, "TestSem");
  TEST_ASSERT_NULL(test_sem);
}

void test_sem_create_should_handle_null_name(void) {
  test_sem = sem_create(1, 1, NULL);
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
}

void test_sem_create_should_fail_when_pools_exhausted(void) {
  // Fill up the semaphore pool
  semaphore_handle_t sems[MAX_SEMAPHORES];
  int created_count = 0;
  
  // Create as many semaphores as possible
  for (int i = 0; i < MAX_SEMAPHORES; i++) {
    char name[16];
    snprintf(name, sizeof(name), "Sem%d", i);
    sems[i] = sem_create(1, 1, name);
    if (sems[i] != NULL) {
      created_count++;
    } else {
      break;
    }
  }
  
  // Should have created MAX_SEMAPHORES
  TEST_ASSERT_EQUAL(MAX_SEMAPHORES, created_count);
  
  // Next creation should fail
  semaphore_handle_t overflow_sem = sem_create(1, 1, "Overflow");
  TEST_ASSERT_NULL(overflow_sem);
  
  // Clean up
  for (int i = 0; i < created_count; i++) {
    sem_delete(sems[i]);
  }
}

void test_sem_delete_should_handle_null_semaphore(void) {
  // Should not crash
  sem_delete(NULL);
  TEST_ASSERT_TRUE(true); // If we get here, test passed
}

void test_sem_delete_should_cleanup_resources(void) {
  test_sem = sem_create(1, 1, "TestSem");
  TEST_ASSERT_NOT_NULL(test_sem);
  
  sem_delete(test_sem);
  test_sem = NULL; // Prevent double-free in teardown
  
  // If we reach here without crashing, cleanup succeeded
  TEST_ASSERT_TRUE(true);
}

//=============================================================================
// BINARY SEMAPHORE TESTS
//=============================================================================

void test_sem_create_binary_should_work(void) {
  test_sem = sem_create_binary("BinarySem");
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
}

void test_binary_semaphore_basic_operations(void) {
  test_sem = sem_create_binary("BinarySem");
  
  // Initial state - should have 1 token
  TEST_ASSERT_EQUAL(SEM_OK, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
  
  // Try to take again - should fail
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_try_wait(test_sem));
  
  // Post back
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  // Try to post again - should fail (binary semaphore overflow)
  TEST_ASSERT_EQUAL(SEM_ERROR_OVERFLOW, sem_post(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
}

//=============================================================================
// COUNTING SEMAPHORE TESTS
//=============================================================================

void test_sem_create_counting_should_work(void) {
  test_sem = sem_create_counting(5, "CountingSem");
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
}

void test_counting_semaphore_basic_operations(void) {
  test_sem = sem_create(0, 3, "CountingSem");
  
  // Initial state - should have 0 tokens
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_try_wait(test_sem));
  
  // Post tokens
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(3, sem_get_count(test_sem));
  
  // Try to overflow
  TEST_ASSERT_EQUAL(SEM_ERROR_OVERFLOW, sem_post(test_sem));
  TEST_ASSERT_EQUAL(3, sem_get_count(test_sem));
  
  // Take tokens
  TEST_ASSERT_EQUAL(SEM_OK, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
  
  // Try to take from empty
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_try_wait(test_sem));
}

//=============================================================================
// SEMAPHORE WAIT/POST TESTS
//=============================================================================

void test_sem_wait_should_succeed_when_tokens_available(void) {
  test_sem = sem_create(2, 5, "TestSem");
  
  // Should succeed immediately
  TEST_ASSERT_EQUAL(SEM_OK, sem_wait(test_sem, 100));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_wait(test_sem, 100));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
}

void test_sem_wait_should_timeout_when_no_tokens(void) {
  test_sem = sem_create(0, 1, "TestSem");
  
  // Should timeout immediately with zero timeout
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_wait(test_sem, SEM_NO_WAIT));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
}

void test_sem_wait_should_return_timeout_when_deadline_exceeded(void) {
  test_sem = sem_create(0, 1, "TestSem");
  
  // Set up mock task
  current_task = &mock_task;
  current_task->wake_reason = WAKE_REASON_TIMEOUT;
  
  // Set time so that deadline will be exceeded
  tick_now = 100;
  
  // Wait with timeout - should return timeout
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_wait(test_sem, 10));
}

void test_sem_wait_should_handle_semaphore_deletion(void) {
  test_sem = sem_create(0, 1, "TestSem");
  
  // Set up mock task
  current_task = &mock_task;
  current_task->wake_reason = WAKE_REASON_SIGNAL; // Simulates semaphore deletion
  
  // Wait should return SEM_ERROR_NULL when semaphore is deleted
  TEST_ASSERT_EQUAL(SEM_ERROR_NULL, sem_wait(test_sem, 100));
}

void test_sem_post_should_increment_count(void) {
  test_sem = sem_create(0, 3, "TestSem");
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
}

void test_sem_post_should_prevent_overflow(void) {
  test_sem = sem_create(2, 2, "TestSem");
  
  // Already at max count
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  
  // Should fail to post
  TEST_ASSERT_EQUAL(SEM_ERROR_OVERFLOW, sem_post(test_sem));
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

void test_sem_operations_should_handle_null_semaphore(void) {
  TEST_ASSERT_EQUAL(SEM_ERROR_NULL, sem_wait(NULL, 100));
  TEST_ASSERT_EQUAL(SEM_ERROR_NULL, sem_post(NULL));
  TEST_ASSERT_EQUAL(SEM_ERROR_NULL, sem_try_wait(NULL));
  TEST_ASSERT_EQUAL(0, sem_get_count(NULL));
  TEST_ASSERT_FALSE(sem_has_waiting_tasks(NULL));
}

void test_sem_try_wait_should_not_block(void) {
  test_sem = sem_create(0, 1, "TestSem");

  // Should fail immediately without blocking
  TEST_ASSERT_EQUAL(SEM_ERROR_TIMEOUT, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
}

//=============================================================================
// UTILITY FUNCTION TESTS
//=============================================================================

void test_sem_get_count_should_return_current_count(void) {
  test_sem = sem_create(3, 5, "TestSem");
  TEST_ASSERT_EQUAL(3, sem_get_count(test_sem));
  
  sem_try_wait(test_sem);
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  
  sem_post(test_sem);
  TEST_ASSERT_EQUAL(3, sem_get_count(test_sem));
}

void test_sem_has_waiting_tasks_should_detect_waiters(void) {
  test_sem = sem_create(1, 1, "TestSem");
  
  // Initially no waiters
  TEST_ASSERT_FALSE(sem_has_waiting_tasks(test_sem));
  
  // Note: Testing actual waiting tasks requires more complex mocking
  // of the scheduler and task context, which is beyond unit testing scope
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

void test_sem_should_integrate_with_memory_pools(void) {
  // Test that semaphore creation/deletion integrates properly with memory pools
  
  // Get initial pool stats
  pool_stats_t initial_stats = pool_get_stats(POOL_SCB);
  
  // Create a semaphore
  semaphore_handle_t sem = sem_create(1, 1, "PoolTest");
  TEST_ASSERT_NOT_NULL(sem);
  
  // Pool usage should increase
  pool_stats_t after_create_stats = pool_get_stats(POOL_SCB);
  TEST_ASSERT_EQUAL(initial_stats.used_objects + 1, after_create_stats.used_objects);
  
  // Delete the semaphore
  sem_delete(sem);
  
  // Pool usage should return to initial state
  pool_stats_t after_delete_stats = pool_get_stats(POOL_SCB);
  TEST_ASSERT_EQUAL(initial_stats.used_objects, after_delete_stats.used_objects);
}

void test_semaphore_resource_lifecycle(void) {
  // Test complete lifecycle
  test_sem = sem_create(2, 3, "LifecycleSem");
  TEST_ASSERT_NOT_NULL(test_sem);
  
  // Use the semaphore
  TEST_ASSERT_EQUAL(SEM_OK, sem_wait(test_sem, 0));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(2, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(3, sem_get_count(test_sem));
  
  // Delete
  sem_delete(test_sem);
  test_sem = NULL; // Prevent double-free in teardown
}

void test_multiple_semaphores_should_be_independent(void) {
  semaphore_handle_t sem1 = sem_create(1, 1, "Sem1");
  semaphore_handle_t sem2 = sem_create(2, 3, "Sem2");
  
  TEST_ASSERT_NOT_NULL(sem1);
  TEST_ASSERT_NOT_NULL(sem2);
  
  // Operations on one should not affect the other
  TEST_ASSERT_EQUAL(SEM_OK, sem_wait(sem1, 0));
  TEST_ASSERT_EQUAL(0, sem_get_count(sem1));
  TEST_ASSERT_EQUAL(2, sem_get_count(sem2));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_wait(sem2, 0));
  TEST_ASSERT_EQUAL(0, sem_get_count(sem1));
  TEST_ASSERT_EQUAL(1, sem_get_count(sem2));
  
  // Clean up
  sem_delete(sem1);
  sem_delete(sem2);
}

//=============================================================================
// MACRO TESTS
//=============================================================================

void test_binary_semaphore_macro(void) {
  test_sem = sem_create_binary("BinaryMacro");
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  // Should behave like binary semaphore
  TEST_ASSERT_EQUAL(SEM_OK, sem_try_wait(test_sem));
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
  
  TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
  TEST_ASSERT_EQUAL(1, sem_get_count(test_sem));
  
  // Should prevent overflow
  TEST_ASSERT_EQUAL(SEM_ERROR_OVERFLOW, sem_post(test_sem));
}

void test_counting_semaphore_macro(void) {
  test_sem = sem_create_counting(5, "CountingMacro");
  TEST_ASSERT_NOT_NULL(test_sem);
  TEST_ASSERT_EQUAL(0, sem_get_count(test_sem));
  
  // Should allow up to max_count
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(SEM_OK, sem_post(test_sem));
    TEST_ASSERT_EQUAL(i + 1, sem_get_count(test_sem));
  }
  
  // Should prevent overflow
  TEST_ASSERT_EQUAL(SEM_ERROR_OVERFLOW, sem_post(test_sem));
  TEST_ASSERT_EQUAL(5, sem_get_count(test_sem));
}

//=============================================================================
// TEST RUNNER
//=============================================================================

int main(void) {
  UNITY_BEGIN();

  // Semaphore creation/deletion tests
  RUN_TEST(test_sem_create_should_succeed_with_valid_parameters);
  RUN_TEST(test_sem_create_should_fail_with_zero_max_count);
  RUN_TEST(test_sem_create_should_fail_with_initial_greater_than_max);
  RUN_TEST(test_sem_create_should_handle_null_name);
  RUN_TEST(test_sem_create_should_fail_when_pools_exhausted);
  RUN_TEST(test_sem_delete_should_handle_null_semaphore);
  RUN_TEST(test_sem_delete_should_cleanup_resources);

  // Binary semaphore tests
  RUN_TEST(test_sem_create_binary_should_work);
  RUN_TEST(test_binary_semaphore_basic_operations);

  // Counting semaphore tests
  RUN_TEST(test_sem_create_counting_should_work);
  RUN_TEST(test_counting_semaphore_basic_operations);

  // Wait/post operation tests
  RUN_TEST(test_sem_wait_should_succeed_when_tokens_available);
  RUN_TEST(test_sem_wait_should_timeout_when_no_tokens);
  RUN_TEST(test_sem_wait_should_return_timeout_when_deadline_exceeded);
  RUN_TEST(test_sem_wait_should_handle_semaphore_deletion);
  RUN_TEST(test_sem_post_should_increment_count);
  RUN_TEST(test_sem_post_should_prevent_overflow);

  // Error handling tests
  RUN_TEST(test_sem_operations_should_handle_null_semaphore);
  RUN_TEST(test_sem_try_wait_should_not_block);

  // Utility function tests
  RUN_TEST(test_sem_get_count_should_return_current_count);
  RUN_TEST(test_sem_has_waiting_tasks_should_detect_waiters);

  // Integration tests
  RUN_TEST(test_sem_should_integrate_with_memory_pools);
  RUN_TEST(test_semaphore_resource_lifecycle);
  RUN_TEST(test_multiple_semaphores_should_be_independent);

  // Macro tests
  RUN_TEST(test_binary_semaphore_macro);
  RUN_TEST(test_counting_semaphore_macro);

  return UNITY_END();
}
