#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

#define W 80
#define H 25

// Layout
#define ICON_W 9
#define ICON_H 4
#define ICON_MARGIN_LEFT 2
#define ICON_MARGIN_RIGHT 2
#define ICON_MARGIN_TOP 3
#define ICON_MARGIN_BOTTOM 2

#define MAX_ITEMS 64
#define NAME_MAX 32

#define TARGET_NONE   -1
#define TARGET_REFRESH -2
#define TARGET_QUIT    -3
#define TARGET_PREV    -4
#define TARGET_NEXT    -5

// VGA colors
#define C_BLACK 0x0
#define C_BLUE 0x1
#define C_GREEN 0x2
#define C_CYAN 0x3
#define C_RED 0x4
#define C_MAGENTA 0x5
#define C_BROWN 0x6
#define C_LIGHTGRAY 0x7
#define C_DARKGRAY 0x8
#define C_LIGHTBLUE 0x9
#define C_YELLOW 0xE
#define C_WHITE 0xF
#define ATTR(fg, bg) (((bg) << 4) | ((fg) & 0x0F))

static inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void put(int x, int y, char ch, unsigned char a) {
    if (x < 0 || x >= W || y < 0 || y >= H)
        return;
    sys_put_at(x, y, ch, a);
}

static void text(int x, int y, const char* s, unsigned char a) {
    for (int i = 0; s[i]; ++i)
        put(x + i, y, s[i], a);
}

static void fill_rect(int x, int y, int w, int h, unsigned char a) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            put(x + i, y + j, ' ', a);
}

typedef struct {
    char name[NAME_MAX];
    char label[NAME_MAX];
    int is_bin;
    int x;
    int y;
} DesktopItem;

static DesktopItem items[MAX_ITEMS];
static int item_count = 0;
static int icon_cols = 1;
static int icon_rows = 1;
static int items_per_page = 1;
static int total_pages = 1;
static int current_page = 0;

static int prev_x0 = -1, prev_x1 = -1;
static int refresh_x0 = -1, refresh_x1 = -1;
static int next_x0 = -1, next_x1 = -1;
static int quit_x0 = -1, quit_x1 = -1;
static const int BUTTON_Y = H - 2;

static const char default_status[] = "Left click icons to launch apps or open files. Right click to refresh.";

