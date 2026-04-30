#include "SEGGER_SYSVIEW.h"

#include "kernel.h"
#include "task.h"
#include <stddef.h>

// Minimal STM32F407 Hardware Definitions
#define RCC_AHB1ENR (*((volatile uint32_t *)0x40023830))
#define GPIOD_MODER (*((volatile uint32_t *)0x40020C00))
#define GPIOD_ODR (*((volatile uint32_t *)0x40020C14))

// Bitmasks
#define GPIOD_EN (1 << 3)
#define LED_GREEN (1 << 12)
#define LED_ORANGE (1 << 13)
#define LED_RED (1 << 14)
#define LED_BLUE (1 << 15)

// MCU Hardware Initialization
void hardware_init(void) {

  RCC_AHB1ENR |= GPIOD_EN;

  // Clear the bits first
  GPIOD_MODER &= ~((3 << 24) | (3 << 26) | (3 << 28) | (3 << 30));

  // Set '01' for each pin
  GPIOD_MODER |= ((1 << 24) | (1 << 26) | (1 << 28) | (1 << 30));
}

// ---------------------------------------------------------
// RTOS Tasks
// ---------------------------------------------------------

void task_red_led(void *param) {
  (void)param;
  while (1) {
    GPIOD_ODR ^= LED_RED;
    task_delay(750); // (ms)
  }
}

void task_green_led(void *param) {
  (void)param;
  while (1) {
    GPIOD_ODR ^= LED_GREEN;
    task_delay(250);
  }
}

void task_blue_led(void *param) {
  (void)param;
  while (1) {
    task_delay(1000); // Initial offset before starting
    GPIOD_ODR ^= LED_BLUE;
    task_delay(500);
  }
}

int main(void) {
  hardware_init();
  kernel_init();

  // Priority 3
  task_create(task_blue_led, "Blue", 512, NULL, 3);
  task_create(task_red_led, "Red", 512, NULL, 3);
  task_create(task_green_led, "Green", 512, NULL, 3);

  kernel_start();

  // Scheduler should never return but if it does spin here.
  while (1) {
  }

  return 0;
}
