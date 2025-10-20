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
#define C_LIGHTGRAY 0x7
#define C_DARKGRAY  0x8
#define C_YELLOW  0xE
#define C_WHITE   0xF
#define ATTR(fg,bg)   (((bg)<<4)|((fg)&0x0F))

#define FIELD_W   60
#define FIELD_H   22
#define ORG_X     10
#define ORG_Y     1

#define PADDLE_H      4
#define PADDLE_MARGIN 2
#define MAX_SCORE     7

#define CH_BLOCK  ((char)219) // █
#define CH_BALL   'O'
#define CH_EMPTY  ' '

static int p1y, p2y;
static int ballx, bally;
static int vx, vy; 
static int score1 = 0, score2 = 0;
static int two_players = 0;
static unsigned int tick_delay = 2;
static unsigned int ball_delay = 3;

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

static inline void put_cell(int fx, int fy, char ch, unsigned char attr){
    int sx = ORG_X + fx;
    int sy = ORG_Y + fy;
    if (fx < 0 || fy < 0 || fx >= FIELD_W || fy >= FIELD_H) return;
    sys_put_at(sx, sy, ch, attr);
}

static void draw_border(void){
    unsigned char col = ATTR(C_WHITE, C_BLACK);
    // Top/bottom
    for (int x = -1; x <= FIELD_W; ++x){
        sys_put_at(ORG_X + x, ORG_Y - 1, CH_BLOCK, col);
        sys_put_at(ORG_X + x, ORG_Y + FIELD_H, CH_BLOCK, col);
    }
    // Sides
    for (int y = -1; y <= FIELD_H; ++y){
        sys_put_at(ORG_X - 1, ORG_Y + y, CH_BLOCK, col);
        sys_put_at(ORG_X + FIELD_W, ORG_Y + y, CH_BLOCK, col);
    }
    // Center dashed net
    for (int y = 0; y < FIELD_H; ++y){
        if (y % 2 == 0) put_cell(FIELD_W/2, y, CH_BLOCK, ATTR(C_DARKGRAY, C_BLACK));
    }
}

