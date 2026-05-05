#include "cs43l22.h"
#include "i2c.h"
#include "kernel.h"
#include "stm32f4xx.h"

/*
 * CS43L22 register map (relevant subset)
 */
#define REG_ID 0x01            /* Chip ID – read-only, lower nibble = rev */
#define REG_PWR_CTL1 0x02      /* Power Control 1 */
#define REG_PWR_CTL2 0x04      /* Power Control 2 (output path enables) */
#define REG_CLK_CTL 0x05       /* Clocking Control */
#define REG_IF_CTL1 0x06       /* Interface Control 1 (I2S format) */
#define REG_ANALOG_ZC 0x0A     /* Analog Zero-Cross / Soft Ramp */
#define REG_PLAYBACK_CTL1 0x0D /* Playback Control 1 (mute, channel routing) */
#define REG_MISC_CTL 0x0E      /* Misc: de-emphasis, mono mix */
#define REG_PLAYBACK_CTL2 0x0F /* Playback Control 2 */
#define REG_PCM_VOL_A 0x1A     /* PCM channel A volume */
#define REG_PCM_VOL_B 0x1B     /* PCM channel B volume */
#define REG_MASTER_VOL_A 0x20  /* Master volume channel A */
#define REG_MASTER_VOL_B 0x21  /* Master volume channel B */
#define REG_HP_VOL_A 0x22      /* Headphone volume A */
#define REG_HP_VOL_B 0x23      /* Headphone volume B */
#define REG_SPK_VOL_A 0x24     /* Speaker volume A */
#define REG_SPK_VOL_B 0x25     /* Speaker volume B */

/* Device I2C addresses */
#define ADDR_WRITE 0x94u
#define ADDR_READ 0x95u

/* ── helpers ────────────────────────────────────────────────────────────── */

void cs43l22_write_reg(uint8_t reg, uint8_t data) { i2c_write_reg(ADDR_WRITE, reg, data); }

uint8_t cs43l22_read_reg(uint8_t reg) {
  uint8_t val = 0;
  i2c_read_reg(ADDR_WRITE, ADDR_READ, reg, &val);
  return val;
}

/* ── public API ─────────────────────────────────────────────────────────── */

void cs43l22_hw_reset(void) {
  /* PD4 = RESET line, active-low */
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  (void)RCC->AHB1ENR; /* read-back: wait 2 AHB cycles for clock to propagate */
  GPIOD->MODER &= ~GPIO_MODER_MODER4;
  GPIOD->MODER |= GPIO_MODER_MODER4_0; /* output */

  GPIOD->BSRR = GPIO_BSRR_BR4; /* assert RESET low */
  task_delay(10);               /* hold ≥1 ms; spin loops are ~950 ns at 168 MHz */
  GPIOD->BSRR = GPIO_BSRR_BS4; /* release RESET high */
  task_delay(10);               /* wait for CS43L22 internal regulators to stabilise */
}

