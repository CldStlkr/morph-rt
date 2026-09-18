#include "i2s.h"
#include "stm32f4xx.h"
#include <stddef.h>

/*
 * I2S3 + DMA1 Stream7 driver.
 *
 * Clock derivation (HSE = 8 MHz, PLLM = 8 → 1 MHz VCO input):
 *   PLLI2SN = 271  →  VCO = 271 MHz
 *   PLLI2SR = 6    →  I2S clk = 45.167 MHz
 *   I2SDIV  = 2, ODD = 0, MCKOE = 1
 *   Fs = 45,167,000 / (256 × (2×2 + 0)) = 44,108 Hz  ≈ 44.1 kHz
 *
 * GPIO alternate functions (AF6 = I2S3):
 *   PA4  → I2S3_WS  (word select / LRCK)
 *   PC7  → I2S3_MCK (master clock, 256×Fs)
 *   PC10 → I2S3_CK  (bit clock)
 *   PC12 → I2S3_SD  (serial data)
 *
 * DMA:
 *   DMA1 Stream7 Channel 0 → SPI3_TX (I2S3 data register)
 *   Mode: circular, memory-to-peripheral, 16-bit transfers
 *   HTC and TC interrupts drive the ping-pong fill logic.
 */

/* ── DMA buffer (in SRAM, accessible by DMA) ────────────────────────────── */
int16_t g_audio_buf[AUDIO_BUFFER_SAMPLES];

/* ── RTOS sync ──────────────────────────────────────────────────────────── */
static semaphore_handle_t g_fill_sem;

/* ── helpers ────────────────────────────────────────────────────────────── */

static void plli2s_init(void) {
  /* Disable PLLI2S before configuring */
  RCC->CR &= ~RCC_CR_PLLI2SON;
  while (RCC->CR & RCC_CR_PLLI2SRDY)
    ;

  /* PLLI2SN=271 (bits 14:6), PLLI2SR=6 (bits 30:28) */
  RCC->PLLI2SCFGR = (271u << 6) | (6u << 28);

  /* Enable and wait for lock */
  RCC->CR |= RCC_CR_PLLI2SON;
  while (!(RCC->CR & RCC_CR_PLLI2SRDY))
    ;
}

static void i2s3_gpio_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
  (void)RCC->AHB1ENR; /* 2-cycle read-back before first peripheral access */

  /* PA4: AF6, push-pull, high-speed, no pull */
  GPIOA->MODER &= ~GPIO_MODER_MODER4;
  GPIOA->MODER |= GPIO_MODER_MODER4_1;
  GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR4;
  GPIOA->AFR[0] |= (6u << (4 * 4));

  /* PC7 (MCK), PC10 (CK), PC12 (SD): AF6, push-pull, high-speed, no pull */
  GPIOC->MODER &= ~(GPIO_MODER_MODER7 | GPIO_MODER_MODER10 | GPIO_MODER_MODER12);
  GPIOC->MODER |= (GPIO_MODER_MODER7_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER12_1);
  GPIOC->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR7 | GPIO_OSPEEDER_OSPEEDR10 | GPIO_OSPEEDER_OSPEEDR12);
  GPIOC->AFR[0] |= (6u << (7 * 4));        /* PC7:  AFR[0] bits 31:28 */
  GPIOC->AFR[1] |= (6u << ((10 - 8) * 4)); /* PC10: AFR[1] bits 11:8  */
  GPIOC->AFR[1] |= (6u << ((12 - 8) * 4)); /* PC12: AFR[1] bits 19:16 */
}

static void i2s3_peripheral_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
  (void)RCC->APB1ENR;

  /* Reset SPI3 */
  RCC->APB1RSTR |= RCC_APB1RSTR_SPI3RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI3RST;

  /*
   * I2SCFGR:
   *   I2SMOD = 1  (I2S mode, not SPI)
   *   I2SCFG = 10 (master transmit)
   *   I2SSTD = 00 (Philips I2S)
   *   DATLEN = 00 (16-bit data)
   *   CHLEN  = 0  (16-bit channel)
   */
  SPI3->I2SCFGR = SPI_I2SCFGR_I2SMOD | (2u << SPI_I2SCFGR_I2SCFG_Pos);

  /*
   * I2SPR: I2SDIV=2, ODD=0, MCKOE=1
   * Enables master-clock output (required by CS43L22).
   */
  SPI3->I2SPR = (2u << SPI_I2SPR_I2SDIV_Pos) | SPI_I2SPR_MCKOE;
}