static int ends_with(const char* s, const char* ext) {
    int ns = (int)strlen(s);
    int ne = (int)strlen(ext);
    if (ne > ns)
        return 0;
    const char* tail = s + ns - ne;
    for (int i = 0; i < ne; ++i) {
        char a = tail[i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}

static void make_label(const char* name, char* out) {
    int len = 0;
    while (name[len] && len < NAME_MAX - 1) {
        out[len] = name[len];
        len++;
    }
    out[len] = 0;
    for (int i = len - 1; i >= 0; --i) {
        if (out[i] == '.') {
            out[i] = 0;
            break;
        }
    }
    if (out[0] == 0) {
        len = 0;
        while (name[len] && len < NAME_MAX - 1) {
            out[len] = name[len];
            len++;
        }
        out[len] = 0;
    }
}

static void add_item(const char* name, int is_bin) {
    if (item_count >= MAX_ITEMS || !name[0])
        return;
    DesktopItem* it = &items[item_count++];
    int len = 0;
    while (name[len] && len < NAME_MAX - 1) {
        it->name[len] = name[len];
        len++;
    }
    it->name[len] = 0;
    make_label(name, it->label);
    it->is_bin = is_bin;
    it->x = -1;
    it->y = -1;
}

static void load_items(void) {
    char raw[MAX_ITEMS * NAME_MAX];
    int count = sys_enumfiles(raw, MAX_ITEMS, NAME_MAX);
    item_count = 0;
    if (count <= 0)
        return;

    for (int i = 0; i < count; ++i) {
        const char* name = raw + i * NAME_MAX;
        if (ends_with(name, ".bin"))
            add_item(name, 1);
    }
    for (int i = 0; i < count; ++i) {
        const char* name = raw + i * NAME_MAX;
        if (!ends_with(name, ".bin"))
            add_item(name, 0);
    }
}

static void layout_items(void) {
    int usable_w = W - ICON_MARGIN_LEFT - ICON_MARGIN_RIGHT;
    int usable_h = H - ICON_MARGIN_TOP - ICON_MARGIN_BOTTOM;
    int max_cols = usable_w / ICON_W;
    if (max_cols < 1) max_cols = 1;
    int max_rows = usable_h / ICON_H;
    if (max_rows < 1) max_rows = 1;

    icon_cols = max_cols;
    icon_rows = max_rows;
    items_per_page = icon_cols * icon_rows;
    if (items_per_page < 1) items_per_page = 1;

    if (item_count <= 0) {
        total_pages = 1;
        current_page = 0;
        return;
    }

    total_pages = (item_count + items_per_page - 1) / items_per_page;
    if (total_pages < 1)
        total_pages = 1;
    if (current_page >= total_pages)
        current_page = total_pages - 1;
    if (current_page < 0)
        current_page = 0;

    for (int i = 0; i < item_count; ++i) {
        items[i].x = -1;
        items[i].y = -1;
    }

    int start = current_page * items_per_page;
    int end = start + items_per_page;
    if (end > item_count)
        end = item_count;

    for (int i = start; i < end; ++i) {
        int local = i - start;
        int col = local % icon_cols;
        int row = local / icon_cols;
        items[i].x = ICON_MARGIN_LEFT + col * ICON_W;
        items[i].y = ICON_MARGIN_TOP + row * ICON_H;
    }
}

static void clear_icon_area(void) {
    int area_h = H - ICON_MARGIN_TOP - ICON_MARGIN_BOTTOM;
    if (area_h < 0)
        area_h = 0;
    fill_rect(0, ICON_MARGIN_TOP, W, area_h, ATTR(C_WHITE, C_BLUE));
}

static void draw_header(void) {
    fill_rect(0, 0, W, 2, ATTR(C_LIGHTGRAY, C_BLACK));
    text(2, 0, "ASOS Desktop", ATTR(C_YELLOW, C_BLACK));
    text(2, 1, "Click icons to launch apps or open files. Use the controls below or press Q to quit.", ATTR(C_LIGHTGRAY, C_BLACK));
}

static void draw_background(void) {
    fill_rect(0, 0, W, H, ATTR(C_WHITE, C_BLUE));
    draw_header();
    clear_icon_area();
}

static void draw_button_label_at(int id, const char* label, int* cursor_x, int hovered_button, int pressed_button, int* out_x0, int* out_x1) {
    int x = *cursor_x;
    int len = (int)strlen(label);
    if (out_x0) *out_x0 = x;
    if (out_x1) *out_x1 = x + len;
    unsigned char bg = (pressed_button == id) ? ATTR(C_YELLOW, C_LIGHTBLUE) : (hovered_button == id ? ATTR(C_YELLOW, C_CYAN) : ATTR(C_WHITE, C_BLUE));
    for (int i = 0; i < len; ++i)
        put(x + i, BUTTON_Y, label[i], bg);
    *cursor_x = x + len + 1;
}

static void draw_controls(int hovered_button, int pressed_button) {
    fill_rect(0, BUTTON_Y, W, 1, ATTR(C_WHITE, C_BLUE));

    prev_x0 = prev_x1 = -1;
    refresh_x0 = refresh_x1 = -1;
    next_x0 = next_x1 = -1;
    quit_x0 = quit_x1 = -1;

    int x = 2;
    if (total_pages > 1) {
        draw_button_label_at(TARGET_PREV, "[ Prev ]", &x, hovered_button, pressed_button, &prev_x0, &prev_x1);
    }
    draw_button_label_at(TARGET_REFRESH, "[ Refresh ]", &x, hovered_button, pressed_button, &refresh_x0, &refresh_x1);
    if (total_pages > 1) {
        draw_button_label_at(TARGET_NEXT, "[ Next ]", &x, hovered_button, pressed_button, &next_x0, &next_x1);
    }

    char page_buf[24];
    char tmp[12];
    page_buf[0] = 0;
    strcpy(page_buf, "Page ");
    itoa(total_pages ? current_page + 1 : 1, tmp, 10);
    strcat(page_buf, tmp);
    strcat(page_buf, "/");
    itoa(total_pages ? total_pages : 1, tmp, 10);
    strcat(page_buf, tmp);
    text(x, BUTTON_Y, page_buf, ATTR(C_LIGHTGRAY, C_BLUE));

    const char* quit_label = "[ Quit ]";
    int len = (int)strlen(quit_label);
    int qx = W - len - 2;
    if (qx <= x)
        qx = x + 1;
    quit_x0 = qx;
    quit_x1 = qx + len;
    unsigned char q_attr = (pressed_button == TARGET_QUIT) ? ATTR(C_YELLOW, C_LIGHTBLUE) : (hovered_button == TARGET_QUIT ? ATTR(C_YELLOW, C_CYAN) : ATTR(C_WHITE, C_BLUE));
    for (int i = 0; i < len; ++i)
        put(qx + i, BUTTON_Y, quit_label[i], q_attr);
}

static void draw_item(const DesktopItem* it, int highlighted, int pressed) {
    if (it->x < 0 || it->y < 0)
        return;
    unsigned char bg_color = pressed ? C_LIGHTBLUE : (highlighted ? C_CYAN : C_BLUE);
    unsigned char fill_attr = ATTR(C_WHITE, bg_color);
    for (int j = 0; j < ICON_H; ++j)
        for (int i = 0; i < ICON_W; ++i)
            put(it->x + i, it->y + j, ' ', fill_attr);

    unsigned char border_attr = ATTR(C_BLACK, bg_color);
    unsigned char symbol_attr = ATTR(it->is_bin ? C_YELLOW : C_WHITE, bg_color);
    int cx = it->x + (ICON_W - 3) / 2;
    put(cx + 0, it->y + 0, 218, border_attr);
    put(cx + 1, it->y + 0, 196, border_attr);
    put(cx + 2, it->y + 0, 191, border_attr);
    put(cx + 0, it->y + 1, 179, border_attr);
    put(cx + 1, it->y + 1, it->is_bin ? '*' : 'F', symbol_attr);
    put(cx + 2, it->y + 1, 179, border_attr);
    put(cx + 0, it->y + 2, 192, border_attr);
    put(cx + 1, it->y + 2, 196, border_attr);
    put(cx + 2, it->y + 2, 217, border_attr);

    unsigned char label_attr = ATTR(highlighted ? C_YELLOW : C_LIGHTGRAY, bg_color);
    for (int i = 0; i < ICON_W; ++i)
        put(it->x + i, it->y + ICON_H - 1, ' ', label_attr);
    int len = (int)strlen(it->label);
    if (len > ICON_W - 1)
        len = ICON_W - 1;
    int start = it->x + (ICON_W - len) / 2;
    for (int i = 0; i < len; ++i)
        put(start + i, it->y + ICON_H - 1, it->label[i], label_attr);
}

static void draw_items(int hovered_item, int pressed_item) {
    for (int i = 0; i < item_count; ++i) {
        draw_item(&items[i], i == hovered_item, i == pressed_item);
    }
}

static void status(const char* s) {
    unsigned char base = ATTR(C_WHITE, C_BLUE);
    for (int i = 0; i < W; ++i)
        put(i, H - 1, ' ', base);
    if (!s)
        return;
    int len = (int)strlen(s);
    if (len > W - 2)
        len = W - 2;
    for (int i = 0; i < len; ++i)
        put(1 + i, H - 1, s[i], ATTR(C_YELLOW, C_BLUE));
}

static void update_status_for_target(int target) {
    char msg[96];
    msg[0] = 0;
    if (target >= 0 && target < item_count && items[target].x >= 0) {
        if (items[target].is_bin) {
            strcpy(msg, "Launch ");
            strcat(msg, items[target].name);
        } else {
            strcpy(msg, "Open ");
            strcat(msg, items[target].name);
            strcat(msg, " in TextEdit");
        }
        status(msg);
    } else if (target == TARGET_REFRESH) {
        status("Reload the file list");
    } else if (target == TARGET_PREV) {
        status("Previous page");
    } else if (target == TARGET_NEXT) {
        status("Next page");
    } else if (target == TARGET_QUIT) {
        status("Exit to the shell");
    } else {
        status(default_status);
    }
}

static int hit_test(int cell_x, int cell_y) {
    if (cell_y == BUTTON_Y) {
        if (prev_x0 >= 0 && cell_x >= prev_x0 && cell_x < prev_x1) return TARGET_PREV;
        if (refresh_x0 >= 0 && cell_x >= refresh_x0 && cell_x < refresh_x1) return TARGET_REFRESH;
        if (next_x0 >= 0 && cell_x >= next_x0 && cell_x < next_x1) return TARGET_NEXT;
        if (quit_x0 >= 0 && cell_x >= quit_x0 && cell_x < quit_x1) return TARGET_QUIT;
    }
    for (int i = 0; i < item_count; ++i) {
        if (items[i].x < 0 || items[i].y < 0)
            continue;
        if (cell_x >= items[i].x && cell_x < items[i].x + ICON_W &&
            cell_y >= items[i].y && cell_y < items[i].y + ICON_H)
            return i;
    }
    return TARGET_NONE;
}

static void rebuild_desktop(int* hovered_item, int* hovered_button, int* pressed_item, int* pressed_button, const char* message) {
    load_items();
    layout_items();
    draw_background();
    draw_controls(TARGET_NONE, TARGET_NONE);
    clear_icon_area();
    draw_items(-1, -1);
    status(message ? message : default_status);
    *hovered_item = -1;
    *hovered_button = TARGET_NONE;
    *pressed_item = -1;
    *pressed_button = TARGET_NONE;
}

static void change_page(int delta, int* hovered_item, int* hovered_button, int* pressed_item, int* pressed_button) {
    if (total_pages <= 1)
        return;
    int new_page = current_page + delta;
    if (new_page < 0)
        new_page = 0;
    if (new_page >= total_pages)
        new_page = total_pages - 1;
    if (new_page == current_page)
        return;
    current_page = new_page;
    layout_items();
    clear_icon_area();
    draw_items(-1, -1);
    draw_controls(TARGET_NONE, TARGET_NONE);
    status(default_status);
    *hovered_item = -1;
    *hovered_button = TARGET_NONE;
    *pressed_item = -1;
    *pressed_button = TARGET_NONE;
}

static void handle_refresh(int* hovered_item, int* hovered_button, int* pressed_item, int* pressed_button) {
    rebuild_desktop(hovered_item, hovered_button, pressed_item, pressed_button, "Refreshed file list.");
}

static void handle_quit(void) {
    status("Leaving desktop...");
    sys_mouse_show(0);
    sys_exit();
}

static void launch_item(int idx, int* hovered_item, int* hovered_button, int* pressed_item, int* pressed_button) {
    if (idx < 0 || idx >= item_count)
        return;
    char name[NAME_MAX];
    for (int i = 0; i < NAME_MAX; ++i) {
        name[i] = items[idx].name[i];
        if (!items[idx].name[i])
            break;
    }
    name[NAME_MAX - 1] = 0;

    char status_msg[96];
    status_msg[0] = 0;
    if (items[idx].is_bin) {
        strcpy(status_msg, "Launching ");
        strcat(status_msg, name);
        strcat(status_msg, "...");
        status(status_msg);
        sys_mouse_show(0);
        sys_exec(name);
        sys_mouse_show(1);
        rebuild_desktop(hovered_item, hovered_button, pressed_item, pressed_button, "Back from app.");
    } else {
        strcpy(status_msg, "Opening ");
        strcat(status_msg, name);
        strcat(status_msg, "...");
        status(status_msg);
        char cmd[NAME_MAX + 16];
        strcpy(cmd, "textedit.bin ");
        strcat(cmd, name);
        sys_mouse_show(0);
        sys_exec(cmd);
        sys_mouse_show(1);
        rebuild_desktop(hovered_item, hovered_button, pressed_item, pressed_button, "Back from file.");
    }
}

static void handle_button(int target, int* hovered_item, int* hovered_button, int* pressed_item, int* pressed_button) {
    if (target == TARGET_REFRESH) {
        handle_refresh(hovered_item, hovered_button, pressed_item, pressed_button);
    } else if (target == TARGET_PREV) {
        change_page(-1, hovered_item, hovered_button, pressed_item, pressed_button);
    } else if (target == TARGET_NEXT) {
        change_page(1, hovered_item, hovered_button, pressed_item, pressed_button);
    } else if (target == TARGET_QUIT) {
        handle_quit();
    }
}

void main(void) {
    sys_clear();
    sys_mouse_show(1);

    load_items();
    layout_items();
    draw_background();
    draw_controls(TARGET_NONE, TARGET_NONE);
    draw_items(-1, -1);
    status(default_status);

    mouse_info_t mi;
    unsigned prev_buttons = 0;
    int hovered_item = -1;
    int pressed_item = -1;
    int hovered_button = TARGET_NONE;
    int pressed_button = TARGET_NONE;

    sys_setcursor(W - 1, H - 1);

    while (1) {
        unsigned int key = sys_trygetchar();
        if (key) {
            char c = (char)key;
            if (c == 'q' || c == 'Q') {
                handle_quit();
            } else if (c == 'r' || c == 'R') {
                handle_refresh(&hovered_item, &hovered_button, &pressed_item, &pressed_button);
            }
        }

        sys_mouse_get(&mi);
        int cell_x = clamp(mi.x / 8, 0, W - 1);
        int cell_y = clamp(mi.y / 16, 0, H - 1);

        int target = hit_test(cell_x, cell_y);
        int new_hovered_item = (target >= 0) ? target : -1;
        int new_hovered_button = (target < 0) ? target : TARGET_NONE;
        if (target == TARGET_NONE)
            new_hovered_button = TARGET_NONE;

        int redraw_items = 0;
        int redraw_buttons = 0;

        if (new_hovered_item != hovered_item || new_hovered_button != hovered_button) {
            hovered_item = new_hovered_item;
            hovered_button = new_hovered_button;
            redraw_items = 1;
            redraw_buttons = 1;
            update_status_for_target(target);
        }

        unsigned buttons = mi.buttons;
        int left_down = (buttons & 1) != 0;
        int left_prev = (prev_buttons & 1) != 0;

        if (left_down && !left_prev) {
            if (hovered_item >= 0) {
                pressed_item = hovered_item;
                pressed_button = TARGET_NONE;
                redraw_items = 1;
            } else if (hovered_button != TARGET_NONE) {
                pressed_item = -1;
                pressed_button = hovered_button;
                redraw_buttons = 1;
            } else {
                pressed_item = -1;
                pressed_button = TARGET_NONE;
            }
        } else if (!left_down && left_prev) {
            int activated = TARGET_NONE;
            if (pressed_item >= 0 && hovered_item == pressed_item)
                activated = pressed_item;
            else if (pressed_button != TARGET_NONE && hovered_button == pressed_button)
                activated = pressed_button;

            if (pressed_item >= 0) {
                pressed_item = -1;
                redraw_items = 1;
            }
            if (pressed_button != TARGET_NONE) {
                pressed_button = TARGET_NONE;
                redraw_buttons = 1;
            }

            if (activated != TARGET_NONE) {
                if (activated >= 0) {
                    launch_item(activated, &hovered_item, &hovered_button, &pressed_item, &pressed_button);
                } else {
                    handle_button(activated, &hovered_item, &hovered_button, &pressed_item, &pressed_button);
                }
            }
        }

        if ((buttons & 2) && !(prev_buttons & 2)) {
            handle_refresh(&hovered_item, &hovered_button, &pressed_item, &pressed_button);
        }

        if (redraw_items)
            draw_items(hovered_item, pressed_item);
        if (redraw_buttons)
            draw_controls(hovered_button, pressed_button);

        sys_setcursor(W - 1, H - 1);
        prev_buttons = buttons;
        asm volatile("hlt");
    }
}
