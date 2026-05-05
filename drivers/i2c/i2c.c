#include "i2c.h"
#include "stm32f4xx.h"
#include <stddef.h>

/*
 * STM32F407 I2C1 interrupt-driven driver.
 *
 * Design overview
 * ───────────────
 * Each public transaction (i2c_write_reg / i2c_read_reg) follows three steps:
 *   1. Lock the bus mutex  – prevents two tasks from interleaving frames.
 *   2. Load the transfer descriptor and generate a START condition, then
 *      enable the I2C event + error interrupts.
 *   3. Call sem_wait(done_sem) – the calling task blocks and the CPU is
 *      given to another task.  The ISR advances a small state machine one
 *      step per interrupt, posting done_sem when the STOP condition is done
 *      (or on error).
 *
 * State machine (write transaction)
 * ──────────────────────────────────
 *   START → ADDR_W → REG → DATA → STOP → DONE
 *
 * State machine (read transaction)
 * ─────────────────────────────────
 *   START → ADDR_W → REG → RESTART → ADDR_R → RX → DONE
 *
 * NVIC priority for I2C1_EV and I2C1_ER is set to 0xA0 (above PendSV/SysTick
 * at 0xFF so the ISR can safely call sem_post from interrupt context).
 */

/* ── internal state machine ─────────────────────────────────────────────── */

typedef enum {
  ST_IDLE,
  ST_START,
  ST_ADDR_W,
  ST_REG,
  ST_DATA,
  ST_RESTART,
  ST_ADDR_R,
  ST_RX,
  ST_DONE,
  ST_ERROR,
} xfer_state_t;

typedef struct {
  xfer_state_t state;
  uint8_t dev_addr_write;
  uint8_t dev_addr_read;
  uint8_t reg;
  uint8_t tx_data;
  uint8_t *rx_out;
  bool is_read;
  i2c_result_t result;
} xfer_t;

static volatile xfer_t g_xfer;
static semaphore_handle_t g_done_sem;
static mutex_handle_t g_bus_mutex;

/* ── helpers ────────────────────────────────────────────────────────────── */

static void irq_enable(void) {
  /* Event interrupt (SB, ADDR, TXE, BTF, RXNE) + error interrupt */
  I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN;
}

static void irq_disable(void) { I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN); }

static void finish(i2c_result_t result) {
  irq_disable();
  I2C1->CR1 |= I2C_CR1_STOP;
  g_xfer.state = ST_DONE;
  g_xfer.result = result;
  sem_post(g_done_sem);
}

/* ── public API ─────────────────────────────────────────────────────────── */

void i2c1_init(void) {
  /* ── clocks ── */
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  (void)RCC->APB1ENR; /* read-back: 2-cycle settle before peripheral access */

  /* ── GPIO: PB6 = SCL, PB9 = SDA  (AF4, open-drain, pull-up) ── */
  GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER9);
  GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER9_1);
  GPIOB->OTYPER |= (GPIO_OTYPER_OT6 | GPIO_OTYPER_OT9);
  GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR6_1 | GPIO_OSPEEDER_OSPEEDR9_1);
  GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR9);
  GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR9_0);
  GPIOB->AFR[0] |= (4u << (6 * 4));
  GPIOB->AFR[1] |= (4u << ((9 - 8) * 4));

  /* ── I2C peripheral: 100 kHz, timing derived from actual APB1 clock ── */
  /* APB1 prescaler in RCC_CFGR bits [12:10]: 0xx=÷1, 100=÷2 .. 111=÷16 */
  uint32_t ppre1    = (RCC->CFGR >> 10) & 0x7u;
  uint32_t apb1_div = (ppre1 & 0x4u) ? (1u << (ppre1 - 3u)) : 1u;
  uint32_t apb1_hz  = SystemCoreClock / apb1_div;
  uint32_t apb1_mhz = apb1_hz / 1000000u;

  I2C1->CR1 |= I2C_CR1_SWRST;
  I2C1->CR1 &= ~I2C_CR1_SWRST;
  I2C1->CR2   = apb1_mhz;                 /* PCLK1 in MHz */
  I2C1->CCR   = apb1_hz / (2u * 100000u); /* standard mode 100 kHz */
  I2C1->TRISE = apb1_mhz + 1u;            /* max rise time for SM */
  I2C1->CR1 |= I2C_CR1_PE | I2C_CR1_ACK;

  /* ── RTOS synchronisation objects ── */
  g_done_sem = sem_create(0, 1, "i2c_done");
  g_bus_mutex = mutex_create("i2c_bus");

  /* ── NVIC: priority above PendSV (0xFF) but below everything critical ── */
  NVIC_SetPriority(I2C1_EV_IRQn, 0xA0);
  NVIC_SetPriority(I2C1_ER_IRQn, 0xA0);
  NVIC_EnableIRQ(I2C1_EV_IRQn);
  NVIC_EnableIRQ(I2C1_ER_IRQn);

  g_xfer.state = ST_IDLE;
}

