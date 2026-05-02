#include "SEGGER_RTT.h"
#include "kernel.h"
#include "queue.h"
#include "scheduler.h"
#include "task.h"

// ---------------------------------------------------------
// Minimal Hardware Definitions
// ---------------------------------------------------------
#define RCC_AHB1ENR (*((volatile uint32_t *)0x40023830))
#define GPIOA_MODER (*((volatile uint32_t *)0x40020000))
#define GPIOA_PUPDR (*((volatile uint32_t *)0x4002000C))
#define GPIOA_IDR (*((volatile uint32_t *)0x40020010))

#define GPIOD_MODER (*((volatile uint32_t *)0x40020C00))
#define GPIOD_ODR (*((volatile uint32_t *)0x40020C14))

#define LED_GREEN (1 << 12)
#define LED_ORANGE (1 << 13) // Yellow
#define LED_RED (1 << 14)
#define LED_BLUE (1 << 15)

#define DBGMCU_CR (*((volatile uint32_t *)0xE0042004))

void hardware_init(void) {
  // 1. Enable Clocks for GPIOA (Button) and GPIOD (LEDs)
  RCC_AHB1ENR |= (1 << 0) | (1 << 3);

  // 2. Keep debug clocks running during sleep/stop/standby
  // This is critical for SystemView to work when the IDLE task calls WFI
  DBGMCU_CR |= (1 << 0) | (1 << 1) | (1 << 2);

  // 3. Configure User Button (PA0)
  GPIOA_MODER &= ~(3 << 0); // Input mode (00)
  GPIOA_PUPDR &= ~(3 << 0); // Clear pull config
  GPIOA_PUPDR |= (2 << 0);  // Pull-down resistor (10)

  // 3. Configure LEDs (PD12-PD15) as Outputs (01)
  GPIOD_MODER &= ~((3 << 24) | (3 << 26) | (3 << 28) | (3 << 30));
  GPIOD_MODER |= ((1 << 24) | (1 << 26) | (1 << 28) | (1 << 30));
}

// ---------------------------------------------------------
// FFI Implementations for Rust
// ---------------------------------------------------------

void hardware_set_traffic_light(uint32_t active_led) {
  // Turn off all traffic LEDs
  GPIOD_ODR &= ~(LED_GREEN | LED_ORANGE | LED_RED);
  // Turn on target LED
  GPIOD_ODR |= active_led;
}

void hardware_set_blue_led(uint32_t state) {
  if (state) {
    GPIOD_ODR |= LED_BLUE;
  } else {
    GPIOD_ODR &= ~LED_BLUE;
  }
}

uint32_t hardware_read_button(void) { return (GPIOA_IDR & 1); }

// ---------------------------------------------------------
// Rust FFI Declarations
// ---------------------------------------------------------
extern void rust_task_button_poll(void *param);
extern void rust_task_traffic_light(void *param);
extern void rust_task_car_sim(void *param);

int main(void) {
  hardware_init();
  kernel_init();

  SEGGER_RTT_WriteString(0, "Boot: kernel_init complete\n");
  SEGGER_RTT_WriteString(0, "Boot: about to start tasks\n");

  // Create our message queue holding up to 4 events
  queue_handle_t event_queue = queue_create(4, 4);

  // Register all independent tasks from Rust!
  if (!task_create(rust_task_button_poll, "Btn", 512, event_queue, 2)) {
    SEGGER_RTT_WriteString(0, "Error: Failed to create Btn task\n");
  }
  if (!task_create(rust_task_traffic_light, "Traffic", 512, event_queue, 3)) {
    SEGGER_RTT_WriteString(0, "Error: Failed to create Traffic task\n");
  }
  if (!task_create(rust_task_car_sim, "Cars", 512, NULL, 4)) {
    SEGGER_RTT_WriteString(0, "Error: Failed to create Cars task\n");
  }

  kernel_start();
  while (1)
    ;
  return 0;
}
