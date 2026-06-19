#include "mouse.h"

static Mouse::State state = { 400, 300, false, false, false };
static int32_t max_x = 800;
static int32_t max_y = 600;
static uint8_t packet[3];
static uint8_t packet_idx = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static void wait_in()  { uint32_t t=100000; while(t-- && !(inb(0x64)&1)); }
static void wait_out() { uint32_t t=100000; while(t-- && (inb(0x64)&2)); }

static void mwrite(uint8_t d) { wait_out(); outb(0x64,0xD4); wait_out(); outb(0x60,d); }
static uint8_t mread() { wait_in(); return inb(0x60); }

void Mouse::init() {
    state.x = max_x/2; state.y = max_y/2;
    packet_idx = 0;

    wait_out(); outb(0x64, 0xA8);          // enable aux device
    wait_out(); outb(0x64, 0x20);          // read config byte
    uint8_t cfg = mread();
    cfg |= 0x02;                            // enable IRQ12
    cfg &= ~0x20;                           // enable mouse clock
    wait_out(); outb(0x64, 0x60);
    wait_out(); outb(0x60, cfg);

    mwrite(0xFF); mread(); mread(); mread(); // reset, ack, id, id
    mwrite(0xF6); mread();                   // set defaults
    mwrite(0xF4); mread();                   // enable streaming
}

// Each call processes exactly one raw byte from the controller.
// Resyncs aggressively: byte0 must have bit3=1 and bits6-7=0.
void Mouse::handle_irq() {
    uint8_t data = inb(0x60);

    if (packet_idx == 0) {
        if ((data & 0x08) == 0 || (data & 0xC0) != 0) {
            return; // not a valid start byte — drop, stay synced at 0
        }
    }

    packet[packet_idx++] = data;
    if (packet_idx < 3) return;
    packet_idx = 0;

    uint8_t flags = packet[0];
    state.left   = (flags & 0x01) != 0;
    state.right  = (flags & 0x02) != 0;
    state.middle = (flags & 0x04) != 0;

    int dx = packet[1];
    int dy = packet[2];
    if (flags & 0x10) dx -= 256;
    if (flags & 0x20) dy -= 256;

    // Reject implausibly large single-step jumps (likely desync garbage)
    if (dx > -120 && dx < 120 && dy > -120 && dy < 120) {
        state.x += dx;
        state.y -= dy;
        if (state.x < 0) state.x = 0;
        if (state.y < 0) state.y = 0;
        if (state.x >= max_x) state.x = max_x - 1;
        if (state.y >= max_y) state.y = max_y - 1;
    }
}

void Mouse::poll() {
    for (int guard = 0; guard < 64; guard++) {
        uint8_t status = inb(0x64);
        if (!(status & 0x01)) break;
        if (status & 0x20) {
            handle_irq();
        } else {
            inb(0x60); // discard non-mouse byte
        }
    }
}

Mouse::State& Mouse::get_state() { return state; }
void Mouse::set_bounds(int32_t mx, int32_t my) { max_x = mx; max_y = my; }
