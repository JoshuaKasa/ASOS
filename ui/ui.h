
#pragma once
#include <stdint.h>

#define UI_WIDTH  80
#define UI_HEIGHT 25

// VGA colors 
enum UIColor {
    UI_BLACK   = 0x0,
    UI_BLUE    = 0x1,
    UI_GREEN   = 0x2,
    UI_CYAN    = 0x3,
    UI_RED     = 0x4,
    UI_MAGENTA = 0x5,
    UI_BROWN   = 0x6,
    UI_LIGHT_GRAY  = 0x7,
    UI_DARK_GRAY   = 0x8,
    UI_LIGHT_BLUE  = 0x9,
    UI_LIGHT_GREEN = 0xA,
    UI_LIGHT_CYAN  = 0xB,
    UI_LIGHT_RED   = 0xC,
    UI_LIGHT_MAGENTA = 0xD,
    UI_YELLOW = 0xE,
    UI_WHITE  = 0xF,
};

void ui_clear(uint8_t color);
void ui_setcolor(uint8_t fg, uint8_t bg);
void ui_print(int x, int y, const char* str);
void ui_center_text(int y, const char* str);
void ui_box(int x, int y, int w, int h, uint8_t color);
void ui_set_cursor(int x, int y);
void ui_fill_rect(int x, int y, int w, int h, uint8_t color);
void ui_draw_frame(int x, int y, int w, int h, const char* title, uint8_t color);
void ui_printf(int x, int y, const char* fmt, ...);
