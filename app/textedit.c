#include <stdint.h>
#include "asoapi.h"

#define MAX_COLS 200  
#define MAX_ROWS 100
#define TAB_WIDTH 4

#define ATTR_TEXT 0x0F       
#define ATTR_STATUS 0x1E     
#define ATTR_GUTTER 0x08     
#define ATTR_GUTTERSEP 0x07  

static char buf[8192];  
static int len = 0;     
static char filename[32] = "note.txt";

static int scr_cols = 80, scr_rows = 25;  
static int gutter_w = 6;  

static int doc_x = 0, doc_y = 0;

static int view_x = 0, view_y = 0;

static int insert_mode = 1;
static int dirty = 0;

static aso_cell_t fb[MAX_COLS * MAX_ROWS];

static inline int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static int utoa10(unsigned v, char* out) {
    char tmp[16];
    int n = 0;
    if (v == 0) {
        out[0] = '0';
        return 1;
    }
    while (v && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = 0; i < n; ++i)
        out[i] = tmp[n - 1 - i];

    return n;
}

static int s_len(const char* s) {
    int n = 0;
    while (s && s[n])
        n++;
    return n;
}

static char* s_cat(char* d, const char* s) {
    while (*s)
        *d++ = *s++;
    return d;
}

static int count_lines(void) {
    int cnt = 1;  
    for (int i = 0; i < len; i++)
        if (buf[i] == '\n')
            cnt++;
    return cnt;
}

static int line_start_from_pos(int pos) {
    int p = pos;
    while (p > 0 && buf[p - 1] != '\n')
        p--;
    return p;
}

static int line_len_from_start(int start) {
    int p = start;
    while (p < len && buf[p] != '\n')
        p++;
    return p - start;
}

static int pos_from_xy(int x, int y) {
    int p = 0;

    for (int r = 0; r < y && p < len; r++) {
        while (p < len && buf[p] != '\n')
            p++;
        if (p < len && buf[p] == '\n')
            p++;  
    }

    int start = p;
    int L = line_len_from_start(start);
    if (x > L)
        x = L;
    return start + x;
}

static void cur_line_info(int* out_start, int* out_len) {
    int s = pos_from_xy(0, doc_y);
    if (out_start)
        *out_start = s;
    if (out_len)
        *out_len = line_len_from_start(s);
}

static inline void fb_put(int x, int y, char ch, uint8_t attr) {
    fb[y * scr_cols + x].ch = ch;
    fb[y * scr_cols + x].attr = attr;
}

static void fb_clear(uint8_t attr) {
    int total = scr_cols * scr_rows;
    for (int i = 0; i < total; i++) {
        fb[i].ch = ' ';
        fb[i].attr = attr;
    }
}

static void draw_status_line(const char* msg) {

    int y = scr_rows - 1;
    int i = 0;
    for (; i < scr_cols && msg[i]; ++i)
        fb_put(i, y, msg[i], ATTR_STATUS);
    for (; i < scr_cols; ++i)
        fb_put(i, y, ' ', ATTR_STATUS);
}

static void build_status(char* out, int cap) {

    char* d = out;
    if (dirty)
        *d++ = '*';
    d = s_cat(d, filename);
    *d++ = ' ';

    d = s_cat(d, " Ln ");
    d += utoa10((unsigned)(doc_y + 1), d);
    d = s_cat(d, ", Col ");
    d += utoa10((unsigned)(doc_x + 1), d);

    d = s_cat(d, " | bytes ");
    d += utoa10((unsigned)len, d);

    d = s_cat(d, " | lines ");
    d += utoa10((unsigned)count_lines(), d);

    d = s_cat(d, " | ");
    d = s_cat(d, insert_mode ? "INS" : "OVR");

    d = s_cat(d, " | ");
    d += utoa10((unsigned)scr_cols, d);
    *d++ = 'x';
    d += utoa10((unsigned)scr_rows, d);

    *d = 0;
}

