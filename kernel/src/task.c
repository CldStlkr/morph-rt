#include "scheduler.h"
#include "task.h"
// Task Control Block

#include <stdlib.h>
#include <string.h>

// Task management functions
task_handle_t task_create_internal(task_function_t function, const char *name,
                                   uint16_t stack_size, void *param,
                                   task_priority_t priority) {
  if (!function || !name || stack_size == 0 || priority > MAX_PRIORITY) {
    return NULL;
  }

  task_control_block *tcb = malloc(sizeof(task_control_block));
  if (!tcb) {
    return NULL;
  }

  uint32_t *stack = malloc(stack_size);
  if (!stack) {
    free(tcb);
    return NULL;
  }

  tcb->stack_base = stack;
  tcb->stack_size = stack_size;
  strncpy(tcb->name, name, sizeof(tcb->name) - 1);
  tcb->name[sizeof(tcb->name) - 1] = '\0';
  tcb->priority = priority;
  tcb->state = TASK_READY;
  tcb->wake_tick = 0;
  tcb->wake_reason = WAKE_REASON_NONE;
  tcb->waiting_on = NULL;
  tcb->run_count = 0;
  tcb->total_runtime = 0;

  list_init(&tcb->ready_link);
  list_init(&tcb->delay_link);
  list_init(&tcb->wait_link);

  task_init_stack(tcb, function, param);

  return (task_handle_t)tcb;
}

void task_delete_internal(task_handle_t task) {
  if (!task) {
    return;
  }

  scheduler_remove_task(task);

  task->state = TASK_DELETED;

  if (task->stack_base) {
    free(task->stack_base);
    task->stack_base = NULL;
  }

  free(task);
}

void task_set_state(task_handle_t task, task_state_t state) {
  if (!task) {
    return;
  }

  task->state = state;
}
task_state_t task_get_state(task_handle_t task) {
  if (!task) {
    return TASK_DELETED;
  }

  return task->state;
}

// Stack management
void task_init_stack(task_handle_t task, task_function_t function,
                     void *param) {

  // Stack grows downward towards base, so start from top
  uint32_t *stack_top =
      task->stack_base + (task->stack_size / sizeof(uint32_t));
  uint32_t *sp = stack_top;

  // ARM Cortex-M4 automatically pushes these registers during interrupt entry
  // We simulate this by preloading the stack

  // Hardware-saved registers (pushed automatically by CPU during interrupt)
  *(--sp) = 0x01000000;          // xPSR (Thumb bit set)
  *(--sp) = (uintptr_t)function; // PC (where tasks should start)
  *(--sp) = 0;                   // LR (Link Register - return address)
  *(--sp) = 0;                   // R12
  *(--sp) = 0;                   // R3
  *(--sp) = 0;                   // R2
  *(--sp) = 0;                   // R1
  *(--sp) = (uintptr_t)param;    // R0 (first parameter to function)

  // Software-saved registers (context switcher will save/restore these)
  *(--sp) = 0; // R11
  *(--sp) = 0; // R10
  *(--sp) = 0; // R9
  *(--sp) = 0; // R8
  *(--sp) = 0; // R7
  *(--sp) = 0; // R6
  *(--sp) = 0; // R5
  *(--sp) = 0; // R4

  task->stack_pointer = sp;
}

bool task_stack_check(task_handle_t task) {
  if (!task) {
    return false;
  }

  uint32_t *stack_top =
      task->stack_base + (task->stack_size / sizeof(uint32_t));
  uint32_t used_bytes = (stack_top - task->stack_pointer) * sizeof(uint32_t);

  return used_bytes < task->stack_size;
}

uint32_t task_stack_used_bytes(task_handle_t task) {
  if (!task) {
    return 0;
  }

  uint32_t *stack_top =
      task->stack_base + (task->stack_size / sizeof(uint32_t));
  uint32_t used_bytes = (stack_top - task->stack_pointer) * sizeof(uint32_t);

  return used_bytes;
}
