#include "circular_buffer.h"
#include "critical.h"
#include "port.h"
#include "scheduler.h"
#include <stddef.h>
#include <stdio.h>

// Ready queues - one per priority level
list_head_t ready_queues[MAX_PRIORITY + 1];
task_handle_t current_task = NULL;

// Scheduler Lock State
static volatile uint32_t scheduler_lock_count = 0;
static volatile uint32_t pending_ticks = 0;
static list_head_t pending_ready_list;

#define TIMING_WHEEL_SIZE 256
static list_head_t wheel_buckets[TIMING_WHEEL_SIZE];
static circular_buffer_t timing_wheel;

volatile uint32_t tick_now = 0;

// DWT Profiling Buffers
#define PROFILING_BUF_SIZE 1024
volatile uint32_t systick_periods[PROFILING_BUF_SIZE];
volatile uint32_t ctxsw_durations[PROFILING_BUF_SIZE];
volatile uint32_t tick_durations[PROFILING_BUF_SIZE];
volatile uint32_t last_systick_cyc = 0;
volatile uint32_t ctxsw_idx = 0;
volatile uint32_t period_idx = 0;
volatile uint32_t tick_dur_idx = 0;

// Running min/max for existing metrics
volatile uint32_t systick_period_min = 0xFFFFFFFF;
volatile uint32_t systick_period_max = 0;
volatile uint32_t ctxsw_duration_min = 0xFFFFFFFF;
volatile uint32_t ctxsw_duration_max = 0;
volatile uint32_t tick_duration_min = 0xFFFFFFFF;
volatile uint32_t tick_duration_max = 0;

// Unlock-drain profiling
volatile uint32_t unlock_drain_durations[PROFILING_BUF_SIZE];
volatile uint32_t unlock_drain_idx = 0;
volatile uint32_t unlock_drain_min = 0xFFFFFFFF;
volatile uint32_t unlock_drain_max = 0;

// DWT Registers
#define DWT_CYCCNT (*((volatile uint32_t *)0xE0001004))
#define DWT_CONTROL (*((volatile uint32_t *)0xE0001000))
#define SCB_DEMCR (*((volatile uint32_t *)0xE000EDFC))
#define DWT_LAR (*((volatile uint32_t *)0xE0001FB0))
#define DWT_UNLOCK_KEY 0xC5ACCE55

static void _scheduler_process_tick(void) {
  KERNEL_CRITICAL_BEGIN();
  uint32_t now = ++tick_now;
  KERNEL_CRITICAL_END();

  uint32_t bucket_idx = now & timing_wheel.mask;
  list_head_t *buckets = (list_head_t *)timing_wheel.buffer;
  list_head_t *bucket = &buckets[bucket_idx];

  list_head_t *pos, *n;

  // Use mut iterator because we are removing nodes during iteration
  {
    KERNEL_CRITICAL_BEGIN();
    list_iter_mut(pos, n, bucket) {
      task_handle_t t = tcb_from_delay_link(pos);
      if (time_lte(t->wake_tick, now)) {
        list_remove(pos);

        scheduler_expire_timeout(t);
      }
    }
    KERNEL_CRITICAL_END();
  }
}

void scheduler_lock(void) {
  KERNEL_CRITICAL_BEGIN();
  scheduler_lock_count++;
  KERNEL_CRITICAL_END();
}

void scheduler_unlock(void) {
  // critical section required because decrementing and checking count
  // must be atomic
  KERNEL_CRITICAL_BEGIN();

  if (scheduler_lock_count == 1) {
    uint32_t drain_start = DWT_CYCCNT;
    uint32_t drained_anything = 0;

    while (pending_ticks > 0) {
      _scheduler_process_tick();
      pending_ticks--;
      drained_anything = 1;
    }

    while (!list_is_empty(&pending_ready_list)) {
      list_head_t *node = pending_ready_list.next;
      list_remove(node);
      task_handle_t t = tcb_from_ready_link(node);
      t->state = TASK_READY;
      list_insert_tail(&ready_queues[t->effective_priority], &t->ready_link);
      drained_anything = 1;
    }

    if (drained_anything) {
      uint32_t dur = DWT_CYCCNT - drain_start;
      unlock_drain_durations[unlock_drain_idx & (PROFILING_BUF_SIZE - 1)] = dur;
      unlock_drain_idx++;
      if (dur < unlock_drain_min) unlock_drain_min = dur;
      if (dur > unlock_drain_max) unlock_drain_max = dur;
    }
  }

  scheduler_lock_count--;

  KERNEL_CRITICAL_END();
}

