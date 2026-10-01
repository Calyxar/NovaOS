#include "files.h"

#include "../theme/colors.h"
#include "../core/rect.h"

#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/keyboard/keyboard.h"
#include "../../kernel/fs/vfs.h"


namespace {

    // =========================================================
    // Configuration
    // =========================================================

    constexpr int MAX_ENTRIES =
        32;

    constexpr int MAX_NAME =
        64;

    constexpr int MAX_PATH =
        256;

    constexpr int MAX_FILE_CONTENT =
        4096;


    // =========================================================
    // File / folder card
    // =========================================================

    struct EntryCard {
        Rect bounds;

        char name[MAX_NAME];

        uint32_t size;

        bool directory;
        bool hovered;
    };


    EntryCard entries[MAX_ENTRIES];

    int entryCount =
        0;


    // =========================================================
    // Navigation
    // =========================================================

    Rect backButton = {
        190,
        125,
        42,
        42
    };


    bool backHovered =
        false;


    Rect newFileButton = {
        620,
        72,
        110,
        30
    };


    bool newFileHovered =
        false;


    char currentPath[MAX_PATH] =
        "/";


    // =========================================================
    // File editor state
    // =========================================================

    bool viewingFile =
        false;


    char openedFileName[MAX_NAME] =
        "";


    char openedFileContent[
        MAX_FILE_CONTENT + 1
    ] = "";


    uint32_t openedFileSize =
        0;


    bool fileDirty =
        false;


    Rect saveButton = {
        620,
        185,
        110,
        34
    };


    bool saveHovered =
        false;


    // =========================================================
    // String helpers
    // =========================================================

    void copy_text(
        char* dst,
        const char* src,
        int max
    ) {
        if (
            !dst ||
            !src ||
            max <= 0
        ) {
            return;
        }


        int i =
            0;


        while (
            src[i] &&
            i < max - 1
        ) {
            dst[i] =
                src[i];

            ++i;
        }


        dst[i] =
            '\0';
    }


    int text_length(
        const char* text
    ) {
        if (!text)
            return 0;


        int length =
            0;


        while (
            text[length]
        ) {
            ++length;
        }


        return length;
    }


    bool same_text(
        const char* a,
        const char* b
    ) {
        if (
            !a ||
            !b
        ) {
            return false;
        }


        int i =
            0;


        while (
            a[i] &&
            b[i]
        ) {
            if (
                a[i] !=
                b[i]
            ) {
                return false;
            }


            ++i;
        }


        return
            a[i] == '\0' &&
            b[i] == '\0';
    }


    // =========================================================
    // Path helpers
    // =========================================================

    bool is_root_path() {
        return same_text(
            currentPath,
            "/"
        );
    }


    void go_to_root() {
        currentPath[0] =
            '/';

        currentPath[1] =
            '\0';
    }


    void go_back() {
        if (
            is_root_path()
        ) {
            return;
        }


        int length =
            text_length(
                currentPath
            );


        while (
            length > 1 &&
            currentPath[
                length - 1
            ] != '/'
        ) {
            --length;
        }


        if (
            length <= 1
        ) {
            go_to_root();

            return;
        }


        currentPath[
            length - 1
        ] =
            '\0';
    }


    bool append_path(
        const char* name
    ) {
        if (!name)
            return false;


        int pathLength =
            text_length(
                currentPath
            );


        int nameLength =
            text_length(
                name
            );


        bool needSlash =
            !is_root_path();


        int required =
            pathLength +
            nameLength +
            (
                needSlash
                    ? 1
                    : 0
            ) +
            1;


        if (
            required >
            MAX_PATH
        ) {
            return false;
        }


        int position =
            pathLength;


        if (needSlash) {
            currentPath[
                position++
            ] =
                '/';
        }


        for (
            int i = 0;
            i < nameLength;
            ++i
        ) {
            currentPath[
                position++
            ] =
                name[i];
        }


        currentPath[
            position
        ] =
            '\0';


        return true;
    }


    bool build_file_path(
        const char* name,
        char* output,
        int max
    ) {
        if (
            !name ||
            !output ||
            max <= 0
        ) {
            return false;
        }


        int position =
            0;


        int pathLength =
            text_length(
                currentPath
            );


        for (
            int i = 0;
            i < pathLength &&
            position < max - 1;
            ++i
        ) {
            output[
                position++
            ] =
                currentPath[i];
        }


        if (
            !is_root_path() &&
            position < max - 1
        ) {
            output[
                position++
            ] =
                '/';
        }


        int nameLength =
            text_length(
                name
            );


        for (
            int i = 0;
            i < nameLength &&
            position < max - 1;
            ++i
        ) {
            output[
                position++
            ] =
                name[i];
        }


        output[
            position
        ] =
            '\0';


        return true;
    }


