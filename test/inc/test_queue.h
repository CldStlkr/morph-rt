#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H

//=============================================================================
// QUEUE MANAGEMENT TEST DECLARATIONS
//=============================================================================

// Queue creation/deletion tests
void test_queue_create_should_succeed_with_valid_parameters(void);
void test_queue_create_should_fail_with_zero_length(void);
void test_queue_create_should_fail_with_zero_item_size(void);
void test_queue_create_should_fail_when_pools_exhausted(void);
void test_queue_create_should_select_appropriate_buffer_pool(void);
void test_queue_delete_should_handle_null_queue(void);
void test_queue_delete_should_cleanup_resources(void);

// Basic send/receive tests (immediate - no blocking)
void test_queue_send_receive_immediate_single_item(void);
void test_queue_send_receive_immediate_multiple_items(void);
void test_queue_should_maintain_fifo_order(void);
void test_queue_send_immediate_should_fail_when_full(void);
void test_queue_receive_immediate_should_fail_when_empty(void);

// Queue state tests
void test_queue_is_empty_should_detect_empty_state(void);
void test_queue_is_full_should_detect_full_state(void);
void test_queue_messages_waiting_should_count_correctly(void);
void test_queue_state_functions_should_handle_null_queue(void);

// Error handling tests
void test_queue_send_should_handle_null_parameters(void);
void test_queue_receive_should_handle_null_parameters(void);
void test_queue_send_immediate_should_handle_null_parameters(void);
void test_queue_receive_immediate_should_handle_null_parameters(void);

// Blocking behavior tests (with mocked scheduler)
void test_queue_send_with_zero_timeout_should_not_block(void);
void test_queue_receive_with_zero_timeout_should_not_block(void);
void test_queue_send_should_return_timeout_when_deadline_exceeded(void);
void test_queue_receive_should_return_timeout_when_deadline_exceeded(void);

// Integration tests
void test_queue_should_integrate_with_memory_pools(void);
void test_queue_full_lifecycle_send_receive_pattern(void);

// Capacity and buffer management
void test_queue_should_use_power_of_two_capacity(void);
void test_queue_should_handle_wraparound_correctly(void);

// Module setup/teardown and test runner
void queue_test_setup(void);
void queue_test_teardown(void);
void run_queue_tests(void);

#endif // TEST_QUEUE_H
