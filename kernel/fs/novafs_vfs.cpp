#include "novafs_vfs.h"

#include "novafs_disk.h"

// =============================================================

// Configuration

// =============================================================

static constexpr int MAX_VNODES =

    NOVAFS_MAX_FILES + 1;

// =============================================================

// VNode storage

// =============================================================

static VNode nodes[MAX_VNODES];

static bool nodeUsed[MAX_VNODES];

static VNode rootNode;

// =============================================================

// Helpers

// =============================================================

static void clear_text(

    char* dst,

    int max

) {

    for (int i = 0; i < max; ++i)

        dst[i] = '\0';

}

static void copy_text(

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

// Forward declarations

// =============================================================

static VNode* novafs_readdir(

    VNode* node,

    uint32_t index

);

static VNode* novafs_finddir(

    VNode* node,

    const char* name

);

static VNode* novafs_create(

    VNode* directory,

    const char* name

);

static int novafs_write(

    VNode* node,

    uint8_t* buffer,

    size_t size,

    size_t offset

);

static int novafs_read(

    VNode* node,

    uint8_t* buffer,

    size_t size,

    size_t offset

);

// =============================================================

// Directory listing state

// =============================================================

struct DirectoryLookupState {

    uint32_t parent;

    uint32_t wantedIndex;

    uint32_t currentIndex;

    VNode* result;

};

static DirectoryLookupState lookupState;

// =============================================================

// File read

// =============================================================

static int novafs_read(

    VNode* node,

    uint8_t* buffer,

    size_t size,

    size_t offset

) {

    if (!node || !buffer)

        return -1;

    if (

        (node->flags & VFS_NODE_FILE) == 0

    ) {

        return -1;

    }

    if (offset >= node->size)

        return 0;

    char temp[

        NOVAFS_MAX_FILE_SECTORS * 512 + 1

    ];

    uint32_t loadedSize = 0;

    uint32_t parent =

        (uint32_t)(uintptr_t)

        node->fs_data;

    if (

        !NovaFSDisk::load_file_in(

            parent,

            node->name,

            temp,

            sizeof(temp),

            &loadedSize

        )

    ) {

        return -1;

    }

    if (offset >= loadedSize)

        return 0;

    uint32_t available =

        loadedSize - offset;

    uint32_t toCopy =

        size < available

            ? (uint32_t)size

            : available;

    for (

        uint32_t i = 0;

        i < toCopy;

        ++i

    ) {

        buffer[i] =

            (uint8_t)temp[offset + i];

    }

    return (int)toCopy;

}

// =============================================================

// File write (whole-file replacement, offset zero)

// =============================================================

static int novafs_write(

    VNode* node,

    uint8_t* buffer,

    size_t size,

    size_t offset

) {

    if (!node || !buffer)

        return -1;

    if ((node->flags & VFS_NODE_FILE) == 0)

        return -1;

    if (offset != 0)

        return -1;

    if (size > NOVAFS_MAX_FILE_SECTORS * 512)

        return -1;

    uint32_t parent =

        (uint32_t)(uintptr_t)node->fs_data;

    if (!NovaFSDisk::save_file_in(

        parent,

        node->name,

        (const char*)buffer,

        (uint32_t)size

    )) {

        return -1;

    }

    node->size = (uint32_t)size;

    return (int)size;

}

// =============================================================

// Convert NovaFS entry to VNode

// =============================================================

static VNode* make_vnode(

    const NovaFSEntry& entry,

    uint32_t entryIndex

) {

    if (

        entryIndex >=

        NOVAFS_MAX_FILES

    ) {

        return nullptr;

    }

    VNode* node =

        &nodes[entryIndex];

    nodeUsed[entryIndex] =

        true;

    // Clear old state because VNodes are reused.

    clear_text(

        node->name,

        256

    );

    node->inode = 0;

    node->size = 0;

    node->flags = 0;

    node->uid = 0;

    node->gid = 0;

    node->fs_data = nullptr;

    node->read = nullptr;

    node->write =

            nullptr;

    node->open = nullptr;

    node->close = nullptr;

    node->readdir = nullptr;

    node->finddir = nullptr;

    node->create = nullptr;

    // ---------------------------------------------------------

    // Common entry information

    // ---------------------------------------------------------

    copy_text(

        node->name,

        entry.name,

        256

    );

    node->inode =

        entryIndex;

    node->size =

        entry.size;

    // Remember the actual NovaFS parent.

    node->fs_data =

        (void*)(uintptr_t)

        entry.parent;

    // ---------------------------------------------------------

    // Directory

    // ---------------------------------------------------------

    if (

        entry.type ==

        NOVAFS_ENTRY_DIRECTORY

    ) {

        node->flags =

            VFS_NODE_DIRECTORY;

        node->readdir =

            novafs_readdir;

        node->finddir =

            novafs_finddir;

        node->create =

            novafs_create;

    }

    // ---------------------------------------------------------

    // File

    // ---------------------------------------------------------

    else {

        node->flags =

            VFS_NODE_FILE;

        node->read =

            novafs_read;

        node->write =

            novafs_write;

        node->readdir =

            nullptr;

        node->finddir =

            nullptr;

        node->create =

            nullptr;

    }

    return node;

}

// =============================================================

// readdir callback

// =============================================================

static void readdir_callback(

    const NovaFSEntry& entry,

    uint32_t index

) {

    if (lookupState.result)

        return;

    if (

        lookupState.currentIndex ==

        lookupState.wantedIndex

    ) {

        lookupState.result =

            make_vnode(

                entry,

                index

            );

        return;

    }

    ++lookupState.currentIndex;

}

// =============================================================

// readdir

// =============================================================

static VNode* novafs_readdir(

    VNode* node,

    uint32_t index

) {

    if (!node)

        return nullptr;

    uint32_t parent;

    if (

        node == &rootNode

    ) {

        parent =

            NOVAFS_ROOT_PARENT;

    }

    else {

        parent =

            node->inode;

    }

    lookupState.parent =

        parent;

    lookupState.wantedIndex =

        index;

    lookupState.currentIndex =

        0;

    lookupState.result =

        nullptr;

    NovaFSDisk::list_directory(

        parent,

        readdir_callback

    );

    return lookupState.result;

}

// =============================================================

// finddir state

// =============================================================

struct FindDirState {

    const char* name;

    VNode* result;

};

static FindDirState findState;

// =============================================================

// String compare

// =============================================================

static bool same_text(

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

// =============================================================

// finddir callback

// =============================================================

static void finddir_callback(

    const NovaFSEntry& entry,

    uint32_t index

) {

    if (findState.result)

        return;

    if (

        same_text(

            entry.name,

            findState.name

        )

    ) {

        findState.result =

            make_vnode(

                entry,

                index

            );

    }

}

// =============================================================

// finddir

// =============================================================

static VNode* novafs_finddir(

    VNode* node,

    const char* name

) {

    if (!node || !name)

        return nullptr;

    uint32_t parent;

    if (

        node == &rootNode

    ) {

        parent =

            NOVAFS_ROOT_PARENT;

    }

    else {

        parent =

            node->inode;

    }

    findState.name =

        name;

    findState.result =

        nullptr;

    NovaFSDisk::list_directory(

        parent,

        finddir_callback

    );

    return findState.result;

}

// =============================================================

// Create file

// =============================================================

static VNode* novafs_create(

    VNode* directory,

    const char* name

) {

    if (!directory || !name)

        return nullptr;

    if (

        (directory->flags &

         VFS_NODE_DIRECTORY) == 0

    ) {

        return nullptr;

    }

    if (name[0] == '\0')

        return nullptr;

    // Don't allow duplicates.

    if (

        directory->finddir &&

        directory->finddir(

            directory,

            name

        )

    ) {

        return nullptr;

    }

    uint32_t parent;

    if (

        directory == &rootNode

    ) {

        parent =

            NOVAFS_ROOT_PARENT;

    }

    else {

        parent =

            directory->inode;

    }

    // Create an empty file.

    const char* emptyData = "";

    if (

        !NovaFSDisk::save_file_in(

            parent,

            name,

            emptyData,

            0

        )

    ) {

        return nullptr;

    }

    // Look it up again so we can return

    // its new VNode.

    return novafs_finddir(

        directory,

        name

    );

}

// =============================================================

// Root setup

// =============================================================

static void build_root_node() {

    clear_text(

        rootNode.name,

        256

    );

    copy_text(

        rootNode.name,

        "/",

        256

    );

    rootNode.inode =

        NOVAFS_ROOT_PARENT;

    rootNode.size =

        0;

    rootNode.flags =

        VFS_NODE_DIRECTORY;

    rootNode.uid = 0;

    rootNode.gid = 0;

    rootNode.fs_data =

        nullptr;

    rootNode.read =

        nullptr;

    rootNode.write =

        nullptr;

    rootNode.open =

        nullptr;

    rootNode.close =

        nullptr;

    rootNode.readdir =

        novafs_readdir;

    rootNode.finddir =

        novafs_finddir;

    rootNode.create =

        novafs_create;

}

// =============================================================

// Public interface

// =============================================================

VNode* NovaFSVFS::get_root() {

    return &rootNode;

}

bool NovaFSVFS::mount_root() {

    for (

        int i = 0;

        i < MAX_VNODES;

        ++i

    ) {

        nodeUsed[i] =

            false;

    }

    build_root_node();

    return VFS::mount(

        "/",

        &rootNode

    );

}
