#pragma once
#include <stdint.h>

void vga_clear(void);
void vga_backspace(void);
void vga_putchar(char c);
void vga_putchar_at(int x, int y, char c, uint8_t color);
void vga_write(const char* s);
void vga_set_pos(int x, int y);
void vga_set_color(uint8_t attr);
