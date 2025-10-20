; Stage-2 Bootloader
; - Sets a VBE LFB graphics mode only if it is 32 bpp (otherwise stays in text mode).
; - Saves VBE ModeInfo at 0x00080000 and copies BIOS VGA 8x16 font to 0x00080100.
; - Loads the kernel from disk via INT 13h extensions.
; - Enters protected mode and jumps to the kernel at 0x00100000.

org 0x7E00
bits 16

jmp start

%ifndef KERNEL_SECTORS
    ; Fallback default if the Makefile did not -D inject the real value.
    KERNEL_SECTORS equ 17
%endif

KERNEL_LBA       equ 5

; Choose a VBE mode that your BIOS supports with 32 bpp.
%ifndef VBE_MODE
    VBE_MODE      equ 0x143     ; Example: 1024x768x32 (adjust if needed)
%endif

; Linear framebuffer bit for SetMode (bit 14).
VBE_LFB_FLAG     equ 0x4000

; Where we store VBE ModeInfo (512 bytes block) and the copied VGA font.
MODEINFO_SEG     equ 0x8000
MODEINFO_OFF     equ 0x0000                ; 0x00080000 physical
FONT_OFF         equ 0x0100                ; 0x00080100 physical
; Physical addresses for reference:
;   ModeInfo  = 0x00080000
;   VGA 8x16  = 0x00080100 (4096 bytes)

; BSS / data
boot_drive db 0

; Disk Address Packet for INT 13h AH=42h
kernel_dap:
    db 0x10                 ; Size of DAP (16 bytes)
    db 0x00
    dw KERNEL_SECTORS       ; Number of sectors to read
    dw 0x0000               ; Offset buffer (0x9000:0000 -> 0x00090000)
    dw 0x9000               ; Segment buffer
    dd KERNEL_LBA           ; Starting LBA
    dd 0x00000000           ; Upper 32 bits of LBA (unused)

; GDT (flat 32-bit)
gdt_start:
    dq 0x0000000000000000       ; Null
    dq 0x00CF9A000000FFFF       ; Code 0x08: base=0, limit=4GB, rx
    dq 0x00CF92000000FFFF       ; Data 0x10: base=0, limit=4GB, rw
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Code — real mode
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    mov [boot_drive], dl

    ; Enable A20 via BIOS (INT 15h, AX=2401).
    mov ax, 0x2401
    int 0x15
    jc disk_error

    ; Query VBE ModeInfo into 0x00080000 (ES:DI).
    mov ax, MODEINFO_SEG
    mov es, ax
    mov di, MODEINFO_OFF
    mov ax, 0x4F01              ; VBE: Get ModeInfo
    mov cx, VBE_MODE
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; Check BitsPerPixel == 32 in the returned ModeInfo.
    mov ax, MODEINFO_SEG
    mov es, ax
    mov si, MODEINFO_OFF
    mov al, [es:si + 0x19]      ; BitsPerPixel offset
    cmp al, 32
    jne .skip_vbe_set

    ; Set VBE mode with LFB (BX = mode | 0x4000).
    mov ax, 0x4F02              ; VBE: Set Mode
    mov bx, VBE_MODE | VBE_LFB_FLAG
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

.skip_vbe_set:

    ; Fetch BIOS VGA 8x16 font: INT 10h AX=1130h, BH=06h → ES:BP points to glyphs.
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10

    ; Copy the font to 0x00080100 (4096 bytes).
    push ds
    push es

    mov ax, es                  ; ES:BP is the source
    mov ds, ax
    mov si, bp

    mov ax, MODEINFO_SEG
    mov es, ax
    mov di, FONT_OFF            ; ES:DI = 0x0008:0x0100 (dest)

    mov cx, 4096/2              ; Copy 4096 bytes (words)
    rep movsw

    pop es
    pop ds

    ; Read the kernel from disk using INT 13h extensions (AH=42h) into 0x00090000.
    mov si, kernel_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Enter protected mode.
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

; Code — protected mode
[bits 32]
pm_entry:
    ; Load flat segments.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x80000

    ; Copy kernel from 0x00090000 → 0x00100000.
    mov esi, 0x00090000
    mov edi, 0x00100000
    mov ecx, (KERNEL_SECTORS * 512) / 4
    cld
    rep movsd

    ; Jump to kernel entry (flat 32-bit code segment 0x08).
    jmp 0x08:0x00100000

; Error handlers (real mode)
[bits 16]
disk_error:
    cli
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    hlt

vbe_error:
    cli
    mov ah, 0x0E
    mov al, 'V'
    int 0x10
    hlt
