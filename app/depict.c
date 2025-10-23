#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/gfx2d.h"
#include "asoapi.h"

#define MAX_W 1280
#define MAX_H 1024
static unsigned int backbuf[MAX_W * MAX_H];
static gfx2d_surface_t surface;
static int scr_w = 0, scr_h = 0;

static unsigned brush_size = 12;
static const unsigned palette[] = {
    GFX2D_RGB(236, 64, 64),  GFX2D_RGB(66, 165, 245), GFX2D_RGB(102, 187, 106), GFX2D_RGB(255, 214, 0),
    GFX2D_RGB(171, 71, 188), GFX2D_RGB(255, 138, 101), GFX2D_RGB(0, 188, 212),  GFX2D_RGB(255, 255, 255)
};
static const int palette_count = (int)(sizeof(palette) / sizeof(palette[0]));
static int color_index = 0;
static unsigned current_color = 0;
static const unsigned background_color = GFX2D_RGB(18, 20, 24);

static inline int clamp_coord(int v, int max) {
    if (max <= 0)
        return 0;
    if (v < 0) return 0;
    if (v >= max) return max - 1;
    return v;
}

static void clear_canvas(void) {
    gfx2d_surface_clear(&surface, background_color);
}

static void draw_brush_square(int cx, int cy, unsigned rgb) {
    int size = (int)brush_size;
    if (size < 1)
        size = 1;
    int half = size / 2;
    int x0 = cx - half;
    int y0 = cy - half;
    gfx2d_fill_rect(&surface, x0, y0, size, size, rgb);
}

static void draw_line_brush(int x0, int y0, int x1, int y1, unsigned rgb) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        draw_brush_square(x0, y0, rgb);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void color_to_hex(unsigned rgb, char* out) {
    static const char hx[] = "0123456789ABCDEF";
    unsigned r = (rgb >> 16) & 0xFF;
    unsigned g = (rgb >> 8) & 0xFF;
    unsigned b = rgb & 0xFF;
    out[0] = hx[(r >> 4) & 0xF];
    out[1] = hx[r & 0xF];
    out[2] = hx[(g >> 4) & 0xF];
    out[3] = hx[g & 0xF];
    out[4] = hx[(b >> 4) & 0xF];
    out[5] = hx[b & 0xF];
    out[6] = '\0';
}

static void draw_palette_preview(void) {
    int x0 = 16;
    int y0 = 56;
    int sw = 36;
    int sh = 28;
    for (int i = 0; i < palette_count; ++i) {
        int px = x0 + i * (sw + 10);
        gfx2d_fill_rect(&surface, px, y0, sw, sh, palette[i]);
        unsigned border = (i == color_index) ? GFX2D_RGB(255, 255, 255) : GFX2D_RGB(40, 40, 40);
        gfx2d_stroke_rect(&surface, px - 1, y0 - 1, sw + 2, sh + 2, border);
    }
    int bx = x0 + palette_count * (sw + 10) + 32;
    int by = y0 + sh / 2;
    gfx2d_fill_rect(&surface, bx - 40, y0 - 6, 80, sh + 12, GFX2D_RGB(28, 32, 38));
    gfx2d_stroke_rect(&surface, bx - 40, y0 - 6, 80, sh + 12, GFX2D_RGB(70, 76, 84));
    int preview_size = (brush_size > 48) ? 48 : (int)brush_size;
    if (preview_size < 6)
        preview_size = 6;
    int half = preview_size / 2;
    gfx2d_fill_rect(&surface, bx - half, by - half, preview_size, preview_size, current_color);
    gfx2d_stroke_rect(&surface, bx - half, by - half, preview_size, preview_size, GFX2D_RGB(255, 255, 255));
}

