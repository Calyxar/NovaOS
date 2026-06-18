#include "mouse.h"

static Mouse::State state = { 400, 300, false, false, false };
static int32_t max_x = 800;
static int32_t max_y = 600;
static uint8_t mouse_cycle = 0;
static int8_t  mouse_buf[3];

static uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) if (inb(0x64) & 1) return;
    } else {
        while (timeout--) if (!(inb(0x64) & 2)) return;
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

static uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void Mouse::init() {
    state.x = max_x / 2;
    state.y = max_y / 2;
    state.left = state.right = state.middle = false;
    mouse_cycle = 0;

    // Enable auxiliary mouse device
    mouse_wait(1); outb(0x64, 0xA8);

    // Enable interrupts
    mouse_wait(1); outb(0x64, 0x20);
    mouse_wait(0);
    uint8_t status = inb(0x60) | 2;
    mouse_wait(1); outb(0x64, 0x60);
    mouse_wait(1); outb(0x60, status);

    // Set defaults
    mouse_write(0xF6);
    mouse_read();

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();
}

void Mouse::handle_irq() {
    uint8_t data = inb(0x60);
    mouse_buf[mouse_cycle++] = (int8_t)data;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        // Byte 0: buttons + flags
        uint8_t flags = (uint8_t)mouse_buf[0];
        state.left   = flags & 0x01;
        state.right  = flags & 0x02;
        state.middle = flags & 0x04;

        // Bytes 1,2: X,Y delta (signed)
        int8_t dx = mouse_buf[1];
        int8_t dy = mouse_buf[2];

        // Y is inverted in PS/2
        state.x += dx;
        state.y -= dy;

        // Clamp to screen bounds
        if (state.x < 0)     state.x = 0;
        if (state.y < 0)     state.y = 0;
        if (state.x >= max_x) state.x = max_x - 1;
        if (state.y >= max_y) state.y = max_y - 1;
    }
}


void Mouse::poll() {
    uint8_t status = inb(0x64);
    if ((status & 0x21) == 0x21) {
        handle_irq();
    }
}
Mouse::State& Mouse::get_state() { return state; }

void Mouse::set_bounds(int32_t mx, int32_t my) {
    max_x = mx; max_y = my;
}
