#include "keyboard.h"

// =============================================================
// Keyboard buffer
// =============================================================

static constexpr int KB_BUFFER_SIZE = 256;

// Keep these global for compatibility with shell.cpp.
char kb_buf[KB_BUFFER_SIZE];

int kb_head = 0;
int kb_tail = 0;

// =============================================================
// Keyboard state
// =============================================================

static bool left_shift_held = false;
static bool right_shift_held = false;
static bool caps_lock = false;

static bool left_ctrl_held = false;
static bool right_ctrl_held = false;

// Extended scancode prefix (0xE0).
static bool extended_key = false;

// =============================================================
// Public special-key flags
// =============================================================

bool f1_pressed = false;
bool f2_pressed = false;

bool up_pressed = false;
bool down_pressed = false;
bool esc_pressed = false;

bool left_pressed = false;
bool right_pressed = false;

bool home_pressed = false;
bool end_pressed = false;

bool delete_pressed = false;

bool save_pressed = false;

// =============================================================
// PS/2 Set 1 scancode definitions
// =============================================================

#define SC_ESC          0x01

#define SC_LEFT_CTRL    0x1D

#define SC_LEFT_SHIFT   0x2A
#define SC_RIGHT_SHIFT  0x36

#define SC_CAPS_LOCK    0x3A

#define SC_F1           0x3B
#define SC_F2           0x3C

#define SC_HOME         0x47
#define SC_UP           0x48
#define SC_LEFT         0x4B
#define SC_RIGHT        0x4D
#define SC_END          0x4F
#define SC_DOWN         0x50
#define SC_DELETE       0x53

#define SC_S            0x1F

#define SC_EXTENDED     0xE0
#define SC_RELEASE      0x80

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

    0,

    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',

    0,

    '\\',

    'z','x','c','v','b','n','m',
    ',','.','/',

    0,

    '*',

    0,

    ' ',

    0,

    0,0,0,0,0,0,0,0,0,0,

    0,0,

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

    0,

    'A','S','D','F','G','H','J','K','L',
    ':','"','~',

    0,

    '|',

    'Z','X','C','V','B','N','M',
    '<','>','?',

    0,

    '*',

    0,

    ' ',

    0,

    0,0,0,0,0,0,0,0,0,0,

    0,0,

    0,0,0,'-',0,0,0,'+',0,0,0,0,0,

    0,0,

    0,0
};

// =============================================================
// Port I/O
// =============================================================

