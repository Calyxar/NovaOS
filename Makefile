# NovaOS Master Makefile

PROJECT   := NovaOS
VERSION   := 0.1.0
BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso

CC      := i686-elf-gcc
CXX     := i686-elf-g++
ASM     := nasm
LD      := i686-elf-ld

CFLAGS   := -std=c11 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
CXXFLAGS := -std=c++17 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS  := -T kernel/arch/x86_64/linker.ld -ffreestanding -O2 -nostdlib -lgcc

INCLUDES := -Ikernel -Ilibs/libk

KERNEL_CPP := $(shell find kernel libs/libk shell -name "*.cpp")
KERNEL_C   := $(shell find kernel libs/libk -name "*.c")
KERNEL_ASM := $(shell find kernel -name "*.asm" | grep -v multiboot.asm)

OBJS := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(KERNEL_CPP))
OBJS += $(patsubst %.c,   $(BUILD_DIR)/%.o, $(KERNEL_C))
OBJS += $(patsubst %.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM))

BOOT_OBJ   := $(BUILD_DIR)/boot/boot.o
KERNEL_BIN := $(BUILD_DIR)/NovaOS.bin
ISO_FILE   := $(BUILD_DIR)/NovaOS.iso

.PHONY: all kernel iso run run-kvm clean

all: kernel iso

kernel: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJS) $(BOOT_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)
	@echo "  [LD] $@"

# Bootloader — raw binary, NOT elf
$(BUILD_DIR)/boot/boot.o: boot/boot.asm
	@mkdir -p $(BUILD_DIR)/boot
	$(ASM) -f bin $< -o $@
	@echo "  [ASM-BIN] $<"

# Kernel ASM — elf32
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 $< -o $@
	@echo "  [ASM] $<"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  [CXX] $<"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  [CC] $<"

iso: kernel
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	cp tools/grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
	@echo "  [ISO] $(ISO_FILE)"

run: iso
	qemu-system-i386 -cdrom $(ISO_FILE) -m 256M -display sdl,show-cursor=on

run-kvm: iso
	qemu-system-x86_64 -cdrom $(ISO_FILE) -m 512M -enable-kvm -cpu host -smp 4

clean:
	rm -rf $(BUILD_DIR)
