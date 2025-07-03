#ifndef TEST_CIRCULAR_BUFFER_H
#define TEST_CIRCULAR_BUFFER_H

//=============================================================================
// CIRCULAR BUFFER TEST DECLARATIONS
//=============================================================================

// Basic functionality tests
void test_cb_init_should_succeed_with_valid_parameters(void);
void test_cb_init_should_fail_with_null_pointer(void);
void test_cb_init_should_fail_with_zero_capacity(void);
void test_cb_init_should_fail_with_zero_element_size(void);

// Core operations tests
void test_cb_put_and_get_single_element(void);
void test_cb_should_maintain_fifo_order(void);

// Edge case tests
void test_cb_should_handle_buffer_full_condition(void);
void test_cb_should_handle_buffer_empty_condition(void);
void test_cb_should_handle_wraparound(void);

// Additional functionality tests
void test_cb_peek_should_not_modify_buffer(void);
void test_cb_capacity_functions(void);

// Test suite runner function (optional convenience function)
void run_circular_buffer_tests(void);

#endif // TEST_CIRCULAR_BUFFER_H
