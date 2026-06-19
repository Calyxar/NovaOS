#pragma once
#include <stdint.h>

#define NOVAFS_MAGIC 0x4E4F5641 // "NOVA"
#define NOVAFS_MAX_FILES 32
#define NOVAFS_MAX_NAME 28
#define NOVAFS_DATA_START_SECTOR 9
#define NOVAFS_MAX_FILE_SECTORS 8 // 4KB max per file for now

struct NovaFSSuperblock {
    uint32_t magic;
    uint32_t file_count;
    uint32_t version;
    uint32_t reserved;
};

struct NovaFSEntry {
    char     name[NOVAFS_MAX_NAME];
    uint32_t size;
    uint32_t start_sector;
};

namespace NovaFSDisk {
    void init();              // mount: read superblock + file table from disk
    bool format();            // wipe and write a fresh empty filesystem
    bool save_file(const char* name, const char* data, uint32_t size);
    bool load_file(const char* name, char* out_buf, uint32_t max_size, uint32_t* out_size);
    void list_files(void (*cb)(const char* name, uint32_t size));
    bool delete_file(const char* name);
    void sync();               // flush in-memory file table back to disk
}
