#include "circular_buffer.h"
#include "test_circular_buffer.h"
#include "unity.h"
#include <string.h>

// Test fixture - global variable for the circular buffer
static circular_buffer_t test_cb;
static uint8_t test_buffer[256]; // Static buffer for testing

// Unity requires these functions
void setUp(void) {
  // Called before each test
  // Initialize clean state
  memset(&test_cb, 0, sizeof(test_cb));
  memset(test_buffer, 0, sizeof(test_buffer));
}

void tearDown(void) {
  // Called after each test
  // Clean up - just deinitialize the circular buffer
  cb_deinit(&test_cb);
}

//=============================================================================
// CIRCULAR BUFFER TESTS
//=============================================================================

void test_cb_init_should_succeed_with_valid_parameters(void) {
  cb_result_t result = cb_init(&test_cb, test_buffer, 8, sizeof(int));

  TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  TEST_ASSERT_EQUAL(test_buffer, test_cb.buffer);
  TEST_ASSERT_EQUAL(8, test_cb.capacity);
  TEST_ASSERT_EQUAL(sizeof(int), test_cb.element_size);
  TEST_ASSERT_EQUAL(0, test_cb.size);
  TEST_ASSERT_EQUAL(0, test_cb.head);
  TEST_ASSERT_EQUAL(0, test_cb.tail);
}

void test_cb_init_should_fail_with_null_cb_pointer(void) {
  cb_result_t result = cb_init(NULL, test_buffer, 8, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, result);
}

void test_cb_init_should_fail_with_null_buffer_pointer(void) {
  cb_result_t result = cb_init(&test_cb, NULL, 8, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, result);
}

void test_cb_init_should_fail_with_zero_capacity(void) {
  cb_result_t result = cb_init(&test_cb, test_buffer, 0, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_INVALID_SIZE, result);
}

void test_cb_init_should_fail_with_zero_element_size(void) {
  cb_result_t result = cb_init(&test_cb, test_buffer, 8, 0);
  TEST_ASSERT_EQUAL(CB_ERROR_INVALID_SIZE, result);
}

void test_cb_init_should_round_up_to_power_of_two(void) {
  // Test with non-power-of-2 capacity
  cb_result_t result = cb_init(&test_cb, test_buffer, 6, sizeof(int));
  TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  
  // Should round up to 8 (next power of 2)
  TEST_ASSERT_EQUAL(8, test_cb.capacity);
  TEST_ASSERT_EQUAL(7, test_cb.mask); // mask = capacity - 1
}

void test_cb_put_and_get_single_element(void) {
  // Initialize buffer
  cb_result_t init_result = cb_init(&test_cb, test_buffer, 4, sizeof(int));
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
  cb_init(&test_cb, test_buffer, 4, sizeof(int));

  int input_values[] = {10, 20, 30};
  int output_values[3] = {0};

  // Put multiple values
  for (int i = 0; i < 3; i++) {
    cb_result_t result = cb_put(&test_cb, &input_values[i]);
    TEST_ASSERT_EQUAL(CB_SUCCESS, result);
  }

  TEST_ASSERT_EQUAL(3, cb_size(&test_cb));

  // Get values and verify FIFO order
  for (int i = 0; i < 3; i++) {
    cb_result_t result = cb_get(&test_cb, &output_values[i]);
    TEST_ASSERT_EQUAL(CB_SUCCESS, result);
    TEST_ASSERT_EQUAL(input_values[i], output_values[i]);
  }
}

