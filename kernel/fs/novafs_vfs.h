#pragma once

#include "vfs.h"

namespace NovaFSVFS {

    // Build the NovaFS VNode tree and mount it at "/".
    bool mount_root();

    // Return the root VNode for NovaFS.
    VNode* get_root();
}