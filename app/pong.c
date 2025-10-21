#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asoapi.h"

#define C_BLACK 0x0
#define C_BLUE 0x1
#define C_GREEN 0x2
#define C_CYAN 0x3
#define C_RED 0x4
#define C_MAGENTA 0x5
#define C_BROWN 0x6
#define C_LIGHTGRAY 0x7
#define C_DARKGRAY 0x8
#define C_YELLOW 0xE
#define C_WHITE 0xF
#define ATTR(fg, bg) ((((bg) & 0x0F) << 4) | ((fg) & 0x0F))

#define CH_BLOCK ((char)219)
#define CH_LIGHT ((char)176)
#define CH_BALL 'O'
#define CH_EMPTY ' '

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int sgn(int v) {
    return (v > 0) - (v < 0);
}

enum { FIELD_W = 60, FIELD_H = 22, ORG_X = 10, ORG_Y = 1 };
enum { PADDLE_H = 4, PADDLE_MARGIN = 2, MAX_SCORE = 7 };

enum { VY_MIN = -2, VY_MAX = 2, BALL_DELAY_MIN = 1, BALL_DELAY_MAX = 5 };

enum { GLOW_TICKS = 6 };

enum { SERVE_COUNTDOWN_SEC = 2 };

typedef struct {
    int y;
    int glow;
    int last_move_dir;
} Paddle;

typedef struct {
    int x, y;
    int vx, vy;
    unsigned ball_delay;
} Ball;

typedef enum { DIFF_EASY = 0, DIFF_NORMAL = 1, DIFF_HARD = 2 } Diff;

typedef struct {
    const char* name;
    unsigned ai_step_delay;
    unsigned react_delay;
    int jitter;
} DiffParams;

static const DiffParams DIFF_TABLE[3] = {{"Easy", 2, 6, 2}, {"Normal", 1, 3, 1}, {"Hard", 1, 1, 0}};

static Paddle p1, p2;
static Ball ball;
static int score1 = 0, score2 = 0;
static int two_players = 0;
static Diff difficulty = DIFF_NORMAL;

static unsigned tick_delay_ui = 1;
static unsigned next_ui = 0, next_ball = 0, next_ai = 0;
static unsigned serve_sleep_until = 0;

static inline void put_cell(int fx, int fy, char ch, unsigned char attr) {
    if (fx < 0 || fy < 0 || fx >= FIELD_W || fy >= FIELD_H)
        return;
    sys_put_at(ORG_X + fx, ORG_Y + fy, ch, attr);
}

static void draw_border(void) {
    unsigned char col = ATTR(C_WHITE, C_BLACK);
    for (int x = -1; x <= FIELD_W; ++x) {
        sys_put_at(ORG_X + x, ORG_Y - 1, CH_BLOCK, col);
        sys_put_at(ORG_X + x, ORG_Y + FIELD_H, CH_BLOCK, col);
    }
    for (int y = -1; y <= FIELD_H; ++y) {
        sys_put_at(ORG_X - 1, ORG_Y + y, CH_BLOCK, col);
        sys_put_at(ORG_X + FIELD_W, ORG_Y + y, CH_BLOCK, col);
    }
    for (int y = 0; y < FIELD_H; ++y) {
        if (y % 2 == 0)
            put_cell(FIELD_W / 2, y, CH_LIGHT, ATTR(C_DARKGRAY, C_BLACK));
    }
}

