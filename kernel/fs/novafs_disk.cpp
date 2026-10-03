#include "novafs_disk.h"



#include "../drivers/disk/ata.h"





// =============================================================

// NovaFS state

// =============================================================



static NovaFSSuperblock sb;



static NovaFSEntry entries[NOVAFS_MAX_FILES];



static bool mounted = false;





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

// Superblock

// =============================================================



static void write_superblock() {

    uint8_t buf[512];



    for (int i = 0; i < 512; ++i)

        buf[i] = 0;



    NovaFSSuperblock* disk_sb =

        (NovaFSSuperblock*)buf;



    *disk_sb = sb;



    ATA::write_sector(

        0,

        buf

    );

}





// =============================================================

// File table

// =============================================================

//

// Sector 0:

//     superblock

//

// Sectors 1 - 8:

//     NovaFS entry table

//

// Sector 9+:

//     file contents

//

// =============================================================



static void read_file_table() {

    uint8_t buf[512];



    const int entriesPerSector =

        512 / sizeof(NovaFSEntry);



    int index = 0;



    for (

        int sectorOffset = 0;

        sectorOffset < 8 &&

        index < NOVAFS_MAX_FILES;

        ++sectorOffset

    ) {

        ATA::read_sector(

            1 + sectorOffset,

            buf

        );



        for (

            int i = 0;

            i < entriesPerSector &&

            index < NOVAFS_MAX_FILES;

            ++i,

            ++index

        ) {

            NovaFSEntry* diskEntry =

                (NovaFSEntry*)(

                    buf +

                    i * sizeof(NovaFSEntry)

                );



            entries[index] =

                *diskEntry;

        }

    }

}





static void write_file_table() {

    uint8_t buf[512];



    const int entriesPerSector =

        512 / sizeof(NovaFSEntry);



    int index = 0;



    for (

        int sectorOffset = 0;

        sectorOffset < 8 &&

        index < NOVAFS_MAX_FILES;

        ++sectorOffset

    ) {

        for (int i = 0; i < 512; ++i)

            buf[i] = 0;



        for (

            int i = 0;

            i < entriesPerSector &&

            index < NOVAFS_MAX_FILES;

            ++i,

            ++index

        ) {

            NovaFSEntry* diskEntry =

                (NovaFSEntry*)(

                    buf +

                    i * sizeof(NovaFSEntry)

                );



            *diskEntry =

                entries[index];

        }



        ATA::write_sector(

            1 + sectorOffset,

            buf

        );

    }

}





// =============================================================

// Entry helpers

// =============================================================



static int find_free_slot() {

    for (

        int i = 0;

        i < NOVAFS_MAX_FILES;

        ++i

    ) {

        if (

            entries[i].name[0] ==

            '\0'

        ) {

            return i;

        }

    }



    return -1;

}





static bool valid_parent(

    uint32_t parent

) {

    if (

        parent ==

        NOVAFS_ROOT_PARENT

    ) {

        return true;

    }



    if (

        parent >=

        NOVAFS_MAX_FILES

    ) {

        return false;

    }



    if (

        entries[parent].name[0] ==

        '\0'

    ) {

        return false;

    }



    return

        entries[parent].type ==

        NOVAFS_ENTRY_DIRECTORY;

}





static int find_entry(

    uint32_t parent,

    const char* name

) {

    for (

        int i = 0;

        i < NOVAFS_MAX_FILES;

        ++i

    ) {

        if (

            entries[i].name[0] &&

            entries[i].parent == parent &&

            streq(

                entries[i].name,

                name

            )

        ) {

            return i;

        }

    }



    return -1;

}





static int find_file_entry(

    uint32_t parent,

    const char* name

) {

    int index =

        find_entry(

            parent,

            name

        );



    if (index < 0)

        return -1;



    if (

        entries[index].type !=

        NOVAFS_ENTRY_FILE

    ) {

        return -1;

    }



    return index;

}





// =============================================================

// Mount / initialization

// =============================================================



void NovaFSDisk::init() {

    uint8_t buf[512];



    ATA::read_sector(

        0,

        buf

    );



    NovaFSSuperblock* found =

        (NovaFSSuperblock*)buf;





    // Require both correct magic and version.

    //

    // NovaFS v1 used a different entry layout,

    // so it must not be loaded as v2.



    if (

        found->magic == NOVAFS_MAGIC &&

        found->version == NOVAFS_VERSION

    ) {

        sb = *found;



        read_file_table();



        mounted = true;



        return;

    }





    // No valid NovaFS v2 filesystem.

    format();

}





// =============================================================

// Format

// =============================================================



bool NovaFSDisk::format() {

    sb.magic =

        NOVAFS_MAGIC;



    sb.entry_count =

        0;



    sb.version =

        NOVAFS_VERSION;



    sb.reserved =

        0;





    for (

        int i = 0;

        i < NOVAFS_MAX_FILES;

        ++i

    ) {

        entries[i].name[0] =

            '\0';



        entries[i].size =

            0;



        entries[i].start_sector =

            0;



        entries[i].parent =

            NOVAFS_ROOT_PARENT;



        entries[i].type =

            0;

    }





    mounted = true;





    write_superblock();



    write_file_table();





    return true;

}





