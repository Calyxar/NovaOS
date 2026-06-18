; NovaOS Bootloader Stage 1
[bits 16]
[org 0x7C00]

KERNEL_OFFSET equ 0x1000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [BOOT_DRIVE], dl
    mov si, MSG_BOOT
    call print_string
    call load_kernel
    call switch_to_pm
    jmp $

%include "boot/print.asm"
%include "boot/disk.asm"
%include "boot/gdt.asm"
%include "boot/pm_switch.asm"

load_kernel:
    mov bx, KERNEL_OFFSET
    mov dh, 15
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

[bits 32]
BEGIN_PM:
    mov ebp, 0x90000
    mov esp, ebp
    call KERNEL_OFFSET
    jmp $

MSG_BOOT   db "NovaOS booting...", 0x0D, 0x0A, 0
BOOT_DRIVE db 0

times 510-($-$$) db 0
dw 0xAA55
