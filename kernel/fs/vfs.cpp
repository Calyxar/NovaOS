#include "vfs.h"
void VFS::init() {}
VNode* VFS::open(const char* path, uint32_t flags) { return nullptr; }
int VFS::read(VNode* node, uint8_t* buf, size_t size, size_t offset) { return -1; }
int VFS::write(VNode* node, uint8_t* buf, size_t size, size_t offset) { return -1; }
int VFS::close(VNode* node) { return 0; }
void VFS::mount(const char* path, VNode* fs_root) {}
