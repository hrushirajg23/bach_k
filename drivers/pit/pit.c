/**
 * @file  pit.c
 * @brief Setting up Programmable Interval Timer
 *        from Bran's Kernel Development Tutorial
 * @date 2025-07-14
 */

#include <stdint.h>

#include "io_access.h"

#define PIT_COMMAND_REG 0x43
#define PIT_CHANNEL_0_DATA_REG 0x40

#define CHANNEL0 0x0
#define CHANNEL1 0x1
#define CHANNEL2 0x2

#define ACCESS_LATCH_COUNT 0x0
#define ACCESS_LSB 0x1
#define ACCESS_MSB 0x2
#define ACCESS_LSB_MSB 0x3

#define MODE_INTERRUPT_ON_TERMINAL_COUNT 0x0
#define MODE_RATE_GENERATOR 0x2
#define MODE_SQUARE_WAVE_GENERATOR 0x3

#define BINARY_MODE 0x0
#define BCD_MODE 0x1

/* Initialize PIT with given frequency */
void init_timer(int frequency) {
  uint16_t divisor = 1193180 / frequency;

  /* bit 7 to 6 are for channel
   * bit 5 to 4 are for access mode
   * bit 3 to 1 are for operating mode 
   * bit 0: 0 = 16 bit binary  , 1 = four-digit bcd
   * currently apan channel 0 use karnar ahot , scheduling sathi lagel
   * easy
   */
  uint8_t command = (CHANNEL0 << 6) | (ACCESS_LSB_MSB << 4) |
                    (MODE_RATE_GENERATOR << 1) | BINARY_MODE;

  outb(PIT_COMMAND_REG, command);
  io_wait();
  outb(PIT_CHANNEL_0_DATA_REG, divisor & 0xFF);
  outb(PIT_CHANNEL_0_DATA_REG, (divisor >> 8) & 0xFF);
}
