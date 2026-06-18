/**
 * novaOS — System Call Interface
 * Userspace requests kernel services via int 0x80.
 * TODO: Migrate to SYSCALL/SYSRET for x86-64 performance.
 */
#pragma once
#include <stdint.h>

enum class SyscallNum : uint32_t {
    EXIT    = 0,
    WRITE   = 1,
    READ    = 2,
    OPEN    = 3,
    CLOSE   = 4,
    MMAP    = 5,
    MUNMAP  = 6,
    FORK    = 7,
    EXEC    = 8,
    WAITPID = 9,
    GETPID  = 10,
    YIELD   = 11,
    // IPC
    SEND    = 20,
    RECV    = 21,
    // Display (temp — will move to compositor IPC)
    FB_INFO = 30,
    FB_BLIT = 31,
};

namespace Syscall {
    void init();
    void dispatch(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);
}
