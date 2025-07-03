#include "kernel.h"
#include "scheduler.h"
#include "task.h"
#include <stddef.h>

// Kernel state - private to this module
static bool kernel_initialized = false;
static bool kernel_running = false;

// Private helper functions
static bool _is_valid_task_context(void) { return kernel_running && current_task != NULL; }

static bool _is_kernel_ready(void) { return kernel_initialized; }

// Public kernel API
void kernel_init(void) {
  if (kernel_initialized) {
    return; // Already initialized
  }

  // Initialize the scheduler
  scheduler_init();

  kernel_initialized = true;
  kernel_running = false;
}

void kernel_start(void) {
  if (!kernel_initialized) {
    // Critical error - kernel not initialized
    while (1) {
      // Infinite loop - system cannot start
    }
  }

  if (kernel_running) {
    return; // Already running
  }

  kernel_running = true;

  // Start the scheduler (this should never return)
  scheduler_start();

  // If we reach here, something went wrong
  kernel_running = false;
  while (1) {
    // Infinite loop - critical error
  }
}

// Public task management API
task_handle_t task_create(task_function_t function, const char *name, uint16_t stack_size,
                          void *param, task_priority_t priority) {
  if (!_is_kernel_ready()) {
    return NULL;
  }

  // Use default stack size if 0 is passed
  if (stack_size == 0) {
    stack_size = DEFAULT_STACK_SIZE;
  }

  // Create the task using internal function
  task_handle_t task = task_create_internal(function, name, stack_size, param, priority);

  if (task) {
    // Add task to scheduler's ready queue
    scheduler_add_task(task);
  }

  return task;
}

void task_delete(task_handle_t task) {
  if (!task) {
    return;
  }

  // For public API functions, use the clean interface
  task_handle_t current = task_get_current();

  // If we're trying to delete the current task, we need special handling
  if (task == current) {
    // Mark task for deletion
    task->state = TASK_DELETED;

    // Remove from scheduler
    scheduler_remove_task(task);

    // Trigger context switch to next task
    scheduler_yield();

    // We should never reach here if context switching works
    // The task deletion will be completed by the scheduler
    return;
  }

  // For non-current tasks, delete immediately
  task_delete_internal(task);
}

void task_delay(uint32_t ticks) {
  if (!_is_valid_task_context() || ticks == 0) {
    return;
  }

  // Use scheduler's delay function
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

  // Put current task back in ready queue (end of its priority level)
  if (current->state == TASK_RUNNING) {
    current->state = TASK_READY;
    scheduler_add_task(current);
  }

  // Trigger scheduler to pick next task
  scheduler_yield();

  // Context switch will happen here (when implemented)
  // When we return, this task is running again
}

task_handle_t task_get_current(void) { return current_task; }
