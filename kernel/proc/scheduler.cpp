#include "scheduler.h"
#include "../mm/pmm.h"
#include "../drivers/video/framebuffer.h"

#define MAX_PROCS  16
#define STACK_SIZE 4096

static Process  procs[MAX_PROCS];
static int      current_pid = 0;
static int      proc_count  = 0;
static uint32_t pid_counter = 1;

static void memset_local(void* ptr, uint8_t val, uint32_t size) {
    uint8_t* p = (uint8_t*)ptr;
    while (size--) *p++ = val;
}

void Scheduler::init() {
    for (int i = 0; i < MAX_PROCS; i++) {
        procs[i].state = ProcessState::ZOMBIE;
        procs[i].pid   = 0;
    }
    proc_count = 0;
}

Process* Scheduler::spawn(const char* name, void (*entry)(), Priority p) {
    if (proc_count >= MAX_PROCS) return nullptr;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].state == ProcessState::ZOMBIE) { slot = i; break; }
    }
    if (slot == -1) return nullptr;

    Process* proc = &procs[slot];

    // Copy name
    int i = 0;
    while (name[i] && i < 63) { proc->name[i] = name[i]; i++; }
    proc->name[i] = '\0';

    // Allocate stack
    proc->stack = (uint8_t*)PMM::alloc_page();
    if (!proc->stack) return nullptr;
    memset_local(proc->stack, 0, STACK_SIZE);

    // Set up CPU state — stack grows down
    uint32_t stack_top = (uint32_t)proc->stack + STACK_SIZE;

    proc->cpu.eip    = (uint32_t)entry;
    proc->cpu.esp    = stack_top;
    proc->cpu.ebp    = stack_top;
    proc->cpu.eflags = 0x202; // Interrupts enabled
    proc->cpu.eax = proc->cpu.ebx = proc->cpu.ecx = proc->cpu.edx = 0;
    proc->cpu.esi = proc->cpu.edi = 0;
    proc->cpu.cs = 0x08; // Kernel code segment
    proc->cpu.ds = proc->cpu.ss = 0x10; // Kernel data segment

    proc->pid      = pid_counter++;
    proc->state    = ProcessState::READY;
    proc->priority = p;
    proc->page_dir = nullptr;

    proc_count++;
    return proc;
}

void Scheduler::start() {
    // Simple cooperative: just run shell (proc 0) for now
    // Real preemptive switching happens via PIT IRQ (future)
    for(;;) asm volatile("hlt");
}

Process* Scheduler::current() {
    if (proc_count == 0) return nullptr;
    return &procs[current_pid];
}

uint32_t Scheduler::next_pid() { return pid_counter; }

void Scheduler::yield() {
    // Round-robin: find next READY process
    int next = (current_pid + 1) % MAX_PROCS;
    for (int i = 0; i < MAX_PROCS; i++) {
        int idx = (current_pid + 1 + i) % MAX_PROCS;
        if (procs[idx].state == ProcessState::READY) {
            current_pid = idx;
            return;
        }
    }
}

void Scheduler::exit(int code) {
    (void)code;
    if (proc_count > 0) {
        procs[current_pid].state = ProcessState::ZOMBIE;
        proc_count--;
    }
    yield();
}

void Scheduler::block(Process* proc) {
    if (proc) proc->state = ProcessState::BLOCKED;
}

void Scheduler::unblock(Process* proc) {
    if (proc) proc->state = ProcessState::READY;
}
