// port.h - Hardware abstraction layer

#ifndef PORT_H
#define PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Architecture-specific implementations
#ifdef __ARM_ARCH

// ARM Cortex-M specific functions
extern void start_first_task(uint32_t *first_task_sp);
extern void trigger_context_switch(void);
extern void systick_init(uint32_t ticks_per_second);
extern void set_pendsv_priority(void);

// Interrupt handlers (these need to be in your vector table)
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

static inline void port_wait_for_interrupt(void) {
  __asm volatile("wfi" ::: "memory");
}

// ARM-specific inline functions
static inline void port_disable_interrupts(void) {
  __asm volatile("cpsid i" ::: "memory");
}

static inline void port_enable_interrupts(void) {
  __asm volatile("cpsie i" ::: "memory");
}

#define __WFI() port_wait_for_interrupt()
#define __disable_irq() port_disable_interrupts()
#define __enable_irq() port_enable_interrupts()

#else
// Stub implementations for non-ARM platforms
static inline void start_first_task(uint32_t *first_task_sp) {
  (void)first_task_sp;
}
static inline void trigger_context_switch(void) {}
static inline void systick_init(uint32_t ticks_per_second) {
  (void)ticks_per_second;
}
static inline void set_pendsv_priority(void) {}
static inline void port_disable_interrupts(void) {}
static inline void port_enable_interrupts(void) {}

// Add wait for interrupt stub
static inline void port_wait_for_interrupt(void) {
  // No-op for non-ARM platforms
}

#define __WFI() port_wait_for_interrupt()
#define __disable_irq() port_disable_interrupts()
#define __enable_irq() port_enable_interrupts()

// Simulation handlers
void PendSV_Handler(void);
void SysTick_Handler(void);
#endif // !__ARM_ARCH

#ifdef __cplusplus
}
#endif

#endif // !PORT_H
