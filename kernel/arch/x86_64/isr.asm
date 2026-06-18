[bits 32]

global isr_keyboard
global isr_timer
global isr_mouse

extern keyboard_handler
extern timer_handler
extern mouse_handler

isr_timer:
    pusha
    call timer_handler
    popa
    iret

isr_keyboard:
    pusha
    call keyboard_handler
    popa
    iret

isr_mouse:
    pusha
    call mouse_handler
    popa
    iret