    // =========================================================
    // Entry storage
    // =========================================================

    void clear_entries() {
        entryCount =
            0;


        for (
            int i = 0;
            i < MAX_ENTRIES;
            ++i
        ) {
            entries[i].bounds = {
                0,
                0,
                0,
                0
            };


            entries[i].name[0] =
                '\0';


            entries[i].size =
                0;


            entries[i].directory =
                false;


            entries[i].hovered =
                false;
        }
    }


    // =========================================================
    // Entry layout
    // =========================================================

    void layout_entries() {
        const int startX =
            190;

        const int startY =
            190;


        const int width =
            150;

        const int height =
            90;


        const int gapX =
            20;

        const int gapY =
            20;


        const int columns =
            3;


        for (
            int i = 0;
            i < entryCount;
            ++i
        ) {
            int column =
                i %
                columns;


            int row =
                i /
                columns;


            entries[i].bounds = {
                startX +
                    column *
                    (
                        width +
                        gapX
                    ),

                startY +
                    row *
                    (
                        height +
                        gapY
                    ),

                width,
                height
            };
        }
    }


    // =========================================================
    // Load directory
    // =========================================================

    void load_directory() {
        clear_entries();


        VNode* directory =
            VFS::resolve(
                currentPath
            );


        if (!directory)
            return;


        if (
            !VFS::is_directory(
                directory
            )
        ) {
            return;
        }


        for (
            uint32_t index = 0;
            index < MAX_ENTRIES;
            ++index
        ) {
            VNode* node =
                VFS::readdir(
                    directory,
                    index
                );


            if (!node)
                break;


            EntryCard& entry =
                entries[
                    entryCount
                ];


            copy_text(
                entry.name,
                node->name,
                MAX_NAME
            );


            entry.size =
                node->size;


            entry.directory =
                VFS::is_directory(
                    node
                );


            entry.hovered =
                false;


            ++entryCount;


            if (
                entryCount >=
                MAX_ENTRIES
            ) {
                break;
            }
        }


        layout_entries();
    }


    // =========================================================
    // Draw folder
    // =========================================================

    void draw_folder(
        EntryCard& entry
    ) {
        uint32_t background =
            entry.hovered
                ? NovaColors::SurfaceHover
                : NovaColors::SurfaceRaised;


        Framebuffer::draw_rounded_rect(
            entry.bounds.x,
            entry.bounds.y,
            entry.bounds.width,
            entry.bounds.height,
            12,
            background
        );


        Framebuffer::draw_rect(
            entry.bounds.x + 20,
            entry.bounds.y + 12,
            18,
            8,
            NovaColors::Cyan
        );


        Framebuffer::draw_rect(
            entry.bounds.x + 16,
            entry.bounds.y + 18,
            36,
            24,
            NovaColors::Cyan
        );


        Framebuffer::print_at(
            entry.name,
            entry.bounds.x + 16,
            entry.bounds.y + 56,
            NovaColors::TextPrimary
        );
    }


    // =========================================================
    // Draw file
    // =========================================================

    void draw_file(
        EntryCard& entry
    ) {
        uint32_t background =
            entry.hovered
                ? NovaColors::SurfaceHover
                : NovaColors::SurfaceRaised;


        Framebuffer::draw_rounded_rect(
            entry.bounds.x,
            entry.bounds.y,
            entry.bounds.width,
            entry.bounds.height,
            12,
            background
        );


        Framebuffer::draw_rect(
            entry.bounds.x + 16,
            entry.bounds.y + 16,
            28,
            34,
            NovaColors::Purple
        );


        Framebuffer::print_at(
            entry.name,
            entry.bounds.x + 16,
            entry.bounds.y + 58,
            NovaColors::TextPrimary
        );
    }


    // =========================================================
    // Draw entry
    // =========================================================

    void draw_entry(
        EntryCard& entry
    ) {
        if (
            entry.directory
        ) {
            draw_folder(
                entry
            );
        }

        else {
            draw_file(
                entry
            );
        }
    }


    // =========================================================
    // Back button
    // =========================================================

