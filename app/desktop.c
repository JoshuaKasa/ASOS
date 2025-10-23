#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/gfx2d.h"
#include "asoapi.h"

#define MAX_W 1280
#define MAX_H 1024
#define ICON_W      64
#define ICON_H      64
#define ICON_PAD_X  32
#define ICON_PAD_Y  56

#define MAX_FILES   64
#define NAME_MAX    32

#define ATTR(fg, bg) ((((bg) & 0x0F) << 4) | ((fg) & 0x0F))

static unsigned int backbuf[MAX_W * MAX_H];
static gfx2d_surface_t surface;
static int scr_w = 0, scr_h = 0;
static int text_cols = 80, text_rows = 25;

typedef enum {
    ICON_OTHER = 0,
    ICON_TXT   = 1,
    ICON_BIN   = 2,
} icon_type_t;

typedef struct {
    char name[NAME_MAX];
    icon_type_t type;
    int x, y;
} icon_t;

static icon_t icons[MAX_FILES];
static int icon_count = 0;
static int selected_index = -1;
static char status_line[96] = "Ready.";

static int title_row = 0;
static int hover_row = 0;
static int status_row = 0;

static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline unsigned clampu(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned)v;
}

static inline unsigned adjust_color(unsigned rgb, int delta) {
    int r = ((rgb >> 16) & 0xFF) + delta;
    int g = ((rgb >> 8) & 0xFF) + delta;
    int b = (rgb & 0xFF) + delta;
    return (clampu(r) << 16) | (clampu(g) << 8) | clampu(b);
}

static void draw_background(void) {
    gfx2d_surface_clear_gradient(&surface, GFX2D_RGB(20, 60, 100), 120, 40);
}

static icon_type_t detect_type(const char* name) {
    int len = (int)strlen(name);
    if (len >= 4) {
        const char* ext = name + len - 4;
        if (ext[0] == '.') {
            if ((ext[1] == 't' || ext[1] == 'T') &&
                (ext[2] == 'x' || ext[2] == 'X') &&
                (ext[3] == 't' || ext[3] == 'T'))
                return ICON_TXT;
            if ((ext[1] == 'b' || ext[1] == 'B') &&
                (ext[2] == 'i' || ext[2] == 'I') &&
                (ext[3] == 'n' || ext[3] == 'N'))
                return ICON_BIN;
        }
    }
    return ICON_OTHER;
}

static void load_files(void) {
    char raw[MAX_FILES * NAME_MAX];
    int count = sys_enumfiles(raw, MAX_FILES, NAME_MAX);
    icon_count = 0;
    if (count < 0)
        count = 0;
    for (int i = 0; i < count && icon_count < MAX_FILES; ++i) {
        char* nm = raw + i * NAME_MAX;
        if (!nm[0])
            continue;
        int L = (int)strlen(nm);
        if (L >= NAME_MAX)
            L = NAME_MAX - 1;
        for (int k = 0; k < L; ++k)
            icons[icon_count].name[k] = nm[k];
        icons[icon_count].name[L] = '\0';
        icons[icon_count].type = detect_type(icons[icon_count].name);
        icons[icon_count].x = 0;
        icons[icon_count].y = 0;
        icon_count++;
    }
    if (selected_index >= icon_count)
        selected_index = -1;
}

static void layout_icons(void) {
    int margin_x = 32;
    int margin_y = 48;
    int cell_w = ICON_W + ICON_PAD_X;
    int cell_h = ICON_H + ICON_PAD_Y;
    if (cell_w < ICON_W + 8)
        cell_w = ICON_W + 8;
    if (cell_h < ICON_H + 24)
        cell_h = ICON_H + 24;
    int cols = 1;
    if (scr_w > margin_x * 2 + ICON_W)
        cols = (scr_w - margin_x * 2 + (cell_w - 1)) / cell_w;
    if (cols < 1)
        cols = 1;
    for (int i = 0; i < icon_count; ++i) {
        int col = i % cols;
        int row = i / cols;
        icons[i].x = margin_x + col * cell_w;
        icons[i].y = margin_y + row * cell_h;
        if (icons[i].x + ICON_W >= scr_w - margin_x)
            icons[i].x = scr_w - margin_x - ICON_W - 1;
        if (icons[i].x < margin_x)
            icons[i].x = margin_x;
        if (icons[i].y + ICON_H >= scr_h - 40)
            icons[i].y = scr_h - ICON_H - 40;
    }
}

