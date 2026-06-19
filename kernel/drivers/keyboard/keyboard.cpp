#include "keyboard.h"

char kb_buf[256];
int  kb_head = 0;
int  kb_tail = 0;

static bool shift_held   = false;
bool f1_pressed = false;
static bool caps_lock    = false;

// Unshifted scancode map
static const char scancode_map[128] = {
    0, 0,
    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  // Left Ctrl
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  // Left Shift
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,  // Right Shift
    '*',
    0,  // Alt
    ' ',
    0,  // Caps Lock
    0,0,0,0,0,0,0,0,0,0, // F1-F10
    0,0,  // Num Lock, Scroll Lock
    0,0,0,'-',0,0,0,'+',0,0,0,0,0, // Numpad
    0,0,  // undefined
    0,0   // F11, F12
};

// Shifted scancode map
static const char scancode_shift[128] = {
    0, 0,
    '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  // Left Ctrl
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  // Left Shift
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,  // Right Shift
    '*',
    0,  // Alt
    ' ',
    0,  // Caps Lock
    0,0,0,0,0,0,0,0,0,0,
    0,0,
    0,0,0,'-',0,0,0,'+',0,0,0,0,0,
    0,0,
    0,0
};

#define SC_LEFT_SHIFT  0x2A
#define SC_RIGHT_SHIFT 0x36
#define SC_CAPS_LOCK   0x3A
#define SC_F1          0x3B
#define SC_KEY_RELEASE 0x80

static uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void Keyboard::init() {
    kb_head = kb_tail = 0;
    shift_held = false;
    caps_lock  = false;
}

void Keyboard::handle_irq() {
    uint8_t scancode = inb(0x60);

    // Key release
    if (scancode & SC_KEY_RELEASE) {
        uint8_t released = scancode & ~SC_KEY_RELEASE;
        if (released == SC_LEFT_SHIFT || released == SC_RIGHT_SHIFT)
            shift_held = false;
        return;
    }

    // Shift pressed
    if (scancode == SC_LEFT_SHIFT || scancode == SC_RIGHT_SHIFT) {
        shift_held = true;
        return;
    }

    // F1 = simulate click
    if (scancode == SC_F1) { f1_pressed = true; return; }

    // Caps Lock toggle
    if (scancode == SC_CAPS_LOCK) {
        caps_lock = !caps_lock;
        return;
    }

    if (scancode >= 128) return;

    char c = 0;

    if (shift_held) {
        c = scancode_shift[scancode];
    } else {
        c = scancode_map[scancode];
        // Apply caps lock to letters only
        if (caps_lock && c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
    }

    if (c) {
        kb_buf[kb_head] = c;
        kb_head = (kb_head + 1) % 256;
    }
}

char Keyboard::getchar() {
    while (kb_head == kb_tail) { asm volatile("hlt"); }
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % 256;
    return c;
}

bool Keyboard::key_pressed(uint8_t scancode) { (void)scancode; return false; }
