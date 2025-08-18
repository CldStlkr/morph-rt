#ifndef TIME_UTILS_H
#define TIME_UTILS_H
#include <stdbool.h>
#include <stdint.h>

// Wrap-safe time comparisons
static inline bool time_lte(uint32_t a, uint32_t b) { // a <= b ?
  return (int32_t)(a - b) <= 0;
}
static inline bool time_lt(uint32_t a, uint32_t b) { // a < b ?
  return (int32_t)(a - b) < 0;
}

static inline bool time_gte(uint32_t a, uint32_t b) { // a > b ?
  return !time_lt(a, b);
}

static inline bool time_gt(uint32_t a, uint32_t b) { // a > b ?
  return !time_lte(a, b);
}

static inline uint32_t ticks_until(uint32_t deadline, uint32_t now) {
  int32_t dt = (int32_t)(deadline - now);
  return dt <= 0 ? 0u : (uint32_t)dt;
}

#endif // !TIME_UTILS_H