static void ensure_cursor_visible(void) {
    int text_cols = scr_cols - gutter_w;  
    int text_rows = scr_rows - 1;         

    if (doc_y < view_y)
        view_y = doc_y;
    if (doc_y >= view_y + text_rows)
        view_y = doc_y - (text_rows - 1);

    if (doc_x < view_x)
        view_x = doc_x;
    if (doc_x >= view_x + text_cols)
        view_x = doc_x - (text_cols - 1);

    if (view_y < 0)
        view_y = 0;
    if (view_x < 0)
        view_x = 0;
}

static void paint_software_cursor(void){
    int text_rows = scr_rows - 1;
    int cur_sx = gutter_w + (doc_x - view_x);
    int cur_sy = (doc_y - view_y);

    if (cur_sy < 0 || cur_sy >= text_rows) return;
    if (cur_sx < gutter_w || cur_sx >= scr_cols) return;

    aso_cell_t* cell = &fb[cur_sy*scr_cols + cur_sx];
    uint8_t a = cell->attr;
    cell->attr = (uint8_t)(((a & 0x0F) << 4) | ((a & 0xF0) >> 4)); // swap fg/bg
}

static void render() {

    sys_getsize(&scr_cols, &scr_rows);
    if (scr_cols < 20)
        scr_cols = 20;
    if (scr_rows < 5)
        scr_rows = 5;

    int lines = count_lines();
    int digits = 1;
    int t = lines;
    while (t >= 10) {
        t /= 10;
        digits++;
    }
    gutter_w = (digits < 3 ? 3 : digits) + 1;  
    if (gutter_w > 12)
        gutter_w = 12;
    if (gutter_w >= scr_cols)
        gutter_w = scr_cols - 1;

    ensure_cursor_visible();

    fb_clear(ATTR_TEXT);

    int text_rows = scr_rows - 1;
    int text_cols = scr_cols - gutter_w;

    for (int y = 0; y < text_rows; ++y) {

        int ln = view_y + y + 1;

        int tmp_ln = ln;
        for (int gx = gutter_w - 2; gx >= 0; --gx) {
            char ch = ' ';
            if (tmp_ln > 0) {
                ch = (char)('0' + (tmp_ln % 10));
                tmp_ln /= 10;
            }
            fb_put(gx, y, ch, ATTR_GUTTER);
        }

        fb_put(gutter_w - 1, y, '|', ATTR_GUTTERSEP);
    }

    for (int row = 0; row < text_rows; ++row) {
        int doc_row = view_y + row;

        int p = 0;
        for (int r = 0; r < doc_row && p < len; r++) {
            while (p < len && buf[p] != '\n')
                p++;
            if (p < len && buf[p] == '\n')
                p++;
        }
        int start = p;
        int L = (doc_row < lines) ? line_len_from_start(start) : 0;

        int off = (view_x < L) ? view_x : L;  
        for (int x = 0; x < text_cols; ++x) {
            char ch = ' ';
            if (off + x < L)
                ch = buf[start + off + x];
            fb_put(gutter_w + x, row, ch, ATTR_TEXT);
        }
    }

    char status[128];
    status[0] = 0;

    build_status(status, sizeof(status));
    draw_status_line(status);

    int cur_sx = gutter_w + (doc_x - view_x);
    int cur_sy = (doc_y - view_y);

    if (cur_sy < 0) cur_sy = 0;
    if (cur_sy >= text_rows) cur_sy = text_rows - 1;
    if (cur_sx < gutter_w) cur_sx = gutter_w;
    if (cur_sx >= scr_cols) cur_sx = scr_cols - 1;

    unsigned gi = sys_gfx_info(); 
    if (gi) {
        paint_software_cursor();
        sys_blit(fb, scr_cols * scr_rows);
    } else {
        sys_blit(fb, scr_cols * scr_rows);
        sys_setcursor(cur_sx, cur_sy);
    }
}

