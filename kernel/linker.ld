ENTRY(kernel_entry)

SECTIONS {
  . = 0x00100000;

  .text : {
    KEEP(*(.text.startup))
    *(.text .text.*)
  }

  .rodata : { *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) }
  .bss    : { *(.bss .bss.* COMMON) . = ALIGN(16); }
}

