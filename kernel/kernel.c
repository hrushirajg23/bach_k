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
#include "irq.h"
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

#if defined(__linux__)
#error                                                                         \
    "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif




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

    printk("\nGDT init...\n");
    gdt_initialize();

    printk("PIC remap...\n");
    remap_pic(OFFSET1, OFFSET2);

    printk("loading tss\n");
    tss_load(0x28);
    sched_init();

    printk("IDT setup...\n");
    /* setup_idt(); */
    trap_init();

    printk("Install timer & keyboard drivers..\n");
    install_handlers();

    printk("PIT init...\n");
    init_timer(FREQUENCY);

    printk("IDT init...\n");
    initialize_idt();

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



    while (1) {
        asm volatile("hlt");
    }
}

