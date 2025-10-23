#include "vga.h"

#define VGA_MEMORY ((uint16_t*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static uint16_t* const vga = VGA_MEMORY;
static uint8_t color = 0x0F; // White on black
static int row = 0, col = 0;

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) 
        for (int x = 0; x < VGA_WIDTH; x++) 
            vga[y * VGA_WIDTH + x] = vga_entry(' ', color);
    row = col = 0;
}

void vga_putchar(char c) {
    if (c == '\n') {
        row++;
        col = 0;
        return;
    }
    else if (c == '\b') {
        vga_backspace();
        return;
    }

    vga[row * VGA_WIDTH + col] = vga_entry(c, color);
    if (++col >= VGA_WIDTH) {
        col = 0;
        row++;
    }
    if (row >= VGA_HEIGHT) {
        row = 0;
    }
}


void vga_write(const char *str) {
    for (; *str; ++str) 
        vga_putchar(*str);
}

void vga_backspace(void) {
    if (col > 0) {
        col--;
        vga[row * VGA_WIDTH + col] = vga_entry(' ', color);
    }
    else if (row > 0) {
        row--;
        col = VGA_WIDTH - 1;
        vga[row * VGA_WIDTH + col] = vga_entry(' ', color);
    }
}

void vga_putchar_at(int x, int y, char c, uint8_t color) {
    ((uint16_t*)0xB8000)[y * 80 + x] = c | (color << 8);
}

void vga_set_pos(int x, int y) {
    if (x < 0) x = 0; if (x >= VGA_WIDTH) x = VGA_WIDTH-1;
    if (y < 0) y = 0; if (y >= VGA_HEIGHT) y = VGA_HEIGHT-1;

    col = x; row = y;
}

void vga_set_color(uint8_t attr) {
    color = attr;
}
