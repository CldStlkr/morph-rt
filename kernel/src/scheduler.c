#include "scheduler.h"
#include <stddef.h>

// Ready queues - one per priority level
list_head_t ready_queues[MAX_PRIORITY + 1];
task_handle_t current_task = NULL;
task_handle_t next_task = NULL;

static void delayed_insert_sorted(list_head_t *list, task_handle_t t) {
  list_head_t *pos;
  list_iter(pos, list) {
    task_handle_t q = tcb_from_delay_link(pos);
    if (time_lte(t->wake_tick, q->wake_tick)) {
      list_insert_before(&t->delay_link, pos);
      return;
    }
  }
  list_insert_tail(list, &t->delay_link);
}

// Core scheduler functions
void scheduler_init(void) {
  for (int i = 0; i <= MAX_PRIORITY; ++i) {
    list_init(&ready_queues[i]);
  }

  list_init(&delayed_cur);
  list_init(&delayed_ovf);
  tick_now = 0;

  current_task = NULL;
  next_task = NULL;
}

void scheduler_start(void) {
  // Find the first current task to run
  current_task = scheduler_get_next_task();

  if (!current_task) {
    // No tasks to run - this is typically a critical error in embedded
    // In real implementation, I would need to either enter low-power mode
    // or trigger system reset

    while (1)
      ;
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
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; ++priority) {
    if (!list_is_empty(&ready_queues[priority])) {
      // Get first task from this priority
      list_head_t *first = ready_queues[priority].next;
      task_handle_t task = tcb_from_ready_link(first);

      // Remove from head and add to tail (round-robin)
      list_move_to_tail(&ready_queues[priority], first);

      return task;
    }
  }
  // No ready tasks found
  return NULL;
}
void scheduler_add_task(task_handle_t task) {
  if (!task) return;

  task->state = TASK_READY;

  list_insert_tail(&ready_queues[task->priority], &task->ready_link);
}

void scheduler_remove_task(task_handle_t task) {
  if (!task) return;

  if (!list_is_empty(&task->ready_link)) {
    list_remove(&task->ready_link);
  }

  if (!list_is_empty(&task->delay_link)) {
    list_remove(&task->delay_link);
  }

  // Note: Don't remove from wait_link
  // Handled by specific sync object (semaphore, queue, etc.)
}

// Task state transitions
void scheduler_block_current_task(void) {
  if (current_task) {
    current_task->state = TASK_BLOCKED;
    scheduler_remove_task(current_task);
  }
}

void scheduler_unblock_task(task_handle_t task) {
  if (!task) return;

  scheduler_add_task(task);
}

void scheduler_yield(void) {
  // This will trigger a context switch to the next ready task
  // The actual context switch will be handled by the kernel
  // For now, just find next task
  next_task = scheduler_get_next_task();
}

// Helper function for task_delay() implementation
void scheduler_delay_current_task(uint32_t ticks) {
  if (!current_task || ticks == 0) return;

  // Remove from ready queue
  if (!list_is_empty(&current_task->ready_link)) {
    list_remove(&current_task->ready_link);
  }

  current_task->state = TASK_BLOCKED;

  // KERNEL_CRITICAL_BEGIN();
  uint32_t now = tick_now;     // read once
  uint32_t wake = now + ticks; // automatically wraps
  current_task->wake_tick = wake;

  list_head_t *L = time_lt(wake, now) ? &delayed_ovf : &delayed_cur;

  delayed_insert_sorted(L, current_task);
  // KERNEL_CRITICAL_END();

  scheduler_yield();
}

// Timer tick handler - processes delayed tasks
void scheduler_tick(void) {
  uint32_t now = ++tick_now;

  // Release all tasks whose wake_tick <= now from current list
  while (!list_is_empty(&delayed_cur)) {
    task_handle_t t = tcb_from_delay_link(delayed_cur.next); // head
    if (time_gt(t->wake_tick, now)) break;

    list_remove(&t->delay_link);
    scheduler_expire_timeout(t);
  }

  // On wrap to 0, swap lists so former overflow becomes current
  if (now == 0) {
    list_head_t tmp = delayed_cur;
    delayed_cur = delayed_ovf;
    delayed_ovf = tmp;

    // drain again so tasks with wake_tick == 0 don't wait one extra tick
    while (!list_is_empty(&delayed_cur)) {
      task_handle_t t = tcb_from_delay_link(delayed_cur.next);
      if (time_gt(t->wake_tick, now)) break;
      list_remove(&t->delay_link);
      scheduler_expire_timeout(t);
    }
  }
}

void scheduler_set_timeout(task_handle_t t, uint32_t wake_tick) {
  // KERNEL_CRITICAL_BEGIN();
  uint32_t now = tick_now;
  t->wake_tick = wake_tick;
  list_head_t *L = time_lt(wake_tick, now) ? &delayed_ovf : &delayed_cur;
  delayed_insert_sorted(L, t);
  // KERNEL_CRITICAL_END();
}

void scheduler_expire_timeout(task_handle_t t) {
  if (t->waiting_on) {
    if (t->wait_link.next != &t->wait_link) {
      list_remove(&t->wait_link);
    }
    t->wake_reason = WAKE_REASON_TIMEOUT;
    t->waiting_on = NULL;
  }
  scheduler_add_task(t);
}

void scheduler_cancel_timeout(task_handle_t t) {
  // KERNEL_CRITICAL_BEGIN();
  if (!list_is_empty(&t->delay_link)) {
    list_remove(&t->delay_link);
  }
  // KERNEL_CRITICAL_END();
}

// Priority management
task_priority_t scheduler_get_highest_priority(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; ++priority) {
    if (!list_is_empty(&ready_queues[priority])) {
      return priority;
    }
  }
  return MAX_PRIORITY;
}

bool scheduler_has_ready_tasks(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; ++priority) {
    if (!list_is_empty(&ready_queues[priority])) {
      return true;
    }
  }
  return false;
}
