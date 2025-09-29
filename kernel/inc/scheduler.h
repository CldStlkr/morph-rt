#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "time_utils.h"

// Ready queues - one per priority level
extern list_head_t ready_queues[MAX_PRIORITY + 1];
// Delayed task lists
extern list_head_t delayed_cur; // wake_tick in current epoch
extern list_head_t delayed_ovf; // wake_tick after wrap

extern volatile uint32_t tick_now;

extern task_handle_t current_task;
extern task_handle_t next_task;

// Core scheduler functions
void scheduler_init(void);
void scheduler_start(void);
task_handle_t scheduler_get_next_task(void);
void scheduler_add_task(task_handle_t task);
void scheduler_remove_task(task_handle_t task);

// Task state transitions
void scheduler_block_current_task(void);
void scheduler_unblock_task(task_handle_t task);
void scheduler_yield(void);
void scheduler_delay_current_task(uint32_t ticks);

// Timer tick processing
void scheduler_tick(void); // Called from timer interrupt

// Arms timeout for a given task at absolute wake_tick
void scheduler_set_timeout(task_handle_t t, uint32_t wake_tick);
void scheduler_expire_timeout(task_handle_t t);
void scheduler_cancel_timeout(task_handle_t t);

// Priority management
task_priority_t scheduler_get_highest_priority(void);
bool scheduler_has_ready_tasks(void);

void scheduler_boost_priority(task_handle_t task, task_priority_t new_priority);
void scheduler_restore_priority(task_handle_t task);

#endif // !SCHEDULER_H
