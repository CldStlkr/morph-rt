#include "critical.h"
#include "kernel.h"
#include "port.h"
#include "semaphore.h"
#include "task.h"

// DWT cycle counter (already enabled by scheduler_start)
#define DWT_CYCCNT  (*((volatile uint32_t *)0xE0001004))

// NVIC registers
#define NVIC_ISER0  (*((volatile uint32_t *)0xE000E100))
#define NVIC_ISPR0  (*((volatile uint32_t *)0xE000E200))  // Set-Pending Register
#define NVIC_IP6    (*((volatile uint8_t  *)0xE000E406))

// Latency measurement buffers
#define LATENCY_BUF_SIZE 1024
volatile uint32_t irq_to_task_latencies[LATENCY_BUF_SIZE];
volatile uint32_t irq_to_task_idx      = 0;
volatile uint32_t irq_to_task_min      = 0xFFFFFFFF;
volatile uint32_t irq_to_task_max      = 0;
volatile uint32_t irq_fired_cyc        = 0;
volatile uint32_t latency_sample_count = 0;

static semaphore_handle_t latency_sem;

// IRQ6 handler — NVIC clears the pending bit automatically on exception entry
void EXTI0_IRQHandler(void) {
    irq_fired_cyc = DWT_CYCCNT;
    sem_post(latency_sem);      // wake latency_task (ISR-safe: uses KERNEL_CRITICAL only)
    trigger_context_switch();   // pend PendSV so latency_task runs on ISR exit
}

static void latency_task(void *arg) {
    (void)arg;
    while (1) {
        sem_wait(latency_sem, SEM_WAIT_FOREVER);
        uint32_t woke = DWT_CYCCNT;
        uint32_t lat  = woke - irq_fired_cyc;
        irq_to_task_latencies[irq_to_task_idx & (LATENCY_BUF_SIZE - 1)] = lat;
        irq_to_task_idx++;
        if (lat < irq_to_task_min) irq_to_task_min = lat;
        if (lat > irq_to_task_max) irq_to_task_max = lat;
        latency_sample_count++;
    }
}

static void latency_trigger_task(void *arg) {
    (void)arg;
    while (1) {
        task_delay(100);           // 100ms between triggers → ~150 samples in 15s
        NVIC_ISPR0 = (1u << 6);   // directly pend IRQ6 — no EXTI peripheral needed
    }
}

void latency_init(void) {
    // Set IRQ6 priority to 1 — higher urgency than SysTick/PendSV (both 0xFF)
    // STM32F4 uses top 4 bits: priority 1 → write (1 << 4) = 0x10
    NVIC_IP6 = 0x10;

    // Enable IRQ6 in NVIC
    NVIC_ISER0 |= (1u << 6);

    // Binary semaphore starts at 0 so latency_task blocks immediately
    latency_sem = sem_create(0, 1, "lat");

    task_create(latency_task,         "LatMeas", 512, NULL, 0);
    task_create(latency_trigger_task, "LatTrig", 256, NULL, 6);
}
