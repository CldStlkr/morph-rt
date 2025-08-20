#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdint.h>

#ifdef __ARM_ARCH
static inline uint32_t kernel_critical_enter(void) {
  uint32_t primask;
  __asm volatile("mrs %0, primask" : "=r"(primask));
  __asm volatile("cpsid i" ::: "memory");
  return primask;
}

static inline void kernel_critical_exit(uint32_t primask) {
  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
}
#else
// Generic fallback for testing
static inline uint32_t kernel_critical_enter(void) { return 0; }
static inline void kernel_critical_exit(uint32_t primask) { (void)primask; }
#endif

// Simplified macros - single line each to avoid parsing issues
#define KERNEL_CRITICAL_BEGIN()                                                \
  uint32_t _critical_state = kernel_critical_enter()
#define KERNEL_CRITICAL_END() kernel_critical_exit(_critical_state)

#endif /* ifndef CRITICAL_H */
