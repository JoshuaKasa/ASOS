#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

#define W 80
#define H 25

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
    sys_put_at(x, y, ch, a);
}
static void text(int x, int y, const char* s, unsigned char a) {
    for (int i = 0; s[i]; ++i)
        put(x + i, y, s[i], a);
}

static void hline(int x, int y, int w, unsigned char a) {
    for (int i = 0; i < w; i++)
        put(x + i, y, (i % 2) ? 205 : 196, a);
}
static void vline(int x, int y, int h, unsigned char a) {
    for (int j = 0; j < h; j++)
        put(x, y + j, (j % 2) ? 186 : 179, a);
}

// Window with chunky corners
static void window(int x, int y, int w, int h, const char* title, unsigned char frame, unsigned char title_attr, unsigned char fill) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            put(x + i, y + j, ' ', fill);

    put(x, y, 218, frame);
    put(x + w - 1, y, 191, frame);
    put(x, y + h - 1, 192, frame);
    put(x + w - 1, y + h - 1, 217, frame);
    hline(x + 1, y, w - 2, frame);
    hline(x + 1, y + h - 1, w - 2, frame);
    vline(x, y + 1, h - 2, frame);
    vline(x + w - 1, y + 1, h - 2, frame);

    if (title) {
        int len = (int)strlen(title);

        if (len > w - 4)
            len = w - 4;

        int tx = x + (w - len) / 2;
        text(tx, y, title, title_attr);
    }
}

// Status bar bottom
static void status(const char* s) {
    for (int i = 0; i < W; i++)
        put(i, H - 1, ' ', ATTR(C_BLACK, C_BLUE));
    text(1, H - 1, s, ATTR(C_YELLOW, C_BLUE));
}

// Apps grid
typedef struct {
    const char* label;
    const char* bin;
} App;
static const App apps[] = {
    {"Terminal", "terminal.bin"}, {"TextEdit", "textedit.bin"}, {"Snake", "snake.bin"}, {"Pong", "pong.bin"}, {"Calc", "calculator.bin"}, {"Cube", "3d.bin"}
};
static const int APP_N = (int)(sizeof(apps) / sizeof(apps[0]));

static void draw_icon_tile(int rx, int ry, const char* label, int focused) {
    // 9x5 tile
    unsigned char bg = focused ? ATTR(C_BLACK, C_LIGHTBLUE) : ATTR(C_WHITE, C_BLUE);
    unsigned char fr = focused ? ATTR(C_YELLOW, C_BLACK) : ATTR(C_LIGHTGRAY, C_BLACK);
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 9; i++)
            put(rx + i, ry + j, ' ', bg);

    // Fake tiny icon
    put(rx + 2, ry + 1, (char)219, ATTR(C_WHITE, (focused ? C_LIGHTBLUE : C_BLUE)));
    put(rx + 3, ry + 1, (char)219, ATTR(C_WHITE, (focused ? C_LIGHTBLUE : C_BLUE)));
    put(rx + 2, ry + 2, (char)219, ATTR(C_WHITE, (focused ? C_LIGHTBLUE : C_BLUE)));
    put(rx + 3, ry + 2, (char)219, ATTR(C_WHITE, (focused ? C_LIGHTBLUE : C_BLUE)));

    // Label centered under
    int lw = (int)strlen(label);
    if (lw > 9)
        lw = 9;
    int lx = rx + (9 - lw) / 2;
    text(lx, ry + 4, label, fr);
}

static void draw_apps(int sel) {
    int x0 = 2, y0 = 3;
    window(x0 - 2, y0 - 2, 9 * 4 + 6, 8, " Applications ", ATTR(C_LIGHTGRAY, C_BLACK), ATTR(C_YELLOW, C_BLACK), ATTR(C_BLACK, C_BLACK));
    int col = 0, row = 0;

    for (int i = 0; i < APP_N; i++) {
        int rx = x0 + col * 9 + col * 3;
        int ry = y0 + row * 6;

        draw_icon_tile(rx, ry, apps[i].label, i == sel);
        if (++col == 4) {
            col = 0;
            row++;
        }
    }
}

// Files list
#define MAX_FILES 32
#define NAME_MAX 24  // show up to 23 chars + 0
static char file_names[MAX_FILES][NAME_MAX];
static int file_count = 0;

static int ends_with_txt(const char* s) {
    int n = (int)strlen(s);
    if (n < 4)
        return 0;
    const char* e = s + n - 4;
    return (e[0] == '.' || e[0] == '.') && ((e[1] == 't' || e[1] == 'T') && (e[2] == 'x' || e[2] == 'X') && (e[3] == 't' || e[3] == 'T'));
}

static void load_files(void) {
    // Pull all files from FS and keep the .txt ones (case-insensitive)
    char raw[MAX_FILES * NAME_MAX];
    int rc = sys_enumfiles(raw, MAX_FILES, NAME_MAX);

    file_count = 0;
    if (rc > 0) {
        for (int i = 0; i < rc; i++) {

            char* name = raw + i * NAME_MAX;

            if (ends_with_txt(name)) {
                // Store for UI
                int L = (int)strlen(name);

                if (L >= NAME_MAX)
                    L = NAME_MAX - 1;
                for (int k = 0; k < L; k++)
                    file_names[file_count][k] = name[k];

                file_names[file_count][L] = 0;
                file_count++;

                if (file_count >= MAX_FILES)
                    break;
            }
        }
    }
}

