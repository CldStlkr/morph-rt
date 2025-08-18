#include "circular_buffer.h"
#include "test_circular_buffer.h"
#include "unity.h"
#include <string.h>

// Test fixture - global variable for the circular buffer
static circular_buffer_t test_cb;

// Unity requires these functions
void setUp(void) {
  // Called before each test
  // Initialize clean state
  memset(&test_cb, 0, sizeof(test_cb));
}

void tearDown(void) {
  // Called after each test
  // Clean up any allocated resources
  cb_free(&test_cb);
}

//=============================================================================
// CIRCULAR BUFFER TESTS
//=============================================================================

void test_cb_init_should_succeed_with_valid_parameters(void) {
  cb_result_t result = cb_init(&test_cb, 8, sizeof(int));

  TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  TEST_ASSERT_NOT_NULL(test_cb.buffer);
  TEST_ASSERT_EQUAL(8, test_cb.capacity);
  TEST_ASSERT_EQUAL(sizeof(int), test_cb.element_size);
  TEST_ASSERT_EQUAL(0, test_cb.size);
  TEST_ASSERT_EQUAL(0, test_cb.head);
  TEST_ASSERT_EQUAL(0, test_cb.tail);
}

void test_cb_init_should_fail_with_null_pointer(void) {
  cb_result_t result = cb_init(NULL, 8, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, result);
}

void test_cb_init_should_fail_with_zero_capacity(void) {
  cb_result_t result = cb_init(&test_cb, 0, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, result);
}

void test_cb_init_should_fail_with_zero_element_size(void) {
  cb_result_t result = cb_init(&test_cb, 8, 0);
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, result);
}

void test_cb_put_and_get_single_element(void) {
  // Initialize buffer
  cb_result_t init_result = cb_init(&test_cb, 4, sizeof(int));
  TEST_ASSERT_EQUAL(CB_SUCCESS, init_result);

  int test_value = 42;
  int retrieved_value = 0;

  // Test put
  cb_result_t put_result = cb_put(&test_cb, &test_value);
  TEST_ASSERT_EQUAL(CB_SUCCESS, put_result);
  TEST_ASSERT_EQUAL(1, cb_size(&test_cb));
  TEST_ASSERT_FALSE(cb_is_empty(&test_cb));
  TEST_ASSERT_FALSE(cb_is_full(&test_cb));

  // Test get
  cb_result_t get_result = cb_get(&test_cb, &retrieved_value);
  TEST_ASSERT_EQUAL(CB_SUCCESS, get_result);
  TEST_ASSERT_EQUAL(test_value, retrieved_value);
  TEST_ASSERT_EQUAL(0, cb_size(&test_cb));
  TEST_ASSERT_TRUE(cb_is_empty(&test_cb));
}

void test_cb_should_maintain_fifo_order(void) {
  // Initialize buffer
  cb_init(&test_cb, 4, sizeof(int));

  int input_values[] = {10, 20, 30};
  int output_values[3] = {0};

  // Put multiple values
  for (int i = 0; i < 3; ++i) {
    cb_result_t result = cb_put(&test_cb, &input_values[i]);
    TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  }

  TEST_ASSERT_EQUAL(3, cb_size(&test_cb));

  // Get values and verify FIFO order
  for (int i = 0; i < 3; ++i) {
    cb_result_t result = cb_get(&test_cb, &output_values[i]);
    TEST_ASSERT_EQUAL(CB_SUCCESS, result);
    TEST_ASSERT_EQUAL(input_values[i], output_values[i]);
  }
}

void test_cb_should_handle_buffer_full_condition(void) {
  // Initialize small buffer
  cb_init(&test_cb, 4, sizeof(int));

  int values[] = {1, 2, 3, 4};

  // Fill the buffer completely
  for (int i = 0; i < 4; ++i) {
    cb_result_t result = cb_put(&test_cb, &values[i]);
    TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  }

  TEST_ASSERT_TRUE(cb_is_full(&test_cb));
  TEST_ASSERT_EQUAL(4, cb_size(&test_cb));

  // Try to overfill
  int overflow_value = 5;
  cb_result_t overflow_result = cb_put(&test_cb, &overflow_value);
  TEST_ASSERT_EQUAL(CB_ERROR_BUFFER_FULL, overflow_result);

  // Size should remain unchanged
  TEST_ASSERT_EQUAL(4, cb_size(&test_cb));
}