static void mark_dirty(void) {
    dirty = 1;
}

static void insert_byte_at_cursor(char c) {
    if (len >= (int)sizeof(buf) - 1)
        return;
    int pos = pos_from_xy(doc_x, doc_y);

    for (int i = len; i > pos; --i)
        buf[i] = buf[i - 1];
    buf[pos] = c;
    len++;

    if (c == '\n') {
        doc_y++;
        doc_x = 0;
    } 
    else {
        doc_x++;
    }
    mark_dirty();
}

static void overwrite_byte_at_cursor(char c) {
    int pos = pos_from_xy(doc_x, doc_y);
    int s, L;
    cur_line_info(&s, &L);

    if (c == '\n') {

        insert_byte_at_cursor('\n');
        return;
    }

    if (doc_x < L) {
        buf[pos] = c;
        doc_x++;
        mark_dirty();
    } 
    else {

        insert_byte_at_cursor(c);
    }
}

static void backspace_at_cursor(void) {
    int pos = pos_from_xy(doc_x, doc_y);
    if (pos == 0)
        return;

    if (doc_x == 0) {

        int s = line_start_from_pos(pos);
        if (s > 0) {
            for (int i = s - 1; i < len - 1; ++i)
                buf[i] = buf[i + 1];
            len--;

            int ps = line_start_from_pos(s - 1);
            int L = line_len_from_start(ps);
            doc_y--;
            doc_x = L;
            mark_dirty();
            return;
        }
    }

    for (int i = pos - 1; i < len - 1; ++i)
        buf[i] = buf[i + 1];
    len--;
    doc_x--;
    mark_dirty();
}

static void move_left(void) {
    if (doc_x > 0) {
        doc_x--;
        return;
    }
    if (doc_y > 0) {
        doc_y--;
        int s = pos_from_xy(0, doc_y);
        doc_x = line_len_from_start(s);
    }
}

static void move_right(void) {
    int s, L;
    cur_line_info(&s, &L);
    if (doc_x < L) {
        doc_x++;
        return;
    }

    int p = s + L;
    if (p < len && buf[p] == '\n') {
        doc_y++;
        doc_x = 0;
    }
}

static void move_up(void) {
    if (doc_y == 0)
        return;
    int want = doc_x;
    doc_y--;
    int s, L;
    cur_line_info(&s, &L);
    if (want > L)
        want = L;
    doc_x = want;
}

static void move_down(void) {

    int p = pos_from_xy(0, doc_y);
    while (p < len && buf[p] != '\n')
        p++;
    if (p >= len)
        return;

    int want = doc_x;
    doc_y++;
    int s, L;
    cur_line_info(&s, &L);
    if (want > L)
        want = L;
    doc_x = want;
}

static void go_line_start(void) {
    doc_x = 0;
}
static void go_line_end(void) {
    int s, L;
    cur_line_info(&s, &L);
    doc_x = L;
}

static void save_now(void) {
    int r = sys_writefile(filename, buf, len);
    dirty = (r == 0) ? 0 : dirty;  

    draw_status_line((r == 0) ? "[Saved]" : "[Save ERROR]");
    sys_blit(fb, scr_cols * scr_rows);
}

static int prompt_input(const char* title, char* out, int maxlen) {

    int n = 0;
    out[0] = 0;

    while (1) {

        char line[128];
        char* d = line;
        d = s_cat(d, title);
        *d++ = ' ';
        for (int i = 0; i < n && (d - line) < (int)sizeof(line) - 1; ++i)
            *d++ = out[i];
        *d = 0;

        draw_status_line(line);
        sys_blit(fb, scr_cols * scr_rows);

        char c = (char)sys_getchar();
        if (c == 27) {  
            draw_status_line("Canceled");
            sys_blit(fb, scr_cols * scr_rows);
            return -1;
        } 
        else if (c == '\n' || c == '\r') {
            return n;
        } 
        else if (c == '\b') {
            if (n > 0) {
                n--;
                out[n] = 0;
            }
        } 
        else if (c >= 32 && c < 127) {
            if (n < maxlen - 1) {
                out[n++] = c;
                out[n] = 0;
            }
        }
    }
}

