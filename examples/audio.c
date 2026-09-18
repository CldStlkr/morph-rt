#include "SEGGER_RTT.h"
#include "cs43l22.h"
#include "i2c.h"
#include "i2s.h"
#include "kernel.h"
#include "semaphore.h"
#include "stm32f4xx.h"
#include <stdint.h>

/*
 * ALSA-Lite audio demo for Morph-RT on the STM32F407G-DISC1.
 *
 * Two RTOS tasks:
 *
 *   audio_task  (priority 1) – the synthesis engine.
 *     Blocks on i2s_wait_for_buffer_request().  Each time the DMA finishes
 *     half the buffer it wakes this task, which refills that half with a
 *     440 Hz sine wave.  This is a textbook "ping-pong" (double-buffer)
 *     pattern: hardware plays one half while software fills the other.
 *
 *   monitor_task (priority 2) – blinks the green LED (PD12) at 1 Hz and
 *     prints a heartbeat over SEGGER RTT so we know the kernel is alive.
 */

/* ── Sine wave look-up table ─────────────────────────────────────────────── */
/*
 * 256 entries, one full cycle of a sine wave, 16-bit signed, peak ≈ 80%
 * of full scale (0x6666 = 26214).  Generated at compile time:
 *   lut[i] = round(26214 * sin(2π * i / 256))
 */
static const int16_t sine_lut[256] = {
    0,      644,    1287,   1929,   2570,   3208,   3845,   4478,   5108,   5734,   6355,   6970,   7580,   8182,
    8778,   9366,   9946,   10516,  11076,  11626,  12165,  12692,  13207,  13709,  14197,  14671,  15131,  15576,
    16005,  16419,  16816,  17196,  17559,  17904,  18231,  18539,  18828,  19098,  19348,  19578,  19787,  19976,
    20144,  20291,  20416,  20519,  20601,  20660,  20697,  20712,  20705,  20675,  20622,  20547,  20449,  20329,
    20186,  20021,  19833,  19624,  19392,  19139,  18864,  18568,  18251,  17913,  17555,  17177,  16779,  16362,
    15927,  15473,  15001,  14513,  14007,  13486,  12950,  12398,  11833,  11254,  10663,  10060,  9446,   8822,
    8188,   7545,   6894,   6237,   5573,   4903,   4228,   3549,   2867,   2182,   1496,   808,    120,    -568,
    -1256,  -1942,  -2625,  -3305,  -3980,  -4650,  -5313,  -5969,  -6617,  -7256,  -7885,  -8503,  -9109,  -9702,
    -10282, -10847, -11396, -11928, -12443, -12940, -13418, -13876, -14313, -14729, -15123, -15494, -15841, -16165,
    -16464, -16738, -16986, -17209, -17405, -17574, -17716, -17831, -17919, -17979, -18011, -18015, -17991, -17940,
    -17861, -17754, -17619, -17457, -17268, -17052, -16809, -16540, -16245, -15924, -15578, -15207, -14812, -14393,
    -13951, -13486, -12999, -12491, -11962, -11413, -10845, -10259, -9655,  -9034,  -8399,  -7749,  -7085,  -6410,
    -5723,  -5026,  -4320,  -3606,  -2886,  -2160,  -1430,  -697,   37,     771,    1504,   2235,   2962,   3684,
    4400,   5108,   5808,   6499,   7179,   7847,   8503,   9144,   9770,   10379,  10971,  11544,  12097,  12629,
    13139,  13626,  14089,  14527,  14939,  15324,  15682,  16012,  16313,  16585,  16827,  17038,  17219,  17369,
    17488,  17575,  17630,  17654,  17646,  17606,  17534,  17431,  17296,  17130,  16934,  16706,  16449,  16161,
    15845,  15499,  15126,  14725,  14297,  13844,  13365,  12862,  12335,  11786,  11215,  10623,  10012,  9382,
    8735,   8072,   7394,   6703,   5999,   5284,   4560,   3827,   3087,   2342,   1592,   840,    87,     -668,
    -1422,  -2175,  -2923,  -3668,
};

/* ── Phase accumulator for 440 Hz at 44100 Hz sample rate ─────────────────
 * Full-scale phase = 2^32.  Step for 440 Hz:
 *   step = (440 / 44100) × 2^32 = 42,865,408  (≈ 0x28E8B400)
 * LUT index = phase_acc >> 24  (top 8 bits → 256 entries)
 */
#define PHASE_STEP 42865408u

/* ── Audio synthesis task ─────────────────────────────────────────────────── */

static void audio_task(void *arg) {
  (void)arg;
  uint32_t phase = 0;

  for (;;) {
    audio_half_t half = i2s_wait_for_buffer_request();
    int16_t *buf = &g_audio_buf[half * AUDIO_HALF_SAMPLES];

    for (uint32_t i = 0; i < AUDIO_HALF_SAMPLES; i += 2) {
      int16_t sample = sine_lut[phase >> 24];
      buf[i] = sample;     /* left  channel */
      buf[i + 1] = sample; /* right channel (mono source, stereo output) */
      phase += PHASE_STEP;
    }
  }
}

/* ── Monitor / heartbeat task ────────────────────────────────────────────── */

static void monitor_task(void *arg) {
  (void)arg;

  /* PD12 = green LED on STM32F407G-DISC1 */
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  GPIOD->MODER |= GPIO_MODER_MODER12_0;

  for (;;) {
    GPIOD->ODR ^= GPIO_ODR_OD12;
    SEGGER_RTT_WriteString(0, "[audio] 440 Hz playing\r\n");
    task_delay(1000);
  }
}

/* ── Startup task ────────────────────────────────────────────────────────── */
/*
 * All hardware initialisation that calls RTOS primitives (mutex_lock,
 * sem_wait) MUST run inside a task — never in main() before kernel_start().
 * sem_wait() with count=0 calls scheduler_block_current_task(); with
 * current_task==NULL that dereferences NULL and HardFaults.
 *
 * This task runs at priority 0 (highest) so it completes before the audio
 * and monitor tasks are scheduled.  It self-deletes when done.
 */
static void startup_task(void *arg) {
  (void)arg;

  /* i2c1_init() creates the bus mutex + done semaphore, configures GPIO,
   * the I2C1 peripheral, and enables the NVIC lines. */
  i2c1_init();

  /* Pulse PD4 low → high to release the CS43L22 from reset. */
  cs43l22_hw_reset();

  /* Magic errata sequence + full register configuration. */
  cs43l22_init(CS43L22_OUTPUT_HEADPHONE);

  /* Phase 2 / Step 1: write 0x60 to reg 0x0D, read back, compare. */
  bool ok = cs43l22_loopback_verify();
  SEGGER_RTT_WriteString(0, ok ? "[cs43l22] loopback: PASS\r\n" : "[cs43l22] loopback: FAIL\r\n");

  cs43l22_set_volume(128);

  /* Configure PLLI2S clock, I2S3 peripheral, DMA1 Stream7. */
  i2s_init();
  for (uint32_t i = 0; i < AUDIO_BUFFER_SAMPLES; i++)
    g_audio_buf[i] = 0;

  /* Unmute DAC outputs, then start the DMA circular stream. */
  cs43l22_start();
  i2s_start();

  task_create(audio_task, "audio", 1024, NULL, 1);
  task_create(monitor_task, "monitor", 512, NULL, 2);

  task_delete(task_get_current());
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void) {
  kernel_init();
  task_create(startup_task, "startup", 1024, NULL, 0);
  kernel_start();
  return 0;
}
