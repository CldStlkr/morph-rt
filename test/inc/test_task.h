#ifndef TEST_TASK_H
#define TEST_TASK_H

//=============================================================================
// TASK MANAGEMENT TEST DECLARATIONS
//=============================================================================

// Task creation tests
void test_task_create_should_succeed_with_valid_parameters(void);
void test_task_create_should_fail_with_null_function(void);
void test_task_create_should_fail_with_null_name(void);
void test_task_create_should_fail_with_zero_stack_size(void);
void test_task_create_should_fail_with_invalid_priority(void);
void test_task_create_should_use_default_stack_size_when_zero(void);

// Task deletion tests
void test_task_delete_should_cleanup_resources(void);
void test_task_delete_should_handle_null_task(void);
void test_task_delete_should_free_stack_memory(void);

// Task state management tests
void test_task_set_state_should_update_state(void);
void test_task_set_state_should_handle_null_task(void);
void test_task_get_state_should_return_current_state(void);
void test_task_get_state_should_return_deleted_for_null_task(void);

// Stack management tests
void test_task_init_stack_should_setup_cortex_m4_stack(void);
void test_task_init_stack_should_set_function_address(void);
void test_task_init_stack_should_set_parameter_in_r0(void);
void test_task_init_stack_should_set_psr_thumb_bit(void);
void test_task_stack_check_should_detect_valid_stack(void);
void test_task_stack_check_should_handle_null_task(void);
void test_task_stack_used_bytes_should_calculate_correctly(void);
void test_task_stack_used_bytes_should_handle_null_task(void);

// Task properties tests
void test_task_should_store_name_correctly(void);
void test_task_should_store_priority_correctly(void);
void test_task_should_initialize_statistics(void);
void test_task_should_handle_name_truncation(void);

// Memory management tests
void test_task_should_allocate_separate_stack_memory(void);
void test_task_stack_pointer_should_be_within_bounds(void);

// Integration tests (testing interactions)
void test_multiple_tasks_should_have_separate_stacks(void);
void test_task_lifecycle_create_to_delete(void);

void task_test_setup(void);
void task_test_teardown(void);
void run_task_tests(void);

#endif // TEST_TASK_H
