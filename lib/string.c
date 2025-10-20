#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) {
	unsigned char *d = dest;
	const unsigned char *s = src;


	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
	
	return dest;
}

void *memset(void *s, int c, size_t n) {
	unsigned char *p = s;

	for (size_t i = 0; i < n; i++) {
		p[i] = (unsigned char)c;
	}

	return s;
}

size_t strlen(const char *str) {
	size_t len = 0;

	while (str[len]) {
		len++;
	}

	return len;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}

	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}


char *strcpy(char *dest, const char *src) {
    char *d = dest;

    while ((*d++ = *src++));

    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;

    while (*d)
        d++;
    while ((*d++ = *src++));

    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0; 
    }
    return 0;
}
