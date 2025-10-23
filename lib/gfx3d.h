#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;
    int height;
    int stride;
    uint32_t* pixels;
} gfx3d_surface_t;

typedef struct {
    int width;
    int height;
    const uint8_t* cells;
} gfx3d_map_t;

typedef struct {
    float x;
    float y;
    float angle;
    float fov;
} gfx3d_camera_t;

typedef struct {
    uint32_t ceiling;
    uint32_t floor;
    uint32_t walls[16];
} gfx3d_palette_t;

void gfx3d_surface_init(gfx3d_surface_t* s, int width, int height, uint32_t* pixels);
void gfx3d_surface_fill(gfx3d_surface_t* s, uint32_t rgb);
void gfx3d_camera_init(gfx3d_camera_t* cam, float x, float y, float angle, float fov);
void gfx3d_camera_rotate(gfx3d_camera_t* cam, float delta);
void gfx3d_camera_move(const gfx3d_map_t* map, gfx3d_camera_t* cam, float forward, float strafe);
int  gfx3d_map_is_solid(const gfx3d_map_t* map, float x, float y);
void gfx3d_draw_scene(gfx3d_surface_t* s, const gfx3d_map_t* map, const gfx3d_camera_t* cam, const gfx3d_palette_t* pal);
void gfx3d_draw_minimap(gfx3d_surface_t* s, const gfx3d_map_t* map, const gfx3d_camera_t* cam,
                        int origin_x, int origin_y, int scale,
                        uint32_t floor_rgb, uint32_t wall_rgb, uint32_t player_rgb);
void gfx3d_draw_crosshair(gfx3d_surface_t* s, int half_extent, uint32_t rgb);

#ifdef __cplusplus
}
#endif