static uint8_t inb(uint16_t port) {
    uint8_t value;

    asm volatile(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

// =============================================================
// Keyboard state helpers
// =============================================================

static bool shift_active() {
    return left_shift_held || right_shift_held;
}

static bool ctrl_active() {
    return left_ctrl_held || right_ctrl_held;
}

// =============================================================
// Buffer helpers
// =============================================================

static bool buffer_empty() {
    return kb_head == kb_tail;
}

static bool buffer_full() {
    int next = (kb_head + 1) % KB_BUFFER_SIZE;

    return next == kb_tail;
}

static void push_char(char c) {
    if (!c || buffer_full())
        return;

    kb_buf[kb_head] = c;

    kb_head = (kb_head + 1) % KB_BUFFER_SIZE;
}

// =============================================================
// Initialization
// =============================================================

void Keyboard::init() {
    kb_head = 0;
    kb_tail = 0;

    left_shift_held = false;
    right_shift_held = false;

    left_ctrl_held = false;
    right_ctrl_held = false;

    caps_lock = false;
    extended_key = false;

    f1_pressed = false;
    f2_pressed = false;

    up_pressed = false;
    down_pressed = false;
    esc_pressed = false;

    left_pressed = false;
    right_pressed = false;

    home_pressed = false;
    end_pressed = false;

    delete_pressed = false;
    save_pressed = false;
}

// =============================================================
// IRQ handler
// =============================================================

void Keyboard::handle_irq() {
    uint8_t raw = inb(0x60);

    // ---------------------------------------------------------
    // Extended key prefix
    // ---------------------------------------------------------

    if (raw == SC_EXTENDED) {
        extended_key = true;
        return;
    }

    // ---------------------------------------------------------
    // Decode key press / release
    // ---------------------------------------------------------

    bool released = (raw & SC_RELEASE) != 0;

    uint8_t scancode = raw & 0x7F;

    bool isExtended = extended_key;

    extended_key = false;

    // ---------------------------------------------------------
    // Ctrl state
    // ---------------------------------------------------------

    if (scancode == SC_LEFT_CTRL) {
        if (isExtended) {
            right_ctrl_held = !released;
        } else {
            left_ctrl_held = !released;
        }

        return;
    }

    // ---------------------------------------------------------
    // Shift state
    // ---------------------------------------------------------

    if (!isExtended && scancode == SC_LEFT_SHIFT) {
        left_shift_held = !released;
        return;
    }

    if (!isExtended && scancode == SC_RIGHT_SHIFT) {
        right_shift_held = !released;
        return;
    }

    // Ignore other released keys.
    if (released)
        return;

    // ---------------------------------------------------------
    // Extended navigation keys
    // ---------------------------------------------------------

    if (isExtended) {
        switch (scancode) {
            case SC_UP:
                up_pressed = true;
                return;

            case SC_DOWN:
                down_pressed = true;
                return;

            case SC_LEFT:
                left_pressed = true;
                return;

            case SC_RIGHT:
                right_pressed = true;
                return;

            case SC_HOME:
                home_pressed = true;
                return;

            case SC_END:
                end_pressed = true;
                return;

            case SC_DELETE:
                delete_pressed = true;
                return;

            default:
                return;
        }
    }

    // ---------------------------------------------------------
    // Function keys
    // ---------------------------------------------------------

    if (scancode == SC_F1) {
        f1_pressed = true;
        return;
    }

    if (scancode == SC_F2) {
        f2_pressed = true;
        return;
    }

    // ---------------------------------------------------------
    // Escape
    // ---------------------------------------------------------

    if (scancode == SC_ESC) {
        esc_pressed = true;
        return;
    }

    // ---------------------------------------------------------
    // Legacy navigation compatibility
    // ---------------------------------------------------------

    if (scancode == SC_UP) {
        up_pressed = true;
        return;
    }

    if (scancode == SC_DOWN) {
        down_pressed = true;
        return;
    }

    // ---------------------------------------------------------
    // Ctrl + S
    // ---------------------------------------------------------

    if (ctrl_active()) {
        if (scancode == SC_S) {
            save_pressed = true;
        }

        // Prevent shortcuts from inserting ordinary text.
        return;
    }

    // ---------------------------------------------------------
    // Caps Lock
    // ---------------------------------------------------------

    if (scancode == SC_CAPS_LOCK) {
        caps_lock = !caps_lock;
        return;
    }

    if (scancode >= 128)
        return;

    // ---------------------------------------------------------
    // Character conversion
    // ---------------------------------------------------------

    bool shift = shift_active();

    char c = shift
        ? scancode_shift[scancode]
        : scancode_map[scancode];

    // Shift XOR Caps Lock determines letter capitalization.
    if (c >= 'a' && c <= 'z') {
        if (caps_lock) {
            c = c - 'a' + 'A';
        }
    }
    else if (c >= 'A' && c <= 'Z') {
        if (caps_lock) {
            c = c - 'A' + 'a';
        }
    }

    // ---------------------------------------------------------
    // Queue character
    // ---------------------------------------------------------

    if (c) {
        push_char(c);
    }
}

// =============================================================
// Is a character available?
// =============================================================

bool Keyboard::has_char() {
    return !buffer_empty();
}

// =============================================================
// Non-blocking read
// =============================================================

bool Keyboard::try_getchar(char* out_char) {
    if (!out_char)
        return false;

    if (buffer_empty())
        return false;

    *out_char = kb_buf[kb_tail];

    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;

    return true;
}

// =============================================================
// Blocking read
// =============================================================

char Keyboard::getchar() {
    while (buffer_empty()) {
        asm volatile("hlt");
    }

    char c = kb_buf[kb_tail];

    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;

    return c;
}

// =============================================================
// Legacy interface
// =============================================================

bool Keyboard::key_pressed(uint8_t scancode) {
    (void)scancode;
    return false;
}