static void dma1_stream7_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
  (void)RCC->AHB1ENR;

  /* Disable stream before configuring */
  DMA1_Stream7->CR &= ~DMA_SxCR_EN;
  while (DMA1_Stream7->CR & DMA_SxCR_EN)
    ;

  /* Clear all interrupt flags for stream 7 */
  DMA1->HIFCR = DMA_HISR_TCIF7 | DMA_HISR_HTIF7 | DMA_HISR_TEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_FEIF7;

  /* Peripheral address: SPI3 data register */
  DMA1_Stream7->PAR = (uint32_t)&SPI3->DR;

  /* Memory address: audio buffer */
  DMA1_Stream7->M0AR = (uint32_t)g_audio_buf;

  /* Number of data items */
  DMA1_Stream7->NDTR = AUDIO_BUFFER_SAMPLES;

  /*
   * CR:
   *   CHSEL = 000  (channel 0 = SPI3_TX)
   *   DIR   = 01   (memory to peripheral)
   *   CIRC  = 1    (circular mode)
   *   MINC  = 1    (memory address increments)
   *   PINC  = 0    (peripheral address fixed)
   *   MSIZE = 01   (16-bit memory)
   *   PSIZE = 01   (16-bit peripheral)
   *   HTIE  = 1    (half-transfer interrupt)
   *   TCIE  = 1    (transfer-complete interrupt)
   *   PL    = 10   (high priority)
   */
  DMA1_Stream7->CR = (0u << DMA_SxCR_CHSEL_Pos) | (1u << DMA_SxCR_DIR_Pos) | DMA_SxCR_CIRC | DMA_SxCR_MINC |
                     (1u << DMA_SxCR_MSIZE_Pos) | (1u << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_HTIE | DMA_SxCR_TCIE |
                     (2u << DMA_SxCR_PL_Pos);

  /* FIFO control: direct mode (FIFO disabled) */
  DMA1_Stream7->FCR = 0;

  /* NVIC */
  NVIC_SetPriority(DMA1_Stream7_IRQn, 0x90);
  NVIC_EnableIRQ(DMA1_Stream7_IRQn);
}

/* ── public API ─────────────────────────────────────────────────────────── */

void i2s_init(void) {
  g_fill_sem = sem_create(0, 1, "i2s_fill");

  plli2s_init();
  i2s3_gpio_init();
  i2s3_peripheral_init();
  dma1_stream7_init();
}

void i2s_start(void) {
  /* Enable DMA request from SPI3 transmitter */
  SPI3->CR2 |= SPI_CR2_TXDMAEN;

  /* Enable DMA stream */
  DMA1_Stream7->CR |= DMA_SxCR_EN;

  /* Enable I2S peripheral – audio clock begins */
  SPI3->I2SCFGR |= SPI_I2SCFGR_I2SE;
}

audio_half_t i2s_wait_for_buffer_request(void) {
  sem_wait(g_fill_sem, SEM_WAIT_FOREVER);
  /*
   * Determine which half to fill by reading the DMA's live position.
   * NDTR counts DOWN from AUDIO_BUFFER_SAMPLES to 0.
   * NDTR > half  → DMA is playing the first half  → fill the second half.
   * NDTR ≤ half  → DMA is playing the second half → fill the first half.
   * Reading NDTR here (in task context, after waking) is race-free: the
   * half-buffer period is ~5.8 ms at 44100 Hz, far longer than task latency.
   */
  return (DMA1_Stream7->NDTR > AUDIO_HALF_SAMPLES) ? AUDIO_FILL_SECOND_HALF : AUDIO_FILL_FIRST_HALF;
}

/* ── DMA ISR ─────────────────────────────────────────────────────────────── */

void DMA1_Stream7_IRQHandler(void) {
  uint32_t hisr = DMA1->HISR;

  /* Clear all flags immediately to prevent re-entry */
  DMA1->HIFCR = DMA_HISR_TCIF7 | DMA_HISR_HTIF7 | DMA_HISR_TEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_FEIF7;

  /*
   * Post once for either HTC or TC.  max_count=1 silently drops the second
   * post if both flags fired in the same ISR invocation (delayed handler),
   * causing at most one glitched half rather than corrupting state.
   * audio_task reads NDTR to determine the correct half regardless.
   */
  if (hisr & (DMA_HISR_HTIF7 | DMA_HISR_TCIF7)) {
    sem_post(g_fill_sem);
  }
}
