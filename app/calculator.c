// ./app/calculator.c
#include "asoapi.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"

/*
 * ASOS Calculator (select-with-arrows + enter)
 * - Move with Arrow Keys. Focused cell is highlighted.
 * - ENTER on digits/operators appends to the expression.
 * - ENTER on '=' evaluates and prints result.
 * - ENTER on 'C' clears expression & result.
 */

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

#define W 80
#define H 25

#define ORG_X  24
#define ORG_Y   5
#define CELL_W  5
#define CELL_H  3

typedef enum { TK_DIGIT, TK_OP, TK_EQ, TK_CLR } CellType;

typedef struct {
    int x, y;      // grid coords (0..cols-1, 0..rows-1)
    char label[4]; // what we draw in cell center
    CellType t;
} Cell;

/* ----- Layout -----------------------------------------------------------

Grid (cols x rows):

  [ 7 ] [ 8 ] [ 9 ] [ / ]
  [ 4 ] [ 5 ] [ 6 ] [ * ]
  [ 1 ] [ 2 ] [ 3 ] [ - ]
  [ 0 ] [  C ] [ = ] [ + ]

--------------------------------------------------------------------------*/

static Cell cells[] = {
    {0,0,"7",TK_DIGIT},{1,0,"8",TK_DIGIT},{2,0,"9",TK_DIGIT},{3,0,"/",TK_OP},
    {0,1,"4",TK_DIGIT},{1,1,"5",TK_DIGIT},{2,1,"6",TK_DIGIT},{3,1,"*",TK_OP},
    {0,2,"1",TK_DIGIT},{1,2,"2",TK_DIGIT},{2,2,"3",TK_DIGIT},{3,2,"-",TK_OP},
    {0,3,"0",TK_DIGIT},{1,3,"C",TK_CLR  },{2,3,"=",TK_EQ   },{3,3,"+",TK_OP},
};
static const int CELL_COUNT = (int)(sizeof(cells)/sizeof(cells[0]));
static int sel = 0;

static char expr[128];   // expression tape (ASCII like "12+3*7")
static char result[64];  // last result text
static int  expr_len = 0;

// helpers
static inline int clamp(int v,int lo,int hi){ return v<lo?lo:(v>hi?hi:v); }

static void draw_char(int x,int y,char ch,unsigned char attr){ sys_put_at(x,y,ch,attr); }

static void draw_text(int x,int y,const char* s,unsigned char attr){
    for (int i=0;s[i];++i) sys_put_at(x+i,y,s[i],attr);
}

static void box(int x,int y,int w,int h,unsigned char attr){
    for (int i=0;i<w;i++){ sys_put_at(x+i,y,'-',attr); sys_put_at(x+i,y+h-1,'-',attr); }
    for (int j=0;j<h;j++){ sys_put_at(x,y+j,'|',attr); sys_put_at(x+w-1,y+j,'|',attr); }
    sys_put_at(x,y,'+',attr); sys_put_at(x+w-1,y,'+',attr);
    sys_put_at(x,y+h-1,'+',attr); sys_put_at(x+w-1,y+h-1,'+',attr);
}

static void cell_rect(int gx,int gy,int* x,int* y,int* w,int* h){
    *x = ORG_X + gx * CELL_W;
    *y = ORG_Y + gy * CELL_H;
    *w = CELL_W; *h = CELL_H;
}

static void draw_cell(int idx, int focused){
    int rx, ry, rw, rh; cell_rect(cells[idx].x, cells[idx].y, &rx,&ry,&rw,&rh);

    unsigned char frame = focused ? ATTR(C_BLACK, C_YELLOW) : ATTR(C_LIGHTGRAY, C_BLACK);
    unsigned char fill  = focused ? ATTR(C_BLACK, C_YELLOW) : ATTR(C_WHITE, C_BLUE);

    // fill
    for (int j=0;j<rh;j++)
        for (int i=0;i<rw;i++)
            draw_char(rx+i, ry+j, ' ', fill);

    // frame
    box(rx, ry, rw, rh, frame);

    // label centered
    int lx = rx + (rw - (int)strlen(cells[idx].label))/2;
    int ly = ry + rh/2;
    draw_text(lx, ly, cells[idx].label, ATTR(C_WHITE, focused?C_YELLOW:C_BLUE));
}