// Core scheduler functions
void scheduler_init(void) {
  for (int i = 0; i <= MAX_PRIORITY; ++i) {
    list_init(&ready_queues[i]);
  }

  cb_init(&timing_wheel, wheel_buckets, TIMING_WHEEL_SIZE, sizeof(list_head_t));
  for (int i = 0; i < TIMING_WHEEL_SIZE; ++i) {
    list_init(&wheel_buckets[i]);
  }

  tick_now = 0;

  list_init(&pending_ready_list);
  current_task = NULL;
}

void scheduler_start(void) {
  // Set PendSV to lowest priority
  set_pendsv_priority();

  // Enable DWT Cycle Counter
  SCB_DEMCR |= (1 << 24); // TRACEENA
  DWT_LAR = DWT_UNLOCK_KEY;
  DWT_CONTROL |= 1; // CYCCNTENA
  DWT_CYCCNT = 0;
  last_systick_cyc = 0;

  // Initialize SysTick 1ms (1000Hz)
  systick_init(1000);

  current_task = scheduler_get_next_task();

  if (!current_task) {
    // No tasks to run - critical error

    while (1)
      ;
  }

  current_task->state = TASK_RUNNING;

  start_first_task(current_task->stack_pointer);

  // This function should never return
  while (1)
    ;
}

task_handle_t scheduler_get_next_task(void) {
  // Find highest priority (lowest number) that has tasks
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (!list_is_empty(&ready_queues[priority])) {
      // Get first task from this priority
      list_head_t *first = ready_queues[priority].next;
      task_handle_t task = tcb_from_ready_link(first);

      // Remove from head and add to tail (round-robin)
      list_move_to_tail(&ready_queues[priority], first);

      return task;
    }
  }
  // No ready tasks found
  while (1) {
  }
}

void scheduler_add_task(task_handle_t task) {
  if (!task) return;

  KERNEL_CRITICAL_BEGIN();
  if (scheduler_lock_count > 0) {
    list_insert_tail(&pending_ready_list, &task->ready_link);
  } else {
    task->state = TASK_READY;
    list_insert_tail(&ready_queues[task->effective_priority], &task->ready_link);
  }
  KERNEL_CRITICAL_END();
}

void scheduler_remove_task(task_handle_t task) {
  if (!task) return;

  KERNEL_CRITICAL_BEGIN();
  if (!list_is_empty(&task->ready_link)) {
    list_remove(&task->ready_link);
  }

  if (!list_is_empty(&task->delay_link)) {
    list_remove(&task->delay_link);
  }
  KERNEL_CRITICAL_END();

  // Note: Don't remove from wait_link
  // Handled by specific sync object (semaphore, queue, etc.)
}

// Task state transitions
void scheduler_block_current_task(void) {
  if (current_task) {
    current_task->state = TASK_BLOCKED;
    scheduler_remove_task(current_task);
  }
}

void scheduler_unblock_task(task_handle_t task) {
  if (!task) return;

  scheduler_add_task(task);
}

void scheduler_yield(void) { trigger_context_switch(); }

task_handle_t scheduler_switch_context(void) {
  uint32_t start_cyc = DWT_CYCCNT;

  // selection function picks a task AND rotates the queue
  current_task = scheduler_get_next_task();

  uint32_t end_cyc = DWT_CYCCNT;
  uint32_t dur = end_cyc - start_cyc;
  ctxsw_durations[ctxsw_idx & (PROFILING_BUF_SIZE - 1)] = dur;
  ctxsw_idx++;
  if (dur < ctxsw_duration_min) ctxsw_duration_min = dur;
  if (dur > ctxsw_duration_max) ctxsw_duration_max = dur;

  return current_task;
}

