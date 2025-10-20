#pragma once
#include "isr.h"
#include <stdint.h>

void irq_install(void);
void register_interrupt_handler(uint8_t irq, void (*handler)(regs_t *r));
void irq_handler(regs_t *r);
