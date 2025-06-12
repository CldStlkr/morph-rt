#include "../inc/circular_buffer.h"
#include <stdio.h>
#include <string.h>

cb_result_t cb_init(circular_buffer_t *cb, size_t capacity,
                    size_t element_size) {

  if (!cb || capacity == 0 || element_size == 0) {
    return CB_ERROR_NULL_POINTER;
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

  void *buffer = calloc(new_capacity, element_size);

  if (!buffer) {
    return CB_ERROR_ALLOCATION_FAILED;
  }

  cb->capacity = new_capacity;
  cb->buffer = buffer;
  cb->element_size = element_size;
  cb->size = 0;
  cb->head = 0;
  cb->tail = 0;

  return CB_SUCCESS;
}

cb_result_t cb_clear(circular_buffer_t *self) {
  if (!self) {
    return CB_ERROR_NULL_POINTER;
  }
  self->size = 0;
  self->head = 0;
  self->tail = 0;

  return CB_SUCCESS;
}

cb_result_t cb_free(circular_buffer_t *self) {
  if (!self) {
    return CB_ERROR_NULL_POINTER;
  }
  free(self->buffer);
  self->buffer = NULL;
  self->size = 0;

  return CB_SUCCESS;
}

bool cb_is_empty(const circular_buffer_t *self) {
  if (!self) {
    return false;
  }
  return self->size == 0;
}

bool cb_is_full(const circular_buffer_t *self) {
  if (!self) {
    return false;
  }
  return self->size == self->capacity;
}

cb_result_t cb_put(circular_buffer_t *self, const void *data) {
  if (!self || !data) {
    return CB_ERROR_NULL_POINTER;
  }
  if (cb_is_full(self)) {
    return CB_ERROR_BUFFER_FULL;
  }

  uint8_t *buffer_base = (uint8_t *)self->buffer;
  uint8_t *dest = buffer_base + (self->tail * self->element_size);

  memcpy(dest, data, self->element_size);

  self->tail = (self->tail + 1) & self->mask;
  self->size++;

  return CB_SUCCESS;
}

cb_result_t cb_get(circular_buffer_t *self, void *data_out) {
  if (!self || !data_out) {
    return CB_ERROR_NULL_POINTER;
  }
  if (cb_is_empty(self)) {
    return CB_ERROR_BUFFER_EMPTY;
  }

  uint8_t *buffer_base = (uint8_t *)self->buffer;
  uint8_t *src = buffer_base + (self->head * self->element_size);

  memcpy(data_out, src, self->element_size);

  self->head = (self->head + 1) & self->mask;
  self->size--;

  return CB_SUCCESS;
}

cb_result_t cb_peek(const circular_buffer_t *self, void *data_out) {
  if (!self || !data_out) {
    return CB_ERROR_NULL_POINTER;
  }
  if (cb_is_empty(self)) {
    return CB_ERROR_BUFFER_EMPTY;
  }

  uint8_t *buffer_base = (uint8_t *)self->buffer;
  uint8_t *src = buffer_base + (self->head * self->element_size);

  memcpy(data_out, src, self->element_size);

  return CB_SUCCESS;
}

size_t cb_capacity(const circular_buffer_t *self) {
  return self ? self->capacity : 0;
}

size_t cb_size(const circular_buffer_t *self) { return self ? self->size : 0; }

size_t cb_available(const circular_buffer_t *self) {
  return self->capacity - self->size;
}

void cb_print_stats(const circular_buffer_t *self) {
  printf("circular buffer stats:\n");
  printf("\t capacity: %zu\n", self->capacity);
  printf("\t size: %zu\n", self->size);
  printf("\t head index: %zu\n", self->head);
  printf("\t tail index: %zu\n", self->tail);
  printf("\t bit mask value: %zu\n", self->mask);
}
