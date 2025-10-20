#pragma once
#include <stdint.h>

typedef struct {
    uint32_t fb;
    uint16_t w, h; // Resolution
    uint16_t pitch; // Bytes per line
    uint8_t bpp;
} gfx_info_t;


int gfx_init(void);
void gfx_clear(uint32_t rgba);
void gfx_putpixel(int x, int y, uint32_t rgba);
void gfx_fillrect(int x,int y,int w,int h, uint32_t rgba);
void gfx_draw_char(int x,int y, char c, uint32_t fg, uint32_t bg);
void gfx_draw_text(int x,int y, const char* s, uint32_t fg, uint32_t bg);
uint32_t gfx_get_pixel(int x, int y);
const gfx_info_t* gfx_info(void);
