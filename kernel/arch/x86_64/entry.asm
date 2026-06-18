[bits 32]
global _start
extern kernel_main

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000006
    dd -(0x1BADB002 + 0x00000006)
    dd 0, 0, 0, 0, 0
    dd 0
    dd 800
    dd 600
    dd 32

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
_start:
    ; eax = 0x2BADB002 (multiboot magic)
    ; ebx = multiboot info pointer
    ; Do NOT touch ebx or eax until after we set up stack

    mov esp, stack_top      ; set up stack first
    push eax                ; push magic   (2nd arg)
    push ebx                ; push mbi ptr (1st arg)
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
