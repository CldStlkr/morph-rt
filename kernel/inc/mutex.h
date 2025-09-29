#ifndef MUTEX_H
#define MUTEX_H

#include "kernel.h"
#include "list.h"
#include "task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mutex_control_block *mutex_handle_t;

typedef enum {
  MUTEX_OK = 0,
  MUTEX_ERROR_NULL = -1,
  MUTEX_ERROR_TIMEOUT = -2,
  MUTEX_ERROR_NOT_OWNER = -3,
  MUTEX_ERROR_RECURSIVE = -4,
  MUTEX_ERROR_ALREADY_LOCKED = -5,
} mutex_result_t;

typedef struct mutex_control_block {
  task_handle_t owner;
  list_head_t waiting_tasks;
  task_priority_t original_priority;
  char name[16];
} mutex_control_block;

#define tcb_from_mutex_wait_link(ptr) \
  container_of(ptr, task_control_block, wait_link)



// ===================== PUBLIC API ========================

mutex_handle_t mutex_create(const char *name);
void mutex_delete(mutex_handle_t mutex);
mutex_result_t mutex_lock(mutex_handle_t mutex, uint32_t timeout);
mutex_result_t mutex_unlock(mutex_handle_t mutex);

mutex_result_t mutex_try_lock(mutex_handle_t mutex);

// Utility
task_handle_t mutex_get_owner(mutex_handle_t mutex);
bool mutex_has_waiting_tasks(mutex_handle_t mutex);
bool mutex_is_locked(mutex_handle_t mutex);

#define MUTEX_NO_WAIT 0
#define MUTEX_WAIT_FOREVER 0xFFFFFFFF

#endif

