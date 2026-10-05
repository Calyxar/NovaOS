#pragma once

#include "vfs.h"

namespace NovaFSVFS {
    bool mount_root();
    VNode* get_root();
}
