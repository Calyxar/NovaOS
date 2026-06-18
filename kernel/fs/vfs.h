/**
 * novaOS — Virtual Filesystem
 * Abstraction layer over real filesystems (novaFS, ext2, FAT32).
 * Everything is a file — devices, pipes, sockets.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

struct VNode {
    char     name[256];
    uint32_t inode;
    uint32_t size;
    uint32_t flags;
    uint32_t uid, gid;

    int    (*read)   (VNode*, uint8_t* buf, size_t size, size_t offset);
    int    (*write)  (VNode*, uint8_t* buf, size_t size, size_t offset);
    int    (*open)   (VNode*, uint32_t flags);
    int    (*close)  (VNode*);
    VNode* (*readdir)(VNode*, uint32_t index);
    VNode* (*finddir)(VNode*, const char* name);
};

namespace VFS {
    void   init();
    VNode* open  (const char* path, uint32_t flags);
    int    read  (VNode* node, uint8_t* buf, size_t size, size_t offset);
    int    write (VNode* node, uint8_t* buf, size_t size, size_t offset);
    int    close (VNode* node);
    void   mount (const char* path, VNode* fs_root);
}
