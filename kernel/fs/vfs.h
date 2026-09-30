/**
 * NovaOS — Virtual Filesystem
 *
 * Abstraction layer over real filesystems such as:
 * - NovaFS
 * - RAMFS
 * - future FAT32 / ext2
 *
 * Everything exposed through the VFS is represented as a VNode.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>


// =============================================================
// VNode flags
// =============================================================

#define VFS_NODE_FILE       0x01
#define VFS_NODE_DIRECTORY  0x02
#define VFS_NODE_MOUNTPOINT 0x04


// =============================================================
// Open flags
// =============================================================

#define VFS_OPEN_READ   0x01
#define VFS_OPEN_WRITE  0x02
#define VFS_OPEN_CREATE 0x04


// =============================================================
// Forward declaration
// =============================================================

struct VNode;


// =============================================================
// VNode callbacks
// =============================================================

typedef int (*VNodeReadFn)(
    VNode* node,
    uint8_t* buffer,
    size_t size,
    size_t offset
);

typedef int (*VNodeWriteFn)(
    VNode* node,
    uint8_t* buffer,
    size_t size,
    size_t offset
);

typedef int (*VNodeOpenFn)(
    VNode* node,
    uint32_t flags
);

typedef int (*VNodeCloseFn)(
    VNode* node
);

typedef VNode* (*VNodeReadDirFn)(
    VNode* node,
    uint32_t index
);

typedef VNode* (*VNodeFindDirFn)(
    VNode* node,
    const char* name
);

typedef VNode* (*VNodeCreateFn)(
    VNode* directory,
    const char* name
);


// =============================================================
// VNode
// =============================================================

struct VNode {
    char name[256];

    // Filesystem-specific identifier.
    uint32_t inode;

    // File size.
    // Usually 0 for directories.
    uint32_t size;

    // VFS_NODE_FILE
    // VFS_NODE_DIRECTORY
    // VFS_NODE_MOUNTPOINT
    uint32_t flags;

    uint32_t uid;
    uint32_t gid;


    // Filesystem-specific private data.
    void* fs_data;


    // File operations
    VNodeReadFn read;
    VNodeWriteFn write;

    VNodeOpenFn open;
    VNodeCloseFn close;


    // Directory operations
    VNodeReadDirFn readdir;
    VNodeFindDirFn finddir;
    VNodeCreateFn create;
};


// =============================================================
// Mount structure
// =============================================================

struct VFSMount {
    char path[256];

    VNode* root;

    bool used;
};


// =============================================================
// VFS
// =============================================================

namespace VFS {

    // Initialize the VFS layer.
    void init();


    // ---------------------------------------------------------
    // Mounting
    // ---------------------------------------------------------

    bool mount(
        const char* path,
        VNode* fs_root
    );


    // ---------------------------------------------------------
    // Path lookup
    // ---------------------------------------------------------

    VNode* resolve(
        const char* path
    );


    // ---------------------------------------------------------
    // Open / close
    // ---------------------------------------------------------

    VNode* open(
        const char* path,
        uint32_t flags
    );

    int close(
        VNode* node
    );


    // ---------------------------------------------------------
    // File creation
    // ---------------------------------------------------------

    VNode* create(
        const char* path
    );


    // ---------------------------------------------------------
    // File I/O
    // ---------------------------------------------------------

    int read(
        VNode* node,
        uint8_t* buffer,
        size_t size,
        size_t offset
    );

    int write(
        VNode* node,
        uint8_t* buffer,
        size_t size,
        size_t offset
    );


    // ---------------------------------------------------------
    // Directory operations
    // ---------------------------------------------------------

    VNode* readdir(
        VNode* node,
        uint32_t index
    );

    VNode* finddir(
        VNode* node,
        const char* name
    );


    // ---------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------

    bool is_file(
        VNode* node
    );

    bool is_directory(
        VNode* node
    );
}