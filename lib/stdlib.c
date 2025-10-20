#include "stdlib.h"

int abs(int x) {
    return (x < 0) ? -x : x;
}

char *itoa(int value, char *str, int base) {
    char *ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;

    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        *ptr++ = '-';
        ptr1++;
        value = -value;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}

int atoi(const char *str) {
    int res = 0;
    int sign = 1;
    int i = 0;

    while (str[i] == ' ' || str[i] == '\t')
        i++;

    if (str[i] == '-') {
        sign = -1;
        i++;
    } 
    else if (str[i] == '+') {
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }

    return sign * res;
}