static int find_forward(const char* needle) {
    if (!needle || !needle[0])
        return -1;

    int start = pos_from_xy(doc_x, doc_y);
    int nlen = s_len(needle);

    for (int i = start; i + nlen <= len; ++i) {
        int j = 0;

        while (j < nlen && buf[i + j] == needle[j])
            j++;
        if (j == nlen)
            return i;
    }
    return -1;
}

static void jump_cursor_to_pos(int pos) {

    int p = 0;
    int y = 0;
    while (p < pos && p < len) {
        if (buf[p] == '\n') {
            y++;
        }
        p++;
    }
    int s = line_start_from_pos(pos);
    int x = pos - s;
    doc_y = y;
    doc_x = x;
}

void main(void) {

    int n = sys_getarg(filename, sizeof(filename));
    if (n <= 0)
        strcpy(filename, "note.txt");

    int rd = sys_readfile(filename, buf, sizeof(buf) - 1);
    if (rd > 0)
        len = rd;
    else
        len = 0;

    sys_clear();
    sys_getsize(&scr_cols, &scr_rows);
    render();

    while (1) {

        int cols, rows;
        sys_getsize(&cols, &rows);
        if (cols != scr_cols || rows != scr_rows) {
            scr_cols = cols;
            scr_rows = rows;
            render();
        }

        char c = sys_getchar();

        if (c == 27) {

            save_now();
            sys_exit();
        } 
        else if ((unsigned char)c == 19) {

            save_now();
            render();
        } 
        else if ((unsigned char)c == 17) {

            sys_exit();
        } 
        else if ((unsigned char)c == 15) {

            insert_mode = !insert_mode;
            render();
        } 
        else if ((unsigned char)c == 6) {

            char needle[64];
            if (prompt_input("Find:", needle, sizeof(needle)) >= 0) {
                int at = find_forward(needle);
                if (at >= 0)
                    jump_cursor_to_pos(at);
                draw_status_line((at >= 0) ? "[Found]" : "[Not found]");
                sys_blit(fb, scr_cols * scr_rows);
            }
            render();
        } 
        else if ((unsigned char)c == 12) {

            char bufnum[16];
            if (prompt_input("Goto line:", bufnum, sizeof(bufnum)) >= 0) {

                int v = 0;
                for (int i = 0; bufnum[i]; ++i)
                    if (bufnum[i] >= '0' && bufnum[i] <= '9')
                        v = v * 10 + (bufnum[i] - '0');
                int total = count_lines();
                if (v < 1)
                    v = 1;
                if (v > total)
                    v = total;
                doc_y = v - 1;
                go_line_end();  
            }
            render();
        } 
        else if ((unsigned char)c == 1) {

            go_line_start();
            render();
        } 
        else if ((unsigned char)c == 5) {

            go_line_end();
            render();
        } 
        else if (c == '\t') {

            int s, L;
            cur_line_info(&s, &L);
            int to = TAB_WIDTH - (doc_x % TAB_WIDTH);
            while (to-- > 0) {
                if (insert_mode)
                    insert_byte_at_cursor(' ');
                else
                    overwrite_byte_at_cursor(' ');
            }
            render();
        } 
        else if (c == '\n' || c == '\r') {
            if (insert_mode)
                insert_byte_at_cursor('\n');
            else
                overwrite_byte_at_cursor('\n');
            render();
        } 
        else if (c == '\b') {
            backspace_at_cursor();
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
            if (insert_mode)
                insert_byte_at_cursor(c);
            else
                overwrite_byte_at_cursor(c);
            render();
        }

    }
}
