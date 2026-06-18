/**
 * NovaOS — RAM Filesystem (ramfs)
 * Simple in-memory filesystem. Files live in RAM — gone on reboot.
 * Foundation for a real persistent filesystem later.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAMFS_MAX_FILES    32
#define RAMFS_MAX_FILENAME 32
#define RAMFS_MAX_FILESIZE 4096

struct RamFile {
    char     name[RAMFS_MAX_FILENAME];
    char     data[RAMFS_MAX_FILESIZE];
    uint32_t size;
    bool     used;
};

namespace RamFS {
    void     init();
    bool     create(const char* name);
    bool     write (const char* name, const char* data, uint32_t len);
    RamFile* find  (const char* name);
    bool     remove(const char* name);
    void     list  (void (*callback)(const char* name, uint32_t size));
    int      file_count();
}
