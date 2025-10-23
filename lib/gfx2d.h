#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX2D_RGB(r, g, b) ((((uint32_t)(r) & 0xFFu) << 16) | \
                           (((uint32_t)(g) & 0xFFu) << 8)  | \
                           ((uint32_t)(b) & 0xFFu))

enum {
    GFX2D_FONT_W = 8,
    GFX2D_FONT_H = 8,
};

typedef struct {
    int width;
    int height;
    int stride;
    uint32_t* pixels;
} gfx2d_surface_t;

typedef int (*gfx2d_presenter_t)(const uint32_t* pixels);

void gfx2d_surface_init(gfx2d_surface_t* surface, int width, int height, uint32_t* pixels);
void gfx2d_surface_init_stride(gfx2d_surface_t* surface, int width, int height, int stride, uint32_t* pixels);
void gfx2d_surface_clear(gfx2d_surface_t* surface, uint32_t rgb);
void gfx2d_surface_clear_gradient(gfx2d_surface_t* surface, uint32_t base_rgb, int vertical_strength, int horizontal_strength);

void gfx2d_plot(gfx2d_surface_t* surface, int x, int y, uint32_t rgb);
void gfx2d_hline(gfx2d_surface_t* surface, int x, int y, int length, uint32_t rgb);
void gfx2d_vline(gfx2d_surface_t* surface, int x, int y, int length, uint32_t rgb);
void gfx2d_fill_rect(gfx2d_surface_t* surface, int x, int y, int w, int h, uint32_t rgb);
void gfx2d_stroke_rect(gfx2d_surface_t* surface, int x, int y, int w, int h, uint32_t rgb);
void gfx2d_blit(gfx2d_surface_t* dest, int dx, int dy, const gfx2d_surface_t* src);
void gfx2d_draw_line(gfx2d_surface_t* surface, int x0, int y0, int x1, int y1, uint32_t rgb);

void gfx2d_draw_char(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, unsigned char ch);
void gfx2d_draw_char_bg(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, uint32_t bg_rgb, unsigned char ch);
void gfx2d_draw_text(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, const char* text);
void gfx2d_draw_text_bg(gfx2d_surface_t* surface, int x, int y, uint32_t rgb, uint32_t bg_rgb, const char* text);

void gfx2d_surface_present(const gfx2d_surface_t* surface, gfx2d_presenter_t presenter);

#ifdef __cplusplus
}
#endif