static void clear_field(void){
    for (int y = 0; y < FIELD_H; ++y)
        for (int x = 0; x < FIELD_W; ++x)
            put_cell(x, y, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void draw_paddle(int left, int topy){
    int fx = left ? PADDLE_MARGIN : (FIELD_W - 1 - PADDLE_MARGIN);
    for (int i = 0; i < PADDLE_H; ++i)
        put_cell(fx, topy + i, CH_BLOCK, ATTR(C_GREEN, C_BLACK));
}

static void erase_paddle(int left, int topy){
    int fx = left ? PADDLE_MARGIN : (FIELD_W - 1 - PADDLE_MARGIN);
    for (int i = 0; i < PADDLE_H; ++i)
        put_cell(fx, topy + i, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void draw_ball(void){
    put_cell(ballx, bally, CH_BALL, ATTR(C_YELLOW, C_BLACK));
}

static void erase_ball(void){
    put_cell(ballx, bally, CH_EMPTY, ATTR(C_BLACK, C_BLACK));
}

static void hud(void){
    char b[96], s1[8], s2[8];
    itoa(score1, s1, 10); itoa(score2, s2, 10);
    strcpy(b, "PONG  ");
    strcat(b, s1); strcat(b, " : "); strcat(b, s2);
    strcat(b, two_players ? "   [W/S = P1, AU/AD = P2, Q=quit]" : "   [W/S = move, AU/AD = toggle 2P, Q=quit]");
    sys_setcursor(0, ORG_Y + FIELD_H + 2);
    sys_write(b); sys_write("\n");
}

// Reset ball & paddles after a point
static void reset_round(int dir){
    p1y = (FIELD_H - PADDLE_H)/2;
    p2y = (FIELD_H - PADDLE_H)/2;

    ballx = FIELD_W/2;
    bally = FIELD_H/2;
    vx = dir;                 // Serve towards scorer’s opponent
    vy = (dir == 0) ? 0 : ( (sys_getticks() & 1) ? 1 : -1 );
    if (vy == 0) vy = 1;

    ball_delay = 3;           // Reset speed
}

// Simple AI for P2 
static void ai_step(void){
    // Follow ball with a small rate limit
    static unsigned int last = 0;
    unsigned int now = sys_getticks();
    if ((int)(now - last) < 1) return; // Limit AI speed
    last = now;

    int center = p2y + PADDLE_H/2;
    if (bally < center) p2y--;
    else if (bally > center) p2y++;
    p2y = clamp(p2y, 0, FIELD_H - PADDLE_H);
}

// One logic step for the ball; returns: -1 = P2 scores, +1 = P1 scores, 0 otherwise 
static int ball_step(void){
    int nextx = ballx + vx;
    int nexty = bally + vy;

    // Wall bounce on top/bottom
    if (nexty < 0) { nexty = 0; vy = -vy; }
    if (nexty >= FIELD_H){ nexty = FIELD_H - 1; vy = -vy; }

    // Check paddles
    int left_x  = PADDLE_MARGIN;
    int right_x = FIELD_W - 1 - PADDLE_MARGIN;

    // Left paddle
    if (nextx == left_x){
        if (nexty >= p1y && nexty < p1y + PADDLE_H){
            vx = 1;

            // tweak vy based on hit position
            int rel = nexty - p1y;

            if (rel < PADDLE_H/2) vy = -1;
            else if (rel > PADDLE_H/2) vy = +1;

            // Speed up (min 1)
            if (ball_delay > 1) ball_delay--;
        }
    }
    // Right paddle
    if (nextx == right_x){
        if (nexty >= p2y && nexty < p2y + PADDLE_H){
            vx = -1;
            int rel = nexty - p2y;
            if (rel < PADDLE_H/2) vy = -1;
            else if (rel > PADDLE_H/2) vy = +1;
            if (ball_delay > 1) ball_delay--;
        }
    }

    // Score check (miss)
    if (nextx < 0)  return -1; // P2 scores
    if (nextx >= FIELD_W) return +1; // P1 scores

    // move
    erase_ball();
    ballx = nextx; bally = nexty;
    draw_ball();
    return 0;
}

static void draw_everything(void){
    clear_field();
    draw_border();
    draw_paddle(1, p1y);
    draw_paddle(0, p2y);
    draw_ball();
    hud();
    sys_setcursor(79, 24);
}

void main(void){
    sys_clear();
    reset_round((sys_getticks() & 1) ? 1 : -1);
    draw_everything();

    unsigned int next_tick = sys_getticks() + tick_delay;
    unsigned int next_ball = sys_getticks() + ball_delay;

    while (1){
        unsigned int ch;
        while ((ch = sys_trygetchar()) != 0){
            char c = (char)ch;
            if (c == 'q' || c == 'Q'){
                sys_write("\nBye!\n");
                sys_exit();
            } else if (c == 'w' || c == 'W'){
                if (p1y > 0){ erase_paddle(1, p1y); p1y--; draw_paddle(1, p1y); }
            } else if (c == 's' || c == 'S'){
                if (p1y < FIELD_H - PADDLE_H){ erase_paddle(1, p1y); p1y++; draw_paddle(1, p1y); }
            } else if ((unsigned char)c == KEY_UP){
                // If player presses arrows, consider it 2P control
                two_players = 1;
                if (p2y > 0){ erase_paddle(0, p2y); p2y--; draw_paddle(0, p2y); }
            } else if ((unsigned char)c == KEY_DOWN){
                two_players = 1;
                if (p2y < FIELD_H - PADDLE_H){ erase_paddle(0, p2y); p2y++; draw_paddle(0, p2y); }
            }
        }

        // AI (if active)
        if (!two_players) {
            ai_step();
        }

        unsigned int now = sys_getticks();

        if ((int)(now - next_ball) >= 0){
            int res = ball_step();
            if (res != 0){
                // point scored
                if (res > 0) score1++; else score2++;
                hud();

                if (score1 >= MAX_SCORE || score2 >= MAX_SCORE){
                    sys_setcursor(0, ORG_Y + FIELD_H + 3);
                    sys_write(score1 > score2 ? "P1 WINS! Press ENTER...\n"
                                              : "P2 WINS! Press ENTER...\n");
                    while (sys_getchar() != '\n') {}
                    sys_exit();
                }

                // Brief pause then new serve toward the player who conceded
                unsigned int pause = sys_getticks() + 18; // ~1s
                while ((int)(sys_getticks() - pause) < 0) asm volatile("hlt");

                // Redraw fresh round
                reset_round(res > 0 ? -1 : +1);
                draw_everything();
            }
            next_ball += ball_delay;
        }

        if ((int)(now - next_tick) >= 0){
            // Steady pace for inputs/hud refresh if needed
            hud();
            next_tick += tick_delay;
        }

        asm volatile("hlt");
    }
}
