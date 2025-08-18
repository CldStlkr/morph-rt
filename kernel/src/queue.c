#include "queue.h"
#include "scheduler.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>

typedef struct queue_control_block {
  circular_buffer_t buffer;

  list_head_t waiting_senders;   // Tasks blocked in queue_send()
  list_head_t waiting_receivers; // Tasks blocked in queue_receive()

  // Add mutex/semaphore for thread safety later
} queue_control_block;

// ============================== HELPER FUNCTIONS =============================

static inline task_handle_t waitlist_pop_head(list_head_t *L) {
  if (list_is_empty(L)) return NULL;

  list_head_t *node = L->next;
  list_remove(node);
  return tcb_from_wait_link(node);
}

static inline void waitlist_push_tail(list_head_t *L, task_handle_t t) {
  list_insert_tail(L, &t->wait_link);
}

static inline void wake_one(list_head_t *L) {
  task_handle_t t = waitlist_pop_head(L);
  if (!t) return;

  t->waiting_on = NULL;

  // If task had timeout armed, cancel from delayed lists
  scheduler_cancel_timeout(t);
  scheduler_add_task(t);
}

// =============================================================================

// Public API
queue_handle_t queue_create(size_t queue_length, size_t item_size) {
  if (queue_length == 0 || item_size == 0) return NULL;

  queue_control_block *qcb =
      (queue_control_block *)calloc(1, sizeof(queue_control_block));
  if (!qcb) return NULL;

  if (cb_init(&qcb->buffer, queue_length, item_size) != CB_SUCCESS) {
    free(qcb);
    return NULL;
  }

  list_init(&qcb->waiting_senders);
  list_init(&qcb->waiting_receivers);

  return (queue_handle_t)qcb;
}

void queue_delete(queue_handle_t queue) {
  if (!queue) return;

  cb_free(&queue->buffer);
  free(queue); // queue_handle_t is an opaque pointer, can free
}

queue_result_t queue_send(queue_handle_t queue, const void *item,
                          uint32_t timeout) {
  if (!queue || !item) return QUEUE_ERROR_NULL_POINTER;

  uint32_t start = tick_now;
  uint32_t deadline = start + timeout;

  for (;;) {
    // KERNEL_CRITICAL_BEGIN();

    if (!cb_is_full(&queue->buffer)) {
      if (cb_put(&queue->buffer, item) != CB_SUCCESS) {
        // Shouldn't happen given cb_is_full() check
        // KERNEL_CRITICAL_END();
        return QUEUE_ERROR_FULL;
      }

      if (!list_is_empty(&queue->waiting_receivers)) {
        wake_one(&queue->waiting_receivers);
      }
      // KERNEL_CRITICAL_END();
      return QUEUE_SUCCESS;
    }

    if (timeout == 0) {
      // KERNEL_CRITICAL_END();
      return QUEUE_ERROR_FULL;
    }

    uint32_t now = tick_now;
    uint32_t remain = ticks_until(deadline, now);
    if (remain == 0) {
      // KERNEL_CRITICAL_END();
      return QUEUE_ERROR_TIMEOUT;
    }

    // Block current task as a SENDER with timeout
    current_task->waiting_on = queue;
    waitlist_push_tail(&queue->waiting_senders, current_task);

    uint32_t wake = now + remain;
    scheduler_set_timeout(current_task, wake);

    task_set_state(current_task, TASK_BLOCKED);
    // KERNEL_CRITICAL_END();
    scheduler_yield();                         // switch out
    timeout = ticks_until(deadline, tick_now); // recompute and retry
  }
}

queue_result_t queue_receive(queue_handle_t queue, void *item,
                             uint32_t timeout) {
  if (!queue || !item) return QUEUE_ERROR_NULL_POINTER;

  uint32_t start = tick_now;
  uint32_t deadline = start + timeout;

  for (;;) {
    // KERNEL_CRITICAL_BEGIN();

    if (!cb_is_empty(&queue->buffer)) {
      if (cb_get(&queue->buffer, item) != CB_SUCCESS) {
        // Shouldn't happen given cb_is_empty() check
        // KERNEL_CRITICAL_END();
        return QUEUE_ERROR_EMPTY;
      }

      // Sender might be waiting; wake one
      if (!list_is_empty(&queue->waiting_senders)) {
        wake_one(&queue->waiting_senders);
      }
      // KERNEL_CRITICAL_END();
      return QUEUE_SUCCESS;
    }

    if (timeout == 0) {
      // KERNEL_CRITICAL_END();
      return QUEUE_ERROR_EMPTY;
    }

    uint32_t now = tick_now;
    uint32_t remain = ticks_until(deadline, now);
    if (remain == 0) {
      // KERNEL_CRITICAL_END();
      return QUEUE_ERROR_TIMEOUT;
    }

    // Block current task as a RECEIVER with timeout
    current_task->waiting_on = queue;
    waitlist_push_tail(&queue->waiting_receivers, current_task);

    uint32_t wake = now + remain;
    scheduler_set_timeout(current_task, wake);

    task_set_state(current_task, TASK_BLOCKED);
    // KERNEL_CRITICAL_END();

    scheduler_yield();                         // switch out
    timeout = ticks_until(deadline, tick_now); // recompute and retry
  }
}

queue_result_t queue_send_immediate(queue_handle_t queue, const void *item) {
  return queue_send(queue, item, 0);
}

queue_result_t queue_receive_immediate(queue_handle_t queue, void *item) {
  return queue_receive(queue, item, 0);
}

bool queue_is_empty(queue_handle_t queue) {
  return queue ? cb_is_empty(&queue->buffer) : true;
}

bool queue_is_full(queue_handle_t queue) {
  return queue ? cb_is_full(&queue->buffer) : false;
}

size_t queue_messages_waiting(queue_handle_t queue) {
  return queue ? cb_size(&queue->buffer) : 0;
}