static void clear_field(void) {
    for (int y = 0; y < FIELD_H; ++y)
        for (int x = 0; x < FIELD_W; ++x)
            put_cell(x, y, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void draw_paddle(int left, int y, int glow) {
    int fx = left ? PADDLE_MARGIN : (FIELD_W - 1 - PADDLE_MARGIN);
    unsigned char col = glow > 0 ? ATTR(C_CYAN, C_BLACK) : ATTR(C_GREEN, C_BLACK);
    for (int i = 0; i < PADDLE_H; ++i)
        put_cell(fx, y + i, CH_BLOCK, col);
}

static void erase_paddle(int left, int y) {
    int fx = left ? PADDLE_MARGIN : (FIELD_W - 1 - PADDLE_MARGIN);
    for (int i = 0; i < PADDLE_H; ++i)
        put_cell(fx, y + i, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void draw_ball(void) {
    put_cell(ball.x, ball.y, CH_BALL, ATTR(C_YELLOW, C_BLACK));
}
static void erase_ball(void) {
    put_cell(ball.x, ball.y, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void hud(void) {
    char line[160], s1[8], s2[8];
    itoa(score1, s1, 10);
    itoa(score2, s2, 10);

    strcpy(line, "PONG  ");
    strcat(line, s1);
    strcat(line, " : ");
    strcat(line, s2);
    strcat(line, "   [W/S=P1, ");
    strcat(line, two_players ? "AU/AD=P2" : "TAB=1P/2P");
    strcat(line, ", 1/2/3=AI, P=pause, R=restart, Q=quit]  ");
    strcat(line, "AI: ");
    strcat(line, DIFF_TABLE[difficulty].name);

    sys_setcursor(0, ORG_Y + FIELD_H + 2);
    sys_write(line);
    sys_write("\n");
}

static void draw_everything(void) {
    clear_field();
    draw_border();
    draw_paddle(1, p1.y, p1.glow);
    draw_paddle(0, p2.y, p2.glow);
    draw_ball();
    hud();
    sys_setcursor(79, 24);
}

static void center_paddles(void) {
    p1.y = (FIELD_H - PADDLE_H) / 2;
    p1.glow = 0;
    p1.last_move_dir = 0;
    p2.y = (FIELD_H - PADDLE_H) / 2;
    p2.glow = 0;
    p2.last_move_dir = 0;
}

static void reset_round(int serve_dir) {
    center_paddles();
    ball.x = FIELD_W / 2;
    ball.y = FIELD_H / 2;
    ball.vx = (serve_dir == 0) ? ((sys_getticks() & 1) ? 1 : -1) : serve_dir;
    ball.vy = ((sys_getticks() % 3) - 1);
    if (ball.vy == 0)
        ball.vy = (sys_getticks() & 1) ? 1 : -1;
    ball.vy = clampi(ball.vy, VY_MIN, VY_MAX);
    ball.ball_delay = 3;
    serve_sleep_until = sys_getticks() + SERVE_COUNTDOWN_SEC * 18;
}

static int predict_intersect_y(void) {
    if (ball.vx <= 0)
        return FIELD_H / 2;
    int x = ball.x, y = ball.y, vy = ball.vy;
    const int target_x = FIELD_W - 1 - PADDLE_MARGIN;
    while (x < target_x) {
        x += ball.vx;
        y += vy;
        if (y < 0) {
            y = 0;
            vy = -vy;
        }
        if (y >= FIELD_H) {
            y = FIELD_H - 1;
            vy = -vy;
        }
    }
    return y;
}

static void ai_step(void) {
    unsigned now = sys_getticks();
    if (two_players)
        return;

    static unsigned sleep_until = 0;
    static int last_vx = 0;

    if (ball.vx > 0 && last_vx <= 0) {
        sleep_until = now + DIFF_TABLE[difficulty].react_delay;
    }
    last_vx = ball.vx;

    if ((int)(now - sleep_until) < 0)
        return;
    if ((int)(now - next_ai) < 0)
        return;
    next_ai = now + DIFF_TABLE[difficulty].ai_step_delay;

    int target = predict_intersect_y();

    int jitter = DIFF_TABLE[difficulty].jitter;
    if (jitter) {
        int r = (int)(sys_getticks() % (2 * jitter + 1)) - jitter;
        target += r;
    }
    target = clampi(target - PADDLE_H / 2, 0, FIELD_H - PADDLE_H);

    if (p2.y < target) {
        erase_paddle(0, p2.y);
        p2.y++;
        p2.last_move_dir = +1;
        draw_paddle(0, p2.y, p2.glow);
    } else if (p2.y > target) {
        erase_paddle(0, p2.y);
        p2.y--;
        p2.last_move_dir = -1;
        draw_paddle(0, p2.y, p2.glow);
    } else
        p2.last_move_dir = 0;
}

static void speedup_ball(void) {
    if (ball.ball_delay > BALL_DELAY_MIN)
        ball.ball_delay--;
}

static void apply_paddle_spin(Paddle* pad, int rel_hit) {
    int center = PADDLE_H / 2;
    int delta = (rel_hit < center) ? -1 : (rel_hit > center ? +1 : 0);
    ball.vy += delta + pad->last_move_dir;
    ball.vy = clampi(ball.vy, VY_MIN, VY_MAX);
}

static int ball_step(void) {
    int nextx = ball.x + ball.vx;
    int nexty = ball.y + ball.vy;

    if (nexty < 0) {
        nexty = 0;
        ball.vy = -ball.vy;
    }
    if (nexty >= FIELD_H) {
        nexty = FIELD_H - 1;
        ball.vy = -ball.vy;
    }

    int left_x = PADDLE_MARGIN;
    int right_x = FIELD_W - 1 - PADDLE_MARGIN;

    if (nextx == left_x && nexty >= p1.y && nexty < p1.y + PADDLE_H) {
        ball.vx = +1;
        apply_paddle_spin(&p1, nexty - p1.y);
        p1.glow = GLOW_TICKS;
        speedup_ball();
    }

    if (nextx == right_x && nexty >= p2.y && nexty < p2.y + PADDLE_H) {
        ball.vx = -1;
        apply_paddle_spin(&p2, nexty - p2.y);
        p2.glow = GLOW_TICKS;
        speedup_ball();
    }

    if (nextx < 0)
        return -1;
    if (nextx >= FIELD_W)
        return +1;

    erase_ball();
    ball.x = nextx;
    ball.y = nexty;
    draw_ball();
    return 0;
}

static void draw_countdown(unsigned now) {
    if ((int)(now - serve_sleep_until) >= 0)
        return;

    unsigned ticks_left = serve_sleep_until - now;
    int sec_left = (int)(ticks_left / 18) + 1;
    if (sec_left < 1)
        sec_left = 1;
    char msg[32];
    strcpy(msg, "Serve in ");
    char n[8];
    itoa(sec_left, n, 10);
    strcat(msg, n);
    sys_setcursor(ORG_X + FIELD_W / 2 - 5, ORG_Y + FIELD_H / 2);
    sys_write(msg);
}

void main(void) {
    sys_clear();
    reset_round((sys_getticks() & 1) ? +1 : -1);
    draw_everything();

    unsigned paused = 0;
    next_ui = sys_getticks() + tick_delay_ui;
    next_ball = sys_getticks() + ball.ball_delay;
    next_ai = sys_getticks() + 1;

    while (1) {
        unsigned ch;
        while ((ch = sys_trygetchar()) != 0) {
            char c = (char)ch;
            if (c == 'q' || c == 'Q') {
                sys_write("\nBye!\n");
                sys_exit();
            }

            if (c == 'p' || c == 'P') {
                paused = !paused;
                hud();
                continue;
            }
            if (c == 'r' || c == 'R') {
                score1 = score2 = 0;
                reset_round((sys_getticks() & 1) ? +1 : -1);
                draw_everything();
                continue;
            }
            if (c == '\t') {
                two_players = !two_players;
                hud();
            }

            if (c == '1') {
                difficulty = DIFF_EASY;
                hud();
            }
            if (c == '2') {
                difficulty = DIFF_NORMAL;
                hud();
            }
            if (c == '3') {
                difficulty = DIFF_HARD;
                hud();
            }

            if (paused)
                continue;

            if (c == 'w' || c == 'W') {
                if (p1.y > 0) {
                    erase_paddle(1, p1.y);
                    p1.y--;
                    p1.last_move_dir = -1;
                    draw_paddle(1, p1.y, p1.glow);
                }
            } else if (c == 's' || c == 'S') {
                if (p1.y < FIELD_H - PADDLE_H) {
                    erase_paddle(1, p1.y);
                    p1.y++;
                    p1.last_move_dir = +1;
                    draw_paddle(1, p1.y, p1.glow);
                }
            }

            else if ((unsigned char)c == KEY_UP) {
                two_players = 1;
                if (p2.y > 0) {
                    erase_paddle(0, p2.y);
                    p2.y--;
                    p2.last_move_dir = -1;
                    draw_paddle(0, p2.y, p2.glow);
                }
            } else if ((unsigned char)c == KEY_DOWN) {
                two_players = 1;
                if (p2.y < FIELD_H - PADDLE_H) {
                    erase_paddle(0, p2.y);
                    p2.y++;
                    p2.last_move_dir = +1;
                    draw_paddle(0, p2.y, p2.glow);
                }
            }
        }

        unsigned now = sys_getticks();

        if (!paused && (int)(now - serve_sleep_until) < 0) {
            draw_countdown(now);
            asm volatile("hlt");
            continue;
        }

        if (!paused)
            ai_step();

        if (!paused && (int)(now - next_ball) >= 0) {
            int res = ball_step();
            if (res != 0) {
                if (res > 0)
                    score1++;
                else
                    score2++;
                hud();

                if (score1 >= MAX_SCORE || score2 >= MAX_SCORE) {
                    sys_setcursor(ORG_X, ORG_Y + FIELD_H + 3);
                    sys_write(score1 > score2 ? "P1 WINS!  Premi ENTER...\n" : "P2 WINS!  Premi ENTER...\n");
                    while (sys_getchar() != '\n') {
                    }
                    sys_exit();
                }

                unsigned pause_until = now + 18;
                while ((int)(sys_getticks() - pause_until) < 0)
                    asm volatile("hlt");

                reset_round(res > 0 ? -1 : +1);
                draw_everything();
                next_ball = sys_getticks() + ball.ball_delay;
                next_ai = sys_getticks() + 1;
                continue;
            }
            next_ball += ball.ball_delay;
        }

        if ((int)(now - next_ui) >= 0) {
            if (p1.glow > 0) {
                p1.glow--;
                draw_paddle(1, p1.y, p1.glow);
            }
            if (p2.glow > 0) {
                p2.glow--;
                draw_paddle(0, p2.y, p2.glow);
            }
            hud();
            next_ui += tick_delay_ui;
        }

        asm volatile("hlt");
    }
}
