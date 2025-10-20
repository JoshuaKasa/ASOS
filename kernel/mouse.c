#include "mouse.h"
#include "io.h"
#include "irq.h"
#include "gfx.h"
#include "console.h"
#include <stdint.h>

#define PS2_CMD      0x64
#define PS2_DATA     0x60

// Status bits
#define ST_OBF  0x01  // Output buffer full
#define ST_IBF  0x02  // Input buffer full
#define ST_AUX  0x20  // 1 = data from mouse

static volatile int mx = 0, my = 0;
static volatile unsigned mbtn = 0;
static int scr_w = 640, scr_h = 480;
static int cursor_visible = 1;
static int painter_enabled = 0;

// 16x16 hand drawn sprite
#define CUR_W 16
#define CUR_H 16
static const uint8_t cursor_mask[CUR_H][CUR_W] = {
    // 1 = draw (white), 0 = transparent 
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static uint32_t saved_bg[CUR_W * CUR_H]; // RGB
static int last_x = -10000, last_y = -10000;
static int have_saved = 0;
static volatile int redraw_needed = 1;

static void wait_write(void) {
    for (int i = 0; i < 100000; ++i) { 
        if (!(inb(PS2_CMD) & ST_IBF)) 
            return; 
    }
}

static void wait_read(void) {
    for (int i = 0; i < 100000; ++i) { 
        if (inb(PS2_CMD) & ST_OBF) 
            return; 
    }
}

static void mouse_write(uint8_t val){
    wait_write(); outb(PS2_CMD, 0xD4);
    wait_write(); outb(PS2_DATA, val);
}
static uint8_t mouse_read(void){
    wait_read(); return inb(PS2_DATA);
}

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

static void cursor_erase(void){
    if (!have_saved) return;

    int w = (last_x + CUR_W > scr_w) ? (scr_w - last_x) : CUR_W;
    int h = (last_y + CUR_H > scr_h) ? (scr_h - last_y) : CUR_H;

    if (w<=0 || h<=0) return;

    for (int j=0; j<h; ++j)
        for (int i=0; i<w; ++i)
            gfx_putpixel(last_x+i, last_y+j, saved_bg[j*CUR_W+i]);
}

static void cursor_draw_at(int x, int y){
    int w = (x + CUR_W > scr_w) ? (scr_w - x) : CUR_W;
    int h = (y + CUR_H > scr_h) ? (scr_h - y) : CUR_H;
    if (w<=0 || h<=0) { have_saved = 0; return; }

    for (int j=0; j<h; ++j)
        for (int i=0; i<w; ++i)
            saved_bg[j*CUR_W+i] = gfx_get_pixel(x+i, y+j);

    for (int j=0; j<h; ++j)
        for (int i=0; i<w; ++i)
            if (cursor_mask[j][i]) gfx_putpixel(x+i, y+j, 0x00FFFFFF);

    have_saved = 1;
    last_x = x; last_y = y;
}

void mouse_on_timer_tick(void){
    if (!painter_enabled || !cursor_visible) return;

    static unsigned ctr = 0;
    if (++ctr % 9 == 0) redraw_needed = 1;

    if (!redraw_needed) return;
    redraw_needed = 0;

    cursor_erase();
    cursor_draw_at(mx, my);
}

void mouse_set_visible(int visible){
    cursor_visible = visible ? 1 : 0;
    redraw_needed = 1;
}

void mouse_get(int* x, int* y, unsigned* buttons){
    if (x) *x = mx;
    if (y) *y = my;
    if (buttons) *buttons = mbtn;
}

static uint8_t pkt[3];
static int    idx = 0;

static void mouse_irq(regs_t* r){
    (void)r;
    for (;;) {
        uint8_t st = inb(PS2_CMD);
        if (!(st & ST_OBF)) break;

        uint8_t b = inb(PS2_DATA);
        if (!(st & ST_AUX)) {
            continue;
        }

        if (idx == 0 && !(b & 0x08)) {
            idx = 0;
            continue;
        }

        pkt[idx++] = b;
        if (idx == 3) {
            idx = 0;

            int dx = (int8_t)pkt[1];
            int dy = -(int8_t)pkt[2]; // PS/2: inverted y

            mx = clamp(mx + dx, 0, scr_w - 1);
            my = clamp(my + dy, 0, scr_h - 1);

            mbtn = ((pkt[0] & 1) ? 1 : 0)
                 | ((pkt[0] & 2) ? 2 : 0)
                 | ((pkt[0] & 4) ? 4 : 0);

            redraw_needed = 1;
        }
    }
}

void mouse_init(int gfx_enabled){
    const gfx_info_t* gi = gfx_info();

    if (gi && gi->w && gi->h) {
        scr_w = gi->w; scr_h = gi->h;
        painter_enabled = gfx_enabled ? 1 : 0;
    } 
    else {
        scr_w = 640; scr_h = 400;
        painter_enabled = 0;
    }

    wait_write(); outb(PS2_CMD, 0xA8);
    wait_write(); outb(PS2_CMD, 0x20);

    wait_read();  

    uint8_t status = inb(PS2_DATA);

    status |= 0x02; // Enable mouse IRQ12
    status &= ~0x20; // Enable mouse clock

    wait_write(); outb(PS2_CMD, 0x60);
    wait_write(); outb(PS2_DATA, status);

    mouse_write(0xF6); (void)mouse_read(); // ACK
    mouse_write(0xF4); (void)mouse_read(); // ACK

    mx = scr_w/2; 
    my = scr_h/2; 
    mbtn = 0;
    last_x = last_y = -10000; 
    have_saved = 0; 
    redraw_needed = 1;

    // Hook IRQ12
    register_interrupt_handler(12, mouse_irq);
}
