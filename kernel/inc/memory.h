#ifndef MEMORY_H
#define MEMORY_H

#include "config.h"
#include "queue.h"
#include "semaphore.h"
#include "task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Memory pool structure
typedef struct memory_pool {
  void *pool_start;     // Starting position in memory of pool
  size_t object_size;   // Object size
  size_t pool_size;     // Math can be done to extract end address of pool
  uint32_t free_bitmap; // Bitmap for free objects (up to 32 objects)
  size_t free_count;    // Number of free objects
} memory_pool_t;

typedef enum {
  POOL_TCB,
  POOL_STACK_SMALL,
  POOL_STACK_DEFAULT,
  POOL_STACK_LARGE,
  POOL_QCB,
  POOL_BUFFER_SMALL,
  POOL_BUFFER_MEDIUM,
  POOL_BUFFER_LARGE,
  POOL_SCB,
  POOL_MCB,
  POOL_COUNT,
} pool_type_t;

// Memory pool API
void memory_pools_init(void);
void *pool_alloc(pool_type_t pool_type);
bool pool_free(pool_type_t pool_type, void *ptr);

// Helper functions for specific types
void *task_pool_alloc_tcb(void);
void *task_pool_alloc_stack(size_t requested_size);
bool task_pool_free_tcb(task_control_block *tcb);
bool task_pool_free_stack(uint32_t *stack);

queue_control_block *queue_pool_alloc_qcb(void);
void *queue_pool_alloc_buffer(size_t requested_size);
bool queue_pool_free_cb(queue_control_block *qcb);
bool queue_pool_free_buffer(void *buffer);

semaphore_control_block *sem_pool_alloc_scb(void);
bool sem_pool_free_scb(semaphore_control_block *sem);

typedef struct pool_stats {
  size_t total_objects;
  size_t free_objects;
  size_t used_objects;
  size_t peak_usage;
} pool_stats_t;

pool_stats_t pool_get_stats(pool_type_t pool_type);
void pool_print_stats(void);

#endif // !MEMORY_H
