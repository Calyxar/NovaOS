/**
 * novaOS — Programmable Interval Timer (PIT 8253)
 * Drives the kernel tick and scheduler preemption.
 */
#pragma once
#include <stdint.h>

namespace PIT {
    void     init(uint32_t frequency_hz);
    uint32_t ticks();
    void     sleep_ms(uint32_t ms);
    void     handle_irq();
}
