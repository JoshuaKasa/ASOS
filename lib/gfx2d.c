#include "gfx2d.h"
#include "font8x8_basic.h"

static int gfx2d_clamp255(int value) {
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}

void gfx2d_surface_init(gfx2d_surface_t* surface, int width, int height, uint32_t* pixels) {
    if (!surface)
        return;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
    surface->pixels = pixels;
}

void gfx2d_surface_init_stride(gfx2d_surface_t* surface, int width, int height, int stride, uint32_t* pixels) {
    if (!surface)
        return;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    if (stride < width)
        stride = width;
    if (stride < 0)
        stride = 0;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
    surface->pixels = pixels;
}

void gfx2d_surface_clear(gfx2d_surface_t* surface, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if (surface->height <= 0 || surface->stride <= 0)
        return;
    for (int y = 0; y < surface->height; ++y) {
        uint32_t* row = surface->pixels + y * surface->stride;
        for (int x = 0; x < surface->width; ++x)
            row[x] = rgb;
    }
}

void gfx2d_surface_clear_gradient(gfx2d_surface_t* surface, uint32_t base_rgb, int vertical_strength, int horizontal_strength) {
    if (!surface || !surface->pixels)
        return;
    int width = surface->width;
    int height = surface->height;
    if (width <= 0 || height <= 0)
        return;

    int base_r = (int)((base_rgb >> 16) & 0xFFu);
    int base_g = (int)((base_rgb >> 8) & 0xFFu);
    int base_b = (int)(base_rgb & 0xFFu);

    if (vertical_strength < 0)
        vertical_strength = 0;
    if (horizontal_strength < 0)
        horizontal_strength = 0;

    int denom_y = height ? height : 1;
    int denom_x = width ? width : 1;
    for (int y = 0; y < height; ++y) {
        int shade = (vertical_strength * y) / denom_y;
        for (int x = 0; x < width; ++x) {
            int tint = (horizontal_strength * x) / denom_x;
            int r = gfx2d_clamp255(base_r + tint);
            int g = gfx2d_clamp255(base_g + shade / 2);
            int b = gfx2d_clamp255(base_b + shade);
            surface->pixels[y * surface->stride + x] = (uint32_t)((r << 16) | (g << 8) | b);
        }
    }
}

void gfx2d_plot(gfx2d_surface_t* surface, int x, int y, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if ((unsigned int)x >= (unsigned int)surface->width || (unsigned int)y >= (unsigned int)surface->height)
        return;
    surface->pixels[y * surface->stride + x] = rgb;
}

void gfx2d_hline(gfx2d_surface_t* surface, int x, int y, int length, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if (length <= 0)
        return;
    if (y < 0 || y >= surface->height)
        return;
    if (x < 0) {
        length += x;
        x = 0;
    }
    if (x >= surface->width)
        return;
    if (x + length > surface->width)
        length = surface->width - x;
    if (length <= 0)
        return;
    uint32_t* row = surface->pixels + y * surface->stride + x;
    for (int i = 0; i < length; ++i)
        row[i] = rgb;
}

void gfx2d_vline(gfx2d_surface_t* surface, int x, int y, int length, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if (length <= 0)
        return;
    if (x < 0 || x >= surface->width)
        return;
    if (y < 0) {
        length += y;
        y = 0;
    }
    if (y >= surface->height)
        return;
    if (y + length > surface->height)
        length = surface->height - y;
    if (length <= 0)
        return;
    uint32_t* ptr = surface->pixels + y * surface->stride + x;
    for (int i = 0; i < length; ++i) {
        *ptr = rgb;
        ptr += surface->stride;
    }
}

void gfx2d_fill_rect(gfx2d_surface_t* surface, int x, int y, int w, int h, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if (w <= 0 || h <= 0)
        return;
    int x2 = x + w;
    int y2 = y + h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x2 > surface->width)
        x2 = surface->width;
    if (y2 > surface->height)
        y2 = surface->height;
    if (x >= x2 || y >= y2)
        return;
    for (int row_idx = y; row_idx < y2; ++row_idx) {
        uint32_t* row = surface->pixels + row_idx * surface->stride;
        for (int col = x; col < x2; ++col)
            row[col] = rgb;
    }
}

