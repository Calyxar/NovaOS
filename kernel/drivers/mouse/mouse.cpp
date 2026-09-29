#include "mouse.h"


namespace {

    Mouse::State state = {
        400,
        300,
        false,
        false,
        false
    };

    int32_t max_x = 800;
    int32_t max_y = 600;

    uint8_t packet[3];
    uint8_t packet_idx = 0;


    inline uint8_t inb(uint16_t port) {
        uint8_t value;

        asm volatile(
            "inb %1, %0"
            : "=a"(value)
            : "Nd"(port)
        );

        return value;
    }


    inline void outb(
        uint16_t port,
        uint8_t value
    ) {
        asm volatile(
            "outb %0, %1"
            :
            : "a"(value),
              "Nd"(port)
        );
    }


    void wait_input_ready() {
        uint32_t timeout = 100000;

        while (
            timeout-- &&
            !(inb(0x64) & 0x01)
        ) {
        }
    }


    void wait_output_ready() {
        uint32_t timeout = 100000;

        while (
            timeout-- &&
            (inb(0x64) & 0x02)
        ) {
        }
    }


    void mouse_write(uint8_t data) {
        wait_output_ready();
        outb(0x64, 0xD4);

        wait_output_ready();
        outb(0x60, data);
    }


    uint8_t mouse_read() {
        wait_input_ready();
        return inb(0x60);
    }

}


// ----------------------------------------------------
// Mouse initialization
// ----------------------------------------------------

void Mouse::init() {
    state.x = max_x / 2;
    state.y = max_y / 2;

    state.left = false;
    state.right = false;
    state.middle = false;

    packet_idx = 0;

    // Enable PS/2 auxiliary device
    wait_output_ready();
    outb(0x64, 0xA8);

    // Read controller configuration byte
    wait_output_ready();
    outb(0x64, 0x20);

    uint8_t config = mouse_read();

    // Enable IRQ12
    config |= 0x02;

    // Enable mouse clock
    config &= ~0x20;

    // Write configuration byte back
    wait_output_ready();
    outb(0x64, 0x60);

    wait_output_ready();
    outb(0x60, config);

    // Reset mouse
    mouse_write(0xFF);

    mouse_read(); // ACK
    mouse_read(); // self-test
    mouse_read(); // device ID

    // Defaults
    mouse_write(0xF6);
    mouse_read();

    // Enable streaming
    mouse_write(0xF4);
    mouse_read();
}


// ----------------------------------------------------
// Process one PS/2 mouse byte
// ----------------------------------------------------

void Mouse::handle_irq() {
    uint8_t data = inb(0x60);

    /*
     * First byte of a normal PS/2 mouse packet must
     * contain bit 3.
     */
    if (packet_idx == 0) {
        if ((data & 0x08) == 0) {
            return;
        }

        /*
         * Overflow bits indicate an invalid movement
         * packet for our purposes.
         */
        if ((data & 0xC0) != 0) {
            return;
        }
    }

    packet[packet_idx++] = data;

    if (packet_idx < 3) {
        return;
    }

    packet_idx = 0;

    uint8_t flags = packet[0];

    // Buttons
    state.left =
        (flags & 0x01) != 0;

    state.right =
        (flags & 0x02) != 0;

    state.middle =
        (flags & 0x04) != 0;

    // Movement
    int32_t dx = packet[1];
    int32_t dy = packet[2];

    // Sign extension
    if (flags & 0x10) {
        dx -= 256;
    }

    if (flags & 0x20) {
        dy -= 256;
    }

    /*
     * Reject huge movements that are probably packet
     * synchronization errors.
     */
    if (
        dx > -120 &&
        dx < 120 &&
        dy > -120 &&
        dy < 120
    ) {
        state.x += dx;

        // PS/2 Y is opposite framebuffer Y.
        state.y -= dy;
    }

    // Clamp cursor to screen
    if (state.x < 0) {
        state.x = 0;
    }

    if (state.y < 0) {
        state.y = 0;
    }

    if (state.x >= max_x) {
        state.x = max_x - 1;
    }

    if (state.y >= max_y) {
        state.y = max_y - 1;
    }
}


// ----------------------------------------------------
// Poll controller for pending mouse packets
// ----------------------------------------------------

void Mouse::poll() {
    for (int guard = 0; guard < 64; ++guard) {

        uint8_t status = inb(0x64);

        // Nothing waiting
        if (!(status & 0x01)) {
            break;
        }

        /*
         * Bit 5 indicates the byte came from the
         * auxiliary PS/2 device (mouse).
         */
        if (status & 0x20) {
            handle_irq();
        } else {
            /*
             * Don't consume keyboard bytes here.
             * Leave them for the keyboard driver.
             */
            break;
        }
    }
}


// ----------------------------------------------------
// Mouse state
// ----------------------------------------------------

Mouse::State& Mouse::get_state() {
    return state;
}


// ----------------------------------------------------
// Screen bounds
// ----------------------------------------------------

void Mouse::set_bounds(
    int32_t width,
    int32_t height
) {
    if (width > 0) {
        max_x = width;
    }

    if (height > 0) {
        max_y = height;
    }

    if (state.x >= max_x) {
        state.x = max_x - 1;
    }

    if (state.y >= max_y) {
        state.y = max_y - 1;
    }
}