#include "io.h"
#include "pic.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define PIC_EOI   0x20

void pic_send_eoi(uint8_t irq) {
	if (irq >= 8) // If it's sent from the slave
		outb(PIC2_CMD, PIC_EOI);
	outb(PIC1_CMD, PIC_EOI);
}

void pic_remap(uint8_t offset1, uint8_t offset2) {
	uint8_t a1 = inb(PIC1_DATA);
	uint8_t a2 = inb(PIC2_DATA);

	// Remap
	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

	// We need to remap our offsets cause we already used 0-31 for the IDT table
	outb(PIC1_DATA, offset1); // New master offset (0x20)
	outb(PIC2_DATA, offset2); // New slave offset (0x28)
				  //
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);

	// Reset original masks
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}
