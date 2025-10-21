#pragma once
#include <stdint.h>

#define KEY_LEFT   0x90
#define KEY_RIGHT  0x91
#define KEY_UP     0x92
#define KEY_DOWN   0x93

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT = 2, 
    SYSCALL_EXEC = 3,
    SYSCALL_GETCHAR = 4,
    SYSCALL_CLEAR = 5,
    SYSCALL_WRITEFILE = 6,
    SYSCALL_LISTFILES = 7,
    SYSCALL_READFILE = 8,
    SYSCALL_GETARG = 9,
    SYSCALL_PUT_AT = 10,
    SYSCALL_SETCURSOR = 11,
    SYSCALL_TRYGETCHAR = 12,
    SYSCALL_GETTICKS = 13,
    SYSCALL_SLEEP = 14,
    SYSCALL_GETSIZE = 15,
    SYSCALL_BLIT = 16,
    SYSCALL_MOUSE_GET = 17,
    SYSCALL_MOUSE_SHOW= 18,
    SYSCALL_ENUMFILES = 19,
    SYSCALL_GFX_INFO  = 20,
    SYSCALL_GFX_CLEAR = 21,
    SYSCALL_GFX_PUTPX = 22,
    SYSCALL_GFX_BLIT = 23,
};
typedef struct { 
    char ch; 
    unsigned char attr; 
} aso_cell_t;

typedef struct {
    int x, y;
    unsigned int buttons; // bit0=L, bit1=R, bit2=M
} mouse_info_t;

static inline unsigned int sys_getticks(void){
    unsigned int t;

    asm volatile("int $0x80" 
                : "=a"(t) 
                : "a"(SYSCALL_GETTICKS) 
                : "memory","cc");

    return t;
}

static inline void sys_sleep(unsigned int ticks){
    asm volatile("int $0x80" 
                :: "a"(SYSCALL_SLEEP), "b"(ticks) 
                : "memory","cc");
}

static inline unsigned int sys_trygetchar(void){
    unsigned int ch;

    asm volatile("int $0x80" 
                : "=a"(ch) 
                : "a"(SYSCALL_TRYGETCHAR) 
                : "memory","cc");

    return ch; // 0 = No key
}

static inline int sys_write(const char* s) {
    int ret;

    asm volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYSCALL_WRITE), "b"(s)
                     : "memory","cc");
    return ret;
}

static inline void sys_exit(void) {
    asm volatile("int $0x80" 
                : : "a"(SYSCALL_EXIT) 
                : "memory","cc");
}

static inline void sys_exec(const char* name) {
    asm volatile("int $0x80" : : "a"(SYSCALL_EXEC), "b"(name) : "memory","cc");
}

static inline char sys_getchar(void) {
    char c;

    asm volatile("int $0x80"
                 : "=a"(c)
                 : "a"(SYSCALL_GETCHAR)
                 : "memory", "cc");
    return c;
}

static inline void sys_clear(void) {
    asm volatile("int $0x80" 
                : : "a"(SYSCALL_CLEAR) 
                : "memory","cc");
}


static inline int sys_writefile(const char* name, const char* data, int size) {
    int ret;

    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYSCALL_WRITEFILE), "b"(name), "c"(data), "d"(size)
                 : "memory", "cc");
    return ret;
}

static inline int sys_listfiles(void) {
    asm volatile("int $0x80" 
                : : "a"(SYSCALL_LISTFILES) 
                : "memory","cc");
}

static inline int sys_readfile(const char* name, char* buf, int max) {
    int ret;

    asm volatile("int $0x80" 
                : "=a"(ret)
                : "a"(SYSCALL_READFILE), "b"(name), "c"(buf), "d"(max)
                : "memory","cc");
    return ret; 
}

static inline int sys_getarg(char* buf, int max) {
    int ret;

    asm volatile("int $0x80" 
                : "=a"(ret)
                : "a"(SYSCALL_GETARG), "b"(buf), "d"(max)
                : "memory","cc");
    return ret; // len, 0 if empty, <0 error
}

static inline void sys_setcursor(int x, int y) {
    asm volatile("int $0x80"
                :: "a"(SYSCALL_SETCURSOR),"b"(x),"c"(y)
                :"memory","cc");
}

static inline void sys_put_at(int x, int y, char ch, unsigned char color) {
    unsigned int edx = ((unsigned int)(uint8_t)color << 8) | (uint8_t)ch;

    asm volatile("int $0x80"
                ::"a"(SYSCALL_PUT_AT),"b"(x),"c"(y),"d"(edx)
                :"memory","cc");
}

static inline int sys_getsize(int* out_cols, int* out_rows){
    unsigned int packed;

    asm volatile("int $0x80" 
                : "=a"(packed) 
                : "a"(SYSCALL_GETSIZE) 
                : "memory","cc");

    int cols = (int)((packed >> 16) & 0xFFFF);
    int rows = (int)(packed & 0xFFFF);

    if (out_cols) *out_cols = cols;
    if (out_rows) *out_rows = rows;

    return 1;
}

static inline int sys_blit(const aso_cell_t* fb, int count){
    int ret;

    asm volatile("int $0x80"
                : "=a"(ret)
                : "a"(SYSCALL_BLIT), "b"(fb), "c"(count)
                : "memory","cc");

    return ret; 
}

static inline int sys_mouse_get(mouse_info_t* out) {
    int ret;

    asm volatile("int $0x80" 
                : "=a"(ret) 
                : "a"(SYSCALL_MOUSE_GET), "b"(out) 
                : "memory", "cc");

    return ret;
}

static inline void sys_mouse_show(int show) {
    asm volatile("int $0x80" 
                ::"a"(SYSCALL_MOUSE_SHOW), "b"(show) 
                : "memory", "cc");
}

static inline int sys_enumfiles(char* out, int max_entries, int name_max) {
    int ret;

    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYSCALL_ENUMFILES), "b"(out), "c"(max_entries), "d"(name_max)
                 : "memory","cc");

    return ret;
}

// Returns 0 if no 32bpp gfx else packs (w,h) into eax = (w<<16)|h
static inline unsigned int sys_gfx_info(void){
    unsigned int packed;

    asm volatile("int $0x80" 
                : "=a"(packed) 
                : "a"(SYSCALL_GFX_INFO) 
                : "memory","cc");

    return packed;
}

static inline int sys_gfx_clear(unsigned int rgb){
    int ret;

    asm volatile("int $0x80" 
                : "=a"(ret) 
                : "a"(SYSCALL_GFX_CLEAR), "b"(rgb)
                : "memory","cc");

    return ret;
}

static inline int sys_gfx_putpixel(int x,int y,unsigned int rgb){
    int ret;

    asm volatile("int $0x80" 
                : "=a"(ret) 
                : "a"(SYSCALL_GFX_PUTPX), "b"(x), "c"(y), "d"(rgb) 
                : "memory","cc");

    return ret;
}

static inline int sys_gfx_blit(const unsigned int* rgb32_fullscreen){
    int ret;

    asm volatile("int $0x80"
                : "=a"(ret)
                : "a"(SYSCALL_GFX_BLIT), "b"(rgb32_fullscreen)
                : "memory","cc");

    return ret;
}
