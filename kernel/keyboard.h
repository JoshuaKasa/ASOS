#pragma once
#include "isr.h"
#include <stdint.h>

#define KEY_LEFT 0x90
#define KEY_RIGHT 0x91
#define KEY_UP 0x92
#define KEY_DOWN 0x93

void kbd_handler(regs_t *r);
void kbd_install(void);
int kbd_available(void);
char kbd_getchar(void);
void kbd_readline(char *buf, int max_len);
