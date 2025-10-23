#include "gfx3d.h"

#define GFX3D_PI 3.14159265358979323846f
#define GFX3D_TWO_PI 6.28318530717958647692f
#define GFX3D_HALF_PI 1.57079632679489661923f

static float gfx3d_wrap_angle(float a) {
    while (a > GFX3D_PI)
        a -= GFX3D_TWO_PI;
    while (a < -GFX3D_PI)
        a += GFX3D_TWO_PI;
    return a;
}

static float gfx3d_absf(float v) {
    return v < 0.0f ? -v : v;
}

static float gfx3d_fast_sin(float x) {
    x = gfx3d_wrap_angle(x);
    float x2 = x * x;
    float term = x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f);
    return term;
}

static float gfx3d_fast_cos(float x) {
    return gfx3d_fast_sin(x + GFX3D_HALF_PI);
}

static float gfx3d_fast_inv(float v) {
    if (v > -0.000001f && v < 0.000001f)
        return v >= 0.0f ? 1.0e30f : -1.0e30f;
    return 1.0f / v;
}

static uint32_t gfx3d_modulate(uint32_t color, float scalar) {
    if (scalar < 0.0f)
        scalar = 0.0f;
    if (scalar > 1.0f)
        scalar = 1.0f;
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;
    r = (uint32_t)(r * scalar);
    g = (uint32_t)(g * scalar);
    b = (uint32_t)(b * scalar);
    return (r << 16) | (g << 8) | b;
}

void gfx3d_surface_init(gfx3d_surface_t* s, int width, int height, uint32_t* pixels) {
    if (!s)
        return;
    s->width = width > 0 ? width : 0;
    s->height = height > 0 ? height : 0;
    s->stride = width > 0 ? width : 0;
    s->pixels = pixels;
}

void gfx3d_surface_fill(gfx3d_surface_t* s, uint32_t rgb) {
    if (!s || !s->pixels)
        return;
    int total = s->height * s->stride;
    for (int i = 0; i < total; ++i)
        s->pixels[i] = rgb;
}

void gfx3d_camera_init(gfx3d_camera_t* cam, float x, float y, float angle, float fov) {
    if (!cam)
        return;
    cam->x = x;
    cam->y = y;
    cam->angle = gfx3d_wrap_angle(angle);
    cam->fov = fov;
}

void gfx3d_camera_rotate(gfx3d_camera_t* cam, float delta) {
    if (!cam)
        return;
    cam->angle = gfx3d_wrap_angle(cam->angle + delta);
}

static int gfx3d_map_cell(const gfx3d_map_t* map, int x, int y) {
    if (!map || !map->cells)
        return 1;
    if (x < 0 || y < 0 || x >= map->width || y >= map->height)
        return 1;
    return map->cells[y * map->width + x];
}

int gfx3d_map_is_solid(const gfx3d_map_t* map, float x, float y) {
    return gfx3d_map_cell(map, (int)x, (int)y) != 0;
}

void gfx3d_camera_move(const gfx3d_map_t* map, gfx3d_camera_t* cam, float forward, float strafe) {
    if (!cam)
        return;
    float sin_a = gfx3d_fast_sin(cam->angle);
    float cos_a = gfx3d_fast_cos(cam->angle);
    float dx = cos_a * forward - sin_a * strafe;
    float dy = sin_a * forward + cos_a * strafe;
    float new_x = cam->x + dx;
    float new_y = cam->y + dy;

    if (!gfx3d_map_is_solid(map, new_x, cam->y))
        cam->x = new_x;
    if (!gfx3d_map_is_solid(map, cam->x, new_y))
        cam->y = new_y;
}