void test_cb_should_handle_buffer_full_condition(void) {
  // Initialize small buffer
  cb_init(&test_cb, test_buffer, 4, sizeof(int));

  int values[] = {1, 2, 3, 4};

  // Fill the buffer completely
  for (int i = 0; i < 4; i++) {
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
  cb_init(&test_cb, test_buffer, 4, sizeof(int));

  // Buffer should be empty initially
  TEST_ASSERT_TRUE(cb_is_empty(&test_cb));

  // Try to get from empty buffer
  int value = 0;
  cb_result_t result = cb_get(&test_cb, &value);
  TEST_ASSERT_EQUAL(CB_ERROR_BUFFER_EMPTY, result);
}

void test_cb_should_handle_wraparound(void) {
  // Initialize buffer
  cb_init(&test_cb, test_buffer, 4, sizeof(int));

  // Fill buffer completely
  for (int i = 0; i < 4; i++) {
    int value = i + 100; // 100, 101, 102, 103
    cb_put(&test_cb, &value);
  }

  // Now do several add/remove cycles to force wraparound
  for (int cycle = 0; cycle < 5; cycle++) {
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
  cb_init(&test_cb, test_buffer, 4, sizeof(int));

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

void test_cb_deinit_should_clear_structure(void) {
  // Initialize buffer
  cb_init(&test_cb, test_buffer, 4, sizeof(int));
  
  // Add some data
  int value = 42;
  cb_put(&test_cb, &value);
  
  // Verify buffer has data
  TEST_ASSERT_FALSE(cb_is_empty(&test_cb));
  TEST_ASSERT_EQUAL(test_buffer, test_cb.buffer);
  
  // Deinitialize
  void *returned_buffer = cb_deinit(&test_cb);
  
  // Should return the original buffer pointer
  TEST_ASSERT_EQUAL(test_buffer, returned_buffer);
  
  // Structure should be cleared
  TEST_ASSERT_NULL(test_cb.buffer);
  TEST_ASSERT_EQUAL(0, test_cb.size);
  TEST_ASSERT_EQUAL(0, test_cb.capacity);
  TEST_ASSERT_EQUAL(0, test_cb.head);
  TEST_ASSERT_EQUAL(0, test_cb.tail);
  TEST_ASSERT_EQUAL(0, test_cb.mask);
}

void test_cb_deinit_should_handle_null_pointer(void) {
  void *result = cb_deinit(NULL);
  TEST_ASSERT_NULL(result);
}

void test_cb_operations_should_handle_null_pointers(void) {
  // Test all operations with null cb pointer
  TEST_ASSERT_FALSE(cb_is_empty(NULL));
  TEST_ASSERT_FALSE(cb_is_full(NULL));
  TEST_ASSERT_EQUAL(0, cb_size(NULL));
  
  int value = 42;
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_put(NULL, &value));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_get(NULL, &value));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_peek(NULL, &value));
  
  // Test operations with null data pointer
  cb_init(&test_cb, test_buffer, 4, sizeof(int));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_put(&test_cb, NULL));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_get(&test_cb, NULL));
  TEST_ASSERT_EQUAL(CB_ERROR_NULL_POINTER, cb_peek(&test_cb, NULL));
}

void test_cb_should_handle_different_element_sizes(void) {
  // Test with different data types
  
  // Test with char
  uint8_t char_buffer[32];
  circular_buffer_t char_cb;
  cb_init(&char_cb, char_buffer, 8, sizeof(char));
  
  char char_value = 'A';
  char retrieved_char = 0;
  TEST_ASSERT_EQUAL(CB_SUCCESS, cb_put(&char_cb, &char_value));
  TEST_ASSERT_EQUAL(CB_SUCCESS, cb_get(&char_cb, &retrieved_char));
  TEST_ASSERT_EQUAL(char_value, retrieved_char);
  
  // Test with larger struct
  typedef struct {
    int a;
    float b;
    char c[8];
  } test_struct_t;
  
  uint8_t struct_buffer[64];
  circular_buffer_t struct_cb;
  cb_init(&struct_cb, struct_buffer, 4, sizeof(test_struct_t));
  
  test_struct_t test_struct = {42, 3.14f, "test"};
  test_struct_t retrieved_struct = {0};
  
  TEST_ASSERT_EQUAL(CB_SUCCESS, cb_put(&struct_cb, &test_struct));
  TEST_ASSERT_EQUAL(CB_SUCCESS, cb_get(&struct_cb, &retrieved_struct));
  TEST_ASSERT_EQUAL(test_struct.a, retrieved_struct.a);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, test_struct.b, retrieved_struct.b);
  TEST_ASSERT_EQUAL_STRING(test_struct.c, retrieved_struct.c);
}

//=============================================================================
// TEST RUNNER
//=============================================================================

int main(void) {
  UNITY_BEGIN();

  // Basic functionality tests
  RUN_TEST(test_cb_init_should_succeed_with_valid_parameters);
  RUN_TEST(test_cb_init_should_fail_with_null_cb_pointer);
  RUN_TEST(test_cb_init_should_fail_with_null_buffer_pointer);
  RUN_TEST(test_cb_init_should_fail_with_zero_capacity);
  RUN_TEST(test_cb_init_should_fail_with_zero_element_size);
  RUN_TEST(test_cb_init_should_round_up_to_power_of_two);

  // Core operations
  RUN_TEST(test_cb_put_and_get_single_element);
  RUN_TEST(test_cb_should_maintain_fifo_order);

  // Edge cases
  RUN_TEST(test_cb_should_handle_buffer_full_condition);
  RUN_TEST(test_cb_should_handle_buffer_empty_condition);
  RUN_TEST(test_cb_should_handle_wraparound);

  // Additional functionality
  RUN_TEST(test_cb_peek_should_not_modify_buffer);
  RUN_TEST(test_cb_deinit_should_clear_structure);
  RUN_TEST(test_cb_deinit_should_handle_null_pointer);
  
  // Error handling
  RUN_TEST(test_cb_operations_should_handle_null_pointers);
  
  // Flexibility tests
  RUN_TEST(test_cb_should_handle_different_element_sizes);

  return UNITY_END();
}
