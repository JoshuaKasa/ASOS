#include "../lib/stdlib.h"
#include "../lib/gfx3d.h"
#include "asoapi.h"

#define MAX_W 1024
#define MAX_H 768
#define TWO_PI 6.2831853f
#define RGB(r, g, b) ((((unsigned)(r) & 0xFF) << 16) | (((unsigned)(g) & 0xFF) << 8) | ((unsigned)(b) & 0xFF))

static uint32_t frame[MAX_W * MAX_H];

#define MAP_W 16
#define MAP_H 16
static const uint8_t MAP_DATA[MAP_W * MAP_H] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 2, 2, 0, 0, 0, 0, 3, 3, 3, 0, 0, 4, 0, 1,
    1, 0, 2, 0, 0, 0, 5, 0, 3, 0, 3, 0, 0, 4, 0, 1,
    1, 0, 2, 0, 0, 0, 5, 0, 0, 0, 3, 0, 0, 4, 0, 1,
    1, 0, 0, 0, 6, 6, 5, 5, 5, 0, 3, 0, 7, 7, 0, 1,
    1, 0, 0, 0, 6, 0, 0, 0, 5, 0, 3, 0, 7, 0, 0, 1,
    1, 0, 8, 0, 6, 0, 9, 0, 5, 0, 3, 0, 7, 0, 0, 1,
    1, 0, 8, 0, 6, 0, 9, 0, 5, 0, 3, 0, 7, 0, 0, 1,
    1, 0, 8, 0, 6, 0, 9, 0, 5, 0, 3, 0, 7, 0, 0, 1,
    1, 0, 8, 0, 0, 0, 9, 0, 5, 0, 3, 0, 0, 0, 0, 1,
    1, 0, 8, 8, 8, 0, 9, 0, 5, 5, 5, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 10, 10, 0, 1,
    1, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 10, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 9, 9, 9, 0, 0, 0, 10, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static const gfx3d_palette_t PALETTE = {
    RGB(48, 64, 96),
    RGB(16, 16, 24),
    {
        RGB(0, 0, 0),
        RGB(200, 60, 60),
        RGB(200, 120, 60),
        RGB(180, 180, 60),
        RGB(100, 200, 80),
        RGB(60, 160, 200),
        RGB(60, 80, 200),
        RGB(140, 80, 200),
        RGB(200, 200, 200),
        RGB(220, 160, 220),
        RGB(220, 220, 140),
        RGB(160, 220, 220),
        RGB(160, 160, 160),
        RGB(180, 120, 120),
        RGB(120, 180, 140),
        RGB(80, 120, 180)
    }
};

static void draw_status_bar(gfx3d_surface_t* surface, const gfx3d_camera_t* cam) {
    if (!surface || !surface->pixels)
        return;
    int h = surface->height;
    int w = surface->width;
    int bar_h = h / 20;
    if (bar_h < 12)
        bar_h = 12;
    for (int y = 0; y < bar_h; ++y) {
        int row = h - 1 - y;
        if (row < 0)
            break;
        uint32_t* line = &surface->pixels[row * surface->stride];
        for (int x = 0; x < w; ++x) {
            uint32_t base = RGB(20, 20, 20);
            if (x < 6 || x > w - 7)
                base = RGB(40, 40, 40);
            line[x] = base;
        }
    }
    int center_y = h - bar_h / 2;
    if (center_y >= h)
        center_y = h - 1;
    int marker = (int)((cam->angle / TWO_PI) * (float)w);
    while (marker < 0)
        marker += w;
    marker %= w;
    for (int dx = -8; dx <= 8; ++dx) {
        int x = (marker + dx) % w;
        if (x < 0)
            x += w;
        if (center_y >= 0)
            surface->pixels[center_y * surface->stride + x] = RGB(240, 200, 80);
    }
}

int main(void) {
    sys_write("miniFPS:\n");
    sys_write("  W/S or Up/Down  -> move forward/back\n");
    sys_write("  A/D             -> strafe\n");
    sys_write("  Q/E or Left/Right -> turn\n");
    sys_write("  ESC/X           -> exit\n");

    unsigned int info = sys_gfx_info();
    if (!info) {
        sys_write("Graphics mode unavailable.\n");
        return 0;
    }

    int width = (int)(info >> 16);
    int height = (int)(info & 0xFFFF);
    if (width > MAX_W)
        width = MAX_W;
    if (height > MAX_H)
        height = MAX_H;

    gfx3d_surface_t surface;
    gfx3d_surface_init(&surface, width, height, frame);

    gfx3d_map_t map = {MAP_W, MAP_H, MAP_DATA};
    gfx3d_camera_t cam;
    gfx3d_camera_init(&cam, 3.5f, 3.5f, 0.0f, 1.2f);

    sys_mouse_show(0);

    unsigned int last_ticks = sys_getticks();
    int running = 1;
    while (running) {
        unsigned int now = sys_getticks();
        unsigned int delta = now - last_ticks;
        last_ticks = now;
        float dt = (float)delta * 0.016f;
        if (dt < 0.001f)
            dt = 0.016f;
        if (dt > 0.1f)
            dt = 0.1f;

        float forward = 0.0f;
        float strafe = 0.0f;
        float rotate = 0.0f;

        unsigned int key;
        while ((key = sys_trygetchar()) != 0) {
            switch (key) {
                case 'w':
                case 'W':
                case KEY_UP:
                    forward += 1.0f;
                    break;
                case 's':
                case 'S':
                case KEY_DOWN:
                    forward -= 1.0f;
                    break;
                case 'a':
                case 'A':
                    strafe -= 1.0f;
                    break;
                case 'd':
                case 'D':
                    strafe += 1.0f;
                    break;
                case 'q':
                case 'Q':
                case KEY_LEFT:
                    rotate -= 1.0f;
                    break;
                case 'e':
                case 'E':
                case KEY_RIGHT:
                    rotate += 1.0f;
                    break;
                case 27:  // ESC
                case 'x':
                case 'X':
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        float move_speed = 3.5f * dt;
        float strafe_speed = 3.0f * dt;
        float turn_speed = 1.8f * dt;

        if (rotate != 0.0f)
            gfx3d_camera_rotate(&cam, rotate * turn_speed);
        if (forward != 0.0f || strafe != 0.0f)
            gfx3d_camera_move(&map, &cam, forward * move_speed, strafe * strafe_speed);

        gfx3d_draw_scene(&surface, &map, &cam, &PALETTE);
        gfx3d_draw_minimap(&surface, &map, &cam, 16, 16, 4,
                           RGB(24, 24, 24), RGB(180, 180, 180), RGB(255, 64, 64));
        gfx3d_draw_crosshair(&surface, 6, RGB(255, 220, 120));
        draw_status_bar(&surface, &cam);

        sys_gfx_blit(surface.pixels);
        sys_sleep(1);
    }

    sys_mouse_show(1);
    sys_clear();
    sys_write("Exiting miniFPS.\n");
    return 0;
}