static void draw_files(int sel) {
    int x = 2, y = 12, w = 76, h = 10;
    window(x, y, w, h, " Files (.txt) ", ATTR(C_LIGHTGRAY, C_BLACK), ATTR(C_YELLOW, C_BLACK), ATTR(C_BLACK, C_BLACK));
    int list_y = y + 2;
    int per_page = h - 3;
    int page = (sel / per_page) * per_page;

    for (int i = 0; i < per_page; i++) {
        int idx = page + i;
        // Blank line
        for (int X = 0; X < w - 4; X++)
            put(x + 2 + X, list_y + i, ' ', ATTR(C_WHITE, C_BLACK));
        if (idx < file_count) {
            unsigned char a = (idx == sel) ? ATTR(C_BLACK, C_YELLOW) : ATTR(C_LIGHTGRAY, C_BLACK);
            const char* nm = file_names[idx];
            text(x + 3, list_y + i, nm, a);
        }
    }
}

// Main 
void main(void) {
    sys_clear();
    sys_mouse_show(0); // Hide cursor sprite just in case
    int cols = 80, rows = 25;
    sys_getsize(&cols, &rows);

    // Layout fits 80x25. If different, still draw within bounds.
    (void)cols;
    (void)rows;

    // load .txt files for the right pane
    load_files();

    // Selection state
    int focus = 0;  // 0 = apps, 1 = files
    int sel_app = 0;
    int sel_file = 0;

    // Draw chrome
    for (int y = 0; y < H - 1; y++)
        for (int x = 0; x < W; x++)
            put(x, y, ' ', ATTR(C_WHITE, C_BLACK));

    text(2, 1, "ASOS Desktop", ATTR(C_YELLOW, C_BLACK));
    text(20, 1, "[Arrows] Move   [TAB] Switch   [ENTER] Open   [Q] Quit", ATTR(C_LIGHTGRAY, C_BLACK));
    draw_apps(sel_app);
    draw_files(sel_file);
    status("Welcome. Pick an app or a .txt file.");

    sys_setcursor(W - 1, H - 1);

    unsigned int refresh = sys_getticks() + 8;

    while (1) {
        unsigned int ch = sys_trygetchar();

        if (ch) {
            char c = (char)ch;
            if (c == 'q' || c == 'Q') {
                sys_write("\nBye!\n");
                sys_exit();
            }

            if (c == '\t') {  // switch pane
                focus = 1 - focus;
                // repaint focus highlight
                draw_apps(sel_app);
                draw_files(sel_file);
            } 
            else if ((unsigned char)c == KEY_LEFT) {
                if (focus == 0) {
                    if (sel_app > 0)
                        sel_app--;
                    draw_apps(sel_app);
                } 
                else { /* files pane doesn't use LEFT */
                }
            } 
            else if ((unsigned char)c == KEY_RIGHT) {
                if (focus == 0) {
                    if (sel_app < APP_N - 1)
                        sel_app++;
                    draw_apps(sel_app);
                }
            } 
            else if ((unsigned char)c == KEY_UP) {
                if (focus == 0) {
                    if (sel_app >= 4)
                        sel_app -= 4;
                    draw_apps(sel_app);
                } 
                else {
                    if (sel_file > 0)
                        sel_file--;
                    draw_files(sel_file);
                }
            } 
            else if ((unsigned char)c == KEY_DOWN) {
                if (focus == 0) {
                    if (sel_app + 4 < APP_N)
                        sel_app += 4;
                    draw_apps(sel_app);
                } 
                else {
                    if (sel_file + 1 < file_count)
                        sel_file++;
                    draw_files(sel_file);
                }
            } 
            else if (c == '\n') {
                if (focus == 0) {
                    // Launch selected app
                    sys_write("Launching ");
                    sys_write(apps[sel_app].label);
                    sys_write("...\n");
                    sys_exec(apps[sel_app].bin);

                    // If back from app, re-draw UI
                    sys_clear();
                    load_files();
                    draw_apps(sel_app);
                    draw_files(sel_file);
                    status("Back from app.");
                } 
                else {
                    if (file_count > 0) {
                        // Open textedit on file
                        char cmd[64];

                        strcpy(cmd, "textedit.bin ");
                        strcat(cmd, file_names[sel_file]);
                        sys_write("Editing ");
                        sys_write(file_names[sel_file]);
                        sys_write("...\n");
                        sys_exec(cmd);

                        // return -> redraw (file list may have changed size if saved)
                        sys_clear();
                        load_files();
                        draw_apps(sel_app);

                        if (sel_file >= file_count && file_count > 0)
                            sel_file = file_count - 1;
                        draw_files(sel_file);
                        status("Back from editor.");
                    }
                }
            }
        }

        unsigned now = sys_getticks();

        if ((int)(now - refresh) >= 0) {
            sys_setcursor(W - 1, H - 1);
            refresh += 8;
        }
        asm volatile("hlt");
    }
}
