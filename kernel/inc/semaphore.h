#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "kernel.h"
#include "task.h"

typedef struct semaphore_control_block *semaphore_handle_t;

typedef enum {
  SEM_OK = 0,
  SEM_ERROR_NULL = -1,
  SEM_ERROR_OVERFLOW = -2,
  SEM_ERROR_UNDERFLOW = -3,
  SEM_ERROR_TIMEOUT = -4,
  SEM_ERROR_NOT_OWNER = -5,
} sem_result_t;


#endif /* ifndef SEMAPHORE_H */