static void draw_file_glyph(const icon_t* ic) {
    int x0 = ic->x + 14;
    int y0 = ic->y + 20;
    int w = ICON_W - 28;
    if (w < 12) w = 12;
    unsigned ink = GFX2D_RGB(240, 240, 240);
    if (ic->type == ICON_BIN)
        ink = GFX2D_RGB(220, 255, 220);
    if (ic->type == ICON_OTHER)
        ink = GFX2D_RGB(250, 250, 200);
    for (int line = 0; line < 4; ++line) {
        int y = y0 + line * 9;
        gfx2d_fill_rect(&surface, x0, y, w, 3, adjust_color(ink, -20));
    }
}

static void draw_icon(const icon_t* ic, int hovered, int pressed, int selected) {
    unsigned base = GFX2D_RGB(160, 160, 160);
    if (ic->type == ICON_TXT)
        base = GFX2D_RGB(70, 120, 220);
    else if (ic->type == ICON_BIN)
        base = GFX2D_RGB(80, 190, 110);

    if (selected)
        base = adjust_color(base, 25);
    if (hovered)
        base = adjust_color(base, 20);
    if (pressed)
        base = adjust_color(base, -35);

    gfx2d_fill_rect(&surface, ic->x + 4, ic->y + 6, ICON_W, ICON_H, adjust_color(base, -40));
    gfx2d_fill_rect(&surface, ic->x, ic->y, ICON_W, ICON_H, base);
    gfx2d_fill_rect(&surface, ic->x, ic->y, ICON_W, 14, adjust_color(base, 35));

    // Folded corner effect
    for (int dy = 0; dy < 12; ++dy) {
        for (int dx = 0; dx < 12 - dy; ++dx)
            gfx2d_plot(&surface, ic->x + ICON_W - 12 + dx, ic->y + dy, adjust_color(base, 55));
    }

    gfx2d_stroke_rect(&surface, ic->x, ic->y, ICON_W, ICON_H, adjust_color(base, -80));
    draw_file_glyph(ic);
}

static void draw_text_row(int row, const char* msg, unsigned char attr) {
    if (row < 0 || row >= text_rows)
        return;
    int n = (int)strlen(msg);
    if (n > text_cols)
        n = text_cols;
    for (int i = 0; i < text_cols; ++i)
        sys_put_at(i, row, ' ', ATTR(0, 0));
    for (int i = 0; i < n; ++i)
        sys_put_at(i, row, msg[i], attr);
}

static void draw_title_bar(void) {
    draw_text_row(title_row, "ASOS Desktop  |  Left-click: open  Right-click: refresh  Q: quit", ATTR(0xF, 0x1));
}

static void set_status(const char* msg) {
    int L = (int)strlen(msg);
    if (L >= (int)sizeof(status_line))
        L = (int)sizeof(status_line) - 1;
    for (int i = 0; i < L; ++i)
        status_line[i] = msg[i];
    status_line[L] = '\0';
    draw_text_row(status_row, status_line, ATTR(0xF, 0x0));
}

static const char* type_name(icon_type_t t) {
    if (t == ICON_TXT) return "Text file";
    if (t == ICON_BIN) return "Program";
    return "File";
}

static void update_hover_line(int hover) {
    if (hover < 0 || hover >= icon_count) {
        draw_text_row(hover_row, "Select a file or application.", ATTR(0xF, 0x0));
        return;
    }
    char line[96];
    line[0] = '\0';
    strcpy(line, icons[hover].name);
    strcat(line, "  [");
    strcat(line, type_name(icons[hover].type));
    strcat(line, "]");
    draw_text_row(hover_row, line, ATTR(0xF, 0x0));
}

static void draw_icon_labels(int hover, int pressed) {
    int label_width = (ICON_W + ICON_PAD_X) / 8;
    if (label_width < 8)
        label_width = 8;
    for (int i = 0; i < icon_count; ++i) {
        int row = (icons[i].y + ICON_H + 8) / 16;
        if (row >= text_rows)
            row = text_rows - 1;
        if (row < 0)
            row = 0;
        int center = (icons[i].x + ICON_W / 2) / 8;
        int width = label_width;
        int start = center - width / 2;
        if (start < 0) {
            width += start;
            start = 0;
        }
        if (start + width > text_cols)
            width = text_cols - start;
        if (width <= 0)
            continue;
        for (int j = 0; j < width; ++j)
            sys_put_at(start + j, row, ' ', ATTR(0, 0));
        unsigned char fg = 0xF;
        if (icons[i].type == ICON_TXT)
            fg = 0x9;
        else if (icons[i].type == ICON_BIN)
            fg = 0xA;
        if (i == hover)
            fg = 0xE;
        if (i == pressed)
            fg = 0xF;
        if (i == selected_index)
            fg = 0xB;
        int len = (int)strlen(icons[i].name);
        if (len > width)
            len = width;
        for (int j = 0; j < len; ++j)
            sys_put_at(start + j, row, icons[i].name[j], ATTR(fg, 0));
    }
}

