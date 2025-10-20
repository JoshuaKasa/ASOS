#pragma once
#include <stdint.h>

void ui_gfx_init(void);
void ui_gfx_clear(uint32_t color);
void ui_gfx_put_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void ui_gfx_put_text(int x, int y, const char* s, uint32_t fg, uint32_t bg);
void ui_gfx_printf(int x, int y, const char* fmt, ...);
void ui_gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
