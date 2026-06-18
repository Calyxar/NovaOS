/**
 * NovaOS — Kernel Panic Handler
 * Called when the kernel hits an unrecoverable error.
 * Displays a styled crash screen and halts the CPU.
 */
#pragma once
#include <stdint.h>

// Call this anywhere in the kernel to trigger a panic
#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)

extern "C" void kernel_panic(const char* message,
                              const char* file,
                              uint32_t    line);
