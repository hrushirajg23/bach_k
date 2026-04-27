# Bach Kernel (bach_k)

> I started this project to understand memory allocators — it ended up growing into a kernel.

## Table of Contents

- [Overview & Motive](#overview--motive)
- [Current State](#current-state)
- [Project Structure](#project-structure)
- [How It Works](#how-it-works)
- [Requirements](#requirements)
- [Build & Run](#build--run)
- [Debugging](#debugging)
- [What's Next](#whats-next)
- [References](#references)
- [License](#license)

---

## Overview & Motive

I wrote a kernel for x86 machines, studied heavily from and hence named after Maurice J. Bach's *The Design of the UNIX Operating System* — **bach_k**, Bach's kernel. Other key references include Daniel Bovet and Marco Cesati's *Understanding the Linux Kernel*, Andrew S. Tanenbaum's *Modern Operating Systems*, and *The Magic Garden Explained* by Berny Goodheart.

My initial motive was simply to understand memory allocators. However, the scope naturally expanded, and it ended up growing into a fully functioning kernel. Memory management remains the main focus, but the architecture has been laid out to support a variety of OS subsystems.

---

## Current State

So far, the kernel includes:

- **Memory management** (buddy allocator, slab allocator, virtual memory paging, etc.)
- **File systems:** Unix System V Release 2 file system and ext2 file system
- **Drivers:** Serial and block device drivers
- **Task management:** Kernel threads, context switching (inspired by Bach's Chapter 6), and interrupt handling
- **User Interface:** Terminal emulator :)

---

## Project Structure

```text
.
├── boot/               # Bootloader code (Multiboot header)
├── build/              # Compiled objects and ISO
├── data_structures/    # Reusable kernel data structures
├── docs/               # Documentation and diagrams
├── drivers/            # Device drivers (Keyboard, Timer, Serial)
├── fs/                 # File system implementations (VFS, ext2, sysv, buffers)
├── grub/               # GRUB configuration
├── include/            # Header files
├── iso/                # ISO build directory for GRUB boot
├── kernel/             # Core kernel logic, tasks, syscalls, shell
│   ├── display/        # VGA text mode output (Terminal emulator)
│   ├── gdt/            # Global Descriptor Table and TSS setup
│   └── idt/            # Interrupt Descriptor Table and IRQs
├── linker/             # Custom linker script
├── memory-management/  # Buddy and Slab allocators, paging
└── Makefile            # Build automation system
```

---

---

## Requirements

To build and run the Bach Kernel, you will need the following tools:

- `i686-elf-gcc` (Cross-compiler)
- `nasm` (Assembler)
- `make` (Build system)
- `qemu-system-i386` or `qemu-system-x86_64` (Emulator)
- `gdb-multiarch` (Optional, for debugging)

---

## Build & Run

### Building the OS

Compile the source and generate the bootable ISO:

```bash
make
```

This generates:
- `build/bach_k.elf` - Kernel ELF binary
- `build/bach_k.iso` - Bootable ISO image

### Running in QEMU

**Standard graphical mode:**
```bash
qemu-system-i386 -cdrom build/bach_k.iso
```

**Non-graphical with serial output:**
```bash
qemu-system-i386 -cdrom build/bach_k.iso -nographic -serial mon:stdio
```

**For older machine profiles (e.g., to inspect legacy PICs):**
```bash
qemu-system-x86_64 -cdrom build/bach_k.iso -serial stdio -machine isapc
```

**To log serial output to a file:**
```bash
qemu-system-x86_64 -cdrom build/bach_k.iso -serial stdio -machine isapc > qemu.log 2>&1
```

---

## Debugging

Run QEMU in debug mode (halts execution and waits for GDB):
```bash
qemu-system-i386 -cdrom build/bach_k.iso -nographic -serial mon:stdio -s -S
```

Attach GDB in a separate terminal:
```bash
gdb-multiarch build/bach_k.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

---

## What's Next

I plan to extend it further with features like:
- Signal handling
- `exec` syscall
- Loadable kernel modules
- Basic networking stack
- And more...

---

---

## References

- *The Design of the UNIX Operating System* by Maurice J. Bach
- *Understanding the Linux Kernel* (UTLK) by Daniel P. Bovet & Marco Cesati
- *The Magic Garden Explained: The Internals of UNIX System V Release 4* by Berny Goodheart & James Cox
- *Modern Operating Systems* by Andrew S. Tanenbaum
- [OSDev Wiki](https://wiki.osdev.org)
- [Bran's Kernel Development Tutorial](https://web.archive.org/web/20130905193045/http://www.osdever.net/tutorials/view/brans-kernel-development-tutorial)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.txt)

---

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
