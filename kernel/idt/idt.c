/**
 * @file  idt.c
 * @brief Setting up Interrupt Descriptor Table
 *        and load with inline assembly
 *        from OSDev
 * @date 2025-07-14
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idt.h"
#include "serial.h"
#include "task.h"
#include "time.h"
#include "keyboard.h"
#include "timer.h"


#define IDT_MAX_DESCRIPTORS 256
#define KERNEL_CS 0x08

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)


__attribute__((aligned(0x10))) static idt_entry_t idt[IDT_MAX_DESCRIPTORS];
static idtr_t idtr;

/** Load the IDT and enable interrupts */
static void load_idt(idtr_t *idt_descriptor) {
  __asm__ volatile("lidt (%0)" : : "r"(idt_descriptor) : "memory");
  /* __asm__ volatile("sti"); */
}

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);

static inline void set_gate(
    idt_entry_t *gate,
    uint32_t addr,
    uint8_t type,
    uint8_t dpl
) {
    gate->isr_low  = addr & 0xFFFF;
    gate->kernel_cs = KERNEL_CS;   
    gate->reserved = 0;
    gate->attributes   = 0x80 | (dpl << 5) | type;
    gate->isr_high = (addr >> 16) & 0xFFFF;
}

#define set_intr_gate(n, addr)   set_gate(&idt[n], (uint32_t)(addr), 0xE, 0)
#define set_trap_gate(n, addr)   set_gate(&idt[n], (uint32_t)(addr), 0xF, 0)
#define set_system_gate(n, addr) set_gate(&idt[n], (uint32_t)(addr), 0xF, 3)

static void die(char * str,long esp_ptr,long nr)
{
	long * esp = (long *) esp_ptr;
	int i;

	printk("%s: %04x\n\r",str,nr&0xffff);
	printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
		esp[1],esp[0],esp[2],esp[4],esp[3]);
	/* printk("fs: %04x\n",_fs()); */
	/* printk("base: %p, limit: %p\n",get_base(current->ldt[1]),get_limit(0x17)); */
	/* if (esp[4] == 0x17) { */
	/* 	printk("Stack: "); */
	/* 	for (i=0;i<4;i++) */
	/* 		printk("%p ",get_seg_long(0x17,i+(long *)esp[3])); */
	/* 	printk("\n"); */
	/* } */
	/* str(i); */
	/* printk("Pid: %d, process nr: %d\n\r",current->pid,0xffff & i); */
	/* for(i=0;i<10;i++) */
	/* 	printk("%02x ",0xff & get_seg_byte(esp[1],(i+(char *)esp[0]))); */
	/* printk("\n\r"); */
	/* do_exit(11);		/1* play segment exception *1/ */
}

void do_double_fault(long esp, long error_code)
{
	die("double fault",esp,error_code);
}

void do_general_protection(long esp, long error_code)
{
	die("general protection",esp,error_code);
}

void do_divide_error(long esp, long error_code)
{
	die("divide error",esp,error_code);
}

void do_int3(long * esp, long error_code,
		long fs,long es,long ds,
		long ebp,long esi,long edi,
		long edx,long ecx,long ebx,long eax)
{
	int tr;

	__asm__("str %%ax":"=a" (tr):"0" (0));
	printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
		eax,ebx,ecx,edx);
	printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
		esi,edi,ebp,(long) esp);
	printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
		ds,es,fs,tr);
	printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r",esp[0],esp[1],esp[2]);
}

void do_nmi(long esp, long error_code)
{
	die("nmi",esp,error_code);
}

void do_debug(long esp, long error_code)
{
	die("debug",esp,error_code);
}

void do_overflow(long esp, long error_code)
{
	die("overflow",esp,error_code);
}

void do_bounds(long esp, long error_code)
{
	die("bounds",esp,error_code);
}

void do_invalid_op(long esp, long error_code)
{
	die("invalid operand",esp,error_code);
}

void do_device_not_available(long esp, long error_code)
{
	die("device not available",esp,error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
	die("coprocessor segment overrun",esp,error_code);
}

void do_invalid_TSS(long esp,long error_code)
{
	die("invalid TSS",esp,error_code);
}

void do_segment_not_present(long esp,long error_code)
{
	die("segment not present",esp,error_code);
}

void do_stack_segment(long esp,long error_code)
{
	die("stack segment",esp,error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
	die("coprocessor error",esp,error_code);
}

void do_reserved(long esp, long error_code)
{
	die("reserved (15,17-31) error",esp,error_code);
}

void trap_init(void)
{
	int i;

	set_trap_gate(0,&divide_error);
	set_trap_gate(1,&debug);
	set_trap_gate(2,&nmi);
	set_system_gate(3,&int3);	/* int3-5 can be called from user process also*/
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
	set_trap_gate(6,&invalid_op);
	set_trap_gate(7,&device_not_available);
	set_trap_gate(8,&double_fault);
	set_trap_gate(9,&coprocessor_segment_overrun);
	set_trap_gate(10,&invalid_TSS);
	set_trap_gate(11,&segment_not_present);
	set_trap_gate(12,&stack_segment);
	set_trap_gate(13,&general_protection);
	/* set_trap_gate(14,&page_fault); */
	set_trap_gate(15,&reserved);
	set_trap_gate(16,&coprocessor_error);
	for (i=17;i<32;i++)
		set_trap_gate(i,&reserved);
    set_intr_gate(32, &timer_intr);
    set_intr_gate(33, &keyboard_driver);
}



/* Initialize the IDT */
void initialize_idt() {
  idtr.limit = sizeof(idt) - 1;
  idtr.base = (uint32_t)idt;

  load_idt(&idtr);
}
