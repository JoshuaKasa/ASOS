#include "io.h"

void pit_init(uint32_t hz){
    if (!hz) hz = 100;

    uint32_t div = 1193182u / hz;

    outb(0x43, 0x36); // ch0, lobyte/hibyte, rate generator
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}
