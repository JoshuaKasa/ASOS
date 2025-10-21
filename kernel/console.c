#include "console.h"
#include "gfx.h"
#include "vga.h"
#include "../ui/ui_gfx.h"
#include "../lib/string.h"
#include <stdint.h>

#define CHAR_W 8
#define CHAR_H 16
#define MAX_COLS 200
#define MAX_ROWS 100

static int use_gfx = 0;
static uint32_t fg_col = 0x00FFFFFF;
static uint32_t bg_col = 0x00000000;

static int cursor_x = 0, cursor_y = 0;
static int cols = 80, rows = 25;

static char screen_buf[MAX_ROWS][MAX_COLS];

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

// VGA 16-color palette -> 0x00RRGGBB
static uint32_t pal[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAAAA00,
    0x0000AA, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};


static void compute_grid_from_gfx(void){
    const gfx_info_t* gi = gfx_info();
    if (!gi) { cols = 80; rows = 25; return; }
    cols = gi->w / CHAR_W; if (cols > MAX_COLS) cols = MAX_COLS;
    rows = gi->h / CHAR_H; if (rows > MAX_ROWS) rows = MAX_ROWS;
}

static void redraw_all_gfx(void){
    for (int y = 0; y < rows; ++y){
        for (int x = 0; x < cols; ++x){
            char c = screen_buf[y][x] ? screen_buf[y][x] : ' ';
            ui_gfx_put_char(x, y, c, fg_col, bg_col);
        }
    }
}

static void clear_buffers(void){
    for (int y = 0; y < MAX_ROWS; ++y)
        for (int x = 0; x < MAX_COLS; ++x)
            screen_buf[y][x] = ' ';
    cursor_x = cursor_y = 0;
}

void console_init(int use_gfx_mode) {
    use_gfx = use_gfx_mode;

    if (use_gfx) {
        compute_grid_from_gfx();
        clear_buffers();
        gfx_clear(bg_col);
        redraw_all_gfx();
    } else {
        cols = 80; rows = 25;
        clear_buffers();
        vga_clear();
    }
}

void console_clear(void) {
    clear_buffers();
    if (use_gfx) {
        gfx_clear(bg_col);
        redraw_all_gfx();
    } else {
        vga_clear();
    }
}

void console_setcolor(uint32_t fg, uint32_t bg) {
    fg_col = fg;
    bg_col = bg;
}

void console_get_size(int* out_cols, int* out_rows){
    if (out_cols) *out_cols = cols;
    if (out_rows) *out_rows = rows;
}

void console_setcursor(int x, int y){
    cursor_x = clamp(x, 0, cols-1);
    cursor_y = clamp(y, 0, rows-1);
    if (!use_gfx) vga_set_pos(cursor_x, cursor_y);
}

static void scroll_up(void){
    for (int y = 0; y < rows-1; ++y)
        for (int x = 0; x < cols; ++x)
            screen_buf[y][x] = screen_buf[y+1][x];

    for (int x = 0; x < cols; ++x)
        screen_buf[rows-1][x] = ' ';

    if (use_gfx) {
        redraw_all_gfx();
    } else {
        vga_clear();
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x)
                vga_putchar_at(x, y, screen_buf[y][x], 0x0F);
    }
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = cols - 1;
        }
        screen_buf[cursor_y][cursor_x] = ' ';
        if (use_gfx) ui_gfx_put_char(cursor_x, cursor_y, ' ', fg_col, bg_col);
        else         vga_putchar_at(cursor_x, cursor_y, ' ', 0x0F);
    } else {
        screen_buf[cursor_y][cursor_x] = c;
        if (use_gfx) ui_gfx_put_char(cursor_x, cursor_y, c, fg_col, bg_col);
        else         vga_putchar_at(cursor_x, cursor_y, c, 0x0F);

        cursor_x++;
        if (cursor_x >= cols) { cursor_x = 0; cursor_y++; }
    }

    if (cursor_y >= rows) {
        scroll_up();
        cursor_y = rows - 1;
    }

    if (!use_gfx) vga_set_pos(cursor_x, cursor_y);
}

void console_backspace(void){
    console_putchar('\b');
}

void console_write(const char* s) {
    for (; *s; ++s) console_putchar(*s);
}

void console_put_at(int x, int y, char c) {
    x = clamp(x, 0, cols-1);
    y = clamp(y, 0, rows-1);
    screen_buf[y][x] = c;

    if (use_gfx) ui_gfx_put_char(x, y, c, fg_col, bg_col);
    else         vga_putchar_at(x, y, c, 0x0F);
}

void console_put_at_color(int x, int y, char c, uint8_t attr) {
    x = clamp(x, 0, cols-1);
    y = clamp(y, 0, rows-1);
    screen_buf[y][x] = c;

    uint32_t fg = pal[attr & 0x0F];
    uint32_t bg = pal[(attr >> 4) & 0x0F];

    if (use_gfx)
        ui_gfx_put_char(x, y, c, fg, bg);
    else
        vga_putchar_at(x, y, c, attr);
}

void console_redraw(void) {
    if (!use_gfx) {
        vga_clear();

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                vga_putchar_at(x, y, screen_buf[y][x], 0x0F);
            }
        }
        vga_set_pos(cursor_x, cursor_y);

        return;
    }

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            char c = screen_buf[y][x];

            if (!c || c == ' ')
                continue;
            gfx_draw_char_fg(x * CHAR_W, y * CHAR_H, c, fg_col);
        }
    }
}

void console_overlay_row_fg(int row) {
    if (!use_gfx)
        return;

    if (row < 0 || row >= rows)
        return;

    int y0 = row * CHAR_H;

    for (int x = 0; x < cols; x++) {
        char c = screen_buf[row][x];

        if (!c || c == ' ')
            continue;
        gfx_draw_char_fg(x * CHAR_W, y0, c, fg_col);
    }
}
