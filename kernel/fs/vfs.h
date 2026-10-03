/** NovaOS — Virtual Filesystem */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define VFS_NODE_FILE       0x01
#define VFS_NODE_DIRECTORY  0x02
#define VFS_NODE_MOUNTPOINT 0x04

#define VFS_OPEN_READ   0x01
#define VFS_OPEN_WRITE  0x02
#define VFS_OPEN_CREATE 0x04

struct VNode;
typedef int (*VNodeReadFn)(VNode*, uint8_t*, size_t, size_t);
typedef int (*VNodeWriteFn)(VNode*, uint8_t*, size_t, size_t);
typedef int (*VNodeOpenFn)(VNode*, uint32_t);
typedef int (*VNodeCloseFn)(VNode*);
typedef VNode* (*VNodeReadDirFn)(VNode*, uint32_t);
typedef VNode* (*VNodeFindDirFn)(VNode*, const char*);
typedef VNode* (*VNodeCreateFn)(VNode*, const char*);
typedef VNode* (*VNodeMkdirFn)(VNode*, const char*);

struct VNode {
    char name[256];
    uint32_t inode, size, flags, uid, gid;
    void* fs_data;
    VNodeReadFn read;
    VNodeWriteFn write;
    VNodeOpenFn open;
    VNodeCloseFn close;
    VNodeReadDirFn readdir;
    VNodeFindDirFn finddir;
    VNodeCreateFn create;
    VNodeMkdirFn mkdir; // New: directory creation callback
};

struct VFSMount {
    char path[256];
    VNode* root;
    bool used;
};

namespace VFS {
    void init();
    bool mount(const char* path, VNode* fs_root);
    VNode* resolve(const char* path);
    VNode* open(const char* path, uint32_t flags);
    int close(VNode* node);
    VNode* create(const char* path);
    VNode* mkdir(const char* path);
    int read(VNode* node, uint8_t* buffer, size_t size, size_t offset);
    int write(VNode* node, uint8_t* buffer, size_t size, size_t offset);
    VNode* readdir(VNode* node, uint32_t index);
    VNode* finddir(VNode* node, const char* name);
    bool is_file(VNode* node);
    bool is_directory(VNode* node);
}
