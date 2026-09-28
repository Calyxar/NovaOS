#include "kernel.h"
#include "terminal/terminal.h"

volatile unsigned short* const VGA = (unsigned short*)0xB8000;

static int cursor = 0;

void print(const char* str)
{
    while (*str)
    {
        VGA[cursor++] = (0x0F << 8) | *str++;
    }
}

void clear_screen()
{
    for (int i = 0; i < 80 * 25; i++)
        VGA[i] = 0x0720;

    cursor = 0;
}

void kernel_main()
{
    volatile char* video = (char*)0xB8000;

    video[0] = 'N';
    video[2] = 'O';
    video[4] = 'V';
    video[6] = 'A';
    video[8] = 'O';
    video[10] = 'S';

    while(1)
    {
        __asm__("hlt");
    }
}