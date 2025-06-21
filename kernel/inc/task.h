#ifndef TASK_H
#define TASK_H

#include "kernel.h"

// Task Control Block - heart of task management
typedef struct task_control_block {
  // CPU context (stack pointer, registers)
  uint32_t* stack_pointer;
  uint32_t* stack_base;
  uint16_t stack_size; // in bytes

  // Task information
  char name[16];
  task_priority_t priority;
  task_state_t state;

  uint32_t delay_ticks;

  // Linked list for scheduler queues
  struct task_control_block* next;

  // Statistics (debugging)
  uint32_t run_count;     // Number of times scheduled
  uint32_t total_runtime; // Total CPU time

} task_control_block;

// Task management functions
task_handle_t task_create_internal(task_function_t function, const char* name,
                                   uint16_t stack_size, void* param,
                                   task_priority_t priority);
void task_delete_internal(task_handle_t task);
void task_set_state(task_handle_t task, task_state_t state);
task_state_t task_get_state(task_handle_t task);

// Stack management
void task_init_stack(task_handle_t task, task_function_t function, void* param);
bool task_stack_check(task_handle_t task);
uint32_t task_stack_used_bytes(task_handle_t task);

#endif // !TASK_H
