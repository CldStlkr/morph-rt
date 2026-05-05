#ifndef I2C_H
#define I2C_H

#include "mutex.h"
#include "semaphore.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Interrupt-driven I2C1 driver for STM32F407.
 *
 * Bus access is serialised with a Morph-RT Mutex so multiple tasks can call
 * i2c_write / i2c_read without corrupting each other's transfers.  Each
 * transaction blocks the calling task (via sem_wait) rather than spinning,
 * yielding the CPU to other tasks while the hardware works.
 *
 * Hardware: I2C1, PB6 = SCL (AF4), PB9 = SDA (AF4), 100 kHz standard mode.
 */

typedef enum {
  I2C_OK = 0,
  I2C_ERROR_TIMEOUT = -1,
  I2C_ERROR_BUS = -2,
  I2C_ERROR_NACK = -3,
} i2c_result_t;

void i2c1_init(void);
i2c_result_t i2c_write_reg(uint8_t dev_addr_write, uint8_t reg, uint8_t data);
i2c_result_t i2c_read_reg(uint8_t dev_addr_write, uint8_t dev_addr_read, uint8_t reg, uint8_t *data_out);

/* Called from I2C1_EV_IRQHandler and I2C1_ER_IRQHandler (defined in i2c.c) */
void I2C1_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);

#endif /* I2C_H */