// =============================================================

// Directories

// =============================================================



bool NovaFSDisk::create_directory(

    uint32_t parent,

    const char* name

) {

    if (!mounted)

        return false;



    if (!name || !name[0])

        return false;

    // Name must fit without truncation; no slash or dot segments.
    int nameLength = 0;
    for (; name[nameLength] && nameLength < NOVAFS_MAX_NAME; ++nameLength) {
        if (name[nameLength] == '/' || name[nameLength] == '\\') return false;
    }
    if (nameLength == 0 || nameLength >= NOVAFS_MAX_NAME) return false;
    if ((nameLength == 1 && name[0] == '.') ||
        (nameLength == 2 && name[0] == '.' && name[1] == '.')) return false;



    if (!valid_parent(parent))

        return false;





    // Don't allow duplicate names in the same directory.



    if (

        find_entry(

            parent,

            name

        ) >= 0

    ) {

        return false;

    }





    int index =

        find_free_slot();



    if (index < 0)

        return false;





    strcopy(

        entries[index].name,

        name,

        NOVAFS_MAX_NAME

    );





    entries[index].size =

        0;



    entries[index].start_sector =

        0;



    entries[index].parent =

        parent;



    entries[index].type =

        NOVAFS_ENTRY_DIRECTORY;





    ++sb.entry_count;





    sync();





    return true;

}





int NovaFSDisk::find_directory(

    uint32_t parent,

    const char* name

) {

    int index =

        find_entry(

            parent,

            name

        );



    if (index < 0)

        return -1;



    if (

        entries[index].type !=

        NOVAFS_ENTRY_DIRECTORY

    ) {

        return -1;

    }



    return index;

}





void NovaFSDisk::list_directory(

    uint32_t parent,

    void (*cb)(

        const NovaFSEntry& entry,

        uint32_t index

    )

) {

    if (!mounted || !cb)

        return;



    for (

        uint32_t i = 0;

        i < NOVAFS_MAX_FILES;

        ++i

    ) {

        if (

            entries[i].name[0] &&

            entries[i].parent ==

                parent

        ) {

            cb(

                entries[i],

                i

            );

        }

    }

}





// =============================================================

// Save file

// =============================================================



bool NovaFSDisk::save_file(

    const char* name,

    const char* data,

    uint32_t size

) {

    return save_file_in(

        NOVAFS_ROOT_PARENT,

        name,

        data,

        size

    );

}





bool NovaFSDisk::save_file_in(

    uint32_t parent,

    const char* name,

    const char* data,

    uint32_t size

) {

    if (!mounted)

        return false;



    if (!name || !name[0])

        return false;

    int nameLength = 0;
    for (; name[nameLength] && nameLength < NOVAFS_MAX_NAME; ++nameLength) {
        if (name[nameLength] == '/' || name[nameLength] == '\\') return false;
    }
    if (nameLength == 0 || nameLength >= NOVAFS_MAX_NAME) return false;
    if ((nameLength == 1 && name[0] == '.') ||
        (nameLength == 2 && name[0] == '.' && name[1] == '.')) return false;



    if (!valid_parent(parent))

        return false;



    if (

        size >

        NOVAFS_MAX_FILE_SECTORS *

        512

    ) {

        return false;

    }





    int index =

        find_entry(

            parent,

            name

        );





    if (index >= 0) {

        // A directory with this name already

        // exists, so it cannot become a file.



        if (

            entries[index].type !=

            NOVAFS_ENTRY_FILE

        ) {

            return false;

        }

    }



    else {

        index =

            find_free_slot();



        if (index < 0)

            return false;





        strcopy(

            entries[index].name,

            name,

            NOVAFS_MAX_NAME

        );





        entries[index].size =

            0;





        entries[index].start_sector =

            NOVAFS_DATA_START_SECTOR +

            index *

            NOVAFS_MAX_FILE_SECTORS;





        entries[index].parent =

            parent;





        entries[index].type =

            NOVAFS_ENTRY_FILE;





        ++sb.entry_count;

    }





    // Assign the file's reserved data area, including legacy empty files
    // which may have start_sector == 0 (superblock).
    entries[index].start_sector =
        NOVAFS_DATA_START_SECTOR +
        (uint32_t)index * NOVAFS_MAX_FILE_SECTORS;

    entries[index].size = size;

    uint32_t sector = entries[index].start_sector;



    uint32_t remaining =

        size;



    uint32_t offset =

        0;





    uint8_t buf[512];





    // Write at least one sector even for

    // an empty file.



    do {

        for (

            int i = 0;

            i < 512;

            ++i

        ) {

            buf[i] = 0;

        }





        uint32_t chunk =

            remaining > 512

                ? 512

                : remaining;





        for (

            uint32_t i = 0;

            i < chunk;

            ++i

        ) {

            buf[i] =

                (uint8_t)

                data[offset + i];

        }





        ATA::write_sector(

            sector,

            buf

        );





        ++sector;



        offset +=

            chunk;



        remaining -=

            chunk;



    } while (

        remaining > 0

    );





    sync();





    return true;

}





