[bits 32]
global kernel_entry
extern kernel_main

section .text.startup

kernel_entry:
    cli
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x200000

    mov byte [0xB8008], 'K'
    mov byte [0xB8009], 0x0A

    call kernel_main

.hang:
    hlt
    jmp .hang
