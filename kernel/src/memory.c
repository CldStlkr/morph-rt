#include "critical.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

// Task Control Blocks
static task_control_block tcb_pool[MAX_TASKS];
static memory_pool_t tcb_pool_mgr;

// Task Stacks
static uint8_t small_stacks[MAX_SMALL_STACKS][SMALL_STACK_SIZE];
static uint8_t default_stacks[MAX_DEFAULT_STACKS][DEFAULT_STACK_SIZE];
static uint8_t large_stacks[MAX_LARGE_STACKS][LARGE_STACK_SIZE];

static memory_pool_t stack_small_pool_mgr;
static memory_pool_t stack_default_pool_mgr;
static memory_pool_t stack_large_pool_mgr;

// Queue Control Blocks
static queue_control_block queue_pool[MAX_QUEUES];
static memory_pool_t queue_pool_mgr;

// Queue Buffers
static uint8_t small_buffers[MAX_SMALL_BUFFERS][SMALL_BUFFER_SIZE];
static uint8_t medium_buffers[MAX_MEDIUM_BUFFERS][DEFAULT_BUFFER_SIZE];
static uint8_t large_buffers[MAX_LARGE_BUFFERS][LARGE_BUFFER_SIZE];

static memory_pool_t buffer_small_pool_mgr;
static memory_pool_t buffer_medium_pool_mgr;
static memory_pool_t buffer_large_pool_mgr;

static semaphore_control_block semaphore_pool[MAX_SEMAPHORES];
static mutex_control_block mutex_pool[MAX_MUTEXES];

static memory_pool_t semaphore_pool_mgr;
static memory_pool_t mutex_pool_mgr;

// Array of all pool managers for easy access

static memory_pool_t *pools[POOL_COUNT] = {
    &tcb_pool_mgr,           &stack_small_pool_mgr,  &stack_default_pool_mgr,
    &stack_large_pool_mgr,   &queue_pool_mgr,        &buffer_small_pool_mgr,
    &buffer_medium_pool_mgr, &buffer_large_pool_mgr, &semaphore_pool_mgr,
    &mutex_pool_mgr};

// Peak usage tracking
static size_t peak_usage[POOL_COUNT] = {0};

// ============================== HELPER FUNCTIONS =============================

static void pool_init(memory_pool_t *pool, void *pool_start, size_t object_size,
                      size_t max_objects) {
  pool->pool_start = pool_start;
  pool->object_size = object_size;
  pool->pool_size = object_size * max_objects;
  pool->free_bitmap =
      (1ULL << max_objects) - 1; // All objects are free upon initialization
  pool->free_count = max_objects;
}

static int find_free_bit(uint32_t bitmap) {
  if (bitmap == 0) return -1;

  // Find first set bit (free object);
  for (int i = 0; i < 32; i++) {
    if (bitmap & (1U << i)) {
      return i;
    }
  }

  return -1;
}

static void *get_object_ptr(memory_pool_t *pool, int index) {
  return (uint8_t *)pool->pool_start + (index * pool->object_size);
}

static int get_object_index(memory_pool_t *pool, void *ptr) {
  if (!ptr || ptr < pool->pool_start) {
    return -1;
  }

  ptrdiff_t offset = (uint8_t *)ptr - (uint8_t *)pool->pool_start;
  if (offset < 0 || (size_t)offset >= pool->pool_size) {
    return -1;
  }

  return offset / pool->object_size;
}

// ============================== PUBLIC API =============================

void memory_pools_init(void) {
  // TCB pool
  pool_init(&tcb_pool_mgr, tcb_pool, sizeof(task_control_block), MAX_TASKS);

  // Stack pools
  pool_init(&stack_small_pool_mgr, small_stacks, SMALL_STACK_SIZE,
            MAX_SMALL_STACKS);
  pool_init(&stack_default_pool_mgr, default_stacks, DEFAULT_STACK_SIZE,
            MAX_DEFAULT_STACKS);

  pool_init(&stack_large_pool_mgr, large_stacks, LARGE_STACK_SIZE,
            MAX_LARGE_STACKS);

  // QCB pool
  pool_init(&queue_pool_mgr, queue_pool, sizeof(queue_control_block),
            MAX_QUEUES);

  // Buffer pools
  pool_init(&buffer_small_pool_mgr, small_buffers, SMALL_BUFFER_SIZE,
            MAX_SMALL_BUFFERS);
  pool_init(&buffer_medium_pool_mgr, medium_buffers, DEFAULT_BUFFER_SIZE,
            MAX_MEDIUM_BUFFERS);

  pool_init(&buffer_large_pool_mgr, large_buffers, LARGE_BUFFER_SIZE,
            MAX_LARGE_BUFFERS);

  pool_init(&semaphore_pool_mgr, semaphore_pool,
            sizeof(semaphore_control_block), MAX_SEMAPHORES);

  // Placeholder mutex pools
  pool_init(&mutex_pool_mgr, mutex_pool, sizeof(mutex_control_block), MAX_MUTEXES);

  // Clear peak usage counters
  memset(peak_usage, 0, sizeof(peak_usage));
}

void *pool_alloc(pool_type_t pool_type) {
  if (pool_type >= POOL_COUNT) {
    return NULL;
  }

  memory_pool_t *pool = pools[pool_type];

  KERNEL_CRITICAL_BEGIN();

  if (pool->free_count == 0) {
    KERNEL_CRITICAL_END();
    return NULL;
  }

  int free_index = find_free_bit(pool->free_bitmap);
  if (free_index < 0) {
    KERNEL_CRITICAL_END();
    return NULL;
  }

  // Mark as allocated
  pool->free_bitmap &= ~(1U << free_index);
  pool->free_count--;

  size_t used = (pool->pool_size / pool->object_size) - pool->free_count;
  if (used > peak_usage[pool_type]) {
    peak_usage[pool_type] = used;
  }

  KERNEL_CRITICAL_END();

  void *ptr = get_object_ptr(pool, free_index);

  // Zero out allocated memory
  memset(ptr, 0, pool->object_size);

  return ptr;
}

