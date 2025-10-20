org 0x7C00
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sub sp, 0x200
    sti

    mov [boot_drive], dl

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x0000:0x7E00

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    hlt

dap:
    db 0x10
    db 0x00
    dw 4
    dw 0x7E00
    dw 0x0000
    dd 1
    dd 0x00000000

boot_drive: db 0

times 510-($-$$) db 0
dw 0xAA55
