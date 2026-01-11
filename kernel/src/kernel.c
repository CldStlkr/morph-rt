#include "kernel.h"
#include "scheduler.h"
#include "task.h"
#include "memory.h"
#include "port.h"
#include <stddef.h>

// Kernel state - private to this module
static bool kernel_initialized = false;
static bool kernel_running = false;

static bool _is_valid_task_context(void) {
  return kernel_running && current_task != NULL;
}

static bool _is_kernel_ready(void) { return kernel_initialized; }



static void idle_task_function(void *param) {
  (void)param;
  while (1) {
    __disable_irq();

    bool can_sleep = true;

    for (task_priority_t p = 0; p < MAX_PRIORITY; p++) {
      if (!list_is_empty(&ready_queues[p])) {
        can_sleep = false;
        break;
      }
    }

    if (can_sleep) {
      // Wait for Interrupt
      __WFI();
    }

    __enable_irq();

    task_yield();
  }
}

static task_handle_t idle_task_handle = NULL;


// Public kernel API
void kernel_init(void) {
  if (kernel_initialized) {
    return;
  }

  memory_pools_init();

  scheduler_init();

  idle_task_handle = task_create_internal(
    idle_task_function,
    "IDLE",
    SMALL_STACK_SIZE,
    NULL,
    MAX_PRIORITY // Lowest Priority (7)
  );

  if (!idle_task_handle) {
    // Critical failure
    while (1) {}
  }

  scheduler_add_task(idle_task_handle);

  kernel_initialized = true;
  kernel_running = false;
}

void kernel_start(void) {
  if (!kernel_initialized) {
    while (1) {
    }
  }

  if (kernel_running) {
    return;
  }

  kernel_running = true;

  // Should never return
  scheduler_start();

  kernel_running = false;
  // Critical error
  while (1) {}
}

// Public task management API
task_handle_t task_create(task_function_t function, const char *name,
                          uint16_t stack_size, void *param,
                          task_priority_t priority) {
  if (!_is_kernel_ready()) {
    return NULL;
  }

  if (stack_size == 0) {
    stack_size = DEFAULT_STACK_SIZE;
  }

  task_handle_t task =
      task_create_internal(function, name, stack_size, param, priority);

  if (task) {
    scheduler_add_task(task);
  }

  return task;
}

void task_delete(task_handle_t task) {
  if (!task || task == idle_task_handle) {
    return;
  }

  // For public API functions, use the clean interface
  task_handle_t current = task_get_current();


  if (task == current) {
    task->state = TASK_DELETED;
    scheduler_remove_task(task);

    // Mark for cleanup by idle task
    // TODO: Implement deferred deletion in idle task

    scheduler_yield();

    // Should never reach here if context switching works
    // The task deletion will be completed by the scheduler
    return;
  }

  task_delete_internal(task);
}

void task_delay(uint32_t ticks) {
  if (!_is_valid_task_context() || ticks == 0) {
    return;
  }

  scheduler_delay_current_task(ticks);

  // After this point, the task will be suspended until delay expires
  // When scheduler_delay_current_task() returns, the delay has expired
}

void task_yield(void) {
  if (!_is_valid_task_context()) {
    return;
  }

  // For public API, use the clean interface
  task_handle_t current = task_get_current();

  if (current->state == TASK_RUNNING) {
    current->state = TASK_READY;
    scheduler_add_task(current);
  }

  scheduler_yield();

  // Context switch will happen here.
  // When we return, this task is running again
}

task_handle_t task_get_current(void) { return current_task; }
