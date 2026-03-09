/**
 * @file  exceptions.h
 * @brief CPU Exceptions handler
 * @date 2025-07-14
 */

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdint.h>

#include "registers.h"

/* Exception messages based on interrupt number */
const char *exception_messages[] = {"Division By Zero",
                                    "Debug",
                                    "Non Maskable Interrupt",
                                    "Breakpoint",
                                    "Overflow",
                                    "Bound Range Exceeded",
                                    "Invalid Opcode",
                                    "Device Not Available",
                                    "Double Fault",
                                    "Coprocessor Segment Overrun",
                                    "Invalid TSS",
                                    "Segment Not Present",
                                    "Stack-Segment Fault",
                                    "General Protection Fault",
                                    "Page Fault",
                                    "Reserved",
                                    "x87 Floating-Point Exception",
                                    "Alignment Check",
                                    "Machine Check",
                                    "SIMD Floating-Point Exception",
                                    "Virtualization Exception",
                                    "Control Protection Exception",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved",
                                    "Reserved"};

__attribute__((noreturn)) void exception_handler(registers_t *regs);

#endif