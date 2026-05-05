#ifndef I2S_H
#define I2S_H

#include "semaphore.h"
#include <stdint.h>

/*
 * I2S3 + DMA1 Stream7 audio output driver for STM32F407.
 *
 * Data path:  audio task → DMA buffer → SPI3/I2S3 → CS43L22 DAC
 *
 * The DMA runs in circular mode over a ping-pong buffer of
 * AUDIO_BUFFER_SAMPLES 16-bit stereo samples (L and R interleaved).
 * Two interrupts let the CPU refill one half while the other is being
 * consumed:
 *   Half-Transfer Complete (HTC) → refill first half  (samples 0 .. N/2-1)
 *   Transfer Complete      (TC)  → refill second half (samples N/2 .. N-1)
 *
 * The audio task calls i2s_wait_for_buffer_request() which blocks on a
 * semaphore posted by the DMA ISR, then writes into the indicated half.
 */

#define AUDIO_SAMPLE_RATE 44100u
#define AUDIO_BUFFER_SAMPLES 512u /* total DMA buffer length (16-bit words) */
#define AUDIO_HALF_SAMPLES (AUDIO_BUFFER_SAMPLES / 2u)

typedef enum {
  AUDIO_FILL_FIRST_HALF = 0,
  AUDIO_FILL_SECOND_HALF = 1,
} audio_half_t;

extern int16_t g_audio_buf[AUDIO_BUFFER_SAMPLES]; /* DMA source buffer */

/*
 * Initialise PLLI2S, I2S3 peripheral and DMA1 Stream7.
 * Call once before kernel_start().
 */
void i2s_init(void);

/*
 * Start the DMA stream – audio begins playing.
 * The DMA ISR will start posting the fill semaphore immediately.
 */
void i2s_start(void);

/*
 * Block the calling task until the DMA needs the next half-buffer filled.
 * Returns which half to write into.  Write AUDIO_HALF_SAMPLES samples
 * starting at g_audio_buf[half * AUDIO_HALF_SAMPLES].
 */
audio_half_t i2s_wait_for_buffer_request(void);

/* Called from DMA1_Stream7_IRQHandler */
void DMA1_Stream7_IRQHandler(void);

#endif /* I2S_H */