bool pool_free(pool_type_t pool_type, void *ptr) {
  if (pool_type >= POOL_COUNT || !ptr) {
    return false;
  }

  memory_pool_t *pool = pools[pool_type];
  int index = get_object_index(pool, ptr);

  if (index < 0) {
    return false; // Invalid pointer
  }

  KERNEL_CRITICAL_BEGIN();

  // Check if already free
  if (pool->free_bitmap & (1U << index)) {
    KERNEL_CRITICAL_END();
    return false;
  }

  pool->free_bitmap |= (1U << index);
  pool->free_count++;

  KERNEL_CRITICAL_END();

  return true;
}

// ========================== TASK-SPECIFIC HELPERS ===========================
void *task_pool_alloc_tcb(void) { return pool_alloc(POOL_TCB); }

// requested_size is in bytes. Will round up to nearest preset stack size.
// SMALL = 512, DEFAULT = 1024, LARGE = 2048
void *task_pool_alloc_stack(size_t requested_size) {
  if (requested_size <= SMALL_STACK_SIZE) {
    return pool_alloc(POOL_STACK_SMALL);
  } else if (requested_size <= DEFAULT_STACK_SIZE) {
    return pool_alloc(POOL_STACK_DEFAULT);
  } else if (requested_size <= LARGE_STACK_SIZE) {
    return pool_alloc(POOL_STACK_LARGE);
  }

  return NULL; // Requested size too large
}

bool task_pool_free_tcb(task_control_block *tcb) {
  return pool_free(POOL_TCB, tcb);
}

bool task_pool_free_stack(uint32_t *stack) {
  if (pool_free(POOL_STACK_SMALL, stack)) return true;
  if (pool_free(POOL_STACK_DEFAULT, stack)) return true;
  if (pool_free(POOL_STACK_LARGE, stack)) return true;

  return false;
}

// ========================== QUEUE-SPECIFIC HELPERS ===========================
queue_control_block *queue_pool_alloc_qcb(void) {
  return (queue_control_block *)pool_alloc(POOL_QCB);
}

void *queue_pool_alloc_buffer(size_t requested_size) {

  if (requested_size <= SMALL_BUFFER_SIZE) {
    return pool_alloc(POOL_BUFFER_SMALL);
  } else if (requested_size <= DEFAULT_BUFFER_SIZE) {
    return pool_alloc(POOL_BUFFER_MEDIUM);
  } else if (requested_size <= LARGE_BUFFER_SIZE) {
    return pool_alloc(POOL_BUFFER_LARGE);
  }

  return NULL; // Requested size too large
}

bool queue_pool_free_qcb(queue_control_block *qcb) {
  return pool_free(POOL_QCB, qcb);
}

bool queue_pool_free_buffer(void *buffer) {
  if (pool_free(POOL_BUFFER_SMALL, buffer)) return true;
  if (pool_free(POOL_BUFFER_MEDIUM, buffer)) return true;
  if (pool_free(POOL_BUFFER_LARGE, buffer)) return true;

  return false;
}

// ==================== SEMAPHORE-SPECIFIC HELPERS ============================
semaphore_control_block *sem_pool_alloc_scb(void) {
  return (semaphore_control_block *)pool_alloc(POOL_SCB);
}

bool sem_pool_free_scb(semaphore_control_block *sem) {
  return pool_free(POOL_SCB, sem);
}


// ==================== MUTEX-SPECIFIC HELPERS ============================
mutex_control_block *mutex_pool_alloc_mcb(void) {
  return (mutex_control_block*)pool_alloc(POOL_MCB);
}

bool mutex_pool_free_mcb(mutex_control_block *mutex) {
  return pool_free(POOL_MCB, mutex);
}

// ======================= STATISTICS AND DEBUG ================================

pool_stats_t pool_get_stats(pool_type_t pool_type) {
  pool_stats_t stats = {0, 0, 0, 0};

  if (pool_type >= POOL_COUNT) {
    return stats;
  }

  memory_pool_t *pool = pools[pool_type];
  size_t total_objects = pool->pool_size / pool->object_size;

  KERNEL_CRITICAL_BEGIN();
  stats.total_objects = total_objects;
  stats.free_objects = pool->free_count;
  stats.used_objects = total_objects - pool->free_count;
  stats.peak_usage = peak_usage[pool_type];
  KERNEL_CRITICAL_END();

  return stats;
}

void pool_print_stats(void) {
  const char *pool_names[POOL_COUNT] = {
      "TCB",          "Stack Small",   "Stack Default", "Stack Large", "QCB",
      "Buffer Small", "Buffer Medium", "Buffer Large",  "SCB",         "MCB"};

  printf("\n=== Memory Pool Statistics ===\n");
  printf("Pool Name        | Total | Used | Free | Peak | Utilization\n");
  printf("-----------------|-------|------|------|------|------------\n");

  for (int i = 0; i < POOL_COUNT; i++) {
    pool_stats_t stats = pool_get_stats((pool_type_t)i);
    float utilization =
        stats.total_objects > 0
            ? (float)stats.used_objects / stats.total_objects * 100.0f
            : 0.0f;

    printf("%-16s | %5zu | %4zu | %4zu | %4zu | %6.1f%%\n", pool_names[i],
           stats.total_objects, stats.used_objects, stats.free_objects,
           stats.peak_usage, utilization);
  }
  printf("\n");
}
