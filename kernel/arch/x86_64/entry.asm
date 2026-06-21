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
    mov esp, stack_top
    push eax
    push ebx
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
