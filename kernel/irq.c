#include <stdint.h>
#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "console.h"
#include "io.h"

extern void irq0();   extern void irq1();   extern void irq2();   extern void irq3();
extern void irq4();   extern void irq5();   extern void irq6();   extern void irq7();
extern void irq8();   extern void irq9();   extern void irq10();  extern void irq11();
extern void irq12();  extern void irq13();  extern void irq14();  extern void irq15();

void (*interrupt_handlers[16])(regs_t *r); // Holds various IRQ (0-15)

void irq_install(void) {
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // NOTE: we can disable IRQ14 (IDE) cause we use polling, not interrupt
    outb(0xA1, inb(0xA1) | (1 << 6));
}

// Registers custom handler for a IRQ
void register_interrupt_handler(uint8_t irq, void (*handler)(regs_t *r)) {
    interrupt_handlers[irq] = handler;
}

void irq_handler(regs_t *r) {
    uint8_t irq = r->int_no - 32; // 32-47 -> IRQ 0-15

    if (interrupt_handlers[irq]) {
        interrupt_handlers[irq](r); // Call real handler
    }
    else {
        console_write("Unhandled IRQ: ");

        char buff[16];
        extern char *itoa(int, char*, int);

        console_write(itoa(irq, buff, 10));
        console_write("\n");
    }

    // Send EOI to the PIC (end of interrupt)
    pic_send_eoi(irq);
}
