#pragma once
#include <stdint.h>

void console_init(int use_gfx);
void console_clear(void);
void console_write(const char* s);
void console_putchar(char c);
void console_setcolor(uint32_t fg, uint32_t bg);
void console_put_at(int x, int y, char c);
void console_setcursor(int x, int y);
void console_put_at_color(int x, int y, char c, uint8_t attr);
void console_setcursor(int x, int y);
void console_get_size(int* cols, int* rows);