static void draw_grid(void){
    for (int i=0;i<CELL_COUNT;i++) draw_cell(i, i==sel);
}

static void draw_ui(void){
    sys_clear();
    draw_text( (W-22)/2, 1, "--- ASOS CALCULATOR ---", ATTR(C_YELLOW, C_BLACK));
    draw_text( (W-54)/2, 3, "Arrows: move  Enter: select   C: clear   =: evaluate   Q: quit", ATTR(C_LIGHTGRAY, C_BLACK));

    draw_grid();

    // Expression + result bars
    // expr
    for (int i=0;i<76;i++) draw_char(2+i, 19, ' ', ATTR(C_WHITE, C_BLACK));
    draw_text(2, 19, "Expr: ", ATTR(C_CYAN, C_BLACK));
    draw_text(8, 19, expr, ATTR(C_WHITE, C_BLACK));
    // result
    for (int i=0;i<76;i++) draw_char(2+i, 21, ' ', ATTR(C_WHITE, C_BLACK));
    draw_text(2, 21, "Result: ", ATTR(C_GREEN, C_BLACK));
    draw_text(10,21, result, ATTR(C_WHITE, C_BLACK));

    sys_setcursor(79,24);
}

static void expr_clear(void){
    expr_len = 0; expr[0] = 0;
    result[0] = 0;
}

// append single character to expr (digits or operator), with simple guards
static void expr_push_char(char c){
    if (expr_len >= (int)sizeof(expr)-2) return;
    // avoid consecutive operators (+-*/), allow minus as unary only if empty
    if (c=='+'||c=='-'||c=='*'||c=='/'){
        if (expr_len==0){
            // allow unary minus only
            if (c!='-') return;
        } else {
            char prev = expr[expr_len-1];
            if (prev=='+'||prev=='-'||prev=='*'||prev=='/') return;
        }
    }
    expr[expr_len++] = c;
    expr[expr_len] = 0;
}

// simple parser/evaluator with operator precedence (*,/ before +,-)
static int eval_expr(const char* s, int* out_ok, int* out_value){
    // Tokenize into numbers and ops
    int nums[64]; char ops[64]; int nc=0, oc=0;
    int i=0, nlen=(int)strlen(s);
    while (i<nlen){
        // read (optional) unary minus for a number
        int sign = 1;
        if ((s[i]=='-' && (i==0 || s[i-1]=='+'||s[i-1]=='-'||s[i-1]=='*'||s[i-1]=='/'))){
            sign = -1; i++;
            if (i>=nlen || s[i]<'0' || s[i]>'9'){ *out_ok=0; return 0; }
        }
        if (s[i]<'0' || s[i]>'9'){ *out_ok=0; return 0; }
        int v=0;
        while (i<nlen && s[i]>='0' && s[i]<='9'){ v = v*10 + (s[i]-'0'); i++; }
        if (nc>=64){ *out_ok=0; return 0; }
        nums[nc++] = sign*v;

        if (i<nlen){
            char op = s[i];
            if (op!='+'&&op!='-'&&op!='*'&&op!='/'){ *out_ok=0; return 0; }
            if (oc>=64){ *out_ok=0; return 0; }
            ops[oc++] = op;
            i++;
        }
    }
    if (nc==0){ *out_ok=0; return 0; }
    // pass 1: handle * and /
    for (int k=0;k<oc;){
        if (ops[k]=='*' || ops[k]=='/'){
            if (k+1>nc-1){ *out_ok=0; return 0; }
            int a = nums[k], b = nums[k+1];
            if (ops[k]=='/'){
                if (b==0){ *out_ok=0; return 0; }
                nums[k] = a / b;
            } else {
                nums[k] = a * b;
            }
            // compact arrays
            for (int m=k+1; m<nc-1; ++m) nums[m] = nums[m+1];
            for (int m=k;   m<oc-1; ++m) ops[m]  = ops[m+1];
            nc--; oc--;
        } else {
            k++;
        }
    }
    // pass 2: left-to-right for + and -
    int acc = nums[0];
    for (int k=0;k<oc;k++){
        if (ops[k]=='+') acc += nums[k+1];
        else             acc -= nums[k+1];
    }
    *out_ok = 1; *out_value = acc; return 1;
}

