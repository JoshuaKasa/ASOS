[BITS 32]
global _start
extern main

_start:
    ; stack? use the kernel one, good for now 
    call main

    ; if main returns, exit 
    mov eax, 2            ; SYSCALL_EXIT
    int 0x80

.hang:
    hlt
    jmp .hang
