#include "ui_gfx.h"
#include "../kernel/gfx.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include <stdarg.h>

static uint32_t fg_col = 0xFFFFFFFF;
static uint32_t bg_col = 0x00000000;

#define CHAR_W 8
#define CHAR_H 16

void ui_gfx_init(void) {
    gfx_init();
    gfx_clear(bg_col);
}

void ui_gfx_clear(uint32_t color) {
    bg_col = color;
    gfx_clear(color);
}

void ui_gfx_put_char(int cx, int cy, char c, uint32_t fg, uint32_t bg) {
    gfx_draw_char(cx * CHAR_W, cy * CHAR_H, (unsigned char)c, fg, bg);
}

void ui_gfx_put_text(int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    for (int i = 0; s[i]; i++)
        ui_gfx_put_char(x + i, y, s[i], fg, bg);
}

void ui_gfx_printf(int x, int y, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    char* out = buf;

    for (const char* p = fmt; *p; ++p) {
        if (*p == '%' && *(p+1)) {
            ++p;
            if (*p == 'd') { int v = va_arg(ap,int); char t[32]; itoa(v,t,10); strcpy(out,t); out += strlen(t); }
            else if (*p == 's') { char* str = va_arg(ap,char*); strcpy(out,str); out += strlen(str); }
            else *out++ = *p;
        } else *out++ = *p;
    }
    *out = 0; va_end(ap);

    ui_gfx_put_text(x, y, buf, fg_col, bg_col);
}

void ui_gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    gfx_fillrect(x, y, w, h, color);
}