void gfx3d_draw_scene(gfx3d_surface_t* s, const gfx3d_map_t* map, const gfx3d_camera_t* cam, const gfx3d_palette_t* pal) {
    if (!s || !s->pixels || !map || !map->cells || !cam || !pal)
        return;

    int w = s->width;
    int h = s->height;
    if (w <= 0 || h <= 0)
        return;

    for (int x = 0; x < w; ++x) {
        float rel = (w > 1) ? ((float)x / (float)(w - 1)) : 0.0f;
        float ray_angle = cam->angle + (rel - 0.5f) * cam->fov;
        float ray_dir_x = gfx3d_fast_cos(ray_angle);
        float ray_dir_y = gfx3d_fast_sin(ray_angle);

        int map_x = (int)cam->x;
        int map_y = (int)cam->y;

        float delta_dist_x = gfx3d_absf(gfx3d_fast_inv(ray_dir_x));
        float delta_dist_y = gfx3d_absf(gfx3d_fast_inv(ray_dir_y));
        float side_dist_x;
        float side_dist_y;

        int step_x;
        int step_y;

        if (ray_dir_x < 0.0f) {
            step_x = -1;
            side_dist_x = (cam->x - (float)map_x) * delta_dist_x;
        } else {
            step_x = 1;
            side_dist_x = ((float)map_x + 1.0f - cam->x) * delta_dist_x;
        }

        if (ray_dir_y < 0.0f) {
            step_y = -1;
            side_dist_y = (cam->y - (float)map_y) * delta_dist_y;
        } else {
            step_y = 1;
            side_dist_y = ((float)map_y + 1.0f - cam->y) * delta_dist_y;
        }

        int tile = 0;
        int side = 0;
        for (int iter = 0; iter < 128; ++iter) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }
            tile = gfx3d_map_cell(map, map_x, map_y);
            if (tile)
                break;
        }

        if (!tile) {
            for (int y = 0; y < h; ++y) {
                uint32_t color = (y < h / 2) ? pal->ceiling : pal->floor;
                s->pixels[y * s->stride + x] = color;
            }
            continue;
        }

        float perp_dist;
        if (side == 0)
            perp_dist = (side_dist_x - delta_dist_x) * gfx3d_fast_cos(ray_angle - cam->angle);
        else
            perp_dist = (side_dist_y - delta_dist_y) * gfx3d_fast_cos(ray_angle - cam->angle);

        if (perp_dist < 0.0001f)
            perp_dist = 0.0001f;

        int line_height = (int)(h / perp_dist);
        if (line_height < 1)
            line_height = 1;
        int draw_start = -line_height / 2 + h / 2;
        int draw_end = line_height / 2 + h / 2;
        if (draw_start < 0)
            draw_start = 0;
        if (draw_end >= h)
            draw_end = h - 1;

        uint32_t wall_rgb = pal->walls[tile & 0x0F];
        float base_light = side ? 0.65f : 0.85f;
        float dist_light = 1.0f / (1.0f + perp_dist * 0.15f);
        uint32_t shade = gfx3d_modulate(wall_rgb, base_light * dist_light);

        for (int y = 0; y < draw_start; ++y)
            s->pixels[y * s->stride + x] = pal->ceiling;
        for (int y = draw_start; y <= draw_end; ++y)
            s->pixels[y * s->stride + x] = shade;
        for (int y = draw_end + 1; y < h; ++y)
            s->pixels[y * s->stride + x] = pal->floor;
    }
}

void gfx3d_draw_minimap(gfx3d_surface_t* s, const gfx3d_map_t* map, const gfx3d_camera_t* cam,
                        int origin_x, int origin_y, int scale,
                        uint32_t floor_rgb, uint32_t wall_rgb, uint32_t player_rgb) {
    if (!s || !s->pixels || !map || !map->cells || !cam)
        return;
    if (scale <= 0)
        return;

    for (int my = 0; my < map->height; ++my) {
        for (int mx = 0; mx < map->width; ++mx) {
            uint32_t color = gfx3d_map_cell(map, mx, my) ? wall_rgb : floor_rgb;
            for (int sy = 0; sy < scale; ++sy) {
                int yy = origin_y + my * scale + sy;
                if (yy < 0 || yy >= s->height)
                    continue;
                uint32_t* row = &s->pixels[yy * s->stride];
                for (int sx = 0; sx < scale; ++sx) {
                    int xx = origin_x + mx * scale + sx;
                    if (xx < 0 || xx >= s->width)
                        continue;
                    row[xx] = color;
                }
            }
        }
    }

    int px = origin_x + (int)(cam->x * scale);
    int py = origin_y + (int)(cam->y * scale);
    for (int dy = -1; dy <= 1; ++dy) {
        int yy = py + dy;
        if (yy < 0 || yy >= s->height)
            continue;
        uint32_t* row = &s->pixels[yy * s->stride];
        for (int dx = -1; dx <= 1; ++dx) {
            int xx = px + dx;
            if (xx < 0 || xx >= s->width)
                continue;
            row[xx] = player_rgb;
        }
    }

    float dir_x = gfx3d_fast_cos(cam->angle);
    float dir_y = gfx3d_fast_sin(cam->angle);
    int line_len = scale * 2;
    for (int i = 0; i < line_len; ++i) {
        int xx = px + (int)(dir_x * i);
        int yy = py + (int)(dir_y * i);
        if (xx < 0 || xx >= s->width || yy < 0 || yy >= s->height)
            break;
        s->pixels[yy * s->stride + xx] = player_rgb;
    }
}

void gfx3d_draw_crosshair(gfx3d_surface_t* s, int half_extent, uint32_t rgb) {
    if (!s || !s->pixels || half_extent <= 0)
        return;
    int cx = s->width / 2;
    int cy = s->height / 2;
    for (int dx = -half_extent; dx <= half_extent; ++dx) {
        int xx = cx + dx;
        if (xx < 0 || xx >= s->width)
            continue;
        s->pixels[cy * s->stride + xx] = rgb;
    }
    for (int dy = -half_extent; dy <= half_extent; ++dy) {
        int yy = cy + dy;
        if (yy < 0 || yy >= s->height)
            continue;
        s->pixels[yy * s->stride + cx] = rgb;
    }
}

