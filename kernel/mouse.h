#pragma once
#include <stdint.h>

void mouse_init(int gfx_enabled);
void mouse_on_timer_tick(void);
void mouse_get(int* x, int* y, unsigned* buttons);
void mouse_set_visible(int visible);
