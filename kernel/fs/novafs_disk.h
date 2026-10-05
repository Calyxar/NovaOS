#pragma once
#include <stdint.h>
#define NOVAFS_MAGIC 0x4E4F5641
#define NOVAFS_VERSION 2
#define NOVAFS_MAX_FILES 32
#define NOVAFS_MAX_NAME 28
#define NOVAFS_DATA_START_SECTOR 9
#define NOVAFS_MAX_FILE_SECTORS 8
#define NOVAFS_ENTRY_FILE 1
#define NOVAFS_ENTRY_DIRECTORY 2
#define NOVAFS_ROOT_PARENT 0xFFFFFFFFu
struct NovaFSSuperblock { uint32_t magic, entry_count, version, reserved; };
struct NovaFSEntry {
    char name[NOVAFS_MAX_NAME];
    uint32_t size, start_sector, parent, type;
};
namespace NovaFSDisk {
    void init(); bool format();
    bool save_file(const char* name, const char* data, uint32_t size);
    bool save_file_in(uint32_t parent, const char* name, const char* data, uint32_t size);
    bool load_file(const char* name, char* out_buf, uint32_t max_size, uint32_t* out_size);
    bool load_file_in(uint32_t parent, const char* name, char* out_buf, uint32_t max_size, uint32_t* out_size);
    void list_files(void (*cb)(const char* name, uint32_t size));
    bool create_directory(uint32_t parent, const char* name);
    int find_directory(uint32_t parent, const char* name);
    void list_directory(uint32_t parent, void (*cb)(const NovaFSEntry& entry, uint32_t index));
    bool rename_entry(uint32_t index, const char* new_name);
    bool delete_file(const char* name);
    bool delete_entry(uint32_t index);
    bool delete_entry_in(uint32_t parent, const char* name);
    void sync();
}
