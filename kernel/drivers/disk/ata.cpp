#include "ata.h"

#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30

#define ATA_SR_BSY     0x80
#define ATA_SR_DRQ     0x08
#define ATA_SR_ERR     0x01

static inline uint8_t inb(uint16_t port) {
    uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t v; asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
static inline void outw(uint16_t port, uint16_t v) {
    asm volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}

static bool wait_ready() {
    // Wait until BSY clears, with a generous timeout
    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t status = inb(ATA_STATUS);
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) return false;
            return true;
        }
    }
    return false;
}

static bool wait_drq() {
    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DRQ) return true;
    }
    return false;
}

void ATA::init() {
    // Nothing fancy needed for PIO mode on the primary bus / master drive
    wait_ready();
}

bool ATA::read_sector(uint32_t lba, uint8_t* buffer) {
    if (!wait_ready()) return false;

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); // master drive, LBA mode
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ);

    if (!wait_ready()) return false;
    if (!wait_drq())   return false;

    uint16_t* buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) { // 512 bytes = 256 words
        buf16[i] = inw(ATA_DATA);
    }
    return true;
}

bool ATA::write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!wait_ready()) return false;

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (!wait_ready()) return false;
    if (!wait_drq())   return false;

    const uint16_t* buf16 = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buf16[i]);
    }

    // Flush cache so the write is committed
    outb(ATA_COMMAND, 0xE7); // CACHE FLUSH
    wait_ready();
    return true;
}
