#include "novafs_setup.h"
#include "novafs_disk.h"


void NovaFSSetup::create_default_layout() {

    // =========================================================
    // Root directories
    // =========================================================

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "System"
    );

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "Users"
    );

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "Apps"
    );

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "Documents"
    );

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "Downloads"
    );

    NovaFSDisk::create_directory(
        NOVAFS_ROOT_PARENT,
        "Temp"
    );


    // =========================================================
    // Root files
    // =========================================================

    const char* welcomeText =
        "Welcome to NovaOS!\n"
        "\n"
        "NovaFS and the VFS are now connected.\n"
        "This file is stored on the NovaFS disk.\n";

    NovaFSDisk::save_file(
        "Welcome.txt",
        welcomeText,
        102
    );


    // =========================================================
    // System
    // =========================================================

    int systemDir =
        NovaFSDisk::find_directory(
            NOVAFS_ROOT_PARENT,
            "System"
        );

    if (systemDir >= 0) {
        NovaFSDisk::create_directory(
            (uint32_t)systemDir,
            "Drivers"
        );

        NovaFSDisk::create_directory(
            (uint32_t)systemDir,
            "Config"
        );

        NovaFSDisk::create_directory(
            (uint32_t)systemDir,
            "Logs"
        );
    }


    // =========================================================
    // Documents
    // =========================================================

    int documentsDir =
        NovaFSDisk::find_directory(
            NOVAFS_ROOT_PARENT,
            "Documents"
        );

    if (documentsDir >= 0) {
        NovaFSDisk::create_directory(
            (uint32_t)documentsDir,
            "Projects"
        );

        NovaFSDisk::create_directory(
            (uint32_t)documentsDir,
            "Notes"
        );
    }
}