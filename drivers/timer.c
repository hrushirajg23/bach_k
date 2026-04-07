/**
 * @file  timer.c
 * @brief Functionality for timer driver
 *        from Bran's Kernel Development Tutorial
 * @date 2025-07-14
 */

/* #include "timer.h" */
#include "serial.h"
#include "time.h"
#include "task.h"

extern struct task_struct *current;

/*
 * sequence for a timer interrupt
 * Crystal oscillator (1.193182 MHz)
        ↓
    PIT Channel 0
        ↓
    IRQ0
        ↓
    8259 PIC (remapped to 0x20)
        ↓
    CPU receives interrupt vector 0x20
        ↓
    IDT[0x20]
        ↓
    timer_intr:

    Jar interrupt user mode madhe ala tar (ring 3)
        

    Jar interrupt kernel mode madhe ala tar (ring 0)
    
*/

void operate_timer(int cpl)
{
    if (cpl)
        current->utime++;
    else
        current->stime++;

    if (--current->counter > 0)
        return;

    current->counter = TIME_QUANTUM;

    /* Schedule from both user mode (cpl!=0) and kernel mode (cpl==0).
     * Kernel threads always run at ring 0 so we must schedule here too. */
    /* printk("[timer] pid=%d counter expired, scheduling\n", current->pid); */
//    schedule();
}

