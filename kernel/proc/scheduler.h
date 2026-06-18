/**
 * novaOS — Process Scheduler
 * Round-robin scheduler with 4 priority levels.
 * Each process gets its own virtual address space.
 */
#pragma once
#include <stdint.h>

enum class ProcessState { READY, RUNNING, BLOCKED, ZOMBIE };
enum class Priority     { LOW = 0, NORMAL = 1, HIGH = 2, REALTIME = 3 };

struct CPUState {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, esp, ebp;
    uint32_t eip, eflags;
    uint32_t cs, ds, ss;
};

struct Process {
    uint32_t     pid;
    char         name[64];
    ProcessState state;
    Priority     priority;
    CPUState     cpu;
    uint32_t*    page_dir;
    uint8_t*     stack;
    Process*     next;
};

namespace Scheduler {
    void     init();
    void     start();
    Process* spawn(const char* name, void (*entry)(),
                   Priority p = Priority::NORMAL);
    void     yield();
    void     exit(int code);
    void     block(Process* proc);
    void     unblock(Process* proc);
    Process* current();
    uint32_t next_pid();
}