i2c_result_t i2c_write_reg(uint8_t dev_addr_write, uint8_t reg, uint8_t data) {
  mutex_lock(g_bus_mutex, MUTEX_WAIT_FOREVER);

  g_xfer.dev_addr_write = dev_addr_write;
  g_xfer.reg = reg;
  g_xfer.tx_data = data;
  g_xfer.is_read = false;
  g_xfer.state = ST_START;
  g_xfer.result = I2C_OK;

  irq_enable();
  I2C1->CR1 |= I2C_CR1_START;

  /* Block until ISR posts done_sem */
  sem_wait(g_done_sem, SEM_WAIT_FOREVER);

  i2c_result_t res = g_xfer.result;
  mutex_unlock(g_bus_mutex);
  return res;
}

i2c_result_t i2c_read_reg(uint8_t dev_addr_write, uint8_t dev_addr_read, uint8_t reg, uint8_t *data_out) {
  mutex_lock(g_bus_mutex, MUTEX_WAIT_FOREVER);

  g_xfer.dev_addr_write = dev_addr_write;
  g_xfer.dev_addr_read = dev_addr_read;
  g_xfer.reg = reg;
  g_xfer.rx_out = data_out;
  g_xfer.is_read = true;
  g_xfer.state = ST_START;
  g_xfer.result = I2C_OK;

  irq_enable();
  I2C1->CR1 |= I2C_CR1_START;

  sem_wait(g_done_sem, SEM_WAIT_FOREVER);

  i2c_result_t res = g_xfer.result;
  mutex_unlock(g_bus_mutex);
  return res;
}

/* ── Event ISR ───────────────────────────────────────────────────────────── */

void I2C1_EV_IRQHandler(void) {
  uint32_t sr1 = I2C1->SR1;

  switch (g_xfer.state) {

  /* START bit generated — send the device address (write mode) */
  case ST_START:
    if (sr1 & I2C_SR1_SB) {
      g_xfer.state = ST_ADDR_W;
      I2C1->DR = g_xfer.dev_addr_write;
    }
    break;

  /* Address ACK'd — send the register address */
  case ST_ADDR_W:
    if (sr1 & I2C_SR1_ADDR) {
      (void)I2C1->SR1;
      (void)I2C1->SR2; /* clear ADDR flag */
      g_xfer.state = ST_REG;
      I2C1->DR = g_xfer.reg;
    }
    break;

  /* Register byte shifted out — branch for write vs read */
  case ST_REG:
    if (sr1 & I2C_SR1_TXE) {
      if (g_xfer.is_read) {
        /* Generate repeated START for read */
        g_xfer.state = ST_RESTART;
        I2C1->CR1 |= I2C_CR1_START;
      } else {
        g_xfer.state = ST_DATA;
        I2C1->DR = g_xfer.tx_data;
      }
    }
    break;

  /* Data byte shifted out — generate STOP */
  case ST_DATA:
    if (sr1 & I2C_SR1_BTF) {
      finish(I2C_OK);
    }
    break;

  /* Repeated START generated — send address in read mode */
  case ST_RESTART:
    if (sr1 & I2C_SR1_SB) {
      g_xfer.state = ST_ADDR_R;
      I2C1->DR = g_xfer.dev_addr_read;
    }
    break;

  /* Read-address ACK'd — disable ACK before clearing ADDR so the
   * hardware sends NACK+STOP after the single incoming byte */
  case ST_ADDR_R:
    if (sr1 & I2C_SR1_ADDR) {
      I2C1->CR1 &= ~I2C_CR1_ACK;
      (void)I2C1->SR1;
      (void)I2C1->SR2;
      I2C1->CR1 |= I2C_CR1_STOP;
      g_xfer.state = ST_RX;
    }
    break;

  /* Byte received */
  case ST_RX:
    if (sr1 & I2C_SR1_RXNE) {
      *g_xfer.rx_out = (uint8_t)I2C1->DR;
      I2C1->CR1 |= I2C_CR1_ACK; /* re-arm ACK for next transaction */
      irq_disable();
      g_xfer.state = ST_DONE;
      g_xfer.result = I2C_OK;
      sem_post(g_done_sem);
    }
    break;

  default:
    break;
  }
}

/* ── Error ISR ───────────────────────────────────────────────────────────── */

void I2C1_ER_IRQHandler(void) {
  uint32_t sr1 = I2C1->SR1;

  /* Acknowledge all error flags */
  I2C1->SR1 = ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT);

  i2c_result_t err = (sr1 & I2C_SR1_AF) ? I2C_ERROR_NACK : I2C_ERROR_BUS;
  finish(err);
}
