#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

#define MAX_W 1024
#define MAX_H 768
#define RGB(r, g, b) (((unsigned)(r) & 0xFF) << 16 | ((unsigned)(g) & 0xFF) << 8 | ((unsigned)(b) & 0xFF))

static unsigned int backbuf[MAX_W * MAX_H];
static int G_W = 0, G_H = 0;

static unsigned int rng_state = 2463534242u;
static inline unsigned int xorshift32(void) {
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

typedef struct {
    int x, y;
} pt;
#define MAX_CELLS 8192

static int MARGIN;
static int HUD_PX;
static int CELL;
static int FX0, FY0;
static int COLS, ROWS;

static pt snake[MAX_CELLS];
static int len, dirx, diry;
static pt apple;
static int score = 0, hi_score = 0;
static int paused = 0;

static inline void pset(int x, int y, unsigned int rgb) {
    if ((unsigned)x < (unsigned)G_W && (unsigned)y < (unsigned)G_H)
        backbuf[y * G_W + x] = rgb;
}
static void fill_rect(int x, int y, int w, int h, unsigned int rgb) {
    int x2 = x + w, y2 = y + h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x2 > G_W)
        x2 = G_W;
    if (y2 > G_H)
        y2 = G_H;
    if (x >= x2 || y >= y2)
        return;
    for (int j = y; j < y2; ++j) {
        unsigned int* row = &backbuf[j * G_W];
        for (int i = x; i < x2; ++i)
            row[i] = rgb;
    }
}
static void frame_rect(int x, int y, int w, int h, unsigned int rgb) {
    if (w <= 0 || h <= 0)
        return;
    for (int i = 0; i < w; i++) {
        pset(x + i, y, rgb);
        pset(x + i, y + h - 1, rgb);
    }
    for (int j = 0; j < h; j++) {
        pset(x, y + j, rgb);
        pset(x + w - 1, y + j, rgb);
    }
}
static inline void draw_cell_px(int cx, int cy, unsigned int rgb, int is_head) {
    int x = FX0 + cx * CELL;
    int y = FY0 + cy * CELL;
    int pad = (CELL >= 18) ? (CELL / 6) : (CELL >= 12 ? CELL / 8 : CELL / 10);
    int w = CELL - pad * 2;
    int h = CELL - pad * 2;
    if (w < 2)
        w = 2;
    if (h < 2)
        h = 2;

    unsigned int core = rgb;
    unsigned int shade = RGB(0, 0, 0);
    unsigned int light = RGB(255, 255, 255);

    fill_rect(x + pad, y + pad, w, h, core);
    for (int i = 0; i < w; i++)
        pset(x + pad + i, y + pad, (core & 0xFEFEFE) | 0x101010);
    for (int j = 0; j < h; j++)
        pset(x + pad, y + pad + j, (core & 0xFEFEFE) | 0x101010);
    for (int i = 0; i < w; i++)
        pset(x + pad + i, y + pad + h - 1, shade);
    for (int j = 0; j < h; j++)
        pset(x + pad + w - 1, y + pad + j, shade);

    if (is_head)
        frame_rect(x + pad, y + pad, w, h, light);
}

static void load_hiscore(void) {
    char buf[32];
    int r = sys_readfile("snake.hi", buf, sizeof(buf) - 1);
    if (r > 0) {
        buf[r] = 0;
        hi_score = atoi(buf);
    }
}
static void save_hiscore_if_needed(void) {
    if (score > hi_score) {
        char buf[16];
        itoa(score, buf, 10);
        sys_writefile("snake.hi", buf, (int)strlen(buf));
        hi_score = score;
    }
}

static void put_str_clipped(int col, int row, const char* s, unsigned char attr) {
    int cols = 80, rows = 25;
    sys_getsize(&cols, &rows);
    if (row < 0 || row >= rows)
        return;
    if (col < 0)
        col = 0;
    for (int i = 0; s[i] && col + i < cols; ++i)
        sys_put_at(col + i, row, s[i], attr);
}
static void hud_draw_panel_and_text(void) {
    fill_rect(0, 0, G_W, HUD_PX, RGB(18, 20, 24));
    frame_rect(0, 0, G_W, HUD_PX, RGB(60, 60, 60));

    int cols = 80, rows = 25;
    sys_getsize(&cols, &rows);

    const char* title = "SNAKE [Arrows=move P=pause  Q=quit]";
    int tx = (cols - (int)strlen(title)) / 2;
    if (tx < 0)
        tx = 0;
    put_str_clipped(tx, 0, title, 0x0F);

    char s1[12], s2[12], line[96];
    s1[0] = s2[0] = line[0] = 0;
    itoa(score, s1, 10);
    itoa(hi_score, s2, 10);
    strcpy(line, "Score: ");
    strcat(line, s1);
    strcat(line, "    Record: ");
    strcat(line, s2);
    put_str_clipped(2, 1, line, 0x0F);

    sys_setcursor(cols - 1, rows - 1);
}

