#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "../lib/gfx2d.h"
#include "../ui/theme.h"
#include "asoapi.h"

#define MAX_W 1280
#define MAX_H 1024
#define ICON_W      64
#define ICON_H      64
#define ICON_PAD_X  32
#define ICON_PAD_Y  56

#define MAX_FILES   64
#define NAME_MAX    32

static unsigned int backbuf[MAX_W * MAX_H];
static gfx2d_surface_t surface;
static int scr_w = 0, scr_h = 0;
static int text_cols = 80, text_rows = 25;

static inline uint8_t attr_fg(uint8_t attr) {
    return attr & 0x0F;
}

static inline uint8_t attr_bg(uint8_t attr) {
    return (attr >> 4) & 0x0F;
}

typedef struct {
    ui_theme_id_t id;
    const ui_theme_t* def;
    uint32_t canvas_rgb;
    uint32_t panel_rgb;
    uint32_t panel_accent_rgb;
    uint32_t accent_rgb;
    uint32_t selection_rgb;
    uint32_t success_rgb;
    uint32_t warning_rgb;
    uint32_t text_rgb;
    uint32_t muted_rgb;
    uint8_t canvas_attr;
    uint8_t panel_attr;
    uint8_t panel_accent_attr;
    uint8_t title_text_attr;
    uint8_t title_fill_attr;
    uint8_t text_canvas_attr;
    uint8_t text_panel_attr;
    uint8_t muted_panel_attr;
    uint8_t accent_panel_attr;
    uint8_t status_text_attr;
    uint8_t status_fill_attr;
    uint8_t hover_attr;
    uint8_t label_default_attr;
    uint8_t label_hover_attr;
    uint8_t label_selected_attr;
    uint8_t label_pressed_attr;
} desktop_theme_t;

static desktop_theme_t g_theme;

static void set_status(const char* msg);

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

static inline uint8_t compose_attr(uint8_t fg, uint8_t bg) {
    return UI_ATTR(fg & 0x0F, bg & 0x0F);
}

static inline uint32_t pick_color(uint32_t preferred, uint32_t fallback) {
    return preferred ? preferred : fallback;
}

static void desktop_apply_theme(ui_theme_id_t id) {
    int count = ui_theme_count();
    if (count <= 0)
        id = UI_THEME_DARK;
    else if ((int)id < 0 || (int)id >= count)
        id = ui_theme_default_id();

    const ui_theme_t* theme = ui_theme_get(id);
    g_theme.id = id;
    g_theme.def = theme;

    g_theme.canvas_rgb        = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_CANVAS_BG), 0x122033u);
    g_theme.panel_rgb         = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_PANEL_BG), g_theme.canvas_rgb);
    g_theme.panel_accent_rgb  = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_PANEL_ACCENT), g_theme.panel_rgb);
    g_theme.accent_rgb        = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_ACCENT), g_theme.panel_accent_rgb);
    g_theme.selection_rgb     = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_SELECTION), g_theme.accent_rgb);
    g_theme.success_rgb       = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_SUCCESS), g_theme.accent_rgb);
    g_theme.warning_rgb       = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_WARNING), g_theme.accent_rgb);
    g_theme.text_rgb          = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_TEXT_PRIMARY), 0xE6EEF5u);
    g_theme.muted_rgb         = pick_color(ui_theme_rgb_role(theme, UI_THEME_ROLE_TEXT_MUTED), g_theme.text_rgb);

    uint8_t canvas_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_CANVAS_BG);
    if (!canvas_attr)
        canvas_attr = UI_ATTR(0x0, 0x0);
    g_theme.canvas_attr = canvas_attr;

    uint8_t panel_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_PANEL_BG);
    if (!panel_attr)
        panel_attr = canvas_attr;
    g_theme.panel_attr = panel_attr;

    uint8_t panel_accent_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_PANEL_ACCENT);
    if (!panel_accent_attr)
        panel_accent_attr = panel_attr;
    g_theme.panel_accent_attr = panel_accent_attr;

    uint8_t text_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_TEXT_PRIMARY);
    if (!text_attr)
        text_attr = compose_attr(0xF, attr_bg(canvas_attr));

    uint8_t muted_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_TEXT_MUTED);
    if (!muted_attr)
        muted_attr = text_attr;

    uint8_t accent_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_ACCENT);
    if (!accent_attr)
        accent_attr = text_attr;

    uint8_t success_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_SUCCESS);
    if (!success_attr)
        success_attr = accent_attr;

    uint8_t selection_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_SELECTION);
    if (!selection_attr)
        selection_attr = compose_attr(attr_fg(text_attr), attr_bg(panel_attr));

    g_theme.text_canvas_attr   = compose_attr(attr_fg(text_attr), attr_bg(canvas_attr));
    g_theme.text_panel_attr    = compose_attr(attr_fg(text_attr), attr_bg(panel_attr));
    g_theme.muted_panel_attr   = compose_attr(attr_fg(muted_attr), attr_bg(panel_attr));
    g_theme.accent_panel_attr  = compose_attr(attr_fg(accent_attr), attr_bg(panel_attr));
    g_theme.status_text_attr   = g_theme.text_panel_attr;
    g_theme.status_fill_attr   = panel_attr;
    g_theme.hover_attr         = selection_attr;
    g_theme.label_default_attr = g_theme.text_canvas_attr;
    g_theme.label_hover_attr   = selection_attr;
    g_theme.label_selected_attr = compose_attr(attr_fg(accent_attr), attr_bg(selection_attr));
    g_theme.label_pressed_attr  = compose_attr(attr_fg(success_attr), attr_bg(selection_attr));
    g_theme.title_fill_attr    = panel_accent_attr;
    g_theme.title_text_attr    = compose_attr(attr_fg(text_attr), attr_bg(panel_accent_attr));
}

