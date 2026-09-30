#include "vfs.h"


// =============================================================
// Configuration
// =============================================================

static constexpr int VFS_MAX_MOUNTS = 8;


// =============================================================
// VFS state
// =============================================================

static VFSMount mounts[VFS_MAX_MOUNTS];


// =============================================================
// String helpers
// =============================================================

static bool streq(
    const char* a,
    const char* b
) {
    while (*a && *b) {
        if (*a != *b)
            return false;

        ++a;
        ++b;
    }

    return *a == *b;
}


static void strcopy(
    char* dst,
    const char* src,
    int max
) {
    int i = 0;

    while (
        src[i] &&
        i < max - 1
    ) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}


// =============================================================
// Initialization
// =============================================================

void VFS::init() {
    for (
        int i = 0;
        i < VFS_MAX_MOUNTS;
        ++i
    ) {
        mounts[i].used = false;

        mounts[i].path[0] = '\0';

        mounts[i].root = nullptr;
    }
}


// =============================================================
// Mounting
// =============================================================

bool VFS::mount(
    const char* path,
    VNode* fs_root
) {
    if (!path || !fs_root)
        return false;

    for (
        int i = 0;
        i < VFS_MAX_MOUNTS;
        ++i
    ) {
        if (
            mounts[i].used &&
            streq(
                mounts[i].path,
                path
            )
        ) {
            return false;
        }
    }


    for (
        int i = 0;
        i < VFS_MAX_MOUNTS;
        ++i
    ) {
        if (!mounts[i].used) {
            mounts[i].used = true;

            strcopy(
                mounts[i].path,
                path,
                256
            );

            mounts[i].root =
                fs_root;

            fs_root->flags |=
                VFS_NODE_MOUNTPOINT;

            return true;
        }
    }

    return false;
}


// =============================================================
// Root mount lookup
// =============================================================

static VNode* get_root_mount() {
    for (
        int i = 0;
        i < VFS_MAX_MOUNTS;
        ++i
    ) {
        if (
            mounts[i].used &&
            streq(
                mounts[i].path,
                "/"
            )
        ) {
            return mounts[i].root;
        }
    }

    return nullptr;
}


// =============================================================
// Path parsing
// =============================================================

VNode* VFS::resolve(
    const char* path
) {
    if (!path)
        return nullptr;

    if (path[0] != '/')
        return nullptr;


    VNode* current =
        get_root_mount();

    if (!current)
        return nullptr;


    // Root path
    if (
        path[0] == '/' &&
        path[1] == '\0'
    ) {
        return current;
    }


    char part[256];

    int pathIndex = 1;


    while (
        path[pathIndex] != '\0'
    ) {

        // Skip repeated slashes
        while (
            path[pathIndex] == '/'
        ) {
            ++pathIndex;
        }


        if (
            path[pathIndex] == '\0'
        ) {
            break;
        }


        int partIndex = 0;


        while (
            path[pathIndex] != '\0' &&
            path[pathIndex] != '/' &&
            partIndex < 255
        ) {
            part[partIndex++] =
                path[pathIndex++];

        }

        part[partIndex] =
            '\0';


        if (
            partIndex == 0
        ) {
            continue;
        }


        if (!current->finddir)
            return nullptr;


        current =
            current->finddir(
                current,
                part
            );


        if (!current)
            return nullptr;
    }


    return current;
}


// =============================================================
// Open
// =============================================================

VNode* VFS::open(
    const char* path,
    uint32_t flags
) {
    VNode* node =
        resolve(path);

    if (!node)
        return nullptr;


    if (node->open) {
        int result =
            node->open(
                node,
                flags
            );

        if (result < 0)
            return nullptr;
    }


    return node;
}

// =============================================================
// Create file
// =============================================================

VNode* VFS::create(
    const char* path
) {
    if (!path)
        return nullptr;

    if (path[0] != '/')
        return nullptr;


    char parentPath[256];
    char fileName[256];


    int length = 0;

    while (
        path[length] &&
        length < 255
    ) {
        ++length;
    }


    if (length <= 1)
        return nullptr;


    int slashIndex =
        length - 1;


    while (
        slashIndex > 0 &&
        path[slashIndex] != '/'
    ) {
        --slashIndex;
    }


    // -------------------------
    // Extract filename
    // -------------------------

    int fileIndex = 0;

    for (
        int i = slashIndex + 1;
        path[i] &&
        fileIndex < 255;
        ++i
    ) {
        fileName[fileIndex++] =
            path[i];
    }

    fileName[fileIndex] =
        '\0';


    if (
        fileName[0] ==
        '\0'
    ) {
        return nullptr;
    }


    // -------------------------
    // Extract parent path
    // -------------------------

    if (slashIndex == 0) {
        parentPath[0] = '/';
        parentPath[1] = '\0';
    }

    else {
        int parentIndex = 0;

        for (
            int i = 0;
            i < slashIndex &&
            parentIndex < 255;
            ++i
        ) {
            parentPath[parentIndex++] =
                path[i];
        }

        parentPath[parentIndex] =
            '\0';
    }


    // -------------------------
    // Resolve parent
    // -------------------------

    VNode* parent =
        resolve(
            parentPath
        );


    if (!parent)
        return nullptr;


    if (
        !is_directory(
            parent
        )
    ) {
        return nullptr;
    }


    // Already exists?
    if (
        parent->finddir &&
        parent->finddir(
            parent,
            fileName
        )
    ) {
        return nullptr;
    }


    if (!parent->create)
        return nullptr;


    return parent->create(
        parent,
        fileName
    );
}

// =============================================================
// Read
// =============================================================

int VFS::read(
    VNode* node,
    uint8_t* buffer,
    size_t size,
    size_t offset
) {
    if (!node)
        return -1;

    if (!buffer)
        return -1;

    if (!node->read)
        return -1;


    return node->read(
        node,
        buffer,
        size,
        offset
    );
}


// =============================================================
// Write
// =============================================================

int VFS::write(
    VNode* node,
    uint8_t* buffer,
    size_t size,
    size_t offset
) {
    if (!node)
        return -1;

    if (!buffer)
        return -1;

    if (!node->write)
        return -1;


    return node->write(
        node,
        buffer,
        size,
        offset
    );
}


// =============================================================
// Close
// =============================================================

int VFS::close(
    VNode* node
) {
    if (!node)
        return -1;


    if (node->close) {
        return node->close(
            node
        );
    }


    return 0;
}


// =============================================================
// Directory wrapper
// =============================================================

VNode* VFS::readdir(
    VNode* node,
    uint32_t index
) {
    if (!node)
        return nullptr;

    if (!is_directory(node))
        return nullptr;

    if (!node->readdir)
        return nullptr;


    return node->readdir(
        node,
        index
    );
}


// =============================================================
// Directory lookup wrapper
// =============================================================

VNode* VFS::finddir(
    VNode* node,
    const char* name
) {
    if (!node)
        return nullptr;

    if (!name)
        return nullptr;

    if (!is_directory(node))
        return nullptr;

    if (!node->finddir)
        return nullptr;


    return node->finddir(
        node,
        name
    );
}


// =============================================================
// Type helpers
// =============================================================

bool VFS::is_file(
    VNode* node
) {
    if (!node)
        return false;


    return
        (node->flags &
         VFS_NODE_FILE) != 0;
}


bool VFS::is_directory(
    VNode* node
) {
    if (!node)
        return false;


    return
        (node->flags &
         VFS_NODE_DIRECTORY) != 0;
}