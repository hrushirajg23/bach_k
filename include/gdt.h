/**
 * @file  gdt.h
 * @brief Setting up Global Descriptor Table
 * @date 2025-07-14
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define RING0 0
#define RING3 3

#define _KERNEL_CS  0x08
#define _KERNEL_DS  0x10
#define _USER_CS    0x18
#define _USER_DS    0x20

/* GDT entry struct */
typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed)) GDTEntry;

/* GDT pointer struct */
typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) GDTPtr;

void gdt_initialize(void);

/* Flush the GDT (reload from assembly)*/
void gdt_flush(void);
#endif