    void draw_back_button() {
        uint32_t background =
            backHovered
                ? NovaColors::SurfaceHover
                : NovaColors::SurfaceRaised;


        Framebuffer::draw_rounded_rect(
            backButton.x,
            backButton.y,
            backButton.width,
            backButton.height,
            8,
            background
        );


        Framebuffer::print_at(
            "<",
            backButton.x + 14,
            backButton.y + 12,
            NovaColors::TextPrimary
        );
    }


    // =========================================================
    // New File button
    // =========================================================

    void draw_new_file_button() {
        uint32_t background =
            newFileHovered
                ? NovaColors::SurfaceHover
                : NovaColors::SurfaceRaised;


        Framebuffer::draw_rounded_rect(
            newFileButton.x,
            newFileButton.y,
            newFileButton.width,
            newFileButton.height,
            8,
            background
        );


        Framebuffer::print_at(
            "+ New File",
            newFileButton.x + 12,
            newFileButton.y + 8,
            NovaColors::TextPrimary
        );
    }


    // =========================================================
    // Save button
    // =========================================================

    void draw_save_button() {
        uint32_t background =
            saveHovered
                ? NovaColors::SurfaceHover
                : NovaColors::SurfaceRaised;


        Framebuffer::draw_rounded_rect(
            saveButton.x,
            saveButton.y,
            saveButton.width,
            saveButton.height,
            8,
            background
        );


        Framebuffer::print_at(
            fileDirty
                ? "Save *"
                : "Save",
            saveButton.x + 22,
            saveButton.y + 10,
            NovaColors::TextPrimary
        );
    }


    // =========================================================
    // Open directory
    // =========================================================

    void open_directory(
        const char* name
    ) {
        if (
            !append_path(
                name
            )
        ) {
            return;
        }


        VNode* node =
            VFS::resolve(
                currentPath
            );


        if (
            !node ||
            !VFS::is_directory(
                node
            )
        ) {
            go_back();

            return;
        }


        load_directory();
    }


    // =========================================================
    // Create new file
    // =========================================================

    void create_new_file() {
        char fullPath[
            MAX_PATH
        ];


        if (
            !build_file_path(
                "NewFile.txt",
                fullPath,
                MAX_PATH
            )
        ) {
            return;
        }


        VNode* file =
            VFS::create(
                fullPath
            );


        if (!file)
            return;


        load_directory();
    }


    // =========================================================
    // Open file
    // =========================================================

    void open_file(
        const char* name
    ) {
        char fullPath[
            MAX_PATH
        ];


        if (
            !build_file_path(
                name,
                fullPath,
                MAX_PATH
            )
        ) {
            return;
        }


        VNode* file =
            VFS::open(
                fullPath,
                VFS_OPEN_READ
            );


        if (!file)
            return;


        if (
            !VFS::is_file(
                file
            )
        ) {
            VFS::close(
                file
            );

            return;
        }


        int bytesRead =
            VFS::read(
                file,
                (uint8_t*)
                    openedFileContent,
                MAX_FILE_CONTENT,
                0
            );


        VFS::close(
            file
        );


        if (
            bytesRead < 0
        ) {
            return;
        }


        if (
            bytesRead >
            MAX_FILE_CONTENT
        ) {
            bytesRead =
                MAX_FILE_CONTENT;
        }


        openedFileContent[
            bytesRead
        ] =
            '\0';


        openedFileSize =
            (uint32_t)
            bytesRead;


        copy_text(
            openedFileName,
            name,
            MAX_NAME
        );


        viewingFile =
            true;


        fileDirty =
            false;


        saveHovered =
            false;
    }


    // =========================================================
    // Save file
    // =========================================================

    bool save_opened_file() {
        if (
            !viewingFile
        ) {
            return false;
        }


        char fullPath[
            MAX_PATH
        ];


        if (
            !build_file_path(
                openedFileName,
                fullPath,
                MAX_PATH
            )
        ) {
            return false;
        }


        VNode* file =
            VFS::open(
                fullPath,
                VFS_OPEN_WRITE
            );


        if (!file)
            return false;


        if (
            !VFS::is_file(
                file
            )
        ) {
            VFS::close(
                file
            );

            return false;
        }


        int written =
            VFS::write(
                file,
                (uint8_t*)
                    openedFileContent,
                openedFileSize,
                0
            );


        VFS::close(
            file
        );


        if (
            written !=
            (int)
            openedFileSize
        ) {
            return false;
        }


        fileDirty =
            false;


        return true;
    }


