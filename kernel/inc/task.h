#ifndef TASK_H
#define TASK_H

#include "kernel.h"
#include "list.h"

#define tcb_from_ready_link(ptr)                                               \
  container_of(ptr, task_control_block, ready_link)
#define tcb_from_wait_link(ptr) container_of(ptr, task_control_block, wait_link)
#define tcb_from_delay_link(ptr)                                               \
  container_of(ptr, task_control_block, delay_link)

typedef enum {
  WAKE_REASON_DATA_AVAILABLE,
  WAKE_REASON_TIMEOUT,
  WAKE_REASON_SIGNAL,
  WAKE_REASON_NONE
} wake_reason_t;

typedef struct task_control_block {
  // CPU context (stack pointer, registers)
  uint32_t *stack_pointer;
  uint32_t *stack_base;
  uint32_t stack_size; // in bytes

  // Task information
  char name[16];
  task_priority_t priority;
  task_state_t state;

  uint32_t wake_tick;

  void *waiting_on; // Pointer to semaphore/queue/mutex we are waiting on
  wake_reason_t wake_reason;

  uint32_t run_count;     // Number of times scheduled
  uint32_t total_runtime; // Total CPU time

  list_head_t ready_link; // Per-priority ready queue
  list_head_t delay_link; // Delayed list
  list_head_t wait_link;  // Waiter list (queue/mutex/sem)

} task_control_block;

task_handle_t task_create_internal(task_function_t function, const char *name,
                                   uint16_t stack_size, void *param,
                                   task_priority_t priority);
void task_delete_internal(task_handle_t task);
void task_set_state(task_handle_t task, task_state_t state);
task_state_t task_get_state(task_handle_t task);

void task_init_stack(task_handle_t task, task_function_t function, void *param);
bool task_stack_check(task_handle_t task);
uint32_t task_stack_used_bytes(task_handle_t task);

#endif // !TASK_H
