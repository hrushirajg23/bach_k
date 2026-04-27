# ====================================
# Bach Kernel Kernel Build System
# ====================================

# === Toolchain ===
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# === Directories ===
BUILD_DIR = build
ISO_DIR = iso
INCLUDE_DIR = include

# === Compiler Flags ===
CFLAGS = -ffreestanding -O0 -g -nostdlib -I$(INCLUDE_DIR)
ASFLAGS = -f elf32
LDFLAGS = -T linker/linker.ld

# === Source Directories ===
# List all directories containing source files here
SRC_DIRS := \
    kernel \
    memory-management \
    drivers \
	fs/ext2 \
	fs \
    kernel/gdt \
    kernel/idt \
    kernel/idt/isr \
    kernel/idt/irq \
    kernel/idt/pic \
    kernel/display \
    drivers/pit \
    data_structures \
    boot

# === Automatic Source Discovery ===
# Find all .c and .asm files in source directories
C_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.asm))
S_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))
SLC_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.s))

# === Object Files ===
# Flatten paths but keep distinct names:
# foo.c   -> build/foo.o
# bar.asm -> build/bar_asm.o  (Prevents collisions like idt.c vs idt.asm)
C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SRCS)))
ASM_OBJS := $(patsubst %.asm,$(BUILD_DIR)/%_asm.o,$(notdir $(ASM_SRCS)))
S_OBJS := $(patsubst %.S,$(BUILD_DIR)/%_S.o,$(notdir $(S_SRCS)))
SLC_OBJS := $(patsubst %.s,$(BUILD_DIR)/%_s.o,$(notdir $(SLC_SRCS)))

OBJS := $(ASM_OBJS) $(C_OBJS) $(S_OBJS) $(SLC_OBJS)

# === VPATH ===
# Tell make where to look for source files
vpath %.c $(SRC_DIRS)
vpath %.asm $(SRC_DIRS)
vpath %.S $(SRC_DIRS)
vpath %.s $(SRC_DIRS)

# === Targets ===
.PHONY: all clean run debug

all: $(BUILD_DIR)/bach_k.iso

# === Build Rules ===

# Setup Directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

# Compile C files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files (note the _asm.o suffix)
$(BUILD_DIR)/%_asm.o: %.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Assemble .S files (capital S — passed through C preprocessor)
$(BUILD_DIR)/%_S.o: %.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble .s files (lowercase — raw GAS, no C preprocessor)
$(BUILD_DIR)/%_s.o: %.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -x assembler -c $< -o $@

# Link Kernel
$(BUILD_DIR)/bach_k.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) $^
	@echo "[OK] Linked ELF binary"

# Create Binary
$(BUILD_DIR)/bach_k.bin: $(BUILD_DIR)/bach_k.elf
	$(OBJCOPY) -O binary $< $@
	@echo "[OK] Created flat binary"

# Create ISO
$(BUILD_DIR)/bach_k.iso: $(BUILD_DIR)/bach_k.elf $(BUILD_DIR)/bach_k.bin | $(ISO_DIR)/boot/grub
	cp $(BUILD_DIR)/bach_k.elf  $(ISO_DIR)/boot/bach_k.elf
	cp $(BUILD_DIR)/bach_k.bin  $(ISO_DIR)/boot/bach_k.bin
	cp grub/grub.cfg           $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $@ $(ISO_DIR) > /dev/null 2>&1
	@grub-file --is-x86-multiboot $(BUILD_DIR)/bach_k.elf && echo "[OK] Multiboot compliant" || echo "[ERROR] Not multiboot compliant"

# === Helpers ===

run: $(BUILD_DIR)/bach_k.iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/bach_k.iso -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR)
	find . -type f -name '*~' -delete
	@echo "[OK] Cleaned build artifacts"

debug:
	@echo "Source Dirs: $(SRC_DIRS)"
	@echo "C Components: $(notdir $(C_SRCS))"
	@echo "ASM Components: $(notdir $(ASM_SRCS))"
	@echo "Objects: $(notdir $(OBJS))"
