; Load DH sectors from drive DL into ES:BX
disk_load:
    pusha
    mov ah, 0x02
    mov al, dh
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    int 0x13
    jc disk_error
    popa
    ret
disk_error:
    mov si, DISK_ERR_MSG
    call print_string
    jmp $
DISK_ERR_MSG db "Disk error!", 0
