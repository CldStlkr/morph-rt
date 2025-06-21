#include "../inc/queue.h"
#include "../inc/task.h"

typedef struct queue_control_block {
  circular_buffer_t buffer;
  task_handle_t waiting_senders;
  task_handle_t waiting_receivers;
  // Add mutex/semaphore for thread safety later
} queue_control_block;

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

queue_result_t queue_send(queue_handle_t queue, const void *item, uint32_t timeout);
queue_result_t queue_receive(queue_handle_t queue, void *item, uint32_t timeout);

bool queue_is_empty(queue_handle_t queue);
bool queue_is_full(queue_handle_t queue);

size_t queue_messages_waiting(queue_handle_t queue);
