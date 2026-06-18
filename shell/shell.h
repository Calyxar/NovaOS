/**
 * NovaOS Shell
 * Basic command interpreter — first userspace program.
 */
#pragma once

namespace Shell {
    void init();
    void run();
    void print(const char* str);
    void println(const char* str);
}