    // =========================================================
    // Keyboard editor update
    // =========================================================
    //
    // Returns true if the text changed and a redraw is needed.
    //
    // IMPORTANT:
    // This is now called from FilesPage::update(), not draw().
    // =========================================================

    bool process_editor_keyboard() {
        if (
            !viewingFile
        ) {
            return false;
        }


        bool changed =
            false;


        char c;


        while (
            Keyboard::try_getchar(
                &c
            )
        ) {

            // -------------------------------------------------
            // Backspace
            // -------------------------------------------------

            if (
                c == '\b'
            ) {
                if (
                    openedFileSize >
                    0
                ) {
                    --openedFileSize;


                    openedFileContent[
                        openedFileSize
                    ] =
                        '\0';


                    fileDirty =
                        true;


                    changed =
                        true;
                }


                continue;
            }


            // -------------------------------------------------
            // Tab
            // -------------------------------------------------

            if (
                c == '\t'
            ) {
                bool added =
                    false;


                for (
                    int i = 0;
                    i < 4;
                    ++i
                ) {
                    if (
                        openedFileSize >=
                        MAX_FILE_CONTENT
                    ) {
                        break;
                    }


                    openedFileContent[
                        openedFileSize++
                    ] =
                        ' ';


                    added =
                        true;
                }


                openedFileContent[
                    openedFileSize
                ] =
                    '\0';


                if (added) {
                    fileDirty =
                        true;


                    changed =
                        true;
                }


                continue;
            }


            // -------------------------------------------------
            // Enter
            // -------------------------------------------------

            if (
                c == '\n'
            ) {
                if (
                    openedFileSize <
                    MAX_FILE_CONTENT
                ) {
                    openedFileContent[
                        openedFileSize++
                    ] =
                        '\n';


                    openedFileContent[
                        openedFileSize
                    ] =
                        '\0';


                    fileDirty =
                        true;


                    changed =
                        true;
                }


                continue;
            }


            // -------------------------------------------------
            // Printable ASCII
            // -------------------------------------------------

            if (
                c >= 32 &&
                c <= 126
            ) {
                if (
                    openedFileSize <
                    MAX_FILE_CONTENT
                ) {
                    openedFileContent[
                        openedFileSize++
                    ] =
                        c;


                    openedFileContent[
                        openedFileSize
                    ] =
                        '\0';


                    fileDirty =
                        true;


                    changed =
                        true;
                }
            }
        }


        return changed;
    }


    // =========================================================
    // Draw opened file/editor
    // =========================================================

    void draw_opened_file_content() {
        char line[80];


        int lineIndex =
            0;


        int x =
            190;

        int y =
            235;


        int cursorX =
            190;

        int cursorY =
            235;


        for (
            uint32_t i = 0;
            i <= openedFileSize;
            ++i
        ) {
            char c =
                openedFileContent[i];


            // -------------------------------------------------
            // End line / wrap / end file
            // -------------------------------------------------

            if (
                c == '\n' ||
                c == '\0' ||
                lineIndex >= 78
            ) {
                line[
                    lineIndex
                ] =
                    '\0';


                if (
                    lineIndex >
                    0
                ) {
                    Framebuffer::print_at(
                        line,
                        x,
                        y,
                        NovaColors::TextPrimary
                    );
                }


                cursorX =
                    x +
                    lineIndex *
                    8;


                cursorY =
                    y;


                if (
                    c == '\0'
                ) {
                    break;
                }


                y +=
                    20;


                lineIndex =
                    0;


                cursorX =
                    x;


                cursorY =
                    y;


                if (
                    c == '\n'
                ) {
                    continue;
                }
            }


            // -------------------------------------------------
            // Add character
            // -------------------------------------------------

            line[
                lineIndex++
            ] =
                c;


            cursorX =
                x +
                lineIndex *
                8;


            cursorY =
                y;
        }


        // -----------------------------------------------------
        // Cursor
        // -----------------------------------------------------

        Framebuffer::draw_rect(
            cursorX,
            cursorY,
            2,
            14,
            NovaColors::TextPrimary
        );
    }

}


// =============================================================
// Initialization
// =============================================================

void FilesPage::init() {
    go_to_root();


    backHovered =
        false;


    newFileHovered =
        false;


    saveHovered =
        false;


    viewingFile =
        false;


    openedFileName[0] =
        '\0';


    openedFileContent[0] =
        '\0';


    openedFileSize =
        0;


    fileDirty =
        false;


    load_directory();
}


