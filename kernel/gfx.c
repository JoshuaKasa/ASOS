#include "gfx.h"

#include "../lib/string.h"

#include <stdint.h>

#define MODEINFO_ADDR ((uint8_t*)0x00080000)
#define FONT8x16_ADDR ((const uint8_t(*)[16])0x00080100)

// VBE ModeInfo Block offset
#define OFF_BytesPerScanLine 0x10     // uint16_t
#define OFF_XResolution 0x12          // uint16_t
#define OFF_YResolution 0x14          // uint16_t
#define OFF_BitsPerPixel 0x19         // uint8_t
#define OFF_PhysBasePtr 0x28          // uint32_t
#define OFF_LinBytesPerScanLine 0x58  // uint32_t

static gfx_info_t G;
static uint8_t* LFB; // Linear Frame Buffer

static inline uint16_t rd16(const uint8_t* p) {
    return *(const uint16_t*)p;
}
static inline uint32_t rd32(const uint8_t* p) {
    return *(const uint32_t*)p;
}

const gfx_info_t* gfx_info(void) {
    return &G;
}

int gfx_init(void) {
    uint8_t* m = MODEINFO_ADDR;

    G.w = rd16(m + OFF_XResolution);
    G.h = rd16(m + OFF_YResolution);
    G.bpp = *(m + OFF_BitsPerPixel);

    uint32_t phys = rd32(m + OFF_PhysBasePtr);
    uint32_t linPitch = rd32(m + OFF_LinBytesPerScanLine);
    uint16_t pitch = rd16(m + OFF_BytesPerScanLine);

    G.pitch = (linPitch != 0) ? (uint16_t)linPitch : pitch;
    G.fb = phys;

    LFB = (uint8_t*)G.fb;

    if (G.bpp != 32) {
        // Can fallback of convert, for now we throw a error
        return -1;
    }
    return 0;
}

static inline void put32(int x, int y, uint32_t rgb)
{
    // rgb = 0x00RRGGBB -> mem = 0x00BBGGRR (BGRX in little-endian)
    uint32_t bgr =
        ((rgb & 0x000000FF) << 16) |   // B -> pos R
        ( rgb & 0x0000FF00)        |   // G -> pos G
        ((rgb & 0x00FF0000) >> 16);    // R -> pos B

    *(uint32_t*)(LFB + (size_t)y * G.pitch + (size_t)x * 4) = bgr;
}


void gfx_clear(uint32_t rgba) {
    for (int y = 0; y < G.h; ++y) {
        uint32_t* row = (uint32_t*)(LFB + (size_t)y * G.pitch);

        for (int x = 0; x < G.w; ++x)
            row[x] = rgba;
    }
}

void gfx_putpixel(int x, int y, uint32_t rgba) {
    if ((unsigned)x >= G.w || (unsigned)y >= G.h)
        return;
    put32(x, y, rgba);
}

void gfx_fillrect(int x, int y, int w, int h, uint32_t rgba) {
    if (w <= 0 || h <= 0)
        return;

    int x2 = x + w, y2 = y + h;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x2 > G.w)
        x2 = G.w;
    if (y2 > G.h)
        y2 = G.h;

    for (int j = y; j < y2; ++j) {
        uint32_t* row = (uint32_t*)(LFB + (size_t)j * G.pitch);

        for (int i = x; i < x2; ++i)
            row[i] = rgba;
    }
}

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t* g = FONT8x16_ADDR[(uint8_t)c];

    for (int dy = 0; dy < 16; ++dy) {
        uint8_t bits = g[dy];

        for (int dx = 0; dx < 8; ++dx) {
            uint32_t col = (bits & (0x80 >> dx)) ? fg : bg;

            gfx_putpixel(x + dx, y + dy, col);
        }
    }
}

void gfx_draw_text(int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    for (int i = 0; s[i]; ++i)
        gfx_draw_char(x + i * 8, y, s[i], fg, bg);
}

uint32_t gfx_get_pixel(int x, int y) {
    if ((unsigned)x >= G.w || (unsigned)y >= G.h)
        return 0;

    uint32_t bgr = *(uint32_t*)(LFB + (size_t)y * G.pitch + (size_t)x * 4);

    // memory: 0x00BBGGRR -> returns: 0x00RRGGBB
    uint32_t rr = (bgr & 0x000000FF) << 16;
    uint32_t gg = (bgr & 0x0000FF00);
    uint32_t bb = (bgr & 0x00FF0000) >> 16;
    return rr | gg | bb;
}

