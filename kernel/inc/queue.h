#ifndef QUEUE_H
#define QUEUE_H

#include "circular_buffer.h"
#include "list.h"

typedef struct queue_control_block {
  circular_buffer_t buffer;

  list_head_t waiting_senders;   // Tasks blocked in queue_send()
  list_head_t waiting_receivers; // Tasks blocked in queue_receive()

  // Add mutex/semaphore for thread safety later
} queue_control_block;

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

queue_result_t queue_send(queue_handle_t queue, const void *item,
                          uint32_t timeout);
queue_result_t queue_receive(queue_handle_t queue, void *item,
                             uint32_t timeout);

size_t queue_messages_waiting(queue_handle_t queue);

// Non-blocking versions
queue_result_t queue_send_immediate(queue_handle_t queue, const void *item);
queue_result_t queue_receive_immediate(queue_handle_t queue, void *item);

bool queue_is_empty(queue_handle_t queue);
bool queue_is_full(queue_handle_t queue);

#endif // !QUEUE_H
