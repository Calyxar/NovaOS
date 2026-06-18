[bits 32]

global isr_keyboard
extern keyboard_handler

global isr_timer
extern timer_handler

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