// =============================================================
// Update
// =============================================================
//
// Keyboard input is handled here instead of draw().
//
// Desktop::run() should call this every loop while the Files
// page is active.
//
// Returns true when the page needs to be redrawn.
// =============================================================

bool FilesPage::update() {
    return
        process_editor_keyboard();
}


// =============================================================
// Drawing
// =============================================================

void FilesPage::draw() {

    // ---------------------------------------------------------
    // Header
    // ---------------------------------------------------------

    Framebuffer::print_at(
        "Files",
        190,
        72,
        NovaColors::TextPrimary
    );


    Framebuffer::print_at(
        viewingFile
            ? "Edit NovaFS file"
            : "Browse NovaFS",
        190,
        98,
        NovaColors::TextSecondary
    );


    // ---------------------------------------------------------
    // Back
    // ---------------------------------------------------------

    draw_back_button();


    // ---------------------------------------------------------
    // New File
    // ---------------------------------------------------------

    if (
        !viewingFile
    ) {
        draw_new_file_button();
    }


    // ---------------------------------------------------------
    // Path bar
    // ---------------------------------------------------------

    Framebuffer::draw_rounded_rect(
        245,
        125,
        485,
        42,
        8,
        NovaColors::SurfaceRaised
    );


    Framebuffer::print_at(
        currentPath,
        262,
        138,
        NovaColors::TextSecondary
    );


    // ---------------------------------------------------------
    // Editor
    // ---------------------------------------------------------

    if (
        viewingFile
    ) {
        Framebuffer::print_at(
            openedFileName,
            190,
            190,
            NovaColors::TextPrimary
        );


        draw_save_button();


        draw_opened_file_content();


        return;
    }


    // ---------------------------------------------------------
    // Directory entries
    // ---------------------------------------------------------

    for (
        int i = 0;
        i < entryCount;
        ++i
    ) {
        draw_entry(
            entries[i]
        );
    }


    // ---------------------------------------------------------
    // Empty folder
    // ---------------------------------------------------------

    if (
        entryCount ==
        0
    ) {
        Framebuffer::print_at(
            "This folder is empty.",
            190,
            200,
            NovaColors::TextMuted
        );
    }
}


// =============================================================
// Mouse hover
// =============================================================

void FilesPage::handle_hover(
    int mouseX,
    int mouseY
) {
    backHovered =
        backButton.contains(
            mouseX,
            mouseY
        );


    newFileHovered =
        !viewingFile &&
        newFileButton.contains(
            mouseX,
            mouseY
        );


    saveHovered =
        viewingFile &&
        saveButton.contains(
            mouseX,
            mouseY
        );


    if (
        viewingFile
    ) {
        return;
    }


    for (
        int i = 0;
        i < entryCount;
        ++i
    ) {
        entries[i].hovered =
            entries[i].bounds.contains(
                mouseX,
                mouseY
            );
    }
}


// =============================================================
// Mouse click
// =============================================================

void FilesPage::handle_click(
    int mouseX,
    int mouseY
) {

    // ---------------------------------------------------------
    // Back
    // ---------------------------------------------------------

    if (
        backButton.contains(
            mouseX,
            mouseY
        )
    ) {
        if (
            viewingFile
        ) {
            viewingFile =
                false;


            openedFileName[0] =
                '\0';


            openedFileContent[0] =
                '\0';


            openedFileSize =
                0;


            fileDirty =
                false;


            saveHovered =
                false;


            return;
        }


        go_back();


        load_directory();


        return;
    }


    // ---------------------------------------------------------
    // Save
    // ---------------------------------------------------------

    if (
        viewingFile &&
        saveButton.contains(
            mouseX,
            mouseY
        )
    ) {
        save_opened_file();

        return;
    }


    // ---------------------------------------------------------
    // New File
    // ---------------------------------------------------------

    if (
        !viewingFile &&
        newFileButton.contains(
            mouseX,
            mouseY
        )
    ) {
        create_new_file();

        return;
    }


    // ---------------------------------------------------------
    // No file cards while editing
    // ---------------------------------------------------------

    if (
        viewingFile
    ) {
        return;
    }


    // ---------------------------------------------------------
    // File / directory cards
    // ---------------------------------------------------------

    for (
        int i = 0;
        i < entryCount;
        ++i
    ) {
        if (
            !entries[i].bounds.contains(
                mouseX,
                mouseY
            )
        ) {
            continue;
        }


        if (
            entries[i].directory
        ) {
            open_directory(
                entries[i].name
            );

            return;
        }


        open_file(
            entries[i].name
        );


        return;
    }
}