#ifndef QUEUE_H
#define QUEUE_H

#include "circular_buffer.h"

// Opaque pointers
typedef struct queue_control_block *queue_handle_t;

typedef enum {
  QUEUE_SUCCESS,
  QUEUE_ERROR_NULL_POINTER,
  QUEUE_ERROR_FULL,
  QUEUE_ERROR_EMPTY,
  QUEUE_ERROR_TIMEOUT,
  QUEUE_ERROR_ALLOCATION_FAILED,
} queue_result_t;

// Public API
queue_handle_t queue_create(size_t queue_length, size_t item_size);
void queue_delete(queue_handle_t queue);

queue_result_t queue_send(queue_handle_t queue, const void *item, uint32_t timeout);
queue_result_t queue_receive(queue_handle_t queue, void *item, uint32_t timeout);

bool queue_is_empty(queue_handle_t queue);
bool queue_is_full(queue_handle_t queue);

size_t queue_messages_waiting(queue_handle_t queue);

// Non-blocking versions
// queue_result_t queue_send_immediate(queue_handle_t queue, const void *item);
// queue_result_t queue_receive_immediate(queue_handle_t queue, const void *item);

#endif // !QUEUE_H
