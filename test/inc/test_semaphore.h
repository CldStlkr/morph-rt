#ifndef TEST_SEMAPHORE_H
#define TEST_SEMAPHORE_H

//=============================================================================
// SEMAPHORE TEST DECLARATIONS
//=============================================================================

// Semaphore creation/deletion tests
void test_sem_create_should_succeed_with_valid_parameters(void);
void test_sem_create_should_fail_with_zero_max_count(void);
void test_sem_create_should_fail_with_initial_greater_than_max(void);
void test_sem_create_should_handle_null_name(void);
void test_sem_create_should_fail_when_pools_exhausted(void);
void test_sem_delete_should_handle_null_semaphore(void);
void test_sem_delete_should_cleanup_resources(void);

// Binary semaphore tests
void test_sem_create_binary_should_work(void);
void test_binary_semaphore_basic_operations(void);

// Counting semaphore tests
void test_sem_create_counting_should_work(void);
void test_counting_semaphore_basic_operations(void);

// Wait/post operation tests
void test_sem_wait_should_succeed_when_tokens_available(void);
void test_sem_wait_should_timeout_when_no_tokens(void);
void test_sem_wait_should_return_timeout_when_deadline_exceeded(void);
void test_sem_wait_should_handle_semaphore_deletion(void);
void test_sem_post_should_increment_count(void);
void test_sem_post_should_prevent_overflow(void);

// Error handling tests
void test_sem_operations_should_handle_null_semaphore(void);
void test_sem_try_wait_should_not_block(void);

// Utility function tests
void test_sem_get_count_should_return_current_count(void);
void test_sem_has_waiting_tasks_should_detect_waiters(void);

// Integration tests
void test_sem_should_integrate_with_memory_pools(void);
void test_semaphore_resource_lifecycle(void);
void test_multiple_semaphores_should_be_independent(void);

// Macro tests
void test_binary_semaphore_macro(void);
void test_counting_semaphore_macro(void);

// Module setup/teardown and test runner
void semaphore_test_setup(void);
void semaphore_test_teardown(void);
void run_semaphore_tests(void);

#endif // TEST_SEMAPHORE_H
