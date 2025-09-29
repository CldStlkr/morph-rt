#ifndef TEST_CIRCULAR_BUFFER_H
#define TEST_CIRCULAR_BUFFER_H

//=============================================================================
// CIRCULAR BUFFER TEST DECLARATIONS
//=============================================================================

// Basic functionality tests
void test_cb_init_should_succeed_with_valid_parameters(void);
void test_cb_init_should_fail_with_null_cb_pointer(void);
void test_cb_init_should_fail_with_null_buffer_pointer(void);
void test_cb_init_should_fail_with_zero_capacity(void);
void test_cb_init_should_fail_with_zero_element_size(void);
void test_cb_init_should_round_up_to_power_of_two(void);

// Core operations tests
void test_cb_put_and_get_single_element(void);
void test_cb_should_maintain_fifo_order(void);

// Edge case tests
void test_cb_should_handle_buffer_full_condition(void);
void test_cb_should_handle_buffer_empty_condition(void);
void test_cb_should_handle_wraparound(void);

// Additional functionality tests
void test_cb_peek_should_not_modify_buffer(void);
void test_cb_deinit_should_clear_structure(void);
void test_cb_deinit_should_handle_null_pointer(void);

// Error handling tests
void test_cb_operations_should_handle_null_pointers(void);

// Flexibility tests
void test_cb_should_handle_different_element_sizes(void);

// Module setup/teardown and test runner
void cb_test_setup(void);
void cb_test_teardown(void);
void run_circular_buffer_tests(void);

#endif // TEST_CIRCULAR_BUFFER_H
