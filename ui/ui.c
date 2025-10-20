#include "ui.h"
#include "../kernel/vga.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include <stdarg.h>

static uint8_t current_color = 0x0F;

void ui_clear(uint8_t color) {
    uint8_t old = current_color;

    current_color = color;
    vga_clear();
    current_color = old;
}

void ui_setcolor(uint8_t fg, uint8_t bg) {
    current_color = (bg << 4) | (fg & 0x0F);
}

void ui_print(int x, int y, const char* str) {
    int i = 0;

    while (str[i]) {
        vga_putchar_at(x + i, y, str[i], current_color);
        i++;
    }
}

void ui_center_text(int y, const char* str) {
    int len = strlen(str);
    int x = (UI_WIDTH - len) / 2;

    ui_print(x, y, str);
}

void ui_box(int x, int y, int w, int h, uint8_t color) {
    for (int i = 0; i < w; i++) {
        vga_putchar_at(x + i, y, '-', color);
        vga_putchar_at(x + i, y + h - 1, '-', color);
    }

    for (int j = 0; j < h; j++) {
        vga_putchar_at(x, y + j, '|', color);
        vga_putchar_at(x + w - 1, y + j, '|', color);
    }

    vga_putchar_at(x, y, '+', color);
    vga_putchar_at(x + w - 1, y, '+', color);
    vga_putchar_at(x, y + h - 1, '+', color);
    vga_putchar_at(x + w - 1, y + h - 1, '+', color);
}

void ui_fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            vga_putchar_at(x + i, y + j, ' ', color);
        }
    }
}

void ui_draw_frame(int x, int y, int w, int h, const char* title, uint8_t color) {
    ui_box(x, y, w, h, color);

    if (title && *title) {
        int len = strlen(title);
        int tx = x + (w - len) / 2;

        ui_print(tx, y, title);
    }
}

void ui_printf(int x, int y, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);

    char* out = buffer;
    for (const char* p = fmt; *p; ++p) {
        if (*p == '%' && *(p + 1)) {
            p++;

            if (*p == 'd') {
                int val = va_arg(args, int);
                char tmp[32];

                itoa(val, tmp, 10);
                strcpy(out, tmp);
                out += strlen(tmp);
            } 
            else if (*p == 's') {
                char* s = va_arg(args, char*);
                strcpy(out, s);
                out += strlen(s);
            } 
            else {
                *out++ = *p;
            }
        } 
        else {
            *out++ = *p;
        }
    }

    *out = '\0';
    va_end(args);

    ui_print(x, y, buffer);
}

