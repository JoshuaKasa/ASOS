#include "asoapi.h"
#include "../lib/string.h"

void main(void) {
    char buf[64];
    int pos = 0;

    while (1) {
        sys_write("$ASOS>> ");

        pos = 0;
        while (1) {
            char c = sys_getchar();

            if (c == '\n') {
                buf[pos] = 0;
                sys_write("\n");
                break;
            } else if (c == '\b' && pos > 0) {
                pos--;
                sys_write("\b");
            } else if (c >= 32 && pos < 63) {
                buf[pos++] = c;
                char tmp[2] = {c, 0};
                sys_write(tmp);
            }
        }

        if (buf[0] == 0) continue;

        if (!strcmp(buf, "help")) {
            sys_write("Commands: help, clear, run <app>, exit\n");
        }
        else if (!strcmp(buf, "clear")) {
            sys_clear();
        }
        else if (!strcmp(buf, "exit")) {
            sys_mouse_show(0);
            sys_exit();
        }
        else if (!strncmp(buf, "run ", 4)) {
            sys_mouse_show(0);
            sys_write("Launching...\n");
            sys_exec(buf + 4);
            sys_mouse_show(0);
        }
        else if (!strcmp(buf, "files")) {
            sys_listfiles();
        }
        else {
            sys_write("Unknown command.\n");
        }
    }
}
