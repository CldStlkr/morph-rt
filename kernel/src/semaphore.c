#include "critical.h"
#include "memory.h"
#include "scheduler.h"
#include "semaphore.h"
#include "task.h"
#include "time_utils.h"

#include <stdlib.h>
#include <string.h>

// =========================== HELPER FUNCTIONS ============================

static inline task_handle_t sem_waitlist_pop(list_head_t *waitlist) {
  if (list_is_empty(waitlist)) {
    return NULL;
  }

  list_head_t *node = waitlist->next;
  list_remove(node);
  return tcb_from_sem_wait_link(node);
}

static inline void sem_waitlist_push(list_head_t *waitlist,
                                     task_handle_t task) {
  list_insert_tail(waitlist, &task->wait_link);
}

static void sem_wait_one_waiter(semaphore_handle_t sem) {
  task_handle_t task = sem_waitlist_pop(&sem->waiting_tasks);

  if (!task) return;

  task->waiting_on = NULL;
  task->wake_reason = WAKE_REASON_DATA_AVAILABLE;

  // Cancel any timeout that was set
  scheduler_cancel_timeout(task);

  scheduler_add_task(task);
}

// =========================== PUBLIC API ============================

semaphore_handle_t sem_create(uint32_t initial_count, uint32_t max_count,
                              const char *name) {
  if (max_count == 0 || initial_count > max_count) return NULL;

  semaphore_control_block *sem = sem_pool_alloc_scb();

  if (!sem) return NULL;

  sem->count = initial_count;
  sem->max_count = max_count;
  list_init(&sem->waiting_tasks);

  // Copy name for debugging
  if (name) {
    strncpy(sem->name, name, sizeof(sem->name) - 1);
    sem->name[sizeof(sem->name) - 1] = '\0';
  } else {
    sem->name[0] = '\0';
  }

  return (semaphore_handle_t)sem;
}

void sem_delete(semaphore_handle_t sem) {
  if (!sem) return;

  KERNEL_CRITICAL_BEGIN();

  // Wake all waiting tasks with error condition
  while (!list_is_empty(&sem->waiting_tasks)) {
    task_handle_t task = sem_waitlist_pop(&sem->waiting_tasks);

    // Redundant if statement, may remove
    if (task) {
      task->waiting_on = NULL;
      task->wake_reason = WAKE_REASON_SIGNAL; // Sem deleted
      scheduler_cancel_timeout(task);
      scheduler_add_task(task);
    }
  }

  KERNEL_CRITICAL_END();

  // Return to pool instead of free
  sem_pool_free_scb(sem);
}

sem_result_t sem_wait(semaphore_handle_t sem, uint32_t timeout) {
  if (!sem) return SEM_ERROR_NULL;

  uint32_t start_time = tick_now;
  uint32_t deadline = start_time + timeout;

  for (;;) {
    KERNEL_CRITICAL_BEGIN();

    if (sem->count > 0) {
      sem->count--;
      KERNEL_CRITICAL_END();
      return SEM_OK;
    }

    if (timeout == SEM_NO_WAIT) {
      KERNEL_CRITICAL_END();
      return SEM_ERROR_TIMEOUT;
    }

    // Check if timeout has expired
    uint32_t now = tick_now;
    uint32_t remaining = ticks_until(deadline, now);
    if (remaining == 0) {
      KERNEL_CRITICAL_END();
      return SEM_ERROR_TIMEOUT;
    }

    // Block current task
    current_task->waiting_on = sem;
    sem_waitlist_push(&sem->waiting_tasks, current_task);

    // Set timeout if not waiting forever
    if (timeout != SEM_WAIT_FOREVER) {
      uint32_t wake_time = now + remaining;
      scheduler_set_timeout(current_task, wake_time);
    }

    task_set_state(current_task, TASK_BLOCKED);

    KERNEL_CRITICAL_END();

    // Task will resume here when unblocked
    scheduler_yield();

    // Check why we woke up
    if (current_task->wake_reason == WAKE_REASON_TIMEOUT) {
      return SEM_ERROR_TIMEOUT;
    }

    // If sem was deleted while waiting
    if (current_task->wake_reason == WAKE_REASON_SIGNAL) {
      return SEM_ERROR_NULL;
    }

    // Update timeout for next iteration
    if (timeout != SEM_WAIT_FOREVER) {
      timeout = ticks_until(deadline, now);
      if (timeout == 0) {
        return SEM_ERROR_TIMEOUT;
      }
    }
  }
}

sem_result_t sem_post(semaphore_handle_t sem) {
  if (!sem) return SEM_ERROR_NULL;

  KERNEL_CRITICAL_BEGIN();

  if (!list_is_empty(&sem->waiting_tasks)) {
    sem_wait_one_waiter(sem);
    KERNEL_CRITICAL_END();
    return SEM_OK;
  }

  if (sem->count < sem->max_count) {
    sem->count++;
    KERNEL_CRITICAL_END();
    return SEM_OK;
  }

  // Count would overflow
  KERNEL_CRITICAL_END();
  return SEM_ERROR_OVERFLOW;
}

sem_result_t sem_try_wait(semaphore_handle_t sem) {
  return sem_wait(sem, SEM_NO_WAIT);
}

uint32_t sem_get_count(semaphore_handle_t sem) {
  if (!sem) return 0;

  KERNEL_CRITICAL_BEGIN();
  uint32_t count = sem->count;
  KERNEL_CRITICAL_END();

  return count;
}

bool sem_has_waiting_tasks(semaphore_handle_t sem) {
  if (!sem) return false;

  KERNEL_CRITICAL_BEGIN();
  bool has_waiters = (!list_is_empty(&sem->waiting_tasks));
  KERNEL_CRITICAL_END();

  return has_waiters;
}
