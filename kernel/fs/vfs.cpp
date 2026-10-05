#include "vfs.h"

static constexpr int VFS_MAX_MOUNTS = 8;
static VFSMount mounts[VFS_MAX_MOUNTS];

static bool streq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

static void strcopy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

void VFS::init() {
    for (int i = 0; i < VFS_MAX_MOUNTS; ++i) {
        mounts[i].used = false;
        mounts[i].path[0] = '\0';
        mounts[i].root = nullptr;
    }
}

bool VFS::mount(const char* path, VNode* fs_root) {
    if (!path || !fs_root) return false;
    for (int i = 0; i < VFS_MAX_MOUNTS; ++i)
        if (mounts[i].used && streq(mounts[i].path, path)) return false;
    for (int i = 0; i < VFS_MAX_MOUNTS; ++i) {
        if (mounts[i].used) continue;
        mounts[i].used = true;
        strcopy(mounts[i].path, path, 256);
        mounts[i].root = fs_root;
        fs_root->flags |= VFS_NODE_MOUNTPOINT;
        return true;
    }
    return false;
}

static VNode* get_root_mount() {
    for (int i = 0; i < VFS_MAX_MOUNTS; ++i)
        if (mounts[i].used && streq(mounts[i].path, "/")) return mounts[i].root;
    return nullptr;
}

VNode* VFS::resolve(const char* path) {
    if (!path || path[0] != '/') return nullptr;
    VNode* current = get_root_mount();
    if (!current) return nullptr;
    if (path[1] == '\0') return current;
    char part[256];
    int pathIndex = 1;
    while (path[pathIndex] != '\0') {
        while (path[pathIndex] == '/') ++pathIndex;
        if (path[pathIndex] == '\0') break;
        int partIndex = 0;
        while (path[pathIndex] && path[pathIndex] != '/' && partIndex < 255)
            part[partIndex++] = path[pathIndex++];
        // Reject overlong components rather than resolving a truncated name.
        if (path[pathIndex] && path[pathIndex] != '/') return nullptr;
        part[partIndex] = '\0';
        if (!partIndex) continue;
        if (!current->finddir) return nullptr;
        current = current->finddir(current, part);
        if (!current) return nullptr;
    }
    return current;
}

VNode* VFS::open(const char* path, uint32_t flags) {
    VNode* node = resolve(path);
    if (!node) return nullptr;
    if (node->open && node->open(node, flags) < 0) return nullptr;
    return node;
}

// Extract an absolute path's parent and final component; reject trailing slashes.
static bool split_path(const char* path, char* parentPath, char* name) {
    if (!path || path[0] != '/') return false;
    int length = 0;
    while (path[length] && length < 256) ++length;
    if (length <= 1 || length >= 256 || path[length - 1] == '/') return false;
    int slashIndex = length - 1;
    while (slashIndex > 0 && path[slashIndex] != '/') --slashIndex;
    int nameLength = length - slashIndex - 1;
    if (nameLength <= 0 || nameLength >= 256) return false;
    for (int i = 0; i < nameLength; ++i) name[i] = path[slashIndex + 1 + i];
    name[nameLength] = '\0';
    if (slashIndex == 0) {
        parentPath[0] = '/'; parentPath[1] = '\0';
    } else {
        for (int i = 0; i < slashIndex; ++i) parentPath[i] = path[i];
        parentPath[slashIndex] = '\0';
    }
    return true;
}

VNode* VFS::create(const char* path) {
    char parentPath[256], name[256];
    if (!split_path(path, parentPath, name)) return nullptr;
    VNode* parent = resolve(parentPath);
    if (!is_directory(parent) || !parent->create) return nullptr;
    if (parent->finddir && parent->finddir(parent, name)) return nullptr;
    return parent->create(parent, name);
}

VNode* VFS::mkdir(const char* path) {
    char parentPath[256], name[256];
    if (!split_path(path, parentPath, name)) return nullptr;
    VNode* parent = resolve(parentPath);
    if (!is_directory(parent) || !parent->mkdir) return nullptr;
    if (parent->finddir && parent->finddir(parent, name)) return nullptr;
    return parent->mkdir(parent, name);
}

// Rename a file or directory in place. newName is a single component,
// not a destination path (moving entries is a separate future operation).
bool VFS::rename(const char* oldPath, const char* newName) {
    if (!oldPath || !newName || !newName[0]) return false;
    if (streq(oldPath, "/")) return false;
    for (int i = 0; newName[i]; ++i)
        if (newName[i] == '/' || newName[i] == '\\') return false;
    if (streq(newName, ".") || streq(newName, "..")) return false;
    VNode* node = resolve(oldPath);
    if (!node || !node->rename || (node->flags & VFS_NODE_MOUNTPOINT))
        return false;
    return node->rename(node, newName);
}

// Remove a file or empty directory. Filesystem backends decide whether
// removal is legal; NovaFS refuses non-empty directories.
bool VFS::remove(const char* path) {
    if (!path || streq(path, "/")) return false;
    VNode* node = resolve(path);
    if (!node || !node->remove || (node->flags & VFS_NODE_MOUNTPOINT))
        return false;
    return node->remove(node);
}

int VFS::read(VNode* node, uint8_t* buffer, size_t size, size_t offset) {
    if (!node || !buffer || !node->read) return -1;
    return node->read(node, buffer, size, offset);
}
int VFS::write(VNode* node, uint8_t* buffer, size_t size, size_t offset) {
    if (!node || !buffer || !node->write) return -1;
    return node->write(node, buffer, size, offset);
}
int VFS::close(VNode* node) {
    if (!node) return -1;
    return node->close ? node->close(node) : 0;
}
VNode* VFS::readdir(VNode* node, uint32_t index) {
    if (!is_directory(node) || !node->readdir) return nullptr;
    return node->readdir(node, index);
}
VNode* VFS::finddir(VNode* node, const char* name) {
    if (!name || !is_directory(node) || !node->finddir) return nullptr;
    return node->finddir(node, name);
}
bool VFS::is_file(VNode* node) {
    return node && (node->flags & VFS_NODE_FILE) != 0;
}
bool VFS::is_directory(VNode* node) {
    return node && (node->flags & VFS_NODE_DIRECTORY) != 0;
}
