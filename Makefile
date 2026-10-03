# =============================================================
# NovaOS Master Makefile
# =============================================================

PROJECT := NovaOS
VERSION := 0.1.0

# -------------------------------------------------------------
# Directories
# -------------------------------------------------------------

BUILD_DIR  := build
ISO_DIR    := $(BUILD_DIR)/iso

# Persistent OS data must live OUTSIDE build/.
DATA_DIR   := data
BACKUP_DIR := backups

# -------------------------------------------------------------
# Toolchain
# -------------------------------------------------------------

CC  := i686-elf-gcc
CXX := i686-elf-g++
ASM := nasm
LD  := i686-elf-ld

CFLAGS := -std=c11 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti

CXXFLAGS := -std=c++17 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-use-cxa-atexit

LDFLAGS := -T kernel/arch/x86_64/linker.ld -ffreestanding -O2 -nostdlib -lgcc

INCLUDES := -Ikernel -Ilibs/libk -Iui

# -------------------------------------------------------------
# Source discovery
# -------------------------------------------------------------

KERNEL_CPP := $(shell find kernel libs/libk shell ui -name "*.cpp")

KERNEL_C := $(shell find kernel libs/libk -name "*.c")

KERNEL_ASM := $(shell find kernel -name "*.asm" | grep -v multiboot.asm)

# -------------------------------------------------------------
# Objects
# -------------------------------------------------------------

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CPP))
OBJS += $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C))
OBJS += $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM))

BOOT_OBJ   := $(BUILD_DIR)/boot/boot.o
KERNEL_BIN := $(BUILD_DIR)/NovaOS.bin
ISO_FILE   := $(BUILD_DIR)/NovaOS.iso

# -------------------------------------------------------------
# Persistent virtual hard drive
# -------------------------------------------------------------

DISK_IMG := $(DATA_DIR)/novadisk.img

# Previous location, used for automatic migration.
OLD_DISK_IMG := $(BUILD_DIR)/novadisk.img

# -------------------------------------------------------------
# Phony targets
# -------------------------------------------------------------

.PHONY: all kernel iso run run-kvm
.PHONY: clean reset-disk backup-disk disk

# =============================================================
# Default build
# =============================================================

all: kernel iso

kernel: $(KERNEL_BIN)

# =============================================================
# Kernel linking
# =============================================================

$(KERNEL_BIN): $(OBJS) $(BOOT_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)
	@echo "  [LD] $@"

# =============================================================
# Bootloader — raw binary
# =============================================================

$(BUILD_DIR)/boot/boot.o: boot/boot.asm
	@mkdir -p $(BUILD_DIR)/boot
	$(ASM) -f bin $< -o $@
	@echo "  [ASM-BIN] $<"

# =============================================================
# Kernel assembly — ELF32
# =============================================================

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 $< -o $@
	@echo "  [ASM] $<"

# =============================================================
# C++ compilation
# =============================================================

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  [CXX] $<"

# =============================================================
# C compilation
# =============================================================

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  [CC] $<"

# =============================================================
# ISO generation
# =============================================================

iso: kernel
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	cp tools/grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
	@echo "  [ISO] $(ISO_FILE)"

# =============================================================
# Virtual disk creation / migration
# =============================================================
#
# If an old disk exists inside build/, copy it into data/.
#
# Otherwise, create a new 64 MB disk.
#
# This rule DOES NOT overwrite an existing persistent disk.
# =============================================================

$(DISK_IMG):
	@mkdir -p $(DATA_DIR)
	@if [ -f "$(OLD_DISK_IMG)" ]; then \
		echo "  [DISK] Migrating existing NovaOS disk..."; \
		cp "$(OLD_DISK_IMG)" "$(DISK_IMG)"; \
	else \
		echo "  [DISK] Creating new 64 MB NovaOS disk..."; \
		dd if=/dev/zero of="$(DISK_IMG)" bs=1M count=64; \
	fi
	@echo "  [DISK] Persistent disk ready: $(DISK_IMG)"

disk: $(DISK_IMG)
	@echo "  [DISK] $(DISK_IMG)"

# =============================================================
# Run NovaOS in QEMU
# =============================================================

run: iso $(DISK_IMG)
	qemu-system-i386 \
		-cdrom $(ISO_FILE) \
		-drive file=$(DISK_IMG),format=raw \
		-m 256M \
		-display sdl,show-cursor=on

# =============================================================
# Run using x86_64 QEMU + KVM
# =============================================================

run-kvm: iso $(DISK_IMG)
	qemu-system-x86_64 \
		-cdrom $(ISO_FILE) \
		-drive file=$(DISK_IMG),format=raw \
		-m 512M \
		-enable-kvm \
		-cpu host \
		-smp 4

# =============================================================
# Clean build artifacts
# =============================================================
#
# IMPORTANT:
# Preserve the virtual hard drive.
#
# If the disk still lives inside build/, migrate it first.
# =============================================================

clean:
	@echo "  [CLEAN] Preparing to remove build artifacts..."
	@mkdir -p $(DATA_DIR)
	@if [ ! -f "$(DISK_IMG)" ] && [ -f "$(OLD_DISK_IMG)" ]; then \
		echo "  [DISK] Preserving legacy disk..."; \
		cp "$(OLD_DISK_IMG)" "$(DISK_IMG)"; \
	fi
	rm -rf $(BUILD_DIR)
	@echo "  [CLEAN] Build files removed."
	@echo "  [SAFE] Persistent NovaOS data preserved."

# =============================================================
# Back up persistent disk
# =============================================================
#
# Shut down QEMU before creating a backup.
# =============================================================

backup-disk: $(DISK_IMG)
	@mkdir -p $(BACKUP_DIR)
	@timestamp=$$(date +%Y%m%d_%H%M%S); \
	backup="$(BACKUP_DIR)/novadisk_$$timestamp.img"; \
	if [ -e "$$backup" ]; then \
		echo "  [ERROR] Backup filename already exists."; \
		exit 1; \
	fi; \
	cp "$(DISK_IMG)" "$$backup" || exit 1; \
	echo "  [BACKUP] $$backup"

# =============================================================
# Intentionally reset the virtual disk
# =============================================================
#
# This is the ONLY target intended to erase NovaOS data.
# Requires explicit confirmation.
#
# Shut down QEMU before resetting the disk.
# =============================================================

reset-disk:
	@echo ""
	@echo "============================================="
	@echo " WARNING: NOVAOS DISK RESET"
	@echo "============================================="
	@echo "This will permanently erase:"
	@echo "$(DISK_IMG)"
	@echo ""
	@printf "Type RESET to continue: "
	@read confirmation; \
	if [ "$$confirmation" != "RESET" ]; then \
		echo "  [CANCELLED] Disk was not modified."; \
		exit 1; \
	fi; \
	mkdir -p "$(DATA_DIR)" || exit 1; \
	dd if=/dev/zero of="$(DISK_IMG)" bs=1M count=64 || exit 1; \
	echo "  [RESET] Fresh NovaOS disk created."