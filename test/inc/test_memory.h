#ifndef TEST_MEMORY_H
#define TEST_MEMORY_H

//=============================================================================
// MEMORY POOL TEST DECLARATIONS
//=============================================================================

// Initialization tests
void test_memory_pools_init_should_initialize_all_pools(void);
void test_memory_pools_init_should_set_correct_pool_sizes(void);

// Basic allocation/deallocation tests
void test_pool_alloc_should_return_valid_pointer(void);
void test_pool_alloc_should_return_zeroed_memory(void);
void test_pool_alloc_should_return_different_pointers(void);
void test_pool_free_should_return_true_for_valid_pointer(void);
void test_pool_free_should_return_false_for_null_pointer(void);
void test_pool_free_should_return_false_for_invalid_pointer(void);
void test_pool_free_should_return_false_for_double_free(void);

// Pool exhaustion tests
void test_pool_alloc_should_fail_when_pool_exhausted(void);
void test_pool_alloc_should_succeed_after_free(void);

// Peak usage tracking tests
void test_pool_should_track_peak_usage(void);

// Type-specific allocation tests
void test_task_pool_alloc_tcb_should_return_task_control_block(void);
void test_task_pool_alloc_stack_should_select_appropriate_size(void);

// Error handling tests
void test_pool_alloc_should_handle_invalid_pool_type(void);
void test_pool_free_should_handle_invalid_pool_type(void);
void test_pool_get_stats_should_handle_invalid_pool_type(void);

// Stress tests
void test_pool_should_handle_allocation_deallocation_cycles(void);
void test_pool_should_maintain_consistency_across_multiple_types(void);

// Integration tests
void test_task_stack_allocation_integration(void);

// Module setup/teardown
void memory_test_setup(void);
void memory_test_teardown(void);
void run_memory_tests(void);

#endif // TEST_MEMORY_H
