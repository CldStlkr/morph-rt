#ifndef TEST_MUTEX_H
#define TEST_MUTEX_H

//=============================================================================
// MUTEX TEST DECLARATIONS
//=============================================================================

// Mutex creation/deletion tests
void test_mutex_create_should_succeed_with_valid_parameters(void);
void test_mutex_create_should_handle_null_name(void);
void test_mutex_create_should_fail_when_pools_exhausted(void);
void test_mutex_delete_should_handle_null_mutex(void);
void test_mutex_delete_should_cleanup_resources(void);
void test_mutex_delete_should_wake_waiting_tasks(void);

// Basic mutex operation tests
void test_mutex_lock_should_succeed_when_available(void);
void test_mutex_lock_should_fail_when_already_owned_by_current_task(void);
void test_mutex_unlock_should_succeed_when_owned(void);
void test_mutex_unlock_should_fail_when_not_owned(void);
void test_mutex_try_lock_should_not_block(void);

// Timeout handling tests
void test_mutex_lock_should_timeout_when_unavailable(void);
void test_mutex_lock_should_timeout_when_deadline_exceeded(void);
void test_mutex_lock_should_handle_mutex_deletion_while_waiting(void);
void test_mutex_lock_should_handle_infinite_timeout(void);

// Ownership tests
void test_mutex_get_owner_should_return_current_owner(void);
void test_mutex_get_owner_should_return_null_when_unowned(void);
void test_mutex_is_locked_should_reflect_lock_state(void);

// Priority inheritance tests
void test_mutex_should_apply_priority_inheritance(void);
void test_mutex_should_restore_priority_on_unlock(void);
void test_mutex_should_restore_priority_on_delete(void);

// Waiting tasks tests
void test_mutex_has_waiting_tasks_should_detect_waiters(void);
void test_mutex_should_wake_one_waiter_on_unlock(void);

// Error handling tests
void test_mutex_operations_should_handle_null_mutex(void);
void test_mutex_unlock_should_fail_for_non_owner(void);

// Integration tests
void test_mutex_should_integrate_with_memory_pools(void);
void test_mutex_resource_lifecycle(void);
void test_multiple_mutexes_should_be_independent(void);

// Edge case tests
void test_mutex_recursive_lock_should_fail(void);
void test_mutex_unlock_without_lock_should_fail(void);

// Module setup/teardown and test runner
void mutex_test_setup(void);
void mutex_test_teardown(void);
void run_mutex_tests(void);

#endif // TEST_MUTEX_H