static void compute_layout(void) {
    int minSide = (G_W < G_H ? G_W : G_H);
    MARGIN = minSide / 32;
    if (MARGIN < 8)
        MARGIN = 8;
    HUD_PX = 32;

    int availW = G_W - 2 * MARGIN;
    int availH = G_H - 2 * MARGIN - HUD_PX;
    if (availW < 64)
        availW = 64;
    if (availH < 64)
        availH = 64;

    int cellW = availW / 50;
    int cellH = availH / 30;
    CELL = (cellW < cellH ? cellW : cellH);
    if (CELL < 10)
        CELL = 10;
    if (CELL > 40)
        CELL = 40;

    COLS = availW / CELL;
    ROWS = availH / CELL;
    if (COLS < 24)
        COLS = 24;
    if (ROWS < 18)
        ROWS = 18;

    int guard = 0;
    while (COLS * ROWS > MAX_CELLS && guard++ < 100) {
        CELL++;
        COLS = availW / CELL;
        ROWS = availH / CELL;
    }

    int fieldWpx = COLS * CELL;
    int fieldHpx = ROWS * CELL;
    FX0 = (G_W - fieldWpx) / 2;
    FY0 = ((G_H - HUD_PX) - fieldHpx) / 2 + HUD_PX;
}

static int inside(int x, int y) {
    return x >= 0 && x < COLS && y >= 0 && y < ROWS;
}
static int on_snake(int x, int y) {
    for (int i = 0; i < len; ++i)
        if (snake[i].x == x && snake[i].y == y)
            return 1;
    return 0;
}
static void spawn_apple(void) {
    for (int tries = 0; tries < 5000; ++tries) {
        int rx = (int)(xorshift32() % (unsigned)COLS);
        int ry = (int)(xorshift32() % (unsigned)ROWS);
        if (!on_snake(rx, ry)) {
            apple.x = rx;
            apple.y = ry;
            return;
        }
    }
    apple.x = COLS / 2;
    apple.y = ROWS / 2;
}
static void reset_game(void) {
    int cx = COLS / 2, cy = ROWS / 2;
    len = 3;
    dirx = 1;
    diry = 0;
    score = 0;
    paused = 0;
    snake[0] = (pt){cx, cy};
    snake[1] = (pt){cx - 1, cy};
    snake[2] = (pt){cx - 2, cy};
    spawn_apple();
}
static int step(void) {
    if (paused)
        return 1;

    pt head = snake[0];
    head.x += dirx;
    head.y += diry;

    if (!inside(head.x, head.y))
        return 0;
    for (int i = 0; i < len; i++)
        if (snake[i].x == head.x && snake[i].y == head.y)
            return 0;

    int grow = (head.x == apple.x && head.y == apple.y);
    if (grow) {
        if (len < MAX_CELLS)
            len++;
        score++;
        spawn_apple();
    }
    for (int i = len - 1; i > 0; --i)
        snake[i] = snake[i - 1];
    snake[0] = head;
    return 1;
}

static void draw_everything(void) {
    fill_rect(0, 0, G_W, G_H, RGB(12, 14, 18));

    frame_rect(FX0 - 2, FY0 - 2, COLS * CELL + 4, ROWS * CELL + 4, RGB(220, 220, 220));
    frame_rect(FX0 - 1, FY0 - 1, COLS * CELL + 2, ROWS * CELL + 2, RGB(80, 80, 80));

    if (CELL >= 16) {
        unsigned int gridCol = RGB(28, 32, 38);
        for (int c = 1; c < COLS; ++c) {
            int x = FX0 + c * CELL;
            for (int y = FY0; y < FY0 + ROWS * CELL; ++y)
                pset(x, y, gridCol);
        }
        for (int r = 1; r < ROWS; ++r) {
            int y = FY0 + r * CELL;
            for (int x = FX0; x < FX0 + COLS * CELL; ++x)
                pset(x, y, gridCol);
        }
    }

    draw_cell_px(apple.x, apple.y, RGB(215, 55, 45), 0);

    for (int i = 0; i < len; i++) {
        int head = (i == 0);
        unsigned int col = head ? RGB(90, 220, 110) : RGB(60, 180, 85);
        draw_cell_px(snake[i].x, snake[i].y, col, head);
    }

    hud_draw_panel_and_text();

    sys_gfx_blit(backbuf);
}

