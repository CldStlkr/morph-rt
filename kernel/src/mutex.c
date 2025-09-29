#include "critical.h"
#include "memory.h"
#include "mutex.h"
#include "scheduler.h"
#include "task.h"
#include "time_utils.h"

#include <stdlib.h>
#include <string.h>

static inline task_handle_t mutex_waitlist_pop(list_head_t *waitlist) {
  if (list_is_empty(waitlist)) return NULL;

  list_head_t *node = waitlist->next;
  list_remove(node);

  return tcb_from_mutex_wait_link(node);
}

static inline void mutex_waitlist_push(list_head_t *waitlist, task_handle_t task) {
  list_insert_tail(waitlist, &task->wait_link);
}

static void mutex_wake_one_waiter(mutex_handle_t mutex) {
  task_handle_t task = mutex_waitlist_pop(&mutex->waiting_tasks);

  if (!task) return;

  task->waiting_on = NULL;
  task->wake_reason = WAKE_REASON_DATA_AVAILABLE;

  // Cancel any timeout
  scheduler_cancel_timeout(task);
  scheduler_add_task(task);
}

static void mutex_apply_priority_inheritance(mutex_handle_t mutex) {
  if (!mutex->owner || list_is_empty(&mutex->waiting_tasks)) return;

  // Find highest priority (lowest number) among waiting tasks
  task_priority_t highest_priority = MAX_PRIORITY;
  list_head_t *pos;
  list_iter(pos, &mutex->waiting_tasks) {
    task_handle_t waiting_task = tcb_from_mutex_wait_link(pos);
    highest_priority = waiting_task->effective_priority < highest_priority ? waiting_task->effective_priority : highest_priority;
  }

  // Boost owner if waiting task has higher priority than owner
  if (highest_priority < mutex->owner->effective_priority) {
    // Save original priority if this is the first inheritance
    if (mutex->original_priority == MAX_PRIORITY) {
      mutex->original_priority = mutex->owner->base_priority;
    }

    scheduler_boost_priority(mutex->owner, highest_priority);
  }
}


static void mutex_restore_priority(mutex_handle_t mutex) {
  if (!mutex->owner || mutex->original_priority == MAX_PRIORITY) return;

  scheduler_boost_priority(mutex->owner, mutex->original_priority);
  mutex->original_priority = MAX_PRIORITY;
}


mutex_handle_t mutex_create(const char *name) {
  mutex_control_block *mutex = mutex_pool_alloc_mcb();

  if (!mutex) return NULL;

  mutex->owner = NULL;
  mutex->original_priority = MAX_PRIORITY; // Not inherited
  list_init(&mutex->waiting_tasks);

  if (name) {
    strncpy(mutex->name, name, sizeof(mutex->name) - 1);
    mutex->name[sizeof(mutex->name) - 1] = '\0';
  } else {
    mutex->name[0] = '\0';
  }

  return (mutex_handle_t)mutex;
}

void mutex_delete(mutex_handle_t mutex) {
  if (!mutex) return;

  KERNEL_CRITICAL_BEGIN();

  // If mutex currently owned, restore priority
  if (mutex->owner) {
    mutex_restore_priority(mutex);
  }

  // Wake all waiting tasks with error condition
  while(!list_is_empty(&mutex->waiting_tasks)) {
    task_handle_t task = mutex_waitlist_pop(&mutex->waiting_tasks);
    if (task) {
      task->waiting_on = NULL;
      task->wake_reason = WAKE_REASON_SIGNAL;
      scheduler_cancel_timeout(task);
      scheduler_add_task(task);
    }
  }

  KERNEL_CRITICAL_END();

  mutex_pool_free_mcb(mutex);
}

mutex_result_t mutex_lock(mutex_handle_t mutex, uint32_t timeout) {
  if (!mutex) return MUTEX_ERROR_NULL;

  uint32_t start_time = tick_now;
  uint32_t deadline = start_time + timeout;

  for (;;) {
    KERNEL_CRITICAL_BEGIN();

    // If mutex is not owned, aqcuire it
    if (!mutex->owner) {
      mutex->owner = current_task;
      KERNEL_CRITICAL_END();
      return MUTEX_OK;
    }

    if (mutex->owner == current_task) {
      KERNEL_CRITICAL_END();
      return MUTEX_ERROR_RECURSIVE;
    }

    if (timeout == MUTEX_NO_WAIT) {
      KERNEL_CRITICAL_END();
      return MUTEX_ERROR_TIMEOUT;
    }

    // Check for timeout expiration
    uint32_t now = tick_now;
    uint32_t remaining = ticks_until(deadline, now);

    if (remaining == 0) {
      KERNEL_CRITICAL_END();
      return MUTEX_ERROR_TIMEOUT;
    }

    // Block current task
    current_task->waiting_on = mutex;
    mutex_waitlist_push(&mutex->waiting_tasks, current_task);

    // Apply priority inheritance
    mutex_apply_priority_inheritance(mutex);

    // Set timeout if not waiting forever
    if (timeout != MUTEX_WAIT_FOREVER) {
      uint32_t wake_time = now + remaining;
      scheduler_set_timeout(current_task, wake_time);
    }

    task_set_state(current_task, TASK_BLOCKED);
    KERNEL_CRITICAL_END();

    scheduler_yield();

    //Check why we woke up
    if(current_task->wake_reason == WAKE_REASON_TIMEOUT) return MUTEX_ERROR_TIMEOUT;

    if (current_task->wake_reason == WAKE_REASON_SIGNAL) return MUTEX_ERROR_NULL;

    if (timeout != MUTEX_WAIT_FOREVER) {
      uint32_t now_updated = tick_now;
      timeout = ticks_until(deadline, now_updated);
      if (timeout == 0) return MUTEX_ERROR_TIMEOUT;
    }
  }
}

mutex_result_t mutex_unlock(mutex_handle_t mutex) {
  if (!mutex) return MUTEX_ERROR_NULL;

  KERNEL_CRITICAL_BEGIN();

  if (mutex->owner != current_task) {
    KERNEL_CRITICAL_END();
    return MUTEX_ERROR_NOT_OWNER;
  }

  mutex_restore_priority(mutex);

  // Release mutex
  mutex->owner = NULL;

  // Wake up one waiting task if any
  if (!list_is_empty(&mutex->waiting_tasks)) {
    mutex_wake_one_waiter(mutex);
  }

  KERNEL_CRITICAL_END();
  return MUTEX_OK;
}

mutex_result_t mutex_try_lock(mutex_handle_t mutex) {
  return mutex_lock(mutex, MUTEX_NO_WAIT);
}

task_handle_t mutex_get_owner(mutex_handle_t mutex) {
  if (!mutex) return NULL;

  KERNEL_CRITICAL_BEGIN();
  task_handle_t owner = mutex->owner;
  KERNEL_CRITICAL_END();

  return owner;
}

bool mutex_has_waiting_tasks(mutex_handle_t mutex) {
  if (!mutex) return false;

  KERNEL_CRITICAL_BEGIN();
  bool has_waiters = !list_is_empty(&mutex->waiting_tasks);
  KERNEL_CRITICAL_END();

  return has_waiters;
}

bool mutex_is_locked(mutex_handle_t mutex) {
  if (!mutex) return false;

  KERNEL_CRITICAL_BEGIN();
  bool is_locked = (mutex->owner != NULL);
  KERNEL_CRITICAL_END();

  return is_locked;
}