// =============================================================

// Load file

// =============================================================



bool NovaFSDisk::load_file(

    const char* name,

    char* out_buf,

    uint32_t max_size,

    uint32_t* out_size

) {

    return load_file_in(

        NOVAFS_ROOT_PARENT,

        name,

        out_buf,

        max_size,

        out_size

    );

}





bool NovaFSDisk::load_file_in(

    uint32_t parent,

    const char* name,

    char* out_buf,

    uint32_t max_size,

    uint32_t* out_size

) {

    if (!mounted)

        return false;



    if (!out_buf || !out_size)

        return false;



    if (max_size == 0)

        return false;





    int index =

        find_file_entry(

            parent,

            name

        );



    if (index < 0)

        return false;





    uint32_t size =

        entries[index].size;





    // Keep one byte free for '\0'.



    if (

        size >=

        max_size

    ) {

        size =

            max_size - 1;

    }





    *out_size =

        size;





    uint32_t sector =

        entries[index].start_sector;



    uint32_t remaining =

        size;



    uint32_t offset =

        0;





    uint8_t buf[512];





    while (

        remaining > 0

    ) {

        ATA::read_sector(

            sector,

            buf

        );





        uint32_t chunk =

            remaining > 512

                ? 512

                : remaining;





        for (

            uint32_t i = 0;

            i < chunk;

            ++i

        ) {

            out_buf[offset + i] =

                (char)buf[i];

        }





        ++sector;



        offset +=

            chunk;



        remaining -=

            chunk;

    }





    out_buf[size] =

        '\0';





    return true;

}





// =============================================================

// Existing root-file listing

// =============================================================



void NovaFSDisk::list_files(

    void (*cb)(

        const char* name,

        uint32_t size

    )

) {

    if (!mounted || !cb)

        return;





    for (

        int i = 0;

        i < NOVAFS_MAX_FILES;

        ++i

    ) {

        if (

            entries[i].name[0] &&

            entries[i].parent ==

                NOVAFS_ROOT_PARENT &&

            entries[i].type ==

                NOVAFS_ENTRY_FILE

        ) {

            cb(

                entries[i].name,

                entries[i].size

            );

        }

    }

}





// =============================================================

// Delete root file

// =============================================================



// Rename metadata in place: file data sectors, entry index and all
// child parent indices remain unchanged.
bool NovaFSDisk::rename_entry(uint32_t index, const char* new_name) {
    if (!mounted || index >= NOVAFS_MAX_FILES || !new_name || !new_name[0])
        return false;
    if (!entries[index].name[0]) return false;

    int length = 0;
    for (; new_name[length] && length < NOVAFS_MAX_NAME; ++length) {
        char c = new_name[length];
        if (c == '/' || c == '\\' || c < 32 || c > 126) return false;
    }
    if (length == 0 || length >= NOVAFS_MAX_NAME) return false;
    if ((length == 1 && new_name[0] == '.') ||
        (length == 2 && new_name[0] == '.' && new_name[1] == '.'))
        return false;

    // Renaming to the same name is a successful no-op.
    if (streq(entries[index].name, new_name)) return true;
    if (find_entry(entries[index].parent, new_name) >= 0) return false;

    strcopy(entries[index].name, new_name, NOVAFS_MAX_NAME);
    sync();
    return true;
}

bool NovaFSDisk::delete_file(

    const char* name

) {

    if (!mounted)

        return false;





    int index =

        find_file_entry(

            NOVAFS_ROOT_PARENT,

            name

        );





    if (index < 0)

        return false;





    return delete_entry(

        (uint32_t)index

    );

}





// =============================================================

// Delete entry

// =============================================================



bool NovaFSDisk::delete_entry(

    uint32_t index

) {

    if (!mounted)

        return false;





    if (

        index >=

        NOVAFS_MAX_FILES

    ) {

        return false;

    }





    if (

        entries[index].name[0] ==

        '\0'

    ) {

        return false;

    }





    // Don't delete a directory if it

    // still contains files/folders.



    if (

        entries[index].type ==

        NOVAFS_ENTRY_DIRECTORY

    ) {

        for (

            int i = 0;

            i < NOVAFS_MAX_FILES;

            ++i

        ) {

            if (

                entries[i].name[0] &&

                entries[i].parent ==

                    index

            ) {

                return false;

            }

        }

    }





    entries[index].name[0] =

        '\0';



    entries[index].size =

        0;



    entries[index].start_sector =

        0;



    entries[index].parent =

        NOVAFS_ROOT_PARENT;



    entries[index].type =

        0;





    if (

        sb.entry_count > 0

    ) {

        --sb.entry_count;

    }





    sync();





    return true;

}





// =============================================================

// Synchronize metadata

// =============================================================



void NovaFSDisk::sync() {

    if (!mounted)

        return;





    write_file_table();



    write_superblock();

}