void test_cb_should_handle_buffer_empty_condition(void) {
  // Initialize buffer
  cb_init(&test_cb, 4, sizeof(int));

  // Buffer should be empty initially
  TEST_ASSERT_TRUE(cb_is_empty(&test_cb));

  // Try to get from empty buffer
  int value = 0;
  cb_result_t result = cb_get(&test_cb, &value);
  TEST_ASSERT_EQUAL(CB_ERROR_BUFFER_EMPTY, result);
}

void test_cb_should_handle_wraparound(void) {
  // Initialize buffer
  cb_init(&test_cb, 4, sizeof(int));

  // Fill buffer completely
  for (int i = 0; i < 4; ++i) {
    int value = i + 100; // 100, 101, 102, 103
    cb_put(&test_cb, &value);
  }

  // Now do several add/remove cycles to force wraparound
  for (int cycle = 0; cycle < 5; ++cycle) {
    // Remove one element
    int removed_value;
    cb_result_t get_result = cb_get(&test_cb, &removed_value);
    TEST_ASSERT_EQUAL(CB_SUCCESS, get_result);

    // Add one element
    int new_value = 200 + cycle;
    cb_result_t put_result = cb_put(&test_cb, &new_value);
    TEST_ASSERT_EQUAL(CB_SUCCESS, put_result);

    // Buffer should still have 4 elements
    TEST_ASSERT_EQUAL(4, cb_size(&test_cb));
    TEST_ASSERT_TRUE(cb_is_full(&test_cb));
  }

  // Verify we can still get elements in FIFO order
  int final_value;
  cb_result_t result = cb_get(&test_cb, &final_value);
  TEST_ASSERT_EQUAL(CB_SUCCESS, result);
}

void test_cb_peek_should_not_modify_buffer(void) {
  // Initialize buffer
  cb_init(&test_cb, 4, sizeof(int));

  int test_value = 99;
  cb_put(&test_cb, &test_value);

  size_t size_before = cb_size(&test_cb);

  // Peek at the value
  int peeked_value = 0;
  cb_result_t peek_result = cb_peek(&test_cb, &peeked_value);
  TEST_ASSERT_EQUAL(CB_SUCCESS, peek_result);
  TEST_ASSERT_EQUAL(test_value, peeked_value);

  // Buffer should be unchanged
  TEST_ASSERT_EQUAL(size_before, cb_size(&test_cb));
  TEST_ASSERT_FALSE(cb_is_empty(&test_cb));

  // Should still be able to get the same value
  int actual_value = 0;
  cb_result_t get_result = cb_get(&test_cb, &actual_value);
  TEST_ASSERT_EQUAL(CB_SUCCESS, get_result);
  TEST_ASSERT_EQUAL(test_value, actual_value);
}

void test_cb_capacity_functions(void) {
  cb_init(&test_cb, 8, sizeof(int));

  TEST_ASSERT_EQUAL(8, cb_capacity(&test_cb));
  TEST_ASSERT_EQUAL(0, cb_size(&test_cb));
  TEST_ASSERT_EQUAL(8, cb_available(&test_cb));

  // Add some elements
  int value = 1;
  cb_put(&test_cb, &value);
  cb_put(&test_cb, &value);

  TEST_ASSERT_EQUAL(8, cb_capacity(&test_cb));
  TEST_ASSERT_EQUAL(2, cb_size(&test_cb));
  TEST_ASSERT_EQUAL(6, cb_available(&test_cb));
}

//=============================================================================
// TEST RUNNER
//=============================================================================

void run_circular_buffer_tests(void) {

  // Basic functionality tests
  RUN_TEST(test_cb_init_should_succeed_with_valid_parameters);
  RUN_TEST(test_cb_init_should_fail_with_null_pointer);
  RUN_TEST(test_cb_init_should_fail_with_zero_capacity);
  RUN_TEST(test_cb_init_should_fail_with_zero_element_size);

  // Core operations
  RUN_TEST(test_cb_put_and_get_single_element);
  RUN_TEST(test_cb_should_maintain_fifo_order);

  // Edge cases
  RUN_TEST(test_cb_should_handle_buffer_full_condition);
  RUN_TEST(test_cb_should_handle_buffer_empty_condition);
  RUN_TEST(test_cb_should_handle_wraparound);

  // Additional functionality
  RUN_TEST(test_cb_peek_should_not_modify_buffer);
  RUN_TEST(test_cb_capacity_functions);
}
