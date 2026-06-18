/**
 * novaOS libk — String implementation
 */
#include "string.h"
#include <stdint.h>

void* memset(void* dest, int val, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    while (n--) *d++ = (uint8_t)val;
    return dest;
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const uint8_t*)a - *(const uint8_t*)b;
}

void itoa(int val, char* buf, int base) {
    char tmp[32];
    int  i = 0, neg = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    if (val < 0 && base == 10) { neg = 1; val = -val; }
    while (val > 0) {
        int r = val % base;
        tmp[i++] = (r < 10) ? '0' + r : 'a' + r - 10;
        val /= base;
    }
    if (neg) tmp[i++] = '-';
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = 0;
}
