#include "syscall.h"
#include "keyboard.h"
#include "asofs.h"
#include "idt.h"
#include "console.h"
#include "mouse.h"
#include "gfx.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include <stdint.h>

static char last_exec_arg[32];
extern volatile unsigned int g_ticks;

typedef uint32_t (*sysfn_t)(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

__attribute__((naked)) void syscall_trampoline(void) {
    asm volatile(
        ".intel_syntax noprefix\n"
        "pusha\n"
        "push edx\n"
        "push ecx\n"
        "push ebx\n"
        "push eax\n"
        "call syscall_handler\n"   // EAX
        "add  esp, 16\n"
        "mov  [esp+28], eax\n"
        "popa\n"
        "iretd\n"
        ".att_syntax prefix\n"
    );
}

void syscall_init(void) {
    // 0xEF = present | DPL=3 | 32-bit interrupt gate, IRQ enabled
    idt_set_gate(0x80, (uint32_t)syscall_trampoline, 0x08, 0xEF);
}

static uint32_t sys_write_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ecx; (void)edx;

    const char* s = (const char*) ebx;
    if (s) console_write(s);

    return 0;
}

static uint32_t sys_exit_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ebx; (void)ecx; (void)edx;

    asofs_return_to_kernel();

    return 0; // We never really return here
}


static uint32_t sys_exec_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ecx; (void)edx;

    const char* full = (const char*)ebx;
    if (!full)
        return (uint32_t)-1;

    // Split "app [arg]" -> app in app[], arg in last_exec_arg
    char app[32];
    int i = 0, j = 0;

    while (full[i] == ' ')
        i++;

    while (full[i] && full[i] != ' ' && j < (int)sizeof(app) - 1)
        app[j++] = full[i++];
    app[j] = '\0';

    while (full[i] == ' ')
        i++;

    // Copy remaining as argument
    int k = 0;
    while (full[i] && k < (int)sizeof(last_exec_arg) - 1)
        last_exec_arg[k++] = full[i++];
    last_exec_arg[k] = '\0';

    asofs_run_app(app);
    return 0;
}


static uint32_t sys_getchar_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ebx; (void)ecx; (void)edx;

    while (!kbd_available()) asm volatile("hlt");

    return (uint8_t)kbd_getchar();
}

static uint32_t sys_clear_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ebx; (void)ecx; (void)edx;
    console_clear();

    return 0;
}

static uint32_t sys_writefile_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax;
    const char* name = (const char*) ebx;
    const char* data = (const char*) ecx;
    uint32_t size = edx;

    if (name && data && size > 0) return asofs_write_file(name, data, size);

    return (uint32_t)-1;
}

static uint32_t sys_listfiles_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ebx; (void)ecx; (void)edx;
    asofs_list_files();
    return 0;
}

static uint32_t sys_readfile_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax;
    const char* name = (const char*) ebx;
    char* dest = (char*) ecx;
    uint32_t max = edx;

    if (!name || !dest || max == 0) return (uint32_t)-1;

    asofs_file_entry_t* f = asofs_find_file(name);
    if (!f) return (uint32_t)-2;

    // Read until min(size, max)
    uint32_t to_read = (f->size < max) ? f->size : max;

    if (asofs_load_file(f, (uint8_t*)dest) != 0) return (uint32_t)-3;

    return to_read; // Bytes left
}

static uint32_t sys_enumfiles_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax;

    char* out = (char*)ebx;
    int max_entries = (int)ecx;
    int name_max = (int)edx;
    int rc = asofs_enum_files(out, max_entries, name_max);

    return (rc < 0) ? (uint32_t)-1 : (uint32_t)rc;
}

static uint32_t sys_getarg_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ecx;
    char* out = (char*) ebx;
    uint32_t max = edx;
    if (!out || max == 0) return (uint32_t)-1;

    uint32_t n = 0;
    while (last_exec_arg[n] && n + 1 < max) { out[n] = last_exec_arg[n]; n++; }
    out[n] = 0;
    return n; // Length
}


static uint32_t sys_put_at_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    int x = (int)ebx;
    int y = (int)ecx;
    char c = (char)(edx & 0xFF);
    uint8_t attr = (uint8_t)((edx >> 8) & 0xFF);

    console_put_at_color(x, y, c, attr);
    return 0;
}


static uint32_t sys_setcursor_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)edx;
    console_setcursor((int)ebx, (int)ecx);
    
    return 0;
}

static uint32_t sys_trygetchar_impl(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    (void)a;(void)b;(void)c;(void)d;

    if (!kbd_available()) return 0;

    return (uint8_t)kbd_getchar();
}

static uint32_t sys_getticks_impl(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    (void)a;(void)b;(void)c;(void)d;

    return g_ticks;
}

static uint32_t sys_sleep_impl(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    (void)b;(void)c;(void)d;
    unsigned int until = g_ticks + a;   // 'a' = ticks to wait for

    while (g_ticks < until) asm volatile("hlt");

    return 0;
}