// Helper function for task_delay() implementation
void scheduler_delay_current_task(uint32_t ticks) {
  if (!current_task || ticks == 0) return;

  scheduler_lock();

  // Remove from ready queue
  KERNEL_CRITICAL_BEGIN();
  if (!list_is_empty(&current_task->ready_link)) {
    list_remove(&current_task->ready_link);
  }
  current_task->state = TASK_BLOCKED;

  KERNEL_CRITICAL_END();

  uint32_t now = tick_now;     // read once
  uint32_t wake = now + ticks; // automatically wraps
  current_task->wake_tick = wake;

  list_head_t *buckets = (list_head_t *)timing_wheel.buffer;

  {
    KERNEL_CRITICAL_BEGIN();
    list_insert_tail(&buckets[wake & timing_wheel.mask], &current_task->delay_link);
    KERNEL_CRITICAL_END();
  }

  scheduler_unlock();
  scheduler_yield();
}

// Timer tick handler - processes delayed tasks
void scheduler_tick(void) {
  uint32_t tick_start = DWT_CYCCNT;
  uint32_t now_cyc = DWT_CYCCNT;
  if (last_systick_cyc != 0) {
    uint32_t period = now_cyc - last_systick_cyc;
    systick_periods[period_idx & (PROFILING_BUF_SIZE - 1)] = period;
    period_idx++;
    if (period < systick_period_min) systick_period_min = period;
    if (period > systick_period_max) systick_period_max = period;
  }
  last_systick_cyc = now_cyc;

  if (scheduler_lock_count > 0) {
    pending_ticks++;
  } else {
    _scheduler_process_tick();
    trigger_context_switch();
  }

  uint32_t tick_dur = DWT_CYCCNT - tick_start;
  tick_durations[tick_dur_idx & (PROFILING_BUF_SIZE - 1)] = tick_dur;
  tick_dur_idx++;
  if (tick_dur < tick_duration_min) tick_duration_min = tick_dur;
  if (tick_dur > tick_duration_max) tick_duration_max = tick_dur;
}

// Caller must hold scheduler_lock or be in a critical section
void scheduler_set_timeout(task_handle_t t, uint32_t wake_tick) {
  t->wake_tick = wake_tick;
  list_head_t *buckets = (list_head_t *)timing_wheel.buffer;

  KERNEL_CRITICAL_BEGIN();
  list_insert_tail(&buckets[wake_tick & timing_wheel.mask], &t->delay_link);
  KERNEL_CRITICAL_END();
}

void scheduler_expire_timeout(task_handle_t t) {
  if (t->waiting_on) {
    if (t->wait_link.next != &t->wait_link) {
      KERNEL_CRITICAL_BEGIN();
      list_remove(&t->wait_link);
      KERNEL_CRITICAL_END();
    }
    t->wake_reason = WAKE_REASON_TIMEOUT;
    t->waiting_on = NULL;
  }
  scheduler_add_task(t);
}

void scheduler_cancel_timeout(task_handle_t t) {
  KERNEL_CRITICAL_BEGIN();
  if (!list_is_empty(&t->delay_link)) {
    list_remove(&t->delay_link);
  }
  KERNEL_CRITICAL_END();
}

// Priority management
task_priority_t scheduler_get_highest_priority(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (!list_is_empty(&ready_queues[priority])) {
      return priority;
    }
  }
  return MAX_PRIORITY;
}

bool scheduler_has_ready_tasks(void) {
  for (task_priority_t priority = 0; priority <= MAX_PRIORITY; priority++) {
    if (!list_is_empty(&ready_queues[priority])) {
      return true;
    }
  }
  return false;
}

void scheduler_boost_priority(task_handle_t task, task_priority_t new_priority) {
  if (!task || new_priority >= task->effective_priority) return;

  KERNEL_CRITICAL_BEGIN();

  // Remove from current priority queue
  if (task->state == TASK_READY && !list_is_empty(&task->ready_link)) {
    list_remove(&task->ready_link);
  }

  task->effective_priority = new_priority;

  if (task->state == TASK_READY) {
    list_insert_tail(&ready_queues[new_priority], &task->ready_link);
  }

  KERNEL_CRITICAL_END();
}

void scheduler_restore_priority(task_handle_t task) {
  if (!task || task->effective_priority == task->base_priority) return;

  KERNEL_CRITICAL_BEGIN();
  if (task->state == TASK_READY && !list_is_empty(&task->ready_link)) {
    list_remove(&task->ready_link);
  }

  task->effective_priority = task->base_priority;

  if (task->state == TASK_READY) {
    list_insert_tail(&ready_queues[task->base_priority], &task->ready_link);
  }

  KERNEL_CRITICAL_END();
}

// Dummy function to keep header compatibility
void morph_rt_dump_profiling(void) {}
