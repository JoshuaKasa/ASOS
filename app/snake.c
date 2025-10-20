#include "asoapi.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"

#define C_BLACK   0x0
#define C_BLUE    0x1
#define C_GREEN   0x2
#define C_CYAN    0x3
#define C_RED     0x4
#define C_MAGENTA 0x5
#define C_BROWN   0x6
#define C_GRAY    0x7
#define C_LIGHTGRAY 0x7
#define C_DARKGRAY  0x8
#define C_YELLOW  0xE
#define C_WHITE   0xF

#define ATTR(fg,bg)   (((bg)<<4)|((fg)&0x0F))

#define FIELD_W   30
#define FIELD_H   20
#define ORG_X     10
#define ORG_Y     2

#define CELL_W    2

#define CH_FULL   ((char)219)
#define CH_APPLE  ((char)219)
#define CH_EMPTY  ' '

#define MAX_LEN   (FIELD_W*FIELD_H)
typedef struct { int x, y; } pt;

static pt snake[MAX_LEN];
static int len = 3;
static int dirx = 1, diry = 0;
static pt apple;

static int score = 0, hi_score = 0;
static int paused = 0;

static unsigned int rng_state = 2463534242u;
static inline unsigned int xorshift32(void) {
    unsigned int x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

static int inside(int x, int y) {
    return x >= 0 && x < FIELD_W && y >= 0 && y < FIELD_H;
}

static int on_snake(int x, int y) {
    for (int i = 0; i < len; i++)
        if (snake[i].x == x && snake[i].y == y) return 1;
    return 0;
}

static inline void put2(int x, int y, char ch, unsigned char attr) {
    sys_put_at(x,     y, ch, attr);
    sys_put_at(x + 1, y, ch, attr);
}

static inline void draw_cell(int fx, int fy, char ch, unsigned char attr) {
    int sx = ORG_X + fx * CELL_W;
    int sy = ORG_Y + fy;
    put2(sx, sy, ch, attr);
}

static inline void fill_field_bg(unsigned char attr_bg) {
    for (int y = 0; y < FIELD_H; ++y)
        for (int x = 0; x < FIELD_W; ++x)
            draw_cell(x, y, CH_EMPTY, attr_bg);
}

static void draw_border(void) {
    unsigned char col = ATTR(C_WHITE, C_BLACK);
    // Top/bottom
    for (int x = 0; x < FIELD_W * CELL_W + 2; x++) {
        sys_put_at(ORG_X - 1 + x, ORG_Y - 1, CH_FULL, col);
        sys_put_at(ORG_X - 1 + x, ORG_Y + FIELD_H, CH_FULL, col);
    }
    // Sides
    for (int y = 0; y < FIELD_H + 1; y++) {
        sys_put_at(ORG_X - 1, ORG_Y - 1 + y, CH_FULL, col);
        sys_put_at(ORG_X + FIELD_W * CELL_W, ORG_Y - 1 + y, CH_FULL, col);
    }
}

static void draw_snake(void) {
    draw_cell(snake[0].x, snake[0].y, CH_FULL, ATTR(C_GREEN, C_BLACK));
    for (int i = 1; i < len; i++)
        draw_cell(snake[i].x, snake[i].y, CH_FULL, ATTR(C_GREEN, C_BLACK));
}

static void erase_cell(int fx, int fy) {
    draw_cell(fx, fy, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void draw_apple(void) {
    draw_cell(apple.x, apple.y, CH_APPLE, ATTR(C_RED, C_BLACK));
}

static void hud(void) {
    char b[96], n1[16], n2[16];
    itoa(score, n1, 10); itoa(hi_score, n2, 10);
    strcpy(b, "SNAKE  score: "); strcat(b, n1);
    strcat(b, "   best: "); strcat(b, n2);
    strcat(b, "   [Freccie=muovi, P=pause, Q=esci]");
    sys_setcursor(0, ORG_Y + FIELD_H + 2);
    sys_write(b);
    sys_write("\n");
}

static void load_hiscore(void) {
    char buf[32];
    int r = sys_readfile("snake.hi", buf, sizeof(buf)-1);
    if (r > 0) { buf[r] = 0; hi_score = atoi(buf); }
}

static void save_hiscore(void) {
    if (score > hi_score) {
        char buf[16]; itoa(score, buf, 10);
        sys_writefile("snake.hi", buf, (int)strlen(buf));
        hi_score = score;
    }
}

static void spawn_apple(void) {
    int tries = 0;
    do {
        int rx = (int)(xorshift32() % FIELD_W);
        int ry = (int)(xorshift32() % FIELD_H);
        apple.x = rx; apple.y = ry;
        if (++tries > 2000) break;
    } while (on_snake(apple.x, apple.y));
}

static void init_game(void) {
    sys_clear();
    load_hiscore();

    int cx = FIELD_W/2, cy = FIELD_H/2;
    len = 3; dirx = 1; diry = 0; score = 0; paused = 0;
    snake[0] = (pt){cx, cy};
    snake[1] = (pt){cx-1, cy};
    snake[2] = (pt){cx-2, cy};

    fill_field_bg(ATTR(C_BLACK, C_BLACK));
    draw_border();
    spawn_apple();
    draw_apple();
    draw_snake();
    hud();

    sys_setcursor(79, 24);
}

static int step(void) {
    if (paused) return 1;

    pt head = snake[0];
    head.x += dirx; head.y += diry;

    if (!inside(head.x, head.y)) return 0;
    for (int i = 0; i < len; i++)
        if (snake[i].x == head.x && snake[i].y == head.y) return 0;

    // Apple?
    int grow = (head.x == apple.x && head.y == apple.y);
    if (grow) {
        if (len < MAX_LEN) len++;
        score++;
    } else {
        pt tail = snake[len-1];
        erase_cell(tail.x, tail.y);
    }

    for (int i = len-1; i > 0; i--) snake[i] = snake[i-1];
    snake[0] = head;

    if (grow) {
        spawn_apple();
        draw_apple();
        hud();
    }

    draw_cell(snake[1].x, snake[1].y, CH_FULL, ATTR(C_GREEN, C_BLACK));
    draw_cell(snake[0].x, snake[0].y, CH_FULL, ATTR(C_GREEN, C_BLACK));

    sys_setcursor(79, 24);
    return 1;
}

void main(void) {
    init_game();

    unsigned int base = 2; // ~110ms
    unsigned int speed = base;
    unsigned int next = sys_getticks() + speed;

    while (1) {
        unsigned int ch = sys_trygetchar();
        if (ch) {
            char c = (char)ch;
            if (c == 'q' || c == 'Q') {
                save_hiscore();
                sys_write("\nBye!\n"); 
                sys_exit();
            }
            else if (c == 'p' || c == 'P') {
                paused = !paused;
                sys_setcursor(0, ORG_Y + FIELD_H + 3);
                sys_write(paused ? "[PAUSA]\n" : "       \n");
            }
            else if ((unsigned char)c == KEY_LEFT  && dirx !=  1) { dirx = -1; diry = 0; }
            else if ((unsigned char)c == KEY_RIGHT && dirx != -1) { dirx = +1; diry = 0; }
            else if ((unsigned char)c == KEY_UP    && diry !=  1) { dirx = 0;  diry = -1; }
            else if ((unsigned char)c == KEY_DOWN  && diry != -1) { dirx = 0;  diry = +1; }
        }

        unsigned int now = sys_getticks();
        if ((int)(now - next) >= 0) {
            if (!step()) {
                save_hiscore();
                sys_write("\nGAME OVER!  Score: ");
                char n[16]; itoa(score, n, 10); sys_write(n);
                sys_write("   (best: ");
                itoa(hi_score, n, 10); sys_write(n);
                sys_write(")\nPremi ENTER per uscire...");
                while (sys_getchar() != '\n') {}
                sys_exit();
            }

            unsigned int target = base > 1 ? base - (unsigned)(score/5) : 1;
            if (target < 1) target = 1;
            speed = target;

            next += speed;
        }

        asm volatile("hlt");
    }
}