static void desktop_refresh_theme(void) {
    ui_theme_id_t id = (ui_theme_id_t)sys_theme_current();
    desktop_apply_theme(id);
}

static void desktop_cycle_theme(void) {
    int count = ui_theme_count();
    if (count <= 0)
        return;

    ui_theme_id_t next = (g_theme.id + 1) % count;
    if (sys_theme_set(next) == 0) {
        desktop_apply_theme(next);
        char msg[64];
        strcpy(msg, "Theme changed to ");
        strcat(msg, ui_theme_name(next));
        strcat(msg, ".");
        set_status(msg);
    } else {
        set_status("Unable to change theme.");
    }
}

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

static int color_luminance(uint32_t rgb) {
    int r = (int)((rgb >> 16) & 0xFFu);
    int g = (int)((rgb >> 8) & 0xFFu);
    int b = (int)(rgb & 0xFFu);
    return (r * 3 + g * 6 + b) / 10;
}

static void draw_background(void) {
    uint32_t base = g_theme.canvas_rgb ? g_theme.canvas_rgb : GFX2D_RGB(20, 60, 100);
    int lum = color_luminance(base);
    int vertical_strength = (lum > 180) ? 40 : 120;
    int horizontal_strength = (lum > 180) ? 20 : 48;

    gfx2d_surface_clear_gradient(&surface, base, vertical_strength, horizontal_strength);

    int header_h = ICON_PAD_Y;
    if (header_h < 48)
        header_h = 48;
    if (header_h > scr_h)
        header_h = scr_h;

    if (g_theme.panel_accent_rgb)
        gfx2d_fill_rect(&surface, 0, 0, scr_w, header_h, g_theme.panel_accent_rgb);

    int footer_h = ICON_PAD_Y;
    if (footer_h < 40)
        footer_h = 40;
    if (footer_h > scr_h)
        footer_h = scr_h;

    if (g_theme.panel_rgb)
        gfx2d_fill_rect(&surface, 0, scr_h - footer_h, scr_w, footer_h, g_theme.panel_rgb);
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
    if (w < 12)
        w = 12;

    uint32_t base = g_theme.text_rgb ? g_theme.text_rgb : GFX2D_RGB(240, 240, 240);
    if (ic->type == ICON_BIN)
        base = pick_color(g_theme.success_rgb, base);
    else if (ic->type == ICON_TXT)
        base = pick_color(g_theme.accent_rgb, base);
    else
        base = pick_color(g_theme.muted_rgb, base);

    for (int line = 0; line < 4; ++line) {
        int y = y0 + line * 9;
        uint32_t shade = adjust_color(base, (line % 2 == 0) ? -25 : -10);
        gfx2d_fill_rect(&surface, x0, y, w, 3, shade);
    }
}

static void draw_icon(const icon_t* ic, int hovered, int pressed, int selected) {
    uint32_t base = pick_color(g_theme.panel_accent_rgb, GFX2D_RGB(160, 160, 160));
    if (ic->type == ICON_TXT)
        base = pick_color(g_theme.accent_rgb, base);
    else if (ic->type == ICON_BIN)
        base = pick_color(g_theme.success_rgb, base);
    else
        base = pick_color(g_theme.panel_rgb, base);

    uint32_t body = base;
    if (selected)
        body = adjust_color(body, 30);
    if (hovered)
        body = adjust_color(body, 18);
    if (pressed)
        body = adjust_color(body, -38);

    uint32_t inset = adjust_color(body, -35);
    uint32_t header = adjust_color(body, 35);
    uint32_t fold = adjust_color(body, 55);
    uint32_t outline = adjust_color(body, -75);

    gfx2d_fill_rect(&surface, ic->x + 4, ic->y + 6, ICON_W, ICON_H, inset);
    gfx2d_fill_rect(&surface, ic->x, ic->y, ICON_W, ICON_H, body);
    gfx2d_fill_rect(&surface, ic->x, ic->y, ICON_W, 14, header);

    for (int dy = 0; dy < 12; ++dy) {
        for (int dx = 0; dx < 12 - dy; ++dx)
            gfx2d_plot(&surface, ic->x + ICON_W - 12 + dx, ic->y + dy, fold);
    }

    gfx2d_stroke_rect(&surface, ic->x, ic->y, ICON_W, ICON_H, outline);
    draw_file_glyph(ic);
}

