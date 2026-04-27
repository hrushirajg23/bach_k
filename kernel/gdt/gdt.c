/**
 * @file  gdt.c
 * @brief Setting up Global Descriptor Table
 *        and load with inline assembly
 *        from OSDev
 * @date 2025-07-14
 */

#include <stddef.h>

#include "gdt.h"
#include "task.h"

#define GDT_SIZE 6

static void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity);
static void fill_gdt(void);
static inline void load_gdt(GDTPtr *gdt_descriptor);

extern struct tss_struct cpu_tss;

GDTEntry gdt[GDT_SIZE];
GDTPtr gdt_ptr;

/* Set a GDT entry */
static void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
  gdt[index].limit_low = limit & 0xFFFF;
  gdt[index].base_low = base & 0xFFFF;
  gdt[index].base_middle = (base >> 16) & 0xFF;
  gdt[index].access = access;
  gdt[index].granularity = ((limit >> 16) & 0x0F) | (granularity << 4);
  gdt[index].base_high = (base >> 24) & 0xFF;
}

/* Fill the GDT with each entry */
static void fill_gdt(void) {

  // NULL Descriptor
  set_gdt_entry(0, 0, 0, 0, 0);

  // kernel code segment
  set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xC);

  // kernel data segment
  set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xC);

  // user code segment
  set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xC);

  // user data segment
  set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xC);

  //task state segment
  set_gdt_entry(5, (uint32_t)&cpu_tss, (uint32_t)sizeof(struct tss_struct) - 1, 0x89, 0x0); 

  //ldtr 
}

/* Initialize the GDT */
void gdt_initialize(void) {
  fill_gdt();
  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (uint32_t)gdt;

  load_gdt(&gdt_ptr);
  gdt_flush();
}

/* Load the GDT */
static inline void load_gdt(GDTPtr *gdt_descriptor) {
  __asm__ volatile("lgdt (%0)" : : "r"(gdt_descriptor) : "memory");
}
