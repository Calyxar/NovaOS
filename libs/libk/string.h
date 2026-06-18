/**
 * novaOS libk — Kernel String & Memory Library
 * The kernel cannot use the standard C library.
 * We implement only what we need, from scratch.
 */
#pragma once
#include <stddef.h>

// Memory
void*  memset (void* dest, int val, size_t n);
void*  memcpy (void* dest, const void* src, size_t n);
void*  memmove(void* dest, const void* src, size_t n);
int    memcmp (const void* a, const void* b, size_t n);

// String
size_t strlen (const char* s);
char*  strcpy (char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
int    strcmp (const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strcat (char* dest, const char* src);

// Conversion
void   itoa(int val, char* buf, int base);
int    atoi(const char* s);
