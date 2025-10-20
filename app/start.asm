[BITS 32]
global _start
extern main

_start:
    ; stack? usi quello del kernel: va bene per ora
    call main

    ; se main ritorna, fai una exit di cortesia
    mov eax, 2            ; SYSCALL_EXIT
    int 0x80

.hang:
    hlt
    jmp .hang