static int hit_test(int mx, int my) {
    for (int i = 0; i < icon_count; ++i) {
        if (mx >= icons[i].x && mx < icons[i].x + ICON_W &&
            my >= icons[i].y && my < icons[i].y + ICON_H)
            return i;
    }
    return -1;
}

static void open_icon(int idx) {
    if (idx < 0 || idx >= icon_count)
        return;

    char target[NAME_MAX];
    target[0] = '\0';
    strcpy(target, icons[idx].name);

    selected_index = idx;
    char msg[96];
    strcpy(msg, "Opening ");
    strcat(msg, target);
    set_status(msg);

    sys_mouse_show(0);
    if (icons[idx].type == ICON_TXT) {
        char cmd[64];
        strcpy(cmd, "textedit.bin ");
        strcat(cmd, target);
        sys_exec(cmd);
    } else if (icons[idx].type == ICON_BIN) {
        sys_exec(target);
    } else {
        set_status("No viewer for this file type.");
        sys_mouse_show(1);
        return;
    }
    sys_mouse_show(1);

    set_status("Returned to desktop.");
    load_files();
    layout_icons();

    selected_index = -1;
    for (int i = 0; i < icon_count; ++i) {
        if (strcmp(icons[i].name, target) == 0) {
            selected_index = i;
            break;
        }
    }
}

static void render_scene(int hover, int pressed) {
    draw_background();
    for (int i = 0; i < icon_count; ++i) {
        draw_icon(&icons[i], i == hover, i == pressed, i == selected_index);
    }
    gfx2d_surface_present(&surface, sys_gfx_blit);
    draw_icon_labels(hover, pressed);
    draw_title_bar();
    draw_text_row(status_row, status_line, ATTR(0xF, 0x0));
    update_hover_line(hover);
    sys_setcursor(text_cols - 1, text_rows - 1);
}

void main(void) {
    unsigned int info = sys_gfx_info();
    if (!info) {
        sys_write("desktop: graphics mode required.\n");
        sys_exit();
    }
    scr_w = (int)((info >> 16) & 0xFFFF);
    scr_h = (int)(info & 0xFFFF);
    if (scr_w <= 0 || scr_h <= 0) {
        sys_write("desktop: invalid framebuffer.\n");
        sys_exit();
    }
    if (scr_w > MAX_W)
        scr_w = MAX_W;
    if (scr_h > MAX_H)
        scr_h = MAX_H;
    gfx2d_surface_init(&surface, scr_w, scr_h, backbuf);
    sys_getsize(&text_cols, &text_rows);
    if (text_cols < 40)
        text_cols = 40;
    if (text_rows < 10)
        text_rows = 10;
    title_row = 0;
    status_row = (text_rows >= 2) ? text_rows - 1 : 0;
    hover_row = (text_rows >= 3) ? text_rows - 2 : status_row;
    sys_clear();
    sys_mouse_show(1);
    load_files();
    layout_icons();
    if (icon_count == 0)
        set_status("No files found in ASOFS.");
    else
        set_status("Welcome to the ASOS desktop.");
    int hover = -1;
    int last_hover = -2;
    int pressed = -1;
    int prev_buttons = 0;
    int need_redraw = 1;
    while (1) {
        if (need_redraw || hover != last_hover) {
            render_scene(hover, pressed);
            need_redraw = 0;
            last_hover = hover;
        }
        mouse_info_t mi;
        sys_mouse_get(&mi);
        hover = hit_test(mi.x, mi.y);
        int buttons = (int)mi.buttons;
        if ((buttons & 2) && !(prev_buttons & 2)) {
            load_files();
            layout_icons();
            set_status("File list refreshed.");
            need_redraw = 1;
        }
        if ((buttons & 1) && !(prev_buttons & 1)) {
            pressed = hover;
            need_redraw = 1;
        }
        if (!(buttons & 1) && (prev_buttons & 1)) {
            if (pressed >= 0 && pressed == hover)
                open_icon(pressed);
            pressed = -1;
            need_redraw = 1;
        }
        prev_buttons = buttons;
        unsigned int key = sys_trygetchar();
        if (key) {
            char c = (char)key;
            if (c == 'q' || c == 'Q' || c == 27) {
                break;
            } else if (c == 'r' || c == 'R') {
                load_files();
                layout_icons();
                set_status("File list refreshed.");
                need_redraw = 1;
            }
        }
        asm volatile("hlt");
    }
    sys_mouse_show(0);
    sys_clear();
    sys_exit();
}