void gfx2d_stroke_rect(gfx2d_surface_t* surface, int x, int y, int w, int h, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    if (w <= 1 || h <= 1)
        return;
    gfx2d_hline(surface, x, y, w, rgb);
    gfx2d_hline(surface, x, y + h - 1, w, rgb);
    gfx2d_vline(surface, x, y, h, rgb);
    gfx2d_vline(surface, x + w - 1, y, h, rgb);
}

void gfx2d_blit(gfx2d_surface_t* dest, int dx, int dy, const gfx2d_surface_t* src) {
    if (!dest || !src || !dest->pixels || !src->pixels)
        return;
    if (src->width <= 0 || src->height <= 0)
        return;
    int dest_w = dest->width;
    int dest_h = dest->height;
    for (int sy = 0; sy < src->height; ++sy) {
        int ty = dy + sy;
        if (ty < 0 || ty >= dest_h)
            continue;
        int sx = 0;
        int tx = dx;
        int span = src->width;
        if (tx < 0) {
            sx -= tx;
            span += tx;
            tx = 0;
        }
        if (tx >= dest_w)
            continue;
        if (tx + span > dest_w)
            span = dest_w - tx;
        if (span <= 0)
            continue;
        const uint32_t* src_row = src->pixels + sy * src->stride + sx;
        uint32_t* dst_row = dest->pixels + ty * dest->stride + tx;
        for (int i = 0; i < span; ++i)
            dst_row[i] = src_row[i];
    }
}

void gfx2d_draw_line(gfx2d_surface_t* surface, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (!surface || !surface->pixels)
        return;
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;  // negative absolute value
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        gfx2d_plot(surface, x0, y0, rgb);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx2d_draw_char(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, unsigned char ch) {
    if (!surface || !surface->pixels)
        return;
    if (x >= surface->width || y >= surface->height)
        return;
    if (x + GFX2D_FONT_W <= 0 || y + GFX2D_FONT_H <= 0)
        return;
    const uint8_t* glyph = font8x8_basic[ch];
    for (int row = 0; row < GFX2D_FONT_H; ++row) {
        int py = y + row;
        if (py < 0 || py >= surface->height)
            continue;
        uint8_t bits = glyph[row];
        uint32_t* dst = surface->pixels + py * surface->stride;
        for (int col = 0; col < GFX2D_FONT_W; ++col) {
            int px = x + col;
            if (px < 0 || px >= surface->width)
                continue;
            uint8_t mask = (uint8_t)(1u << col);
            if (bits & mask)
                dst[px] = rgb;
        }
    }
}

void gfx2d_draw_char_bg(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, uint32_t bg_rgb, unsigned char ch) {
    if (!surface || !surface->pixels)
        return;
    gfx2d_fill_rect(surface, x, y, GFX2D_FONT_W, GFX2D_FONT_H, bg_rgb);
    gfx2d_draw_char(surface, x, y, rgb, ch);
}

void gfx2d_draw_text(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, const char* text) {
    if (!surface || !surface->pixels || !text)
        return;
    int cx = x;
    int cy = y;
    for (const char* p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\n') {
            cx = x;
            cy += GFX2D_FONT_H;
            continue;
        }
        gfx2d_draw_char(surface, cx, cy, rgb, ch);
        cx += GFX2D_FONT_W;
    }
}

void gfx2d_draw_text_bg(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, uint32_t bg_rgb, const char* text) {
    if (!surface || !surface->pixels || !text)
        return;
    int cx = x;
    int cy = y;
    for (const char* p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\n') {
            cx = x;
            cy += GFX2D_FONT_H;
            continue;
        }
        gfx2d_draw_char_bg(surface, cx, cy, rgb, bg_rgb, ch);
        cx += GFX2D_FONT_W;
    }
}

void gfx2d_surface_present(const gfx2d_surface_t* surface, gfx2d_presenter_t presenter) {
    if (!surface || !presenter)
        return;
    if (!surface->pixels)
        return;
    (void)presenter(surface->pixels);
}

