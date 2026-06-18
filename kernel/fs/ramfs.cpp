#include "ramfs.h"

static RamFile files[RAMFS_MAX_FILES];

static void strcpy_safe(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}

void RamFS::init() {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].used = false;
        files[i].size = 0;
        files[i].name[0] = '\0';
    }
    // Create some default files
    create("readme.txt");
    write("readme.txt", "Welcome to NovaOS v0.1.0!\nBuilt from scratch in C/C++/ASM.", 57);

    create("about.txt");
    write("about.txt", "NovaOS — Next-gen OS for future computers.\nKernel: C/C++/ASM | GPU: NVIDIA", 74);
}

bool RamFS::create(const char* name) {
    if (find(name)) return false; // Already exists
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = true;
            files[i].size = 0;
            files[i].data[0] = '\0';
            strcpy_safe(files[i].name, name, RAMFS_MAX_FILENAME);
            return true;
        }
    }
    return false; // Full
}

bool RamFS::write(const char* name, const char* data, uint32_t len) {
    RamFile* f = find(name);
    if (!f) return false;
    if (len >= RAMFS_MAX_FILESIZE) len = RAMFS_MAX_FILESIZE - 1;
    for (uint32_t i = 0; i < len; i++) f->data[i] = data[i];
    f->data[len] = '\0';
    f->size = len;
    return true;
}

RamFile* RamFS::find(const char* name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && streq(files[i].name, name))
            return &files[i];
    return nullptr;
}

bool RamFS::remove(const char* name) {
    RamFile* f = find(name);
    if (!f) return false;
    f->used = false;
    f->name[0] = '\0';
    f->size = 0;
    return true;
}

void RamFS::list(void (*callback)(const char* name, uint32_t size)) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used)
            callback(files[i].name, files[i].size);
}

int RamFS::file_count() {
    int count = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used) count++;
    return count;
}
