#include "novafs_disk.h"
#include "../drivers/disk/ata.h"

static NovaFSSuperblock sb;
static NovaFSEntry      entries[NOVAFS_MAX_FILES];
static bool             mounted = false;

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}
static void strcopy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max-1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

// File table occupies sectors 1..8. Each NovaFSEntry is 36 bytes;
// 512/36 = ~14 entries per sector, so 8 sectors comfortably holds 32 entries.
static void read_file_table() {
    uint8_t buf[512];
    int entries_per_sector = 512 / sizeof(NovaFSEntry);
    int idx = 0;
    for (int s = 0; s < 8 && idx < NOVAFS_MAX_FILES; s++) {
        ATA::read_sector(1 + s, buf);
        for (int i = 0; i < entries_per_sector && idx < NOVAFS_MAX_FILES; i++, idx++) {
            NovaFSEntry* e = (NovaFSEntry*)(buf + i*sizeof(NovaFSEntry));
            entries[idx] = *e;
        }
    }
}

static void write_file_table() {
    uint8_t buf[512];
    int entries_per_sector = 512 / sizeof(NovaFSEntry);
    int idx = 0;
    for (int s = 0; s < 8 && idx < NOVAFS_MAX_FILES; s++) {
        for (int i = 0; i < 512; i++) buf[i] = 0;
        for (int i = 0; i < entries_per_sector && idx < NOVAFS_MAX_FILES; i++, idx++) {
            NovaFSEntry* e = (NovaFSEntry*)(buf + i*sizeof(NovaFSEntry));
            *e = entries[idx];
        }
        ATA::write_sector(1 + s, buf);
    }
}

void NovaFSDisk::init() {
    uint8_t buf[512];
    ATA::read_sector(0, buf);
    NovaFSSuperblock* found = (NovaFSSuperblock*)buf;

    if (found->magic == NOVAFS_MAGIC) {
        sb = *found;
        read_file_table();
        mounted = true;
    } else {
        // No valid filesystem found — format a fresh one
        format();
    }
}

bool NovaFSDisk::format() {
    sb.magic = NOVAFS_MAGIC;
    sb.file_count = 0;
    sb.version = 1;
    sb.reserved = 0;

    for (int i = 0; i < NOVAFS_MAX_FILES; i++) {
        entries[i].name[0] = '\0';
        entries[i].size = 0;
        entries[i].start_sector = 0;
    }

    uint8_t buf[512];
    for (int i = 0; i < 512; i++) buf[i] = 0;
    NovaFSSuperblock* sbptr = (NovaFSSuperblock*)buf;
    *sbptr = sb;
    ATA::write_sector(0, buf);

    write_file_table();
    mounted = true;
    return true;
}

static int find_entry(const char* name) {
    for (int i = 0; i < NOVAFS_MAX_FILES; i++) {
        if (entries[i].name[0] && streq(entries[i].name, name)) return i;
    }
    return -1;
}

static int find_free_slot() {
    for (int i = 0; i < NOVAFS_MAX_FILES; i++) {
        if (!entries[i].name[0]) return i;
    }
    return -1;
}

bool NovaFSDisk::save_file(const char* name, const char* data, uint32_t size) {
    if (!mounted) return false;
    if (size > NOVAFS_MAX_FILE_SECTORS * 512) return false;

    int idx = find_entry(name);
    if (idx < 0) {
        idx = find_free_slot();
        if (idx < 0) return false; // file table full
        strcopy(entries[idx].name, name, NOVAFS_MAX_NAME);
        entries[idx].start_sector = NOVAFS_DATA_START_SECTOR + idx * NOVAFS_MAX_FILE_SECTORS;
        sb.file_count++;
    }
    entries[idx].size = size;

    // Write data across however many sectors are needed
    uint32_t sector = entries[idx].start_sector;
    uint32_t remaining = size;
    uint32_t offset = 0;
    uint8_t buf[512];
    while (remaining > 0 || (offset == 0 && size == 0)) {
        for (int i = 0; i < 512; i++) buf[i] = 0;
        uint32_t chunk = remaining > 512 ? 512 : remaining;
        for (uint32_t i = 0; i < chunk; i++) buf[i] = (uint8_t)data[offset+i];
        ATA::write_sector(sector, buf);
        sector++;
        offset += chunk;
        remaining -= chunk;
        if (size == 0) break;
    }

    write_file_table();

    uint8_t sbbuf[512];
    for (int i=0;i<512;i++) sbbuf[i]=0;
    NovaFSSuperblock* sbptr = (NovaFSSuperblock*)sbbuf;
    *sbptr = sb;
    ATA::write_sector(0, sbbuf);

    return true;
}

bool NovaFSDisk::load_file(const char* name, char* out_buf, uint32_t max_size, uint32_t* out_size) {
    if (!mounted) return false;
    int idx = find_entry(name);
    if (idx < 0) return false;

    uint32_t size = entries[idx].size;
    if (size > max_size) size = max_size;
    *out_size = size;

    uint32_t sector = entries[idx].start_sector;
    uint32_t remaining = size;
    uint32_t offset = 0;
    uint8_t buf[512];
    while (remaining > 0) {
        ATA::read_sector(sector, buf);
        uint32_t chunk = remaining > 512 ? 512 : remaining;
        for (uint32_t i = 0; i < chunk; i++) out_buf[offset+i] = (char)buf[i];
        sector++;
        offset += chunk;
        remaining -= chunk;
    }
    out_buf[size] = '\0';
    return true;
}

void NovaFSDisk::list_files(void (*cb)(const char* name, uint32_t size)) {
    if (!mounted) return;
    for (int i = 0; i < NOVAFS_MAX_FILES; i++) {
        if (entries[i].name[0]) cb(entries[i].name, entries[i].size);
    }
}

bool NovaFSDisk::delete_file(const char* name) {
    if (!mounted) return false;
    int idx = find_entry(name);
    if (idx < 0) return false;
    entries[idx].name[0] = '\0';
    entries[idx].size = 0;
    sb.file_count--;
    write_file_table();
    return true;
}

void NovaFSDisk::sync() {
    write_file_table();
}
