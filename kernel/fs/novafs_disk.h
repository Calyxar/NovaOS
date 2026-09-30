#pragma once

#include <stdint.h>


// =============================================================
// NovaFS constants
// =============================================================

#define NOVAFS_MAGIC 0x4E4F5641   // "NOVA"

#define NOVAFS_VERSION 2

#define NOVAFS_MAX_FILES 32
#define NOVAFS_MAX_NAME 28

#define NOVAFS_DATA_START_SECTOR 9

#define NOVAFS_MAX_FILE_SECTORS 8  // 4 KB per file for now


// =============================================================
// Entry types
// =============================================================

#define NOVAFS_ENTRY_FILE      1
#define NOVAFS_ENTRY_DIRECTORY 2


// Special parent value meaning the root directory: /
#define NOVAFS_ROOT_PARENT 0xFFFFFFFFu


// =============================================================
// Superblock
// =============================================================

struct NovaFSSuperblock {
    uint32_t magic;

    // Number of active files + directories.
    uint32_t entry_count;

    uint32_t version;

    uint32_t reserved;
};


// =============================================================
// File / directory entry
// =============================================================

struct NovaFSEntry {
    char name[NOVAFS_MAX_NAME];

    // Files use this for their byte size.
    // Directories keep this at 0.
    uint32_t size;

    // Files point to their first data sector.
    // Directories keep this at 0.
    uint32_t start_sector;

    // Index of the directory containing this entry.
    //
    // NOVAFS_ROOT_PARENT means the entry lives directly in /.
    uint32_t parent;

    // NOVAFS_ENTRY_FILE
    // or
    // NOVAFS_ENTRY_DIRECTORY
    uint32_t type;
};


// =============================================================
// NovaFS disk interface
// =============================================================

namespace NovaFSDisk {

    // Initialize / mount NovaFS.
    void init();


    // Erase and create a fresh NovaFS filesystem.
    bool format();


    // ---------------------------------------------------------
    // Files
    // ---------------------------------------------------------

    // Save a file directly in /.
    bool save_file(
        const char* name,
        const char* data,
        uint32_t size
    );


    // Save a file inside a specific directory.
    bool save_file_in(
        uint32_t parent,
        const char* name,
        const char* data,
        uint32_t size
    );


    // Load a file directly from /.
    bool load_file(
        const char* name,
        char* out_buf,
        uint32_t max_size,
        uint32_t* out_size
    );


    // Load a file from a specific directory.
    bool load_file_in(
        uint32_t parent,
        const char* name,
        char* out_buf,
        uint32_t max_size,
        uint32_t* out_size
    );


    // Existing root-file listing used by the Files UI.
    void list_files(
        void (*cb)(
            const char* name,
            uint32_t size
        )
    );


    // ---------------------------------------------------------
    // Directories
    // ---------------------------------------------------------

    bool create_directory(
        uint32_t parent,
        const char* name
    );


    // Returns the entry index of a directory.
    // Returns -1 if it does not exist.
    int find_directory(
        uint32_t parent,
        const char* name
    );


    // List everything contained in a directory.
    //
    // index is the NovaFS entry index.
    void list_directory(
        uint32_t parent,
        void (*cb)(
            const NovaFSEntry& entry,
            uint32_t index
        )
    );


    // ---------------------------------------------------------
    // Removal
    // ---------------------------------------------------------

    // Existing root file deletion function.
    bool delete_file(
        const char* name
    );


    // Delete an entry using its NovaFS index.
    bool delete_entry(
        uint32_t index
    );


    // ---------------------------------------------------------
    // Disk synchronization
    // ---------------------------------------------------------

    void sync();
}