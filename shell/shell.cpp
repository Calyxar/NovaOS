#include "shell.h"
#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/fs/ramfs.h"
#include "../kernel/proc/scheduler.h"
#include "../kernel/panic.h"

static char input_buf[256];
static int  input_len = 0;

#define CLR_WHITE  0xFFFFFF
#define CLR_GREEN  0x00C2A8
#define CLR_BLUE   0x6C63FF
#define CLR_RED    0xFF6B6B
#define CLR_YELLOW 0xFFFF00
#define CLR_GRAY   0x888888
#define CLR_CYAN   0x00FFFF

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}

static bool startswith(const char* str, const char* prefix) {
    while (*prefix) { if (*str++ != *prefix++) return false; }
    return true;
}

void Shell::print(const char* str)   { Framebuffer::print(str, CLR_WHITE); }
void Shell::println(const char* str) { Framebuffer::print(str, CLR_WHITE); Framebuffer::print("\n", CLR_WHITE); }
static void p(const char* s, uint32_t c) { Framebuffer::print(s, c); }

static void print_num(uint32_t n) {
    if (n == 0) { p("0", CLR_WHITE); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    char out[12];
    for (int j = 0; j < i; j++) out[j] = buf[i-1-j];
    out[i] = '\0';
    p(out, CLR_WHITE);
}

extern uint32_t pit_ticks;

static void print_header() {
    Framebuffer::clear(0);

    // Top status bar
    p("  NovaOS", CLR_CYAN);
    p(" v0.1.0  |  ", CLR_BLUE);
    p("Shell: Nova", CLR_GREEN);
    p("  |  ", CLR_BLUE);
    p("FS: NovaFS", CLR_GREEN);
    p("  |  ", CLR_BLUE);
    p("Procs: ON", CLR_GREEN);
    p("  |  ", CLR_BLUE);
    p("GPU: NVIDIA\n", CLR_GREEN);

    // Divider
    p("----------------------------------------------------------\n", CLR_BLUE);
    p("\n", CLR_WHITE);

    // Welcome
    p("  Welcome to ", CLR_GRAY);
    p("NovaOS", CLR_CYAN);
    p(" — Next-Gen OS for Future Computers\n", CLR_WHITE);
    p("  Type ", CLR_GRAY);
    p("'help'", CLR_GREEN);
    p(" for commands  |  ", CLR_GRAY);
    p("'run hello'", CLR_YELLOW);
    p(" to launch a process\n\n", CLR_GRAY);
}

static void ls_callback(const char* name, uint32_t size) {
    p("  ", CLR_WHITE); p(name, CLR_CYAN);
    p("  (", CLR_GRAY); print_num(size); p(" bytes)\n", CLR_GRAY);
}

static void proc_hello() {
    p("[hello] Hello from a NovaOS process!\n", CLR_GREEN);
    p("[hello] Running independently from the shell.\n", CLR_GREEN);
    Scheduler::exit(0);
}

static void proc_counter() {
    p("[counter] Counting: ", CLR_YELLOW);
    for (int i = 1; i <= 10; i++) {
        char s[4] = {'0' + (char)(i % 10), ' ', '\0'};
        if (i == 10) { s[0] = '1'; s[1] = '0'; s[2] = ' '; s[3] = '\0'; }
        p(s, CLR_YELLOW);
    }
    p("\n[counter] Process complete.\n", CLR_YELLOW);
    Scheduler::exit(0);
}

static void proc_sysinfo() {
    p("[sysinfo] NovaOS v0.1.0\n", CLR_CYAN);
    p("[sysinfo] Arch:   x86 i686 32-bit\n", CLR_CYAN);
    p("[sysinfo] Kernel: C/C++/ASM\n", CLR_CYAN);
    p("[sysinfo] FS:     NovaFS RAM\n", CLR_CYAN);
    p("[sysinfo] Shell:  Nova Shell\n", CLR_CYAN);
    Scheduler::exit(0);
}

static void handle_command(const char* cmd) {
    if (streq(cmd, "help")) {
        p("\n  ", CLR_WHITE);
        p("NovaOS Shell Commands\n", CLR_CYAN);
        p("  --------------------\n", CLR_BLUE);
        p("  help            ", CLR_GREEN); p("This help screen\n", CLR_WHITE);
        p("  version         ", CLR_GREEN); p("OS version\n", CLR_WHITE);
        p("  about           ", CLR_GREEN); p("About NovaOS\n", CLR_WHITE);
        p("  clear           ", CLR_GREEN); p("Clear screen\n", CLR_WHITE);
        p("  cpu             ", CLR_GREEN); p("CPU info\n", CLR_WHITE);
        p("  mem             ", CLR_GREEN); p("Memory info\n", CLR_WHITE);
        p("  uptime          ", CLR_GREEN); p("System uptime\n", CLR_WHITE);
        p("  echo <text>     ", CLR_GREEN); p("Print text\n", CLR_WHITE);
        p("  ls              ", CLR_GREEN); p("List files\n", CLR_WHITE);
        p("  cat <file>      ", CLR_GREEN); p("Read file\n", CLR_WHITE);
        p("  touch <file>    ", CLR_GREEN); p("Create file\n", CLR_WHITE);
        p("  write <f> <txt> ", CLR_GREEN); p("Write to file\n", CLR_WHITE);
        p("  rm <file>       ", CLR_GREEN); p("Delete file\n", CLR_WHITE);
        p("  ps              ", CLR_GREEN); p("List processes\n", CLR_WHITE);
        p("  run <program>   ", CLR_GREEN); p("Run: hello counter sysinfo\n\n", CLR_WHITE);
    }
    else if (streq(cmd, "version")) {
        p("\n  ", CLR_WHITE);
        p("NovaOS ", CLR_CYAN); p("v0.1.0\n", CLR_YELLOW);
        p("  Built from scratch — C/C++/ASM\n", CLR_GRAY);
        p("  Bootloader + Kernel + Shell + FS + Procs\n\n", CLR_GRAY);
    }
    else if (streq(cmd, "about")) {
        p("\n  ", CLR_WHITE);
        p("NovaOS — Next-Generation Operating System\n", CLR_CYAN);
        p("  -----------------------------------------\n", CLR_BLUE);
        p("  Vision:     ", CLR_GRAY); p("macOS polish + gaming power\n", CLR_WHITE);
        p("  Kernel:     ", CLR_GRAY); p("C / C++ / Assembly\n", CLR_GREEN);
        p("  Filesystem: ", CLR_GRAY); p("NovaFS (RAM, persistent planned)\n", CLR_GREEN);
        p("  Processes:  ", CLR_GRAY); p("Cooperative (preemptive planned)\n", CLR_GREEN);
        p("  GPU Target: ", CLR_GRAY); p("NVIDIA open kernel module\n", CLR_GREEN);
        p("  Compositor: ", CLR_GRAY); p("Nova Compositor (Wayland, planned)\n\n", CLR_GREEN);
    }
    else if (streq(cmd, "clear")) { print_header(); }
    else if (streq(cmd, "mem")) {
        p("\n  Memory\n", CLR_CYAN);
        p("  Kernel base: ", CLR_GRAY); p("0x100000\n", CLR_YELLOW);
        p("  VGA buffer:  ", CLR_GRAY); p("0xB8000\n", CLR_YELLOW);
        p("  Available:   ", CLR_GRAY); p("~30MB\n\n", CLR_YELLOW);
    }
    else if (streq(cmd, "cpu")) {
        p("\n  CPU\n", CLR_CYAN);
        p("  Architecture: ", CLR_GRAY); p("x86 (i686)\n", CLR_WHITE);
        p("  Mode:         ", CLR_GRAY); p("32-bit Protected Mode\n", CLR_WHITE);
        p("  Ring:         ", CLR_GRAY); p("Ring 0 (kernel)\n", CLR_WHITE);
        p("  Roadmap:      ", CLR_GRAY); p("x86-64 + ARM64 + NVIDIA\n\n", CLR_GREEN);
    }
    else if (streq(cmd, "uptime")) {
        uint32_t secs  = pit_ticks / 1000u;
        uint32_t mins  = secs / 60u;
        uint32_t hours = mins / 60u;
        secs = secs % 60u;
        mins = mins % 60u;
        p("\n  Uptime\n", CLR_CYAN);
        p("  Hours:   ", CLR_GRAY); print_num(hours); p("\n", CLR_WHITE);
        p("  Minutes: ", CLR_GRAY); print_num(mins);  p("\n", CLR_WHITE);
        p("  Seconds: ", CLR_GRAY); print_num(secs);  p("\n", CLR_WHITE);
        p("  Ticks:   ", CLR_GRAY); print_num(pit_ticks); p("\n\n", CLR_WHITE);
    }
    else if (streq(cmd, "NOPE_SKIP")) {
        p("\n  Uptime\n", CLR_CYAN);
        p("  Ticks:   ", CLR_GRAY); print_num(pit_ticks); p("\n", CLR_WHITE);
        p("  Seconds: ", CLR_GRAY); print_num(pit_ticks / 1000u); p("\n\n", CLR_WHITE);
    }
    else if (startswith(cmd, "echo ")) {
        p("\n  ", CLR_WHITE); p(cmd + 5, CLR_CYAN); p("\n\n", CLR_WHITE);
    }
    else if (streq(cmd, "ls")) {
        p("\n  NovaFS Files\n", CLR_CYAN);
        p("  ------------\n", CLR_BLUE);
        RamFS::list(ls_callback);
        p("  Total: ", CLR_GRAY); print_num(RamFS::file_count()); p(" file(s)\n\n", CLR_GRAY);
    }
    else if (startswith(cmd, "cat ")) {
        RamFile* f = RamFS::find(cmd + 4);
        if (f) { p("\n  ", CLR_WHITE); p(f->data, CLR_WHITE); p("\n\n", CLR_WHITE); }
        else   { p("  Not found: ", CLR_RED); p(cmd+4, CLR_WHITE); p("\n\n", CLR_WHITE); }
    }
    else if (startswith(cmd, "touch ")) {
        if (RamFS::create(cmd+6)) { p("  Created: ", CLR_GREEN); p(cmd+6, CLR_WHITE); p("\n\n", CLR_WHITE); }
        else { p("  Error: file exists or filesystem full\n\n", CLR_RED); }
    }
    else if (startswith(cmd, "write ")) {
        const char* rest = cmd + 6;
        int i = 0;
        while (rest[i] && rest[i] != ' ') i++;
        if (rest[i] == ' ') {
            char fname[32];
            for (int j = 0; j < i && j < 31; j++) fname[j] = rest[j];
            fname[i] = '\0';
            const char* content = rest + i + 1;
            uint32_t len = 0; while (content[len]) len++;
            if (RamFS::write(fname, content, len)) {
                p("  Written to: ", CLR_GREEN); p(fname, CLR_WHITE); p("\n\n", CLR_WHITE);
            } else { p("  File not found. Use touch first.\n\n", CLR_RED); }
        } else { p("  Usage: write <file> <content>\n\n", CLR_YELLOW); }
    }
    else if (startswith(cmd, "rm ")) {
        if (RamFS::remove(cmd+3)) { p("  Deleted: ", CLR_RED); p(cmd+3, CLR_WHITE); p("\n\n", CLR_WHITE); }
        else { p("  Not found\n\n", CLR_RED); }
    }
    else if (streq(cmd, "ps")) {
        p("\n  Processes\n", CLR_CYAN);
        p("  PID  NAME          STATE\n", CLR_BLUE);
        p("  1    nova-shell    ", CLR_WHITE); p("RUNNING\n", CLR_GREEN);
        p("\n  Next PID: ", CLR_GRAY); print_num(Scheduler::next_pid());
        p("\n\n", CLR_WHITE);
    }
    else if (startswith(cmd, "run ")) {
        const char* prog = cmd + 4;
        if (streq(prog, "hello")) {
            p("\n  ", CLR_WHITE);
            Scheduler::spawn("hello", proc_hello);
            proc_hello();
        }
        else if (streq(prog, "counter")) {
            p("\n  ", CLR_WHITE);
            Scheduler::spawn("counter", proc_counter);
            proc_counter();
            p("\n", CLR_WHITE);
        }
        else if (streq(prog, "sysinfo")) {
            p("\n  ", CLR_WHITE);
            Scheduler::spawn("sysinfo", proc_sysinfo);
            proc_sysinfo();
            p("\n", CLR_WHITE);
        }
        else {
            p("  Unknown: ", CLR_RED); p(prog, CLR_WHITE);
            p("\n  Programs: hello, counter, sysinfo\n\n", CLR_GRAY);
        }
    }
    else if (streq(cmd, "panic")) {
        p("  Triggering kernel panic...\n", CLR_RED);
        PANIC("Manual panic triggered from shell");
    }
    else if (cmd[0] == '\0') {}
    else {
        p("  Unknown command: ", CLR_RED); Shell::print(cmd);
        p("\n  Type ", CLR_GRAY); p("'help'", CLR_GREEN); p(" for commands.\n", CLR_GRAY);
    }
}

void Shell::init() {
    input_len = 0;
    RamFS::init();
    Scheduler::init();
    print_header();
}

void Shell::run() {
    while (true) {
        p("> ", CLR_GREEN);
        input_len = 0;
        while (true) {
            char c = Keyboard::getchar();
            if (c == '\n') {
                input_buf[input_len] = '\0';
                Shell::print("\n");
                handle_command(input_buf);
                break;
            } else if (c == '\b') {
                if (input_len > 0) { input_len--; p("\b \b", CLR_WHITE); }
            } else if (input_len < 255) {
                input_buf[input_len++] = c;
                char s[2] = {c, '\0'};
                p(s, CLR_WHITE);
            }
        }
    }
}
