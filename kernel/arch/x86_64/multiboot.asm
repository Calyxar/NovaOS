[bits 32]

; Multiboot 1 header
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ (1<<1) | (1<<2)   ; memory map + video mode
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
    dd 0          ; header_addr   (unused)
    dd 0          ; load_addr     (unused)
    dd 0          ; load_end_addr (unused)
    dd 0          ; bss_end_addr  (unused)
    dd 0          ; entry_addr    (unused)
    dd 0          ; mode_type: 0=linear framebuffer
    dd 800        ; width
    dd 600        ; height
    dd 32         ; depth
