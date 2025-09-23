#include "critical.h"
#include "memory.h"
#include "queue.h"
#include "scheduler.h"
#include "task.h"

#include <string.h>

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
  t->wake_reason = WAKE_REASON_DATA_AVAILABLE;

  // If task had timeout armed, cancel from delayed lists
  scheduler_cancel_timeout(t);
  scheduler_add_task(t);
}

static size_t calculate_buffer_size(size_t queue_length, size_t item_size) {
  return queue_length * item_size;
}

// =============================================================================

// Public API
queue_handle_t queue_create(size_t queue_length, size_t item_size) {
  if (queue_length == 0 || item_size == 0) return NULL;

  queue_control_block *qcb = queue_pool_alloc_qcb();
  if (!qcb) return NULL;

  size_t buffer_size = calculate_buffer_size(queue_length, item_size);

  void *buffer = queue_pool_alloc_buffer(buffer_size);
  if (!buffer) {
    queue_pool_free_cb(qcb);
    return NULL;
  }

  size_t capacity = 1;
  while (capacity < queue_length) {
    capacity <<= 1;
  }

  qcb->buffer.capacity = capacity;
  qcb->buffer.buffer = buffer;
  qcb->buffer.element_size = item_size;
  qcb->buffer.size = 0;
  qcb->buffer.head = 0;
  qcb->buffer.tail = 0;
  qcb->buffer.mask = capacity - 1;

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
    KERNEL_CRITICAL_BEGIN();

    if (!cb_is_full(&queue->buffer)) {
      if (cb_put(&queue->buffer, item) != CB_SUCCESS) {
        // Shouldn't happen given cb_is_full() check
        KERNEL_CRITICAL_END();
        return QUEUE_ERROR_FULL;
      }

      if (!list_is_empty(&queue->waiting_receivers)) {
        wake_one(&queue->waiting_receivers);
      }
      KERNEL_CRITICAL_END();
      return QUEUE_SUCCESS;
    }

    if (timeout == 0) {
      KERNEL_CRITICAL_END();
      return QUEUE_ERROR_FULL;
    }

    uint32_t now = tick_now;
    uint32_t remain = ticks_until(deadline, now);
    if (remain == 0) {
      KERNEL_CRITICAL_END();
      return QUEUE_ERROR_TIMEOUT;
    }

    // Block current task as a SENDER with timeout
    current_task->waiting_on = queue;
    waitlist_push_tail(&queue->waiting_senders, current_task);
    uint32_t wake = now + remain;
    scheduler_set_timeout(current_task, wake);
    task_set_state(current_task, TASK_BLOCKED);
    KERNEL_CRITICAL_END();
    scheduler_yield(); // switch out

    if (current_task->wake_reason == WAKE_REASON_TIMEOUT) {
      return QUEUE_ERROR_TIMEOUT;
    }

    timeout = ticks_until(deadline, tick_now);
    if (timeout == 0) {
      return QUEUE_ERROR_TIMEOUT;
    }
  }
}

queue_result_t queue_receive(queue_handle_t queue, void *item,
                             uint32_t timeout) {
  if (!queue || !item) return QUEUE_ERROR_NULL_POINTER;

  uint32_t start = tick_now;
  uint32_t deadline = start + timeout;

  for (;;) {
    KERNEL_CRITICAL_BEGIN();

    if (!cb_is_empty(&queue->buffer)) {
      if (cb_get(&queue->buffer, item) != CB_SUCCESS) {
        // Shouldn't happen given cb_is_empty() check
        KERNEL_CRITICAL_END();
        return QUEUE_ERROR_EMPTY;
      }

      // Sender might be waiting; wake one
      if (!list_is_empty(&queue->waiting_senders)) {
        wake_one(&queue->waiting_senders);
      }
      KERNEL_CRITICAL_END();
      return QUEUE_SUCCESS;
    }

    if (timeout == 0) {
      KERNEL_CRITICAL_END();
      return QUEUE_ERROR_EMPTY;
    }

    uint32_t now = tick_now;
    uint32_t remain = ticks_until(deadline, now);
    if (remain == 0) {
      KERNEL_CRITICAL_END();
      return QUEUE_ERROR_TIMEOUT;
    }

    // Block current task as a RECEIVER with timeout
    current_task->waiting_on = queue;
    waitlist_push_tail(&queue->waiting_receivers, current_task);
    uint32_t wake = now + remain;
    scheduler_set_timeout(current_task, wake);
    task_set_state(current_task, TASK_BLOCKED);
    KERNEL_CRITICAL_END();

    scheduler_yield(); // switch out

    if (current_task->wake_reason == WAKE_REASON_TIMEOUT) {
      return QUEUE_ERROR_TIMEOUT;
    }

    timeout = ticks_until(deadline, tick_now);
    if (timeout == 0) {
      return QUEUE_ERROR_TIMEOUT;
    }
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
