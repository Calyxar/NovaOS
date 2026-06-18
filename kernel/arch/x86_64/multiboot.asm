; NovaOS Multiboot Header
; GRUB requires this at the very start of the kernel
[bits 32]

MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x3
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
