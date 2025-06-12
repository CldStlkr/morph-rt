#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

// Ready queues - one per priority level
extern task_handle_t ready_queues[MAX_PRIORITY + 1];
extern task_handle_t current_task;
extern task_handle_t next_task;

// Core scheduler functions
void scheduler_init(void);
void scheduler_start(void);
task_handle_t scheduler_get_next_task(void);
void scheduler_add_task(task_handle_t task);
void scheduler_remove_task(task_handle_t task);

// Task state transitions
void scheduelr_block_current_task(void);
void scheduler_unblock_task(task_handle_t task);
void scheduler_yield(void);

// Timer tick processing
void scheduler_tick(void); // Called from timer interrupt

// Prority management
task_priority_t scheduler_get_highest_priority(void);
bool scheduler_has_ready_tasks(void);

#endif // !SCHEDULER_H
