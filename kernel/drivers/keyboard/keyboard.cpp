#include "keyboard.h"


// =============================================================
// Keyboard buffer
// =============================================================

static constexpr int KB_BUFFER_SIZE = 256;

char kb_buf[KB_BUFFER_SIZE];

int kb_head = 0;
int kb_tail = 0;


// =============================================================
// Keyboard state
// =============================================================

static bool shift_held = false;

static bool caps_lock = false;


bool f1_pressed = false;
bool f2_pressed = false;

bool up_pressed = false;
bool down_pressed = false;

bool esc_pressed = false;


// =============================================================
// Unshifted scancode map
// =============================================================

static const char scancode_map[128] = {
    0, 0,

    '1','2','3','4','5','6','7','8','9','0',
    '-','=','\b',

    '\t',

    'q','w','e','r','t','y','u','i','o','p',
    '[',']','\n',

    0,  // Left Ctrl

    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',

    0,  // Left Shift

    '\\',

    'z','x','c','v','b','n','m',
    ',','.','/',

    0,  // Right Shift

    '*',

    0,  // Alt

    ' ',

    0,  // Caps Lock

    0,0,0,0,0,0,0,0,0,0, // F1-F10

    0,0, // Num Lock, Scroll Lock

    0,0,0,'-',0,0,0,'+',0,0,0,0,0,

    0,0,

    0,0
};


// =============================================================
// Shifted scancode map
// =============================================================

static const char scancode_shift[128] = {
    0, 0,

    '!','@','#','$','%','^','&','*','(',')',
    '_','+','\b',

    '\t',

    'Q','W','E','R','T','Y','U','I','O','P',
    '{','}','\n',

    0,  // Left Ctrl

    'A','S','D','F','G','H','J','K','L',
    ':','"','~',

    0,  // Left Shift

    '|',

    'Z','X','C','V','B','N','M',
    '<','>','?',

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


// =============================================================
// Scancodes
// =============================================================

#define SC_LEFT_SHIFT  0x2A
#define SC_RIGHT_SHIFT 0x36

#define SC_CAPS_LOCK   0x3A

#define SC_F1          0x3B
#define SC_F2          0x3C

#define SC_UP          0x48
#define SC_DOWN        0x50

#define SC_ESC         0x01

#define SC_KEY_RELEASE 0x80


// =============================================================
// Port I/O
// =============================================================

static uint8_t inb(
    uint16_t port
) {
    uint8_t value;

    asm volatile(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}


// =============================================================
// Buffer helpers
// =============================================================

static bool buffer_empty() {
    return
        kb_head ==
        kb_tail;
}


static bool buffer_full() {
    int next =
        (kb_head + 1) %
        KB_BUFFER_SIZE;

    return
        next ==
        kb_tail;
}


static void push_char(
    char c
) {
    if (!c)
        return;


    // If the buffer is full, discard the newest
    // character rather than corrupting unread data.
    if (buffer_full())
        return;


    kb_buf[kb_head] =
        c;


    kb_head =
        (kb_head + 1) %
        KB_BUFFER_SIZE;
}


// =============================================================
// Initialization
// =============================================================

void Keyboard::init() {
    kb_head = 0;
    kb_tail = 0;

    shift_held = false;
    caps_lock = false;

    f1_pressed = false;
    f2_pressed = false;

    up_pressed = false;
    down_pressed = false;

    esc_pressed = false;
}


// =============================================================
// IRQ handler
// =============================================================

void Keyboard::handle_irq() {
    uint8_t scancode =
        inb(0x60);


    // ---------------------------------------------------------
    // Key release
    // ---------------------------------------------------------

    if (
        scancode &
        SC_KEY_RELEASE
    ) {
        uint8_t released =
            scancode &
            ~SC_KEY_RELEASE;


        if (
            released == SC_LEFT_SHIFT ||
            released == SC_RIGHT_SHIFT
        ) {
            shift_held =
                false;
        }


        return;
    }


    // ---------------------------------------------------------
    // Shift
    // ---------------------------------------------------------

    if (
        scancode == SC_LEFT_SHIFT ||
        scancode == SC_RIGHT_SHIFT
    ) {
        shift_held =
            true;

        return;
    }


    // ---------------------------------------------------------
    // Special keys
    // ---------------------------------------------------------

    if (scancode == SC_F1) {
        f1_pressed = true;
        return;
    }


    if (scancode == SC_F2) {
        f2_pressed = true;
        return;
    }


    if (scancode == SC_UP) {
        up_pressed = true;
        return;
    }


    if (scancode == SC_DOWN) {
        down_pressed = true;
        return;
    }


    if (scancode == SC_ESC) {
        esc_pressed = true;
        return;
    }


    // ---------------------------------------------------------
    // Caps Lock
    // ---------------------------------------------------------

    if (
        scancode ==
        SC_CAPS_LOCK
    ) {
        caps_lock =
            !caps_lock;

        return;
    }


    if (scancode >= 128)
        return;


    // ---------------------------------------------------------
    // Convert scancode to character
    // ---------------------------------------------------------

    char c = 0;


    if (shift_held) {
        c =
            scancode_shift[
                scancode
            ];
    }

    else {
        c =
            scancode_map[
                scancode
            ];
    }


    // ---------------------------------------------------------
    // Caps Lock
    // ---------------------------------------------------------
    //
    // Caps Lock affects letters only.
    //
    // Shift + Caps Lock produces lowercase,
    // like a normal keyboard.
    // ---------------------------------------------------------

    if (
        c >= 'a' &&
        c <= 'z'
    ) {
        if (caps_lock) {
            c =
                c - 'a' + 'A';
        }
    }

    else if (
        c >= 'A' &&
        c <= 'Z'
    ) {
        if (caps_lock) {
            c =
                c - 'A' + 'a';
        }
    }


    // ---------------------------------------------------------
    // Add printable/control character to queue
    // ---------------------------------------------------------

    if (c) {
        push_char(c);
    }
}


// =============================================================
// Character available
// =============================================================

bool Keyboard::has_char() {
    return
        !buffer_empty();
}


// =============================================================
// Non-blocking character read
// =============================================================

bool Keyboard::try_getchar(
    char* out_char
) {
    if (!out_char)
        return false;


    if (buffer_empty())
        return false;


    *out_char =
        kb_buf[kb_tail];


    kb_tail =
        (kb_tail + 1) %
        KB_BUFFER_SIZE;


    return true;
}


// =============================================================
// Blocking character read
// =============================================================

char Keyboard::getchar() {
    while (buffer_empty()) {
        asm volatile(
            "hlt"
        );
    }


    char c =
        kb_buf[kb_tail];


    kb_tail =
        (kb_tail + 1) %
        KB_BUFFER_SIZE;


    return c;
}


// =============================================================
// Legacy key query
// =============================================================

bool Keyboard::key_pressed(
    uint8_t scancode
) {
    (void)scancode;

    return false;
}