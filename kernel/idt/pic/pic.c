/**
 * @file  pic.c
 * @brief Remappings and initialization of the Programmable Interrupt Controller
 *        from OSDev
 * @date 2025-07-14
 */

#include <stdint.h>

#include "io_access.h"
#include "pic.h"

#define ICW1_ICW4 0x01      /* ICW4 will be present */
#define IC1_SINGLE 0X02     /* single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* level trigger (edge) mode */
#define ICW1_INIT 0x10      /* initialization */

#define ICW4_8086 0x01       /* 8086/88 mode*/
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* special fully nested */

#define PIC_READ_IRR 0x0a
#define PIC_READ_ISR 0x0b

/* Remaps the Programmable Interrupt Controller
Arguments:
    offset1 - vector offset for master PIC (0x20 - 0x27)
    offset2 - vector offset for slave PIC (0x28 - 0x2F)
*/
void remap_pic(int offset1, int offset2) {
  /* ICW1: start the initialization in cascade mode */
  outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();

  /* ICW2: Master and Slave vector offset */
  outb(PIC1_DATA, offset1);
  io_wait();
  outb(PIC2_DATA, offset2);
  io_wait();

  /* ICW3: tell master and slave about cascade */
  outb(PIC1_DATA, 4); /* there is a slave PIC at IRQ2 (0000 0100) */
  io_wait();
  outb(PIC2_DATA, 2); /* tell slave PIC its cascade identity (0000 0010) */
  io_wait();

  /* ICW4: use 8086 mode */
  outb(PIC1_DATA, ICW4_8086);
  io_wait();
  outb(PIC2_DATA, ICW4_8086);
  io_wait();

  /* mask all IRQs first, then unmask only the ones we have handlers for */
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);

  /* unmask IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade to slave), IRQ4 (serial) */
  IRQ_clear_mask(0);
  IRQ_clear_mask(1);
  IRQ_clear_mask(2);
  IRQ_clear_mask(4);
}

/* send End of Interrupt */
void send_EOI(uint8_t irq) {
  if (irq >= 8)
    outb(PIC2_COMMAND, PIC_EOI);

  outb(PIC1_COMMAND, PIC_EOI);
}

/* setting a bit 1 */
void IRQ_set_mask(uint8_t IRQline) {
  uint16_t port;
  uint8_t value;

  if (IRQline < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    IRQline -= 8;
  }
  value = inb(port) | (1 << IRQline);
  outb(port, value);
}

/* setting a bit 0 */
void IRQ_clear_mask(uint8_t IRQline) {
  uint16_t port;
  uint8_t value;

  if (IRQline < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    IRQline -= 8;
  }
  value = inb(port) & ~(1 << IRQline);
  outb(port, value);
}

static uint16_t __pic_get_irq_reg(int ocw3) {
  outb(PIC1_COMMAND, ocw3);
  outb(PIC2_COMMAND, ocw3);

  return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_get_irr(void) { return __pic_get_irq_reg(PIC_READ_IRR); }

uint16_t pic_get_isr(void) { return __pic_get_irq_reg(PIC_READ_ISR); }