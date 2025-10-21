#include "console.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "pic.h"
#include "syscall.h"
#include "keyboard.h"
#include "asofs.h"
#include "gfx.h"
#include "io.h"
#include "mouse.h"
#include "pit.h"
#include "../lib/stdlib.h"
#include "../lib/string.h"

volatile unsigned int g_ticks = 0;
static int s_inited = 0;

void timer_handler(regs_t* r) {
    (void)r;
    g_ticks++;
    mouse_on_timer_tick();
}

void kernel_run_shell_loop(void) {
    for (;;) {
        console_clear();
        asofs_run_app("terminal.bin");
        console_write("[KERNEL] App terminated. Restarting terminal...\n");
    }
}

void kernel_main(void) {
    asm volatile("cli");

    int use_gfx = 0;

    if (!s_inited) {
        use_gfx = (gfx_init() == 0);
        if (use_gfx) gfx_clear(0x00000000);
        console_init(use_gfx);
        console_write("Console ready.\n");

        console_write("Installing IDT...\n");
        idt_install();
        console_write("IDT installed!\n");

        console_write("Installing ISR...\n");
        isr_install();
        console_write("ISR installed!\n");

        console_write("Remapping PIC...\n");
        pic_remap(0x20, 0x28);
        console_write("PIC remapped!\n");

        console_write("Installing IRQ...\n");
        irq_install();
        console_write("IRQ installed!\n");

        register_interrupt_handler(0, timer_handler);
        pit_init(100);

        uint8_t master = inb(0x21);
        uint8_t slave  = inb(0xA1);
        master &= ~((1<<0) | (1<<1)); // IRQ0 (PIT) and IRQ1 (KBD) enabled
        slave  |=  (1<<6); // IRQ14 (IDE) ALWAYS enabled
        outb(0x21, master);
        outb(0xA1, slave);

        console_write("Installing keyboard drivers...\n");
        kbd_install();
        console_write("Keyboard drivers installed!\n");

        console_write("Installing syscalls...\n");
        syscall_init();
        console_write("Syscalls ready!\n");

        s_inited = 1;

        console_write("\nEverything loaded!\n");
        console_write("Enabling interrupts...\n");
        asm volatile("sti");
        console_write("Interrupts enabled!\n");

        console_write("\n[KERNEL] Loading file system...\n");
        if (asofs_load_superblock() != 0) {
            console_write("[KERNEL] Filesystem not available. Halting.\n");
            for(;;) asm volatile("hlt");
        }
        console_write("[KERNEL] Launching terminal: terminal.bin\n");
    } 
    else {
        // Init already one
        asm volatile("sti");
        console_write("[KERNEL] Return from app.\n");
    }

    kernel_run_shell_loop();
}

