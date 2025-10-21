#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

// 0x00RRGGBB
#define RGB(r, g, b)                                                  \
    ((((unsigned)(r) & 0xFF) << 16) | (((unsigned)(g) & 0xFF) << 8) | \
     ((unsigned)(b) & 0xFF))

static inline int imax(int a, int b) {
    return a > b ? a : b;
}
static inline int imin(int a, int b) {
    return a < b ? a : b;
}
static inline int iabs(int x) {
    return x < 0 ? -x : x;
}

// Tiny utils
static float finvsqrt(float x) {
    union {
        float f;
        unsigned int i;
    } u = {x};
    u.i = 0x5f3759df - (u.i >> 1);
    float y = u.f;
    return y * (1.5f - 0.5f * x * y * y);
}

static void fmt_fixed1(float v, char* out) {
    int n = (int)((v >= 0.0f ? v * 10.0f + 0.5f : v * 10.0f - 0.5f));
    if (n < 0) {
        *out++ = '-';
        n = -n;
    }

    int whole = n / 10;
    int frac = n % 10;
    char tmp[32];

    itoa(whole, tmp, 10);
    strcpy(out, tmp);

    out += (int)strlen(tmp);
    *out++ = '.';
    *out++ = (char)('0' + frac);
    *out = 0;
}

// Backbuffer
#define MAX_W 1024
#define MAX_H 768

static unsigned int backbuf[MAX_W * MAX_H];
static int G_W = 0;
static int G_H = 0;

static inline void pset_buf(int x, int y, unsigned int rgb) {
    if ((unsigned)x < (unsigned)G_W && (unsigned)y < (unsigned)G_H)
        backbuf[y * G_W + x] = rgb;
}

static void clear_buf(unsigned int rgb) {
    unsigned int count = (unsigned int)(G_W * G_H);

    for (unsigned int i = 0; i < count; i++)
        backbuf[i] = rgb;
}