void cs43l22_init(cs43l22_output_t output) {
  /*
   * Step 1: "Magic" power-up sequence from CS43L22 errata / AN4.
   * Must be applied before any other register write.
   */
  cs43l22_write_reg(0x00, 0x99);
  cs43l22_write_reg(0x47, 0x80);
  cs43l22_write_reg(0x32, 0x80 | cs43l22_read_reg(0x32));
  cs43l22_write_reg(0x32, (~0x80u) & cs43l22_read_reg(0x32));
  cs43l22_write_reg(0x00, 0x00);

  /*
   * Step 2: Power Control 2 – enable the chosen output path.
   *   Bits [7:6]: SPKB / SPKA   00 = powered on
   *   Bits [5:4]: HPB  / HPA    00 = powered on
   *   Writing 0xAF disables speakers, enables headphone (STM32F407 board
   *   routes DAC output to headphone jack).
   */
  uint8_t pwr2;
  switch (output) {
  case CS43L22_OUTPUT_HEADPHONE:
    pwr2 = 0xAFu;
    break; /* HP on, SPK off */
  case CS43L22_OUTPUT_SPEAKER:
    pwr2 = 0xFAu;
    break; /* SPK on, HP off */
  default:
    pwr2 = 0xAAu;
    break; /* both on */
  }
  cs43l22_write_reg(REG_PWR_CTL2, pwr2);

  /*
   * Step 3: Clocking – auto-detect MCLK speed.
   * Bit 7 (AUTO) = 1: CS43L22 infers speed class from MCLK/LRCK ratio.
   * Bit 0 (MCLKDIV2) = 1: divide MCLK by 2 inside the chip (needed when
   * MCLK > 12.288 MHz, which it is at 44.1 kHz × 256 = 11.29 MHz, so
   * leave it 0).
   */
  cs43l22_write_reg(REG_CLK_CTL, 0x81); /* auto-detect, single-speed */

  /*
   * Step 4: Interface Control 1 – I2S, 16-bit, slave mode.
   * Bits [2:0] = 0b111 selects I2S format, 16-bit data length.
   */
  cs43l22_write_reg(REG_IF_CTL1, 0x07);

  /*
   * Step 5: Zero-cross / soft-ramp – disable to avoid clicks during
   * volume changes in a demo context.
   */
  cs43l22_write_reg(REG_ANALOG_ZC, 0x00);

  /*
   * Step 6: Misc control – de-emphasis off, mono-mix off.
   */
  cs43l22_write_reg(REG_MISC_CTL, 0x02);

  /*
   * Step 7: Default volumes – 0 dB on all paths.
   */
  cs43l22_write_reg(REG_PCM_VOL_A, 0x0A); /* +0 dB PCM mix */
  cs43l22_write_reg(REG_PCM_VOL_B, 0x0A);
  cs43l22_write_reg(REG_MASTER_VOL_A, 0x00); /* 0 dB master */
  cs43l22_write_reg(REG_MASTER_VOL_B, 0x00);
  cs43l22_write_reg(REG_HP_VOL_A, 0x00);
  cs43l22_write_reg(REG_HP_VOL_B, 0x00);
  cs43l22_write_reg(REG_SPK_VOL_A, 0x00);
  cs43l22_write_reg(REG_SPK_VOL_B, 0x00);

  /*
   * Step 8: Playback Control 1 – mute both channels for now.
   * Bits [7:6] = MSTB/MSTA = 1 → muted.
   * Written here; cs43l22_start() clears the mute bits.
   */
  cs43l22_write_reg(REG_PLAYBACK_CTL1, 0xE0);

  /*
   * Step 9: Power up the DAC (Power Control 1).
   * 0x9E = 0b1001_1110 → powered-up state per datasheet table 2.
   */
  cs43l22_write_reg(REG_PWR_CTL1, 0x9E);
}

void cs43l22_start(void) {
  /* Clear mute bits A and B: bits [7:6] of REG_PLAYBACK_CTL1 → 0 */
  uint8_t ctl = cs43l22_read_reg(REG_PLAYBACK_CTL1);
  ctl &= ~(0xC0u);
  cs43l22_write_reg(REG_PLAYBACK_CTL1, ctl);
}

void cs43l22_stop(void) {
  uint8_t ctl = cs43l22_read_reg(REG_PLAYBACK_CTL1);
  ctl |= 0xC0u;
  cs43l22_write_reg(REG_PLAYBACK_CTL1, ctl);
}

void cs43l22_set_volume(uint8_t vol) {
  /*
   * Map 0–255 onto the CS43L22 master volume register range 0x00–0xE4.
   * 0x00 = 0 dB (loudest useful setting), 0xE4 = −90 dB (nearly silent).
   * Values 0xE5–0xFF wrap around to +12 dB; we avoid them.
   */
  uint8_t reg_val = (uint8_t)((vol * 0xE4u) / 255u);
  cs43l22_write_reg(REG_MASTER_VOL_A, reg_val);
  cs43l22_write_reg(REG_MASTER_VOL_B, reg_val);
}

bool cs43l22_loopback_verify(void) {
  /*
   * Phase 2 / Step 1 verification.
   *
   * Register 0x0D (Playback Control 1) is fully R/W on bits [7:2].
   * We write 0x60 (both channels unmuted, PCM source), read back, compare.
   * Then restore the pre-test value so the rest of the driver is unaffected.
   */
  const uint8_t TEST_VALUE = 0x60;

  uint8_t before = cs43l22_read_reg(REG_PLAYBACK_CTL1);
  cs43l22_write_reg(REG_PLAYBACK_CTL1, TEST_VALUE);
  uint8_t readback = cs43l22_read_reg(REG_PLAYBACK_CTL1);
  cs43l22_write_reg(REG_PLAYBACK_CTL1, before); /* restore */

  return (readback == TEST_VALUE);
}
