#include "keyboard.h"
#include "isr.h"
#include "irq.h"
#include "io.h"
#include "vga.h"

#define KBD_BUFFER_SIZE 128
#define SCANCODE_SIZE 128
#define KBD_BUFFER_MASK (KBD_BUFFER_SIZE - 1)

static int e0_prefix = 0;

static const unsigned char scancode_map[SCANCODE_SIZE] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  // Ctrl
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  // Left Shift
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,  // Right Shift
    '*', 0, ' ', // Alt + Space
    0, // CapsLock
};

static const unsigned char scancode_map_shift[SCANCODE_SIZE] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,
    '*', 0, ' ',
    0,
};
static int shift = 0;
static uint8_t last_scancode = 0;
static int key_released = 1;
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static inline void kbd_push(char c) {
    int next = (kbd_head + 1) & KBD_BUFFER_MASK;

    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

void kbd_handler(regs_t *r) {
    uint8_t scancode = inb(0x60);

    // Prefix E0, next scancode is extented
    if (scancode == 0xE0) {
        e0_prefix = 1;
        outb(0x20, 0x20);
        return;
    }

    if (scancode & 0x80) {
        // Key released
        uint8_t code = scancode & 0x7F;

        if (e0_prefix) {
            e0_prefix = 0;
        } 
        else {
            if (code == 0x2A || code == 0x36) {
                // Shift up
                shift = 0;
            }
            // Other ignored releases
        }
    } else {
        // Key pressed
        if (e0_prefix) {
            unsigned char kc = 0;

            if (scancode == 0x4B) kc = KEY_LEFT;   // E0 4B
            else if (scancode == 0x4D) kc = KEY_RIGHT; // E0 4D
            else if (scancode == 0x48) kc = KEY_UP;    // E0 48
            else if (scancode == 0x50) kc = KEY_DOWN;  // E0 50

            if (kc) kbd_push((char)kc);
            e0_prefix = 0; 
        } 
        else if (scancode == 0x2A || scancode == 0x36) {
            // Shift down
            shift = 1;
        } else {
            unsigned char c = shift ? scancode_map_shift[scancode]
                                    : scancode_map[scancode];
            if (c) kbd_push(c);
        }
    }

    outb(0x20, 0x20);
}


void kbd_install(void) {
    register_interrupt_handler(1, kbd_handler);
}

int kbd_available(void) {
    return kbd_head != kbd_tail;
}

char kbd_getchar(void) {
    if (kbd_head == kbd_tail) return 0; // Empty

    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;

    return c;
}

void kbd_readline(char *buf, int max_len) {
    int i = 0;

    while (1) {
        while (!kbd_available()) 
            asm volatile("hlt");  // Put the CPU to sleep until next interrupt

        char c = kbd_getchar();

        if (c == '\n') {
            buf[i] = 0;
            vga_putchar('\n');
            return;
        } 
        else if (c == '\b' && i > 0) {
            i--;
            vga_backspace();
        } 
        else if (c >= 32 && i < max_len - 1) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
}

