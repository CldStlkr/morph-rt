#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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


static inline bool cb_is_empty(const circular_buffer_t *self) { return self && (self->size == 0); }


static inline bool cb_is_full(const circular_buffer_t *self) { return self && (self->size == self->capacity); }


static inline size_t cb_size(const circular_buffer_t *self) { return self ? self->size : 0; }


static inline cb_result_t cb_init(circular_buffer_t *cb, void *buffer, size_t capacity,
                    size_t element_size) {

  if (!cb || !buffer) return CB_ERROR_NULL_POINTER;

  if (capacity == 0 || element_size == 0) {
    return CB_ERROR_INVALID_SIZE;
  }

  size_t new_capacity;

  // Check if already power of 2
  if ((capacity & (capacity - 1)) == 0) {
    new_capacity = capacity;
  } else {
    new_capacity = 1;
    while (new_capacity < capacity) {
      new_capacity <<= 1;
    }
  }

  cb->mask = new_capacity - 1;



  cb->capacity = new_capacity;
  cb->buffer = buffer;
  cb->element_size = element_size;
  cb->size = 0;
  cb->head = 0;
  cb->tail = 0;

  return CB_SUCCESS;
}


static inline cb_result_t cb_clear(circular_buffer_t *self) {
  if (!self) return CB_ERROR_NULL_POINTER;
  self->size = 0;
  self->head = 0;
  self->tail = 0;

  return CB_SUCCESS;
}

// Does not return cb_result_t like other cb functions.
// Instead returns a pointer to the internal buffer,
// which the caller has the responsibility of freeing.
static inline void* cb_deinit(circular_buffer_t *self) {
  if (!self) return NULL;

  void *buffer_p = self->buffer;
  self->buffer = NULL;
  self->capacity = 0;
  self->head = 0;
  self->tail = 0;
  self->size = 0;
  self->mask = 0;

  return buffer_p;
}


static inline cb_result_t cb_put(circular_buffer_t *self, const void *data) {
  if (!self || !data) return CB_ERROR_NULL_POINTER;
  if (cb_is_full(self)) return CB_ERROR_BUFFER_FULL;

  uint8_t *buffer_base = (uint8_t *)self->buffer;
  uint8_t *dest = buffer_base + (self->tail * self->element_size);

  memcpy(dest, data, self->element_size);

  self->tail = (self->tail + 1) & self->mask;
  self->size++;

  return CB_SUCCESS;
}


static inline cb_result_t cb_get(circular_buffer_t *self, void *data_out) {
  if (!self || !data_out) return CB_ERROR_NULL_POINTER;
  if (cb_is_empty(self)) return CB_ERROR_BUFFER_EMPTY;

  uint8_t *buffer_base = (uint8_t *)self->buffer;
  uint8_t *src = buffer_base + (self->head * self->element_size);

  memcpy(data_out, src, self->element_size);

  self->head = (self->head + 1) & self->mask;
  self->size--;

  return CB_SUCCESS;
}


static inline cb_result_t cb_peek(const circular_buffer_t *self, void *data_out) {
  if (!self || !data_out) return CB_ERROR_NULL_POINTER;
  if (cb_is_empty(self)) return CB_ERROR_BUFFER_EMPTY;

  const uint8_t *buffer_base = (const uint8_t *)self->buffer;
  const uint8_t *src = buffer_base + (self->head * self->element_size);

  memcpy(data_out, src, self->element_size);

  return CB_SUCCESS;
}


static inline void cb_print_stats(const circular_buffer_t *self) {
  if (!self) return;
  printf("circular buffer stats:\n");
  printf("\t capacity: %zu\n", self->capacity);
  printf("\t size: %zu\n", self->size);
  printf("\t head index: %zu\n", self->head);
  printf("\t tail index: %zu\n", self->tail);
  printf("\t bit mask value: %zu\n\n", self->mask);
}


#endif // !CIRCULAR_BUFFER_H
