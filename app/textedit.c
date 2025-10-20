#include "asoapi.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"

#define W 80
#define H 24                // 23 Text rows + 1 status bar
#define COLOR_TEXT  0x0F
#define COLOR_STATUS 0x1E   // Yellow on blue (bg=1<<4, fg=14)

static char buf[8192];      // Text buffer (~8KB)
static int  len = 0;        // Bytes used
static int  cx = 0, cy = 0; // Logical cursor (col, row)
static int  top = 0;        // Top-of-screen offset (unused here)
static char filename[32];

// Return start index of the line containing pos.
static int line_start_from_pos(int pos) {
    int p = pos;
    while (p > 0 && buf[p - 1] != '\n') p--;
    return p;
}

// Return length of line starting at 'start' (no trailing '\n').
static int line_len_from_start(int start) {
    int p = start;
    while (p < len && buf[p] != '\n') p++;
    return p - start;
}

// Convert (x,y) to buffer index, clamping x at line end.
static int pos_from_xy(int x, int y) {
    int p = 0;

    // Descend y lines.
    for (int r = 0; r < y && p < len; r++) {
        while (p < len && buf[p] != '\n') p++;
        if (p < len && buf[p] == '\n') p++; // Skip newline
    }

    // Advance x within the line.
    int start = p;
    int L = line_len_from_start(start);

    if (x > L) x = L;
    return start + x;
}

// Clear text area and status bar.
static void screen_clear() {
    for (int y = 0; y < H - 1; y++)
        for (int x = 0; x < W; x++)
            sys_put_at(x, y, ' ', COLOR_TEXT);

    for (int x = 0; x < W; x++)
        sys_put_at(x, H - 1, ' ', COLOR_STATUS);
}

// Draw a message in the status bar.
static void draw_status(const char* msg) {
    char line[W + 1];
    int i = 0;

    for (; msg[i] && i < W; i++) line[i] = msg[i];
    for (; i < W; i++) line[i] = ' ';
    line[W] = 0;

    for (int x = 0; x < W; x++)
        sys_put_at(x, H - 1, line[x], COLOR_STATUS);
}

// Redraw whole screen and set hardware cursor.
static void render() {
    int p = 0;

    for (int y = 0; y < H - 1; y++) {
        for (int x = 0; x < W; x++) sys_put_at(x, y, ' ', COLOR_TEXT);

        int x = 0;
        while (p < len && y < H - 1 && x < W) {
            if (buf[p] == '\n') {
                p++;
                break;
            }
            sys_put_at(x, y, buf[p], COLOR_TEXT);
            x++;
            p++;
        }

        // Skip overflow until newline
        while (p < len && buf[p] != '\n' && x >= W) p++;

        if (p < len && buf[p] == '\n') p++;
    }

    sys_setcursor(cx, cy);
}

// Insert one character at cursor
static void insert_char(char c) {
    if (len >= (int)sizeof(buf) - 1) return;

    int pos = pos_from_xy(cx, cy);

    // Shift right.
    for (int i = len; i > pos; i--) buf[i] = buf[i - 1];

    buf[pos] = c;
    len++;

    if (c == '\n') {
        cx = 0;
        cy++;
    }
    else {
        cx++;
    }
}

// Handle backspace and line merge cases
static void backspace() {
    int pos = pos_from_xy(cx, cy);
    if (pos == 0) return;

    if (cx == 0) {
        int s = line_start_from_pos(pos);
        if (s > 0) {
            // Remove newline at s-1
            for (int i = s - 1; i < len - 1; i++) buf[i] = buf[i + 1];
            len--;

            // Move to end of previous line
            int ps = line_start_from_pos(s - 1);
            int L  = line_len_from_start(ps);
            cy--;
            cx = L;
            return;
        }
    }

    // Remove char before cursor
    for (int i = pos - 1; i < len - 1; i++) buf[i] = buf[i + 1];
    len--;
    cx--;
}

// Move cursor left
static void move_left() {
    if (cx > 0) {
        cx--;
    }
    else if (cy > 0) {
        cy--;
        cx = line_len_from_start(line_start_from_pos(pos_from_xy(0, cy)));
    }
}

// Move cursor right
static void move_right() {
    int pos = pos_from_xy(cx, cy);

    if (pos < len && buf[pos] != '\n') {
        cx++;
    }
    else if (pos < len && buf[pos] == '\n') {
        cy++;
        cx = 0;
    }
}

// Move cursor up with clamping
static void move_up() {
    if (cy == 0) return;
    int curx = cx;
    cy--;
    int L = line_len_from_start(pos_from_xy(0, cy));
    if (curx > L) curx = L;
    cx = curx;
}

// Move cursor down with clamping
static void move_down() {
    int p = pos_from_xy(0, cy);
    while (p < len && buf[p] != '\n') p++;
    if (p >= len) return;

    cy++;
    int L = line_len_from_start(pos_from_xy(0, cy));
    if (cx > L) cx = L;
}

// Save buffer to file and update status
static void save_now() {
    int r = sys_writefile(filename, buf, len);
    draw_status((r == 0) ? "[Saved]" : "[Save ERROR]");
}

// Entry point and input loop
void main(void) {
    // Read filename or use default.
    int n = sys_getarg(filename, sizeof(filename));
    if (n <= 0) {
        strcpy(filename, "note.txt");
    }

    // Load file if present
    int rd = sys_readfile(filename, buf, sizeof(buf) - 1);
    if (rd > 0) len = rd;
    else        len = 0;

    sys_clear();
    screen_clear();
    draw_status("ESC: quit & save | Ctrl+S: save");
    render();

    while (1) {
        char c = sys_getchar();

        if (c == 27) {
            // ESC -> save & exit
            save_now();
            sys_exit();
        }
        else if ((unsigned char)c == 19) {
            // Ctrl+S -> save
            save_now();
            render();
        }
        else if (c == '\n') {
            insert_char('\n');
            render();
        }
        else if (c == '\b') {
            backspace();
            render();
        }
        else if ((unsigned char)c == KEY_LEFT) {
            move_left();
            render();
        }
        else if ((unsigned char)c == KEY_RIGHT) {
            move_right();
            render();
        }
        else if ((unsigned char)c == KEY_UP) {
            move_up();
            render();
        }
        else if ((unsigned char)c == KEY_DOWN) {
            move_down();
            render();
        }
        else if (c >= 32 && c < 127) {
            // Printable ASCII
            insert_char(c);
            render();
        }
        // Otherwise ignore
    }
}