static void draw_hud(void) {
    int panel_h = 48;
    if (panel_h > scr_h)
        panel_h = scr_h;
    gfx2d_fill_rect(&surface, 0, 0, scr_w, panel_h, GFX2D_RGB(18, 20, 24));
    gfx2d_stroke_rect(&surface, 0, 0, scr_w, panel_h, GFX2D_RGB(70, 76, 84));

    const char* banner = "Depict paint  |  Left: draw   Right: next color   Middle: erase   [C] Clear   [+/-] Size   [Q] Quit";
    gfx2d_draw_text(&surface, 16, 12, GFX2D_RGB(220, 220, 220), banner);

    char line[128];
    char size_buf[16];
    itoa((int)brush_size, size_buf, 10);
    char hex[8];
    color_to_hex(current_color, hex);
    line[0] = '\0';
    strcpy(line, "Brush: ");
    strcat(line, size_buf);
    strcat(line, " px    Color: #");
    strcat(line, hex);
    strcat(line, "    Palette colors: ");
    char palette_buf[8];
    itoa(palette_count, palette_buf, 10);
    strcat(line, palette_buf);
    gfx2d_draw_text(&surface, 16, 12 + GFX2D_FONT_H + 6, GFX2D_RGB(200, 200, 200), line);
}

void main(void) {
    unsigned int info = sys_gfx_info();
    if (!info) {
        sys_write("depict: graphics mode required.\n");
        sys_exit();
    }
    scr_w = (int)((info >> 16) & 0xFFFF);
    scr_h = (int)(info & 0xFFFF);
    if (scr_w <= 0 || scr_h <= 0)
        sys_exit();
    if (scr_w > MAX_W) scr_w = MAX_W;
    if (scr_h > MAX_H) scr_h = MAX_H;
    gfx2d_surface_init(&surface, scr_w, scr_h, backbuf);

    sys_clear();
    sys_mouse_show(1);

    current_color = palette[color_index];
    clear_canvas();
    draw_hud();
    draw_palette_preview();
    gfx2d_surface_present(&surface, sys_gfx_blit);

    int prev_buttons = 0;
    int last_draw_x = -1, last_draw_y = -1;
    int last_erase_x = -1, last_erase_y = -1;
    int dirty = 0;
    int hud_dirty = 0;

    while (1) {
        mouse_info_t mi;
        sys_mouse_get(&mi);
        int mx = clamp_coord(mi.x, scr_w);
        int my = clamp_coord(mi.y, scr_h);
        int buttons = (int)mi.buttons;

        if (buttons & 1) {
            if (!(prev_buttons & 1)) {
                draw_brush_square(mx, my, current_color);
            } else if (last_draw_x >= 0) {
                draw_line_brush(last_draw_x, last_draw_y, mx, my, current_color);
            }
            last_draw_x = mx;
            last_draw_y = my;
            dirty = 1;
        } else {
            last_draw_x = last_draw_y = -1;
        }

        if (buttons & 4) {
            if (!(prev_buttons & 4)) {
                draw_brush_square(mx, my, background_color);
            } else if (last_erase_x >= 0) {
                draw_line_brush(last_erase_x, last_erase_y, mx, my, background_color);
            }
            last_erase_x = mx;
            last_erase_y = my;
            dirty = 1;
        } else {
            last_erase_x = last_erase_y = -1;
        }

        if ((buttons & 2) && !(prev_buttons & 2)) {
            color_index = (color_index + 1) % palette_count;
            current_color = palette[color_index];
            hud_dirty = 1;
        }

        prev_buttons = buttons;

        unsigned key = sys_trygetchar();
        if (key) {
            char c = (char)key;
            if (c == 'q' || c == 'Q' || c == 27)
                break;
            else if (c == 'c' || c == 'C') {
                clear_canvas();
                hud_dirty = 1;
                dirty = 1;
            } else if (c == '[' || c == '{' || c == '-' || c == '_') {
                if (brush_size > 2)
                    brush_size--;
                hud_dirty = 1;
            } else if (c == ']' || c == '}' || c == '+' || c == '=') {
                if (brush_size < 96)
                    brush_size++;
                hud_dirty = 1;
            }
        }

        if (hud_dirty)
            dirty = 1;

        if (dirty) {
            draw_hud();
            draw_palette_preview();
            gfx2d_surface_present(&surface, sys_gfx_blit);
            dirty = 0;
            hud_dirty = 0;
        }

        asm volatile("hlt");
    }

    sys_mouse_show(0);
    sys_clear();
    sys_exit();
}
