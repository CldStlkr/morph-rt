#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "kernel.h"
#include "list.h"
#include "task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct semaphore_control_block *semaphore_handle_t;

typedef enum {
  SEM_OK = 0,
  SEM_ERROR_NULL = -1,
  SEM_ERROR_OVERFLOW = -2,
  SEM_ERROR_UNDERFLOW = -3,
  SEM_ERROR_TIMEOUT = -4,
  SEM_ERROR_NOT_OWNER = -5,
} sem_result_t;

typedef struct semaphore_control_block {
  uint32_t count;
  uint32_t max_count;
  list_head_t waiting_tasks;
  char name[16];

} semaphore_control_block;

// Macro for container_of pattern
#define tcb_from_sem_wait_link(ptr)                                            \
  container_of(ptr, task_control_block, wait_link)

// =========================== PUBLIC API ============================
semaphore_handle_t sem_create(uint32_t initial_count, uint32_t max_count,
                              const char *name);
void sem_delete(semaphore_handle_t sem);

sem_result_t sem_wait(semaphore_handle_t sem, uint32_t timeout);
sem_result_t sem_post(semaphore_handle_t sem);

// Non-blocking version
sem_result_t sem_try_wait(semaphore_handle_t sem);

// Utility
uint32_t sem_get_count(semaphore_handle_t sem);
bool sem_has_waiting_tasks(semaphore_handle_t sem);

#define SEM_NO_WAIT 0
#define SEM_WAIT_FOREVER 0xFFFFFFFF

#define sem_create_binary(name) sem_create(1, 1, name)
#define sem_create_counting(max, name) sem_create(0, max, name)

#endif /* SEMAPHORE_H */
