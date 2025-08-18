#ifndef KERNEL_H
#define KERNEL_H

#include <stdbool.h>
#include <stdint.h>

// RTOS Configuration
#define MAX_TASKS 8
#define DEFAULT_STACK_SIZE 1024
#define MAX_PRIORITY 7

typedef enum task_state_t {
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SUSPENDED,
  TASK_DELETED,
} task_state_t;

// Task priority (0 = highest, 7 = lowest)
typedef uint8_t task_priority_t;

// Task function pointer
typedef void (*task_function_t)(void *param);

// Task handle (opaque pointer)
typedef struct task_control_block *task_handle_t;

// Main kernel API
void kernel_init(void);
void kernel_start(void);

// Task management
task_handle_t task_create(task_function_t function, const char *name,
                          uint16_t stack_size, void *param,
                          task_priority_t priority);

void task_delete(task_handle_t task);
void task_delay(uint32_t ticks);
void task_yield(void);
task_handle_t task_get_current(void);

#endif // !KERNEL_H