static void draw_text_row(int row, const char* msg, uint8_t text_attr, uint8_t fill_attr) {
    if (row < 0 || row >= text_rows)
        return;
    if (!msg)
        msg = "";
    int n = (int)strlen(msg);
    if (n > text_cols)
        n = text_cols;
    for (int i = 0; i < text_cols; ++i)
        sys_put_at(i, row, ' ', fill_attr);
    for (int i = 0; i < n; ++i)
        sys_put_at(i, row, msg[i], text_attr);
}

static void draw_title_bar(void) {
    char title[192];
    title[0] = '\0';
    strcpy(title, "ASOS Desktop — ");
    strcat(title, ui_theme_name(g_theme.id));
    strcat(title, " theme  |  Left-click: open  Right-click: refresh  T: toggle theme  Q: quit");
    draw_text_row(title_row, title, g_theme.title_text_attr, g_theme.title_fill_attr);
}

static void set_status(const char* msg) {
    int L = (int)strlen(msg);
    if (L >= (int)sizeof(status_line))
        L = (int)sizeof(status_line) - 1;
    for (int i = 0; i < L; ++i)
        status_line[i] = msg[i];
    status_line[L] = '\0';
    draw_text_row(status_row, status_line, g_theme.status_text_attr, g_theme.status_fill_attr);
}

static const char* type_name(icon_type_t t) {
    if (t == ICON_TXT) return "Text file";
    if (t == ICON_BIN) return "Program";
    return "File";
}

static void update_hover_line(int hover) {
    if (hover < 0 || hover >= icon_count) {
        draw_text_row(hover_row, "Select a file or application. Press T to switch themes.", g_theme.muted_panel_attr, g_theme.status_fill_attr);
        return;
    }
    char line[96];
    line[0] = '\0';
    strcpy(line, icons[hover].name);
    strcat(line, "  [");
    strcat(line, type_name(icons[hover].type));
    strcat(line, "]");
    draw_text_row(hover_row, line, g_theme.status_text_attr, g_theme.status_fill_attr);
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
        uint8_t type_attr = g_theme.label_default_attr;
        if (icons[i].type == ICON_TXT)
            type_attr = compose_attr(attr_fg(g_theme.accent_panel_attr), attr_bg(g_theme.canvas_attr));
        else if (icons[i].type == ICON_BIN)
            type_attr = compose_attr(attr_fg(g_theme.label_pressed_attr), attr_bg(g_theme.canvas_attr));
        else
            type_attr = compose_attr(attr_fg(g_theme.muted_panel_attr), attr_bg(g_theme.canvas_attr));

        uint8_t attr = type_attr;
        uint8_t fill_attr = g_theme.canvas_attr;
        if (i == selected_index) {
            attr = g_theme.label_selected_attr;
            fill_attr = g_theme.label_selected_attr;
        }
        if (i == hover) {
            attr = g_theme.label_hover_attr;
            fill_attr = g_theme.label_hover_attr;
        }
        if (i == pressed) {
            attr = g_theme.label_pressed_attr;
            fill_attr = g_theme.label_pressed_attr;
        }

        for (int j = 0; j < width; ++j)
            sys_put_at(start + j, row, ' ', fill_attr);
        int len = (int)strlen(icons[i].name);
        if (len > width)
            len = width;
        for (int j = 0; j < len; ++j)
            sys_put_at(start + j, row, icons[i].name[j], attr);
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
    draw_text_row(status_row, status_line, g_theme.status_text_attr, g_theme.status_fill_attr);
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
    desktop_refresh_theme();
    sys_clear();
    sys_mouse_show(1);
    load_files();
    layout_icons();
    if (icon_count == 0)
        set_status("No files found in ASOFS.");
    else
        set_status("Welcome to the ASOS desktop.");
    update_hover_line(-1);
    int hover = -1;
    int last_hover = -2;
    int pressed = -1;
    int prev_buttons = 0;
    int need_redraw = 1;
    ui_theme_id_t known_theme = g_theme.id;
    while (1) {
        ui_theme_id_t kernel_theme = (ui_theme_id_t)sys_theme_current();
        if (kernel_theme != known_theme) {
            desktop_apply_theme(kernel_theme);
            char msg[64];
            strcpy(msg, "Theme synced to ");
            strcat(msg, ui_theme_name(kernel_theme));
            strcat(msg, ".");
            set_status(msg);
            update_hover_line(hover);
            known_theme = g_theme.id;
            need_redraw = 1;
        }
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
            } else if (c == 't' || c == 'T') {
                desktop_cycle_theme();
                known_theme = g_theme.id;
                need_redraw = 1;
            }
        }
        asm volatile("hlt");
    }
    sys_mouse_show(0);
    sys_clear();
    sys_exit();
}