static uint32_t sys_getsize_impl(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    (void)a;(void)b;(void)c;(void)d;

    int cols=80, rows=25;
    console_get_size(&cols, &rows);

    uint32_t packed = ((uint32_t)(cols & 0xFFFF) << 16) | (uint32_t)(rows & 0xFFFF);

    return packed;
}

// User passes an array of {char ch; uint8_t attr} cells in row-major.
static uint32_t sys_blit_impl(uint32_t a,uint32_t ebx,uint32_t ecx,uint32_t d){
    (void)a;(void)d;
    const uint8_t* p = (const uint8_t*)ebx;
    uint32_t count = ecx;
    int cols=80, rows=25;

    console_get_size(&cols, &rows);
    uint32_t max = (uint32_t)(cols * rows);

    if (count > max) count = max;

    for (uint32_t i=0; i<count; ++i){
        int x = (int)(i % (uint32_t)cols);
        int y = (int)(i / (uint32_t)cols);
        char ch = (char)p[i*2 + 0];
        uint8_t attr = p[i*2 + 1];

        console_put_at_color(x, y, ch, attr);
    }
    // Park cursor out of the way
    console_setcursor(cols-1, rows-1);

    return count;
}

static uint32_t sys_mouse_get_impl(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a;
    (void)c;
    (void)d;
    int* buf = (int*)b;  // [0]=x, [1]=y, [2]=buttons

    if (!buf)
        return (uint32_t)-1;

    int x, y;
    unsigned btn;

    mouse_get(&x, &y, &btn);
    buf[0] = x;
    buf[1] = y;
    buf[2] = (int)btn;

    return 0;
}

static uint32_t sys_mouse_show_impl(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a;
    (void)c;
    (void)d;

    mouse_set_visible((int)b);

    return 0;
}

static uint32_t sys_gfx_info_impl(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    const gfx_info_t* gi = gfx_info();

    if (!gi || gi->bpp != 32 || gi->w == 0 || gi->h == 0) 
        return 0; // 0 means “not available”
    
    // Pack (w,h) in 16+16 bits
    uint32_t packed = ((uint32_t)(gi->w & 0xFFFF) << 16) | (uint32_t)(gi->h & 0xFFFF);

    return packed;
}
static uint32_t sys_gfx_clear_impl(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)c; (void)d;
    const gfx_info_t* gi = gfx_info();

    if (!gi || gi->bpp != 32) 
        return (uint32_t)-1;

    gfx_clear(b); // b = 0x00RRGGBB
  
    return 0;
}

static uint32_t sys_gfx_putpx_impl(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a;
    int x = (int)b;
    int y = (int)c;
    uint32_t rgb = d; // 0x00RRGGBB
    const gfx_info_t* gi = gfx_info();

    if (!gi || gi->bpp != 32) 
        return (uint32_t)-1;

    gfx_putpixel(x, y, rgb);

    return 0;
}

// Dispatch table
static uint32_t sys_unknown_impl(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)eax; (void)ebx; (void)ecx; (void)edx;

    console_write("[SYSCALL] Unknown syscall!\n");

    return (uint32_t)-1;
}

// Indexes = SYSCALL_* enum values
static const sysfn_t sys_table[] = {
    [0]                 = sys_unknown_impl, // not used 
    [SYSCALL_WRITE]     = sys_write_impl,
    [SYSCALL_EXIT]      = sys_exit_impl,
    [SYSCALL_EXEC]      = sys_exec_impl,
    [SYSCALL_GETCHAR]   = sys_getchar_impl,
    [SYSCALL_CLEAR]     = sys_clear_impl,
    [SYSCALL_WRITEFILE] = sys_writefile_impl,
    [SYSCALL_LISTFILES] = sys_listfiles_impl,
    [SYSCALL_READFILE]  = sys_readfile_impl,
    [SYSCALL_GETARG]    = sys_getarg_impl,
    [SYSCALL_PUT_AT]    = sys_put_at_impl,
    [SYSCALL_SETCURSOR] = sys_setcursor_impl,
    [SYSCALL_TRYGETCHAR] = sys_trygetchar_impl,
    [SYSCALL_GETTICKS]   = sys_getticks_impl,
    [SYSCALL_SLEEP]      = sys_sleep_impl,
    [SYSCALL_GETSIZE]    = sys_getsize_impl,
    [SYSCALL_BLIT]       = sys_blit_impl,
    [SYSCALL_MOUSE_GET]   = sys_mouse_get_impl,
    [SYSCALL_MOUSE_SHOW]  = sys_mouse_show_impl,
    [SYSCALL_ENUMFILES]  = sys_enumfiles_impl,
    [SYSCALL_GFX_INFO]    = sys_gfx_info_impl,
    [SYSCALL_GFX_CLEAR]   = sys_gfx_clear_impl,
    [SYSCALL_GFX_PUTPX]   = sys_gfx_putpx_impl,
};

uint32_t syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    uint32_t num = eax;

    if (num < (sizeof(sys_table)/sizeof(sys_table[0])) && sys_table[num])
        return sys_table[num](eax, ebx, ecx, edx);

    return sys_unknown_impl(eax, ebx, ecx, edx);
}
