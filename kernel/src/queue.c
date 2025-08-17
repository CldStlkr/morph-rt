#include "queue.h"
#include "scheduler.h"
#include "task.h"

typedef struct queue_control_block {
  circular_buffer_t buffer;

  // FIFO wait lists (head/tail)
  task_handle_t waiting_senders_head;
  task_handle_t waiting_senders_tail;
  task_handle_t waiting_receivers_head;
  task_handle_t waiting_receivers_tail;

  // Add mutex/semaphore for thread safety later
} queue_control_block;

// ============================== HELPER FUNCTIONS ================================

static inline void wl_enqueue(task_handle_t *head, task_handle_t *tail, task_handle_t t) {
  if (!t) {
    return;
  }

  t->next = NULL;
  if (!*tail) {
    *head = *tail = t;
  } else {
    (*tail)->next = t;
    *tail = t;
  }
}

static inline task_handle_t wl_dequeue(task_handle_t *head, task_handle_t *tail) {
  if (!*head) {
    return NULL;
  }
  task_handle_t t = *head;

  *head = t->next;

  if (!*head) {
    *tail = NULL; // Is now empty
  }

  t->next = NULL; // No longer a part of the list

  return t;
}

/* ================================================================================ */

// Public API
queue_handle_t queue_create(size_t queue_length, size_t item_size) {
  if (queue_length == 0 || item_size == 0) {
    return NULL;
  }

  queue_control_block *qcb = calloc(1, sizeof(queue_control_block));
  if (!qcb) {
    return NULL;
  }

  cb_result_t res = cb_init(&qcb->buffer, queue_length, item_size);
  if (res != CB_SUCCESS) {
    free(qcb);
    return NULL;
  }

  return (queue_handle_t)qcb;
}

void queue_delete(queue_handle_t queue) {
  if (!queue) {
    return;
  }

  cb_free(&queue->buffer);
  free(queue); // queue_handle_t is an opaque pointer for queue_control_block
}

queue_result_t queue_send(queue_handle_t queue, const void *item, uint32_t timeout) {
  if (!queue || !item) {
    return QUEUE_ERROR_NULL_POINTER;
  }

  cb_result_t result = cb_put(&queue->buffer, item);

  if (result == CB_SUCCESS) {
    task_handle_t receiver =
        wl_dequeue(&queue->waiting_receivers_head, &queue->waiting_receivers_tail);
    if (receiver) {
      scheduler_unblock_task(receiver);
    }
    return QUEUE_SUCCESS;
  }

  if (result == CB_ERROR_BUFFER_FULL) {
    if (timeout == 0) {
      return QUEUE_ERROR_FULL;
    }
    return QUEUE_ERROR_TIMEOUT;
  }

  // TODO: For now, we don't implement blocking with timeout
  // In full implementation, we would:
  // 1. Add current task to waiting_senders list
  // 2. Block current task for 'timeout' ticks
  // 3. When unblocked, retry the operation

  return QUEUE_ERROR_TIMEOUT;
}

queue_result_t queue_receive(queue_handle_t queue, void *item, uint32_t timeout) {
  if (!queue || !item) {
    return QUEUE_ERROR_NULL_POINTER;
  }

  cb_result_t result = cb_get(&queue->buffer, item);

  if (result == CB_SUCCESS) {
    // A slot freed up so wake one sender
    task_handle_t sender = wl_dequeue(&queue->waiting_senders_head, &queue->waiting_senders_tail);
    if (sender) {
      scheduler_unblock_task(sender);
    }
    return QUEUE_SUCCESS;
  }

  if (result == CB_ERROR_BUFFER_EMPTY) {
    if (timeout == 0) {
      return QUEUE_ERROR_EMPTY;
    }

    // TODO: For now, we don't implement blocking with timeout
    // In full implementation, we would:
    // 1. Add current task to waiting_receivers list
    // 2. Block current task for 'timeout' ticks
    // 3. When unblocked, retry the operation
  }
  return QUEUE_ERROR_TIMEOUT;
}

queue_result_t queue_send_immediate(queue_handle_t queue, const void *item) {
  return queue_send(queue, item, 0); // timeout = 0 means non-blocking
}
queue_result_t queue_receive_immediate(queue_handle_t queue, void *item) {
  return queue_receive(queue, item, 0);
}

bool queue_is_empty(queue_handle_t queue) { return queue ? cb_is_empty(&queue->buffer) : true; }
bool queue_is_full(queue_handle_t queue) { return queue ? cb_is_full(&queue->buffer) : false; }

size_t queue_messages_waiting(queue_handle_t queue) { return queue ? cb_size(&queue->buffer) : 0; }
