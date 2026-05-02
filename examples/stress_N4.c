#include "SEGGER_RTT.h"
#include "kernel.h"
#include "latency.h"
#include "scheduler.h"
#include "task.h"

#define PROFILE_WAIT_MS 30000
#define N_SLEEPERS 4
#define IDENTITY_LED (1 << 15) // Blue LED for N=4

// Peripheral definitions
#define RCC_AHB1ENR (*((volatile uint32_t *)0x40023830))
#define GPIOD_MODER (*((volatile uint32_t *)0x40020C00))
#define GPIOD_ODR   (*((volatile uint32_t *)0x40020C14))

const uint32_t primes[] = {7, 11, 13, 17};

void hardware_init(void) {
    RCC_AHB1ENR |= (1 << 3); // GPIOD
    GPIOD_MODER &= ~((3 << 24) | (3 << 26) | (3 << 28) | (3 << 30));
    GPIOD_MODER |= ((1 << 24) | (1 << 26) | (1 << 28) | (1 << 30));
}

void identity_task(void *param) {
    (void)param;
    GPIOD_ODR |= IDENTITY_LED;
    while (1) { task_delay(10000); }
}

void sleeper_task(void *param) {
    uint32_t delay_ms = (uint32_t)param;
    while (1) { task_delay(delay_ms); }
}

void profiler_task(void *param) {
    (void)param;
    task_delay(PROFILE_WAIT_MS);
    SEGGER_RTT_WriteString(0, "Stress Test N=4 Ready: Dumping Buffers\n");
    while (1) { task_delay(1000); }
}

int main(void) {
    hardware_init();
    kernel_init();
    latency_init();
    task_create(identity_task, "Ident", 512, NULL, 1);
    task_create(profiler_task, "Prof", 512, NULL, 0);
    for (int i = 0; i < N_SLEEPERS; i++) {
        task_create(sleeper_task, "Sleep", 256, (void*)primes[i], 4);
    }
    kernel_start();
    return 0;
}
