// ./app/3d.c  — wireframe cube using pixels (LFB), keyboard to quit (Q/ESC)
#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

// Color helper (0x00RRGGBB)
#define RGB(r, g, b) (((unsigned)(r) & 0xFF) << 16 | ((unsigned)(g) & 0xFF) << 8 | ((unsigned)(b) & 0xFF))

static inline int imax(int a, int b) {
    return a > b ? a : b;
}
static inline int imin(int a, int b) {
    return a < b ? a : b;
}
static inline int iabs(int x) {
    return x < 0 ? -x : x;
}

// Fast inverse sqrt for re-normalizing the rotation pairs
static float finvsqrt(float x) {
    union {
        float f;
        unsigned int i;
    } u = {x};

    u.i = 0x5f3759df - (u.i >> 1);
    float y = u.f;

    // One iteration is plenty here
    return y * (1.5f - 0.5f * x * y * y);
}

static void line(int x0, int y0, int x1, int y1, unsigned int rgb) {
    int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        sys_gfx_putpixel(x0, y0, rgb);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

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

void main(void) {
    // Check 32-bpp gfx
    unsigned int info = sys_gfx_info();
    int W = (int)((info >> 16) & 0xFFFF);
    int H = (int)(info & 0xFFFF);

    if (info == 0 || W <= 0 || H <= 0) {
        sys_clear();
        sys_write("No 32-bpp graphics mode. The 3D demo needs VBE 32-bit LFB.\n");
        sys_write("Press ENTER to exit...\n");

        while (sys_getchar() != '\n') {
        }

        sys_exit();
    }

    sys_mouse_show(0);
    sys_clear(); // wipe any text
    sys_gfx_clear(RGB(0, 0, 0));

    // Cube vertices (unit cube), we’ll scale in pixels
    const float verts[8][3] = {{-1, -1, -1}, {+1, -1, -1}, {+1, +1, -1}, {-1, +1, -1}, {-1, -1, +1}, {+1, -1, +1}, {+1, +1, +1}, {-1, +1, +1}};
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    // Screen center & scale
    float cx = W * 0.5f, cy = H * 0.5f;
    float scale = (W < H ? W : H) * 0.28f; // nice size

    // Perspective params
    float zoff = 3.5f;    // Camera distance in “cube units”
    float focal = scale;  // Pixel focal length (cheap & cheerful)

    // Rotation state (cos/sin pairs) — no libm, we use a fixed delta with recurrence
    float cax = 1.0f, sax = 0.0f;  // around X
    float cay = 1.0f, say = 0.0f;  // around Y
                                  
    // ~2.3 deg per frame
    const float COSD = 0.9992001f, SIND = 0.0399893f;
    const float COSD2 = 0.9995000f, SIND2 = 0.0316000f; // Slightly different for Y

    unsigned int last_cursor_park = sys_getticks() + 6;

    while (1) {
        // Quit with Q or ESC
        unsigned int ch = sys_trygetchar();

        if (ch) {
            if (ch == 'q' || ch == 'Q' || (unsigned char)ch == 27) {
                sys_gfx_clear(RGB(0, 0, 0));
                sys_write("\nBye!\n");
                sys_exit();
            }
        }

        // Clear frame
        sys_gfx_clear(RGB(0, 0, 0));

        // Update rotations (complex multiply by fixed delta)
        float ncx = cax * COSD - sax * SIND;
        float nsx = sax * COSD + cax * SIND;
        float ncy = cay * COSD2 - say * SIND2;
        float nsy = say * COSD2 + cay * SIND2;

        // Renormalize a bit so we don’t drift
        float inv1 = finvsqrt(ncx * ncx + nsx * nsx);
        float inv2 = finvsqrt(ncy * ncy + nsy * nsy);

        cax = ncx * inv1;
        sax = nsx * inv1;
        cay = ncy * inv2;
        say = nsy * inv2;

        // Project verts
        int px[8], py[8];

        for (int i = 0; i < 8; i++) {
            float x = verts[i][0];
            float y = verts[i][1];
            float z = verts[i][2];

            // rot X
            float y1 = y * cax - z * sax;
            float z1 = y * sax + z * cax;

            // rot Y
            float x2 = x * cay + z1 * say;
            float z2 = -x * say + z1 * cay;

            float zz = z2 + zoff;
            if (zz < 0.2f)
                zz = 0.2f;  // clamp (avoid div-by-small)

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

        // Draw edges
        unsigned int col = RGB(255, 200, 40);

        for (int e = 0; e < 12; e++) {
            int a = edges[e][0], b = edges[e][1];
            line(px[a], py[a], px[b], py[b], col);
        }

        // Keep HW cursor out of the way
        unsigned int now = sys_getticks();

        if ((int)(now - last_cursor_park) >= 0) {
            sys_setcursor(79, 24);
            last_cursor_park = now + 6;
        }

        // Tiny pacing to avoid melting QEMU
        sys_sleep(1);
    }
}

