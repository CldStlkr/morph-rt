#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum cb_result_t {
  CB_SUCCESS,
  CB_ERROR_NULL_POINTER,
  CB_ERROR_BUFFER_FULL,
  CB_ERROR_BUFFER_EMPTY,
  CB_ERROR_INVALID_SIZE,
  CB_ERROR_ALLOCATION_FAILED
} cb_result_t;

typedef struct circular_buffer_t {
  size_t capacity;
  void *buffer; // Void pointer for flexible generics
  size_t element_size;
  size_t size;
  size_t head;
  size_t tail;
  size_t mask; // Bit mask
} circular_buffer_t;

cb_result_t cb_init(circular_buffer_t *cb, size_t capacity,
                    size_t element_size);
cb_result_t cb_free(circular_buffer_t *self);
cb_result_t cb_clear(circular_buffer_t *self);

cb_result_t cb_put(circular_buffer_t *self, const void *data);
cb_result_t cb_get(circular_buffer_t *self, void *data_out);
cb_result_t cb_peek(const circular_buffer_t *self, void *data_out);

void cb_print_stats(const circular_buffer_t *self); // Debug purposes

static inline bool cb_is_empty(const circular_buffer_t *self) {
  return self && (self->size == 0);
}
static inline bool cb_is_full(const circular_buffer_t *self) {
  return self && (self->size == self->capacity);
}
static inline size_t cb_capacity(const circular_buffer_t *self) {
  return self ? self->capacity : 0;
}
static inline size_t cb_size(const circular_buffer_t *self) {
  return self ? self->size : 0;
}
static inline size_t cb_available(const circular_buffer_t *self) {
  return self ? (self->capacity - self->size) : 0;
}

#endif // !CIRCULAR_BUFFER_H
