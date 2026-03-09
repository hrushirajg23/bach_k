/**
 * @file  kernel.c
 * @brief Main kernel entry point with these phases:
 *        1. Setting up serial output
 *        2. Setting up memory management
 *        3. Setting up GDT
 *        4. Remap PIC
 *        5. Setting up IDT
 *        6. Install drivers
 *        7. Setting up PIT
 *        8. Enable Interrupts
 *        10. Setting up VGA display
 *        11. Start Kernel
 * @date 2025-07-14
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gdt.h"
#include "idt.h"
#include "kernel.h"
#include "manager.h"
#include "multiboot.h"
#include "pic.h"
#include "pit.h"
#include "serial.h"
#include "vga_display.h"
#include "zone.h"
#include "mm.h"
#include "task.h"
#include "disk.h"
#include "fs.h"
#include "string.h"
/* #include "vfs.h" */
#include "buffer.h"
#include "time.h"

#if defined(__linux__)
#error                                                                         \
    "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif



static int test_thread(void *arg) {
    int count = 0;

    if (current->pid == 1) {
        /*
         * Ring 0: print before dropping to user mode.
         * After move_to_user_mode() we are in ring 3 and must NOT
         * call printk — it uses outb (port I/O) which requires IOPL=3.
         */
        printk("[init] pid 1: dropping to ring 3, will fork from user mode\n");
        move_to_user_mode();

        /*
         * Now in ring 3.  Issue fork via int $0x80 (syscall #2).
         * child's eax = 0
         * parent's eax = child pid
         * Both parent and child fall into the busy loop below.
         * The [sched] / [timer] lines printed by the ring-0 interrupt
         * handler will show pid 0, pid 1 and pid 2 all alternating.
         */
        int child_pid;
        __asm__ volatile (
            "movl $2, %%eax \n\t"   /* NR_fork = 2 */
            "int  $0x80     \n\t"
            : "=a"(child_pid)
        );
        /* user-mode busy loop — no printk */
        while (1)
            for (volatile int i = 0; i < 500; i++);
    }

    /* Kernel-mode path (pid 0 idle, or any kernel-thread pid > 1 spawned later) */
    while (1) {
        printk("[sched] Task %d running (iter %d)\n", current->pid, count++);
        for (volatile int i = 0; i < 5000000; i++);
    }
    return 0;
}

/* Main kernel entry point */
void kernel_main(uint32_t magic, uint32_t addr) {

    if (magic != MAGIC) {
        while (1)
          asm volatile("hlt");
    }

    multiboot_info_t *mbi = (multiboot_info_t *)addr;

    serial_init();
    serial_writestring("Serial initialized. Booting...\n");

    serial_writestring("kernel_start= ");
    serial_writehex(KERNEL_START);

    serial_writestring("\nkernel_end= ");
    serial_writehex(KERNEL_END);

    serial_writestring("GRUB passed multiboot info at 0x\n"); 
    serial_writehex(addr);

    if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        serial_writestring("\nLower memory:  KB\n");
        serial_writehex(mbi->mem_lower);
        
        serial_writestring("\nUpper memory:  KB\n");
        serial_writehex(mbi->mem_upper);
    }

    /* machine_specific_memory_setup(mbi); */

    int memory_status = initialize_memeory_manager(mbi);
    if (memory_status < 0)
    while (1)
        asm volatile("hlt");

    init_mem(mbi);

    time_init();

    printk("\nGDT init...\n");
    gdt_initialize();

    printk("PIC remap...\n");
    remap_pic(OFFSET1, OFFSET2);

    printk("loading tss\n");
    tss_load(0x28);
    sched_init();

    printk("IDT setup...\n");
    trap_init();

    /* printk("Install timer & keyboard drivers..\n"); */
    /* install_handlers(); */

    printk("PIT init...\n");
    init_timer(FREQUENCY);

    printk("IDT init...\n");
    initialize_idt();

    /* Enable interrupts globally - without this the timer NEVER fires */
    asm volatile("sti");

    printk("Boot complete.\n");

    printk("working out hard disk \n");
    test_disk();

    printk("initializing buffer cache\n");
    create_buffer_cache();

    /*
     * CAUTION: this erases the whole existing file system
     * remember, for once you use mkfs, comment it out,
     * next time to use the os freely and apply your changes
     */
    /* mkfs(0, 8); // Commented out to test persistence. Uncomment to reformat. */
 /* Create 8MB ext2 filesystem at offset 0 */

    printk("initialising ext2 file system...........................\n");
    ext2_init_fs();

    printk("initializing inode cache \n");
    create_inode_cache();
        
    printk("mounting rootfs.........\n");
    ext2_mount_root();

    printk("testing fs\n");
    /* test_fs(); // Superblock dump and file test */

    printk("syncing fs\n");
    sync();

    terminal_initialize();
    terminal_writestring("Hello, Welcome To Yega Kernel!\n");

    printk("booted..................\n");

    printk("spawning kernel thread for fork test (process 0 -> process 1)...\n");
    int arg0 = 0;
    kernel_thread(test_thread, &arg0);
    printk("kernel threads created. entering loop.\n");

    /* Idle loop: scheduler will preempt this in favour of the threads */
    while (1) {
        asm volatile("hlt");
    }
}