static void apply_cell(int idx){
    Cell *c = &cells[idx];
    if (c->t == TK_DIGIT){
        expr_push_char(c->label[0]);
    } else if (c->t == TK_OP){
        expr_push_char(c->label[0]);
    } else if (c->t == TK_CLR){
        expr_clear();
    } else if (c->t == TK_EQ){
        if (expr_len==0){ strcpy(result, ""); return; }
        int ok=0, val=0;
        eval_expr(expr, &ok, &val);
        if (!ok) strcpy(result, "[error]");
        else {
            char tmp[32]; itoa(val, tmp, 10);
            strcpy(result, tmp);
        }
    }
}

static void redraw_tape(void){
    // expr line
    for (int i=0;i<70;i++) sys_put_at(8+i, 19, ' ', ATTR(C_WHITE, C_BLACK));
    draw_text(8, 19, expr, ATTR(C_WHITE, C_BLACK));
    // result line
    for (int i=0;i<66;i++) sys_put_at(10+i, 21, ' ', ATTR(C_WHITE, C_BLACK));
    draw_text(10, 21, result, ATTR(C_WHITE, C_BLACK));
    sys_setcursor(79,24);
}

static void move_sel(int dx, int dy){
    // grid is 4x4, compute from current selection
    int gx = cells[sel].x, gy = cells[sel].y;
    gx = clamp(gx+dx, 0, 3);
    gy = clamp(gy+dy, 0, 3);
    // find cell with that (gx,gy)
    for (int i=0;i<CELL_COUNT;i++)
        if (cells[i].x==gx && cells[i].y==gy){ 
            draw_cell(sel, 0); sel=i; draw_cell(sel, 1); 
            break; 
        }
    sys_setcursor(79,24);
}

void main(void){
    expr_clear();
    draw_ui();

    unsigned int next_refresh = sys_getticks() + 2;

    while (1){
        unsigned int ch;
        while ((ch = sys_trygetchar()) != 0){
            char c = (char)ch;
            if (c=='q' || c=='Q'){ sys_write("\nBye!\n"); sys_exit(); }
            else if ((unsigned char)c==KEY_LEFT)  move_sel(-1,0);
            else if ((unsigned char)c==KEY_RIGHT) move_sel(+1,0);
            else if ((unsigned char)c==KEY_UP)    move_sel(0,-1);
            else if ((unsigned char)c==KEY_DOWN)  move_sel(0,+1);
            else if (c=='\n'){ 
                apply_cell(sel); 
                redraw_tape(); 
            }
            // typing shortcuts: digits/op directly add to expr
            else if (c>='0' && c<='9'){ expr_push_char(c); redraw_tape(); }
            else if (c=='+'||c=='-'||c=='*'||c=='/'){ expr_push_char(c); redraw_tape(); }
            else if (c=='c' || c=='C'){ expr_clear(); redraw_tape(); }
            else if (c=='='){ // mimic pressing '=' cell
                // jump highlight to '=' for feedback
                int old = sel;
                for (int i=0;i<CELL_COUNT;i++) if (cells[i].t==TK_EQ){ draw_cell(sel,0); sel=i; draw_cell(sel,1); break; }
                apply_cell(sel); redraw_tape();
                // restore selection
                draw_cell(sel,0); sel = old; draw_cell(sel,1);
            }
        }

        // tiny periodic HUD refresh to keep cursor parked
        unsigned int now = sys_getticks();
        if ((int)(now - next_refresh) >= 0){
            sys_setcursor(79,24);
            next_refresh += 6;
        }
        asm volatile("hlt");
    }
}
