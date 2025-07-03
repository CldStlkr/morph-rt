#include "test_circular_buffer.h"
#include "test_task.h"
#include "unity.h"

int main(void) {
  UNITY_BEGIN();

  printf("=== Circular Buffer Tests ===\n");
  run_circular_buffer_tests();

  // printf("=== Task Management Tests ===\n");

  return UNITY_END();
}
