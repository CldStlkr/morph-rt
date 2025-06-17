#include "../inc/scheduler.h"

#include <stddef.h>

// Ready queues - one per priority level
task_handle_t ready_queues[MAX_PRIORITY + 1] = {NULL};
task_handle_t current_task = NULL;
task_handle_t next_task = NULL;

static task_handle_t delayed_tasks = NULL;

// Core scheduler functions
void scheduler_init(void) {
  for (int i = 0; i <= MAX_PRIORITY; i++) {
    ready_queues[i] = NULL;
  }

  current_task = NULL;
  next_task = NULL;
  delayed_tasks = NULL;
}

void scheduler_start(void) {
  // Find the first current task to run
  current_task = scheduler_get_next_task();

  if (!current_task) {
    // No tasks to run - this is typically a critical error in embedded
    // In real implementation, I would need to either enter low-power mode
    // or trigger system reset

    while (1) {
      // Infinite loop
    }
  }

  current_task->state = TASK_RUNNING;

  // This is where we would jump to the task's context
  // In real implementation, this would be assembly code that:
  // 1. Loads the task's stack pointer
  // 2. Restores the task's CPU registers
  // 3. Returns to the task's program counter

  // For now, placeholder - implement actual context switch later
  // start_first_task(current_task->stack_pointer); // Assembly function

  // This function should never return
}

task_handle_t scheduler_get_next_task(void) {
  // Find highest priority (lowest number) that has tasks
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (ready_queues[priority] != NULL) {
      // Get the task at the head of this priority queue
      task_handle_t task = ready_queues[priority];

      if (task->next != NULL) {
        // Next task becomes head if present
        ready_queues[priority] = task->next;

        // Find end of queue and add current_task to the back for Round Robin
        task_handle_t tail = ready_queues[priority];
        while (tail->next != NULL) {
          tail = tail->next;
        }
        tail->next = task;
        task->next = NULL;
      }
      // If task->next is NULL, this is the only task at this priority,
      // so no need to rearange queue

      return task;
    }
  }
  // No ready tasks found
  return NULL;
}
void scheduler_add_task(task_handle_t task) {
  if (!task) {
    return;
  }

  task_priority_t priority = task->priority;
  task->state = TASK_READY;

  // Add to the end of the priority queue (FIFO within priority)
  if (ready_queues[priority] == NULL) {
    // First task at this priority
    ready_queues[priority] = task;
  } else {
    // Find end and append
    task_handle_t tail = ready_queues[priority];
    while (tail->next != NULL) {
      tail = tail->next;
    }
    tail->next = task;
  }
  task->next = NULL;
}
void scheduler_remove_task(task_handle_t task) {
  if (!task) {
    return;
  }

  task_priority_t priority = task->priority;

  // Remove from ready_queue
  if (ready_queues[priority] == task) {
    // Task is at the head, we make next (its ok if NULL) new head
    ready_queues[priority] = task->next;
  } else {
    task_handle_t current = ready_queues[priority];
    while (current && current->next != task) {
      current = current->next;
    }
    if (current) {
      // If current then we know current->next == task
      current->next = task->next;
    }
  }

  // Also remove from delayed tasks list if present
  if (delayed_tasks == task) {
    delayed_tasks = task->next;
  } else {
    task_handle_t current = delayed_tasks;
    while (current && current->next != task) {
      current = current->next;
    }
    if (current) {
      current->next = task->next;
    }
  }

  task->next = NULL;
}

// Task state transitions
void scheduler_block_current_task(void) {
  if (current_task) {
    current_task->state = TASK_BLOCKED;
    scheduler_remove_task(current_task);
  }
}

void scheduler_unblock_task(task_handle_t task) {
  if (!task) {
    return;
  }

  task->state = TASK_READY;
  scheduler_add_task(task);
}

void scheduler_yield(void) {
  // This will trigger a context switch to the next ready task
  // The actual context switch will be handled by the kernel
  // For now, just find next task
  next_task = scheduler_get_next_task();
}

// Timer tick handler - processes delayed tasks
void scheduler_tick(void) {
  task_handle_t current = delayed_tasks;
  task_handle_t prev = NULL;

  while (current != NULL) {
    current->delay_ticks--;

    if (current->delay_ticks == 0) {
      // Task delay has expired, move back to ready queue
      task_handle_t task_to_unblock = current;

      // Remove from delayed list
      if (prev == NULL) {
        delayed_tasks = current->next;
      } else {
        prev->next = current->next;
      }

      current = current->next;

      // Add back to ready_queue
      scheduler_unblock_task(task_to_unblock);
    } else {
      prev = current;
      current = current->next;
    }
  }

  // Check if we need to preempt this current task
  if (current_task && scheduler_has_ready_tasks()) {
    task_priority_t highest_ready = scheduler_get_highest_priority();

    // Lower number means higher priority
    if (highest_ready < current_task->priority) {
      // Higher priority task is ready, trigger preemption
      scheduler_yield();
    }
  }
}

// Helper function for task_delay() implementation
void scheduler_delay_current_task(uint32_t ticks) {
  if (!current_task || ticks == 0) {
    return;
  }
  current_task->delay_ticks = ticks;
  current_task->state = TASK_BLOCKED;

  // Remove from ready queue
  scheduler_remove_task(current_task);

  // Add to delayed tasks list
  current_task->next = delayed_tasks;
  delayed_tasks = current_task;

  // Trigger context switch to next ready task
  scheduler_yield();
}

// Prority management
task_priority_t scheduler_get_highest_priority(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (ready_queues[priority] != NULL) {
      return priority;
    }
  }
  return MAX_PRIORITY; // No ready tasks
}

bool scheduler_has_ready_tasks(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (ready_queues[priority] != NULL) {
      return true;
    }
  }
  return false;
}
