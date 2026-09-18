#ifndef CS43L22_H
#define CS43L22_H

#include <stdbool.h>
#include <stdint.h>

/*
 * CS43L22 stereo audio DAC driver (control plane only).
 *
 * The CS43L22 sits on I2C1 at address 0x94 (write) / 0x95 (read).
 * All register accesses go through the interrupt-driven i2c driver,
 * so calling tasks block rather than spin while the bus works.
 *
 * Typical call sequence:
 *   cs43l22_hw_reset();
 *   cs43l22_init(CS43L22_OUTPUT_HEADPHONE);
 *   cs43l22_set_volume(220);     // 0–255
 *   cs43l22_start();             // unmutes both channels
 */

typedef enum {
  CS43L22_OUTPUT_HEADPHONE = 0,
  CS43L22_OUTPUT_SPEAKER = 1,
  CS43L22_OUTPUT_BOTH = 2,
} cs43l22_output_t;

/* Assert and then release the hardware RESET line (PD4). */
void cs43l22_hw_reset(void);

/*
 * Full power-up sequence:
 *   - Apply the errata "magic" sequence to stabilise internal charge pumps.
 *   - Configure clocking (auto-detect), I2S 16-bit slave, chosen output path.
 *   - Set reasonable default volumes.
 *   - Leaves the DAC muted; call cs43l22_start() to begin playback.
 */
void cs43l22_init(cs43l22_output_t output);

/* Unmute both channels and transition to the PLAY power state. */
void cs43l22_start(void);

/* Mute both channels (leaves DAC powered). */
void cs43l22_stop(void);

/*
 * Set the master output volume.
 * vol = 0   → 0 dB  (maximum loudness, register value 0x00)
 * vol = 255 → −102 dB (nearly silent, register value 0xFF → wraps to −102)
 * The CS43L22 volume encoding: 0x00..0x18 = 0..+12 dB, 0x19..0xFF = −102..−1 dB.
 * This helper maps 0–255 linearly onto the useful 0x00–0xE4 range (0..−90 dB).
 */
void cs43l22_set_volume(uint8_t vol);

/*
 * Phase 2 / Step 1 loopback verification.
 * Writes a known value to register 0x0D (Playback Control 1), reads it back,
 * and returns true if they match.  Call after cs43l22_init().
 */
bool cs43l22_loopback_verify(void);

/* Raw register access (useful for debugging from a task). */
void cs43l22_write_reg(uint8_t reg, uint8_t data);
uint8_t cs43l22_read_reg(uint8_t reg);

#endif /* CS43L22_H */