void main(void) {
    unsigned int info = sys_gfx_info();
    if (!info) {
        sys_clear();
        sys_write("No 32 bpp mode available.\n");
        sys_write("Press ENTER to exit...\n");
        while (sys_getchar() != '\n') {
        }
        sys_exit();
    }
    int W = (int)((info >> 16) & 0xFFFF);
    int H = (int)(info & 0xFFFF);
    if (W > MAX_W || H > MAX_H) {
        sys_clear();
        sys_write("Resolution too big (requires <= 1024x768).\n");
        sys_write("Press ENTER to exit...\n");
        while (sys_getchar() != '\n') {
        }
        sys_exit();
    }
    G_W = W;
    G_H = H;

    sys_mouse_show(0);
    sys_gfx_clear(RGB(0, 0, 0));
    sys_clear();

    load_hiscore();
    compute_layout();
    reset_game();

    const unsigned int STEP_INITIAL = 14;
    const unsigned int STEP_MIN = 5;
    unsigned int step_ticks = STEP_INITIAL;

    unsigned int next_step = sys_getticks() + step_ticks;

    draw_everything();

    while (1) {
        unsigned int ch;
        while ((ch = sys_trygetchar()) != 0) {
            char c = (char)ch;
            if (c == 'q' || c == 'Q') {
                save_hiscore_if_needed();
                sys_exit();
            } else if (c == 'p' || c == 'P') {
                paused = !paused;
            } else if ((unsigned char)c == KEY_LEFT && dirx != +1) {
                dirx = -1;
                diry = 0;
            } else if ((unsigned char)c == KEY_RIGHT && dirx != -1) {
                dirx = +1;
                diry = 0;
            } else if ((unsigned char)c == KEY_UP && diry != +1) {
                dirx = 0;
                diry = -1;
            } else if ((unsigned char)c == KEY_DOWN && diry != -1) {
                dirx = 0;
                diry = +1;
            }
        }

        unsigned int now = sys_getticks();
        if ((int)(now - next_step) >= 0) {
            unsigned int target = STEP_INITIAL - (unsigned)(score / 8);
            if (target < STEP_MIN)
                target = STEP_MIN;
            step_ticks = target;

            if (!step()) {
                fill_rect(0, 0, G_W, G_H, RGB(10, 10, 10));
                hud_draw_panel_and_text();

                int bw = (G_W * 3) / 5, bh = (G_H * 2) / 5;
                int bx = (G_W - bw) / 2, by = (G_H - bh) / 2;
                fill_rect(bx, by, bw, bh, RGB(24, 26, 30));
                frame_rect(bx, by, bw, bh, RGB(220, 220, 220));

                char line1[] = "GAME OVER!  Press ENTER to exit...";
                char buf1[64], buf2[64], ns[16], nh[16];
                itoa(score, ns, 10);
                itoa(hi_score, nh, 10);
                strcpy(buf1, "Score: ");
                strcat(buf1, ns);
                strcpy(buf2, "Record: ");
                strcat(buf2, nh);

                int cols = 80, rows = 25;
                sys_getsize(&cols, &rows);
                int cx1 = (cols - (int)strlen(line1)) / 2;
                if (cx1 < 0)
                    cx1 = 0;
                int cx2 = (cols - (int)strlen(buf1)) / 2;
                if (cx2 < 0)
                    cx2 = 0;
                int cx3 = (cols - (int)strlen(buf2)) / 2;
                if (cx3 < 0)
                    cx3 = 0;

                put_str_clipped(cx1, 3, line1, 0x0F);
                put_str_clipped(cx2, 5, buf1, 0x0F);
                put_str_clipped(cx3, 6, buf2, 0x0F);

                sys_gfx_blit(backbuf);

                while (sys_getchar() != '\n') {
                }
                save_hiscore_if_needed();
                sys_exit();
            }

            draw_everything();

            next_step += step_ticks;
            if ((int)(now - next_step) >= 0)
                next_step = now + step_ticks;
        }

        asm volatile("hlt");
    }
}
