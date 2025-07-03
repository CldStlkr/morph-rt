#include "queue.h"
#include "scheduler.h"
#include "task.h"

typedef struct queue_control_block {
  circular_buffer_t buffer;
  task_handle_t waiting_senders;
  task_handle_t waiting_receivers;
  // Add mutex/semaphore for thread safety later
} queue_control_block;

// ======================================== HELPER FUNCTIONS =====================================

static void add_to_waiting_list(task_handle_t *list_head, task_handle_t task) {
  if (!task) {
    return;
  }
  task->next = *list_head;
  *list_head = task;
}

static task_handle_t remove_from_waiting_list(task_handle_t *list_head) {
  if (!list_head || !*list_head) {
    return NULL;
  }

  task_handle_t task = *list_head;
  *list_head = task->next;
  task->next = NULL;
  return task;
}
// ===============================================================================================

// Public API
queue_handle_t queue_create(size_t queue_length, size_t item_size) {
  if (queue_length == 0 || item_size == 0) {
    return NULL;
  }

  queue_control_block *qcb = malloc(sizeof(queue_control_block));
  if (!qcb) {
    return NULL;
  }

  cb_result_t res = cb_init(&qcb->buffer, queue_length, item_size);
  if (res != CB_SUCCESS) {
    free(qcb);
    return NULL;
  }

  qcb->waiting_senders = NULL;
  qcb->waiting_receivers = NULL;

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
    task_handle_t waiting_receiver = remove_from_waiting_list(&queue->waiting_receivers);
    if (waiting_receiver) {
      scheduler_unblock_task(waiting_receiver);
    }

    return QUEUE_SUCCESS;
  }

  if (result == CB_ERROR_BUFFER_FULL) {
    if (timeout == 0) {
      return QUEUE_ERROR_FULL;
    }
  }

  // TODO: For now, we don't implement blocking with timeout
  // In full implementation, we would:
  // 1. Add current task to waiting_senders list
  // 2. Block current task for 'timeout' ticks
  // 3. When unblocked, retry the operation

  return QUEUE_ERROR_NULL_POINTER;
}

queue_result_t queue_receive(queue_handle_t queue, void *item, uint32_t timeout) {
  if (!queue || !item) {
    return QUEUE_ERROR_NULL_POINTER;
  }

  cb_result_t result = cb_get(&queue->buffer, item);

  if (result == CB_SUCCESS) {
    if (timeout == 0) {
      return QUEUE_ERROR_EMPTY;
    }
  }

  // TODO: For now, we don't implement blocking with timeout
  // In full implementation, we would:
  // 1. Add current task to waiting_receivers list
  // 2. Block current task for 'timeout' ticks
  // 3. When unblocked, retry the operation

  return QUEUE_ERROR_EMPTY;
}

queue_result_t queue_send_immediate(queue_handle_t queue, const void *item) {
  return queue_send(queue, item, 0); // timeout = 0 means non-blocking
}
queue_result_t queue_receive_immediate(queue_handle_t queue, const void *item) {
  return queue_send(queue, item, 0);
}

bool queue_is_empty(queue_handle_t queue) { return queue ? cb_is_empty(&queue->buffer) : true; }
bool queue_is_full(queue_handle_t queue) { return queue ? cb_is_full(&queue->buffer) : false; }

size_t queue_messages_waiting(queue_handle_t queue) { return queue ? cb_size(&queue->buffer) : 0; }