static void line_buf(int x0, int y0, int x1, int y1, unsigned int rgb) {
    int dx = iabs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -iabs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        pset_buf(x0, y0, rgb);
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

// Shapes

typedef struct {
    const float (*V)[3];
    int vn;
    const int (*E)[2];
    int en;
    const char* name;
} shape_t;

// cube
static const float CUBE_V[8][3] = {{-1, -1, -1}, {+1, -1, -1}, {+1, +1, -1},
                                   {-1, +1, -1}, {-1, -1, +1}, {+1, -1, +1},
                                   {+1, +1, +1}, {-1, +1, +1}};
static const int CUBE_E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                  {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                  {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// tetrahedron
static const float TET_V[4][3] = {{1, 1, 1},
                                  {-1, -1, 1},
                                  {-1, 1, -1},
                                  {1, -1, -1}};
static const int TET_E[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

// octahedron
static const float OCT_V[6][3] = {{+1, 0, 0}, {-1, 0, 0}, {0, +1, 0},
                                  {0, -1, 0}, {0, 0, +1}, {0, 0, -1}};
static const int OCT_E[12][2] = {{0, 2}, {0, 3}, {0, 4}, {0, 5},
                                 {1, 2}, {1, 3}, {1, 4}, {1, 5},
                                 {2, 4}, {2, 5}, {3, 4}, {3, 5}};

static const shape_t SHAPES[] = {
    {CUBE_V, 8, CUBE_E, 12, "Cube"},
    {TET_V, 4, TET_E, 6, "Tetra"},
    {OCT_V, 6, OCT_E, 12, "Octa"},
};

static const shape_t* cur_shape = &SHAPES[0];
static int shape_index = 0;

static void set_shape(int idx) {
    int n = (int)(sizeof(SHAPES) / sizeof(SHAPES[0]));
    if (idx < 0)
        idx = n - 1;
    if (idx >= n)
        idx = 0;
    shape_index = idx;
    cur_shape = &SHAPES[idx];
}

// Disco color (integer HSV wheel: hue e [0..1535])

static unsigned int hue_to_rgb(int h) {
    h &= 1535;  // 6 * 256 - 1
    int seg = h >> 8;
    int x = h & 255;

    int r = 0, g = 0, b = 0;

    switch (seg) {
        case 0:
            r = 255;
            g = x;
            b = 0;
            break;  // R -> Y
        case 1:
            r = 255 - x;
            g = 255;
            b = 0;
            break;  // Y -> G
        case 2:
            r = 0;
            g = 255;
            b = x;
            break;  // G -> C
        case 3:
            r = 0;
            g = 255 - x;
            b = 255;
            break;  // C -> B
        case 4:
            r = x;
            g = 0;
            b = 255;
            break;  // B -> M
        default:
            r = 255;
            g = 0;
            b = 255 - x;
            break;  // M -> R
    }
    return RGB(r, g, b);
}

// HUD

static void hud_print_static(void) {
    sys_clear();
    sys_write("ASOS 3D viewer\n\n");
    sys_write("Controls\n");
    sys_write("  w a s d   rotate manually\n");
    sys_write("  P         resume auto\n");
    sys_write("  + / -     zoom in / out\n");
    sys_write("  D         toggle disco\n");
    sys_write("  S / M     faster / slower\n");
    sys_write("  AL / AR   switch shape (also , / .)\n");
    sys_write("  Q / ESC   quit\n\n");
}

static const char* speed_label(int level) {
    switch (level) {
        case -2:
            return "0.25x";
        case -1:
            return "0.5x";
        case 0:
            return "1x";
        case 1:
            return "2x";
        default:
            return "4x";
    }
}

static void hud_print_status(int auto_mode, float zoff, int speed_level, int disco_on) {
    char ztxt[32];
    fmt_fixed1(zoff, ztxt);

    sys_write("Shape: ");
    sys_write(cur_shape->name);
    sys_write("    Mode: ");
    sys_write(auto_mode ? "auto" : "manual");
    sys_write("    Zoom: ");
    sys_write(ztxt);
    sys_write("    Speed: ");
    sys_write(speed_label(speed_level));
    sys_write("    Disco: ");
    sys_write(disco_on ? "ON " : "OFF");
    sys_write("        \n");
}

// Main

void main(void) {
    unsigned int info = sys_gfx_info();

    if (info == 0) {
        sys_clear();
        sys_write("No 32-bpp graphics mode available\n");
        sys_write("Press ENTER to exit...\n");
        while (sys_getchar() != '\n') {
        }
        sys_exit();
    }

    int W = (int)((info >> 16) & 0xFFFF);
    int H = (int)(info & 0xFFFF);

    if (W > MAX_W || H > MAX_H) {
        sys_clear();
        sys_write("Mode too big for app backbuffer\n");
        sys_write("Reduce resolution to <= 1024x768\n");
        sys_write("Press ENTER to exit...\n");

        while (sys_getchar() != '\n') {  }

        sys_exit();
    }

    G_W = W;
    G_H = H;

    sys_mouse_show(0);
    sys_gfx_clear(RGB(0, 0, 0));
    hud_print_static();

    set_shape(0);

    float cx = W * 0.5f;
    float cy = H * 0.5f;
    float scale = (W < H ? (float)W : (float)H) * 0.28f;
    float zoff = 3.5f;
    float focal = scale;

    float cax = 1.0f, sax = 0.0f;
    float cay = 1.0f, say = 0.0f;

    // Base deltas (~5 deg on X, ~4 deg on Y)
    const float COS_AX = 0.9961947f, SIN_AX = 0.0871557f;
    const float COS_AY = 0.9975640f, SIN_AY = 0.0697565f;

    const float MC = 0.9993908f, MS = 0.0348995f;

    int auto_mode = 1;
    int disco_on = 0;

    int speed_level = 0;  // -2, -1, 0, +1, +2
    unsigned int frame_counter = 0;

    hud_print_status(auto_mode, zoff, speed_level, disco_on);

    unsigned int last_cursor_park = sys_getticks() + 6;

    const unsigned int frame_ticks = 2;
    unsigned int next_frame = sys_getticks();

    for (;;) {
        // Input
        for (;;) {
            unsigned int ch = sys_trygetchar();
            if (!ch)
                break;

            char c = (char)ch;

            if (c == 27) {
                // Try to parse ESC [ A/B/C/D
                unsigned int c1 = sys_trygetchar();
                if (c1 == '[') {
                    unsigned int c2 = sys_trygetchar();
                    if (c2 == 'C') {  // right
                        set_shape(shape_index + 1);
                        hud_print_status(auto_mode, zoff, speed_level,
                                         disco_on);
                        continue;
                    }
                    if (c2 == 'D') {  // left
                        set_shape(shape_index - 1);
                        hud_print_status(auto_mode, zoff, speed_level,
                                         disco_on);
                        continue;
                    }
                    if (c2 == 'A') {  // up -> faster
                        if (speed_level < 2)
                            speed_level++;
                        hud_print_status(auto_mode, zoff, speed_level,
                                         disco_on);
                        continue;
                    }
                    if (c2 == 'B') {  // down -> slower
                        if (speed_level > -2)
                            speed_level--;
                        hud_print_status(auto_mode, zoff, speed_level,
                                         disco_on);
                        continue;
                    }
                }
                // Lone ESC = quit
                sys_write("\nBye\n");
                sys_exit();
            }

            if (c == 'q' || c == 'Q') {
                sys_write("\nBye\n");
                sys_exit();
            }

            if (c == '+' || c == '=') {
                zoff -= 0.1f;
                if (zoff < 1.1f)
                    zoff = 1.1f;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == '-') {
                zoff += 0.1f;
                if (zoff > 10.0f)
                    zoff = 10.0f;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == ',') {  // Prev shape fallback
                set_shape(shape_index - 1);
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == '.') {  // Next shape fallback
                set_shape(shape_index + 1);
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'p' || c == 'P') {
                auto_mode = 1;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'D') {  // Disco toggle (upper-case)
                disco_on = !disco_on;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'S') {  // Faster (upper-case)
                if (speed_level < 2)
                    speed_level++;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'M') {  // Slower (upper-case)
                if (speed_level > -2)
                    speed_level--;
                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            // Manual rotation on lower-case wasd
            if (c == 'a') {
                float nc = cay * MC - say * MS;
                float ns = say * MC + cay * MS;
                float inv = finvsqrt(nc * nc + ns * ns);

                cay = nc * inv;
                say = ns * inv;
                auto_mode = 0;

                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'd') {
                float nc = cay * MC + say * MS;
                float ns = say * MC - cay * MS;
                float inv = finvsqrt(nc * nc + ns * ns);

                cay = nc * inv;
                say = ns * inv;
                auto_mode = 0;

                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 'w') {
                float nc = cax * MC - sax * MS;
                float ns = sax * MC + cax * MS;
                float inv = finvsqrt(nc * nc + ns * ns);

                cax = nc * inv;
                sax = ns * inv;
                auto_mode = 0;

                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }

            if (c == 's') {
                float nc = cax * MC + sax * MS;
                float ns = sax * MC - cax * MS;
                float inv = finvsqrt(nc * nc + ns * ns);

                cax = nc * inv;
                sax = ns * inv;
                auto_mode = 0;

                hud_print_status(auto_mode, zoff, speed_level, disco_on);
                continue;
            }
        }

        unsigned int now = sys_getticks();
        if ((int)(now - next_frame) < 0) {
            asm volatile("hlt");
            continue;
        }
        next_frame += frame_ticks;

        // Auto rotation with speed levels
        // level -2: rotate every 4th frame
        // level -1: rotate every 2nd frame
        // level  0: rotate once per frame
        // level +1: rotate 2x per frame
        // level +2: rotate 4x per frame
        int steps = 0;

        if (auto_mode) {
            if (speed_level <= 0) {
                int div = (speed_level == -2) ? 4 : (speed_level == -1) ? 2 : 1;
                steps = ((frame_counter % div) == 0) ? 1 : 0;
            } else {
                steps = (speed_level == 1) ? 2 : 4;
            }
        }

        for (int s = 0; s < steps; s++) {
            float ncx = cax * COS_AX - sax * SIN_AX;
            float nsx = sax * COS_AX + cax * SIN_AX;
            float ncy = cay * COS_AY - say * SIN_AY;
            float nsy = say * COS_AY + cay * SIN_AY;

            float inv1 = finvsqrt(ncx * ncx + nsx * nsx);
            float inv2 = finvsqrt(ncy * ncy + nsy * nsy);

            cax = ncx * inv1;
            sax = nsx * inv1;
            cay = ncy * inv2;
            say = nsy * inv2;
        }

        frame_counter++;

        // Render
        clear_buf(RGB(0, 0, 0));

        int px[16];
        int py[16];

        for (int i = 0; i < cur_shape->vn; i++) {
            float x = cur_shape->V[i][0];
            float y = cur_shape->V[i][1];
            float z = cur_shape->V[i][2];

            float y1 = y * cax - z * sax;
            float z1 = y * sax + z * cax;

            float x2 = x * cay + z1 * say;
            float z2 = -x * say + z1 * cay;

            float zz = z2 + zoff;
            if (zz < 0.2f)
                zz = 0.2f;

            float sx = cx + (x2 * focal) / zz;
            float sy = cy + (y1 * focal) / zz;

            int ix = (int)(sx + 0.5f);
            int iy = (int)(sy + 0.5f);

            if (ix < 0)
                ix = 0;
            if (ix >= W)
                ix = W - 1;
            if (iy < 0)
                iy = 0;
            if (iy >= H)
                iy = H - 1;

            px[i] = ix;
            py[i] = iy;
        }

        for (int e = 0; e < cur_shape->en; e++) {
            unsigned int col;
            if (disco_on) {
                // Animate hue by time + edge index offset
                int hue = (int)((frame_counter << 4) + (e << 7));
                col = hue_to_rgb(hue);
            } 
            else {
                col = RGB(255, 200, 40);
            }

            int a = cur_shape->E[e][0];
            int b = cur_shape->E[e][1];
            line_buf(px[a], py[a], px[b], py[b], col);
        }

        sys_gfx_blit(backbuf);

        if ((int)(now - last_cursor_park) >= 0) {
            sys_setcursor(79, 24);
            last_cursor_park = now + 6;
        }
    }
}
