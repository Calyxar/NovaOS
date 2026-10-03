#include "files.h"

#include "../theme/colors.h"
#include "../core/rect.h"
#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/keyboard/keyboard.h"
#include "../../kernel/fs/vfs.h"
#include "../../kernel/fs/novafs_disk.h"

#include <stdint.h>

namespace {

constexpr int MAX_ENTRIES = 32;
constexpr int MAX_NAME = 64;
constexpr int MAX_PATH = 256;
constexpr uint32_t MAX_FILE_CONTENT = 4096;

// Framebuffer text uses eight pixels per glyph.
constexpr int EDITOR_X = 190;
constexpr int EDITOR_Y = 235;
constexpr int EDITOR_COLUMNS = 66;
constexpr int EDITOR_VISIBLE_ROWS = 17;
constexpr int EDITOR_ROW_HEIGHT = 20;

struct EntryCard {
    Rect bounds;
    char name[MAX_NAME];
    uint32_t size;
    bool directory;
    bool hovered;
};

EntryCard entries[MAX_ENTRIES];
int entryCount = 0;

Rect backButton = {190, 125, 42, 42};
Rect newFileButton = {620, 72, 110, 30};
Rect newFolderButton = {485, 72, 125, 30};

// The naming field is shown instead of directory cards while active.
bool namingFolder = false;
bool folderNameError = false;
char folderName[NOVAFS_MAX_NAME] = "";
int folderNameLength = 0;
bool newFolderHovered = false;
Rect saveButton = {620, 185, 110, 34};

bool backHovered = false;
bool newFileHovered = false;
bool saveHovered = false;

char currentPath[MAX_PATH] = "/";

bool viewingFile = false;
char openedFileName[MAX_NAME] = "";
char openedFileContent[MAX_FILE_CONTENT + 1] = "";
uint32_t openedFileSize = 0;
uint32_t editorCursor = 0; // Index between characters: [0, openedFileSize]
int editorScrollRow = 0;
int preferredColumn = -1;
bool fileDirty = false;
bool confirmDiscard = false;
bool saveFailed = false;

void copy_text(char* dst, const char* src, int max) {
    if (!dst || !src || max <= 0) return;
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

int text_length(const char* text) {
    if (!text) return 0;
    int length = 0;
    while (text[length]) ++length;
    return length;
}

bool same_text(const char* a, const char* b) {
    if (!a || !b) return false;
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

bool is_root_path() { return same_text(currentPath, "/"); }

void go_to_root() {
    currentPath[0] = '/';
    currentPath[1] = '\0';
}

void go_back() {
    if (is_root_path()) return;
    int length = text_length(currentPath);
    while (length > 1 && currentPath[length - 1] != '/') --length;
    if (length <= 1) {
        go_to_root();
        return;
    }
    currentPath[length - 1] = '\0';
}

bool append_path(const char* name) {
    if (!name) return false;
    int pathLength = text_length(currentPath);
    int nameLength = text_length(name);
    bool needSlash = !is_root_path();
    if (pathLength + nameLength + (needSlash ? 1 : 0) + 1 > MAX_PATH)
        return false;

    int position = pathLength;
    if (needSlash) currentPath[position++] = '/';
    for (int i = 0; i < nameLength; ++i) currentPath[position++] = name[i];
    currentPath[position] = '\0';
    return true;
}

bool build_file_path(const char* name, char* output, int max) {
    if (!name || !output || max <= 0) return false;
    int pathLength = text_length(currentPath);
    int nameLength = text_length(name);
    bool needSlash = !is_root_path();
    // Never silently truncate a path: that could target a different file.
    if (pathLength + nameLength + (needSlash ? 1 : 0) + 1 > max)
        return false;

    int position = 0;
    for (int i = 0; i < pathLength; ++i) output[position++] = currentPath[i];
    if (needSlash) output[position++] = '/';
    for (int i = 0; i < nameLength; ++i) output[position++] = name[i];
    output[position] = '\0';
    return true;
}

void clear_entries() {
    entryCount = 0;
    for (int i = 0; i < MAX_ENTRIES; ++i) {
        entries[i].bounds = {0, 0, 0, 0};
        entries[i].name[0] = '\0';
        entries[i].size = 0;
        entries[i].directory = false;
        entries[i].hovered = false;
    }
}

void layout_entries() {
    for (int i = 0; i < entryCount; ++i) {
        entries[i].bounds = {
            190 + (i % 3) * 170,
            190 + (i / 3) * 110,
            150,
            90
        };
    }
}

void load_directory() {
    clear_entries();
    VNode* directory = VFS::resolve(currentPath);
    if (!directory || !VFS::is_directory(directory)) return;

    for (uint32_t index = 0; index < MAX_ENTRIES; ++index) {
        VNode* node = VFS::readdir(directory, index);
        if (!node) break;
        EntryCard& entry = entries[entryCount];
        copy_text(entry.name, node->name, MAX_NAME);
        entry.size = node->size;
        entry.directory = VFS::is_directory(node);
        entry.hovered = false;
        ++entryCount;
    }
    layout_entries();
}

void draw_folder(EntryCard& entry) {
    uint32_t background = entry.hovered ? NovaColors::SurfaceHover
                                        : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(entry.bounds.x, entry.bounds.y,
                                   entry.bounds.width, entry.bounds.height,
                                   12, background);
    Framebuffer::draw_rect(entry.bounds.x + 20, entry.bounds.y + 12,
                           18, 8, NovaColors::Cyan);
    Framebuffer::draw_rect(entry.bounds.x + 16, entry.bounds.y + 18,
                           36, 24, NovaColors::Cyan);
    Framebuffer::print_at(entry.name, entry.bounds.x + 16,
                           entry.bounds.y + 56, NovaColors::TextPrimary);
}

void draw_file(EntryCard& entry) {
    uint32_t background = entry.hovered ? NovaColors::SurfaceHover
                                        : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(entry.bounds.x, entry.bounds.y,
                                   entry.bounds.width, entry.bounds.height,
                                   12, background);
    Framebuffer::draw_rect(entry.bounds.x + 16, entry.bounds.y + 16,
                           28, 34, NovaColors::Purple);
    Framebuffer::print_at(entry.name, entry.bounds.x + 16,
                           entry.bounds.y + 58, NovaColors::TextPrimary);
}

void draw_back_button() {
    uint32_t background = backHovered ? NovaColors::SurfaceHover
                                      : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(backButton.x, backButton.y,
                                   backButton.width, backButton.height,
                                   8, background);
    Framebuffer::print_at("<", backButton.x + 14, backButton.y + 12,
                           NovaColors::TextPrimary);
}

void draw_new_file_button() {
    uint32_t background = newFileHovered ? NovaColors::SurfaceHover
                                         : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(newFileButton.x, newFileButton.y,
                                   newFileButton.width, newFileButton.height,
                                   8, background);
    Framebuffer::print_at("+ New File", newFileButton.x + 12,
                           newFileButton.y + 8, NovaColors::TextPrimary);
}

void draw_new_folder_button() {
    uint32_t background = newFolderHovered ? NovaColors::SurfaceHover
                                            : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(newFolderButton.x, newFolderButton.y,
                                   newFolderButton.width, newFolderButton.height,
                                   8, background);
    Framebuffer::print_at("+ New Folder", newFolderButton.x + 8,
                          newFolderButton.y + 8, NovaColors::TextPrimary);
}

void draw_folder_prompt() {
    Framebuffer::print_at("Create folder", 190, 195, NovaColors::TextPrimary);
    Framebuffer::draw_rounded_rect(190, 225, 400, 38, 8,
                                   NovaColors::SurfaceRaised);
    Framebuffer::print_at(folderName, 203, 237, NovaColors::TextPrimary);
    Framebuffer::draw_rect(203 + folderNameLength * 8, 235, 2, 17,
                           NovaColors::Cyan);
    Framebuffer::print_at("Type name, Enter: create, Esc: cancel", 190, 282,
                          NovaColors::TextSecondary);
    if (folderNameError)
        Framebuffer::print_at("Cannot create folder (duplicate, full, or invalid).",
                              190, 310, NovaColors::TextPrimary);
    Framebuffer::print_at("Maximum 27 characters; names cannot contain / or \\.",
                          190, 339, NovaColors::TextMuted);
}

void draw_save_button() {
    uint32_t background = saveHovered ? NovaColors::SurfaceHover
                                      : NovaColors::SurfaceRaised;
    Framebuffer::draw_rounded_rect(saveButton.x, saveButton.y,
                                   saveButton.width, saveButton.height,
                                   8, background);
    Framebuffer::print_at(fileDirty ? "Save *" : "Save",
                           saveButton.x + 22, saveButton.y + 10,
                           NovaColors::TextPrimary);
}

void open_directory(const char* name) {
    if (!append_path(name)) return;
    VNode* node = VFS::resolve(currentPath);
    if (!node || !VFS::is_directory(node)) {
        go_back();
        return;
    }
    load_directory();
}

void create_new_file() {
    char fullPath[MAX_PATH];
    if (!build_file_path("NewFile.txt", fullPath, MAX_PATH)) return;
    VNode* file = VFS::create(fullPath);
    if (!file) return;
    load_directory();
}

void begin_new_folder() {
    namingFolder = true;
    folderNameError = false;
    folderNameLength = 0;
    folderName[0] = '\0';
    esc_pressed = false;
    // Clear any stale normal characters before entering a naming field.
    char ignored;
    while (Keyboard::try_getchar(&ignored)) {}
}

bool process_folder_keyboard() {
    if (!namingFolder) return false;
    bool changed = false;
    if (esc_pressed) {
        esc_pressed = false;
        namingFolder = false;
        folderNameError = false;
        return true;
    }
    char c;
    while (Keyboard::try_getchar(&c)) {
        if (c == '\b') {
            if (folderNameLength > 0) {
                folderName[--folderNameLength] = '\0';
                folderNameError = false;
                changed = true;
            }
            continue;
        }
        if (c == '\n') {
            if (folderNameLength == 0) {
                folderNameError = true;
                return true;
            }
            char fullPath[MAX_PATH];
            if (!build_file_path(folderName, fullPath, MAX_PATH) ||
                !VFS::mkdir(fullPath)) {
                folderNameError = true;
                return true;
            }
            namingFolder = false;
            folderNameError = false;
            load_directory();
            return true;
        }
        // Avoid forbidden path separators and unsupported control characters.
        if (c >= 32 && c <= 126 && c != '/' && c != '\\' &&
            folderNameLength < NOVAFS_MAX_NAME - 1) {
            folderName[folderNameLength++] = c;
            folderName[folderNameLength] = '\0';
            folderNameError = false;
            changed = true;
        }
    }
    return changed;
}

// Position is computed with the same wrap rules used by the renderer.
struct TextPosition {
    int row;
    int column;
};

TextPosition position_at(uint32_t index) {
    TextPosition p = {0, 0};
    if (index > openedFileSize) index = openedFileSize;
    for (uint32_t i = 0; i < index; ++i) {
        if (openedFileContent[i] == '\n') {
            ++p.row;
            p.column = 0;
        } else {
            ++p.column;
            if (p.column >= EDITOR_COLUMNS) {
                ++p.row;
                p.column = 0;
            }
        }
    }
    return p;
}

void ensure_cursor_visible() {
    TextPosition p = position_at(editorCursor);
    if (p.row < editorScrollRow) editorScrollRow = p.row;
    if (p.row >= editorScrollRow + EDITOR_VISIBLE_ROWS)
        editorScrollRow = p.row - EDITOR_VISIBLE_ROWS + 1;
    if (editorScrollRow < 0) editorScrollRow = 0;
}

void clear_keyboard_navigation() {
    left_pressed = false;
    right_pressed = false;
    up_pressed = false;
    down_pressed = false;
    home_pressed = false;
    end_pressed = false;
    delete_pressed = false;
    save_pressed = false;
}

void open_file(const char* name) {
    char fullPath[MAX_PATH];
    if (!build_file_path(name, fullPath, MAX_PATH)) return;
    VNode* file = VFS::open(fullPath, VFS_OPEN_READ);
    if (!file) return;
    if (!VFS::is_file(file)) {
        VFS::close(file);
        return;
    }

    int bytesRead = VFS::read(file, (uint8_t*)openedFileContent,
                              MAX_FILE_CONTENT, 0);
    VFS::close(file);
    if (bytesRead < 0) return;
    if (bytesRead > (int)MAX_FILE_CONTENT) bytesRead = MAX_FILE_CONTENT;

    openedFileContent[bytesRead] = '\0';
    openedFileSize = (uint32_t)bytesRead;
    copy_text(openedFileName, name, MAX_NAME);
    viewingFile = true;
    fileDirty = false;
    confirmDiscard = false;
    saveFailed = false;
    saveHovered = false;
    editorCursor = openedFileSize;
    editorScrollRow = 0;
    preferredColumn = -1;
    clear_keyboard_navigation();
    ensure_cursor_visible();
}

bool save_opened_file() {
    if (!viewingFile) return false;
    saveFailed = false;
    char fullPath[MAX_PATH];
    if (!build_file_path(openedFileName, fullPath, MAX_PATH)) {
        saveFailed = true;
        return false;
    }
    VNode* file = VFS::open(fullPath, VFS_OPEN_WRITE);
    if (!file) {
        saveFailed = true;
        return false;
    }
    if (!VFS::is_file(file)) {
        VFS::close(file);
        saveFailed = true;
        return false;
    }
    int written = VFS::write(file, (uint8_t*)openedFileContent,
                              openedFileSize, 0);
    VFS::close(file);
    if (written != (int)openedFileSize) {
        saveFailed = true;
        return false;
    }
    fileDirty = false;
    confirmDiscard = false;
    return true;
}

// Insertion and deletion work on the underlying buffer, not just its end.
bool insert_character(char c) {
    if (openedFileSize >= MAX_FILE_CONTENT) return false;
    for (uint32_t i = openedFileSize; i > editorCursor; --i)
        openedFileContent[i] = openedFileContent[i - 1];
    openedFileContent[editorCursor++] = c;
    ++openedFileSize;
    openedFileContent[openedFileSize] = '\0';
    return true;
}

bool remove_before_cursor() {
    if (editorCursor == 0) return false;
    for (uint32_t i = editorCursor - 1; i < openedFileSize; ++i)
        openedFileContent[i] = openedFileContent[i + 1];
    --editorCursor;
    --openedFileSize;
    return true;
}

bool remove_at_cursor() {
    if (editorCursor >= openedFileSize) return false;
    for (uint32_t i = editorCursor; i < openedFileSize; ++i)
        openedFileContent[i] = openedFileContent[i + 1];
    --openedFileSize;
    return true;
}

// Move to the nearest column in an adjacent visual (wrapped) row.
uint32_t index_on_row(int targetRow, int wantedColumn) {
    uint32_t bestIndex = editorCursor;
    int bestDifference = EDITOR_COLUMNS + 1;
    bool found = false;
    for (uint32_t i = 0; i <= openedFileSize; ++i) {
        TextPosition p = position_at(i);
        if (p.row != targetRow) continue;
        int diff = p.column - wantedColumn;
        if (diff < 0) diff = -diff;
        if (!found || diff < bestDifference) {
            bestDifference = diff;
            bestIndex = i;
            found = true;
            if (diff == 0) break;
        }
    }
    return bestIndex;
}

bool move_vertical(int direction) {
    TextPosition p = position_at(editorCursor);
    int target = p.row + direction;
    if (target < 0) return false;
    if (preferredColumn < 0) preferredColumn = p.column;
    uint32_t next = index_on_row(target, preferredColumn);
    if (position_at(next).row != target || next == editorCursor) return false;
    editorCursor = next;
    return true;
}

bool move_home() {
    TextPosition p = position_at(editorCursor);
    uint32_t next = index_on_row(p.row, 0);
    if (next == editorCursor) return false;
    editorCursor = next;
    return true;
}

bool move_end() {
    TextPosition p = position_at(editorCursor);
    uint32_t next = editorCursor;
    for (uint32_t i = editorCursor; i <= openedFileSize; ++i) {
        if (position_at(i).row != p.row) break;
        next = i;
    }
    if (next == editorCursor) return false;
    editorCursor = next;
    return true;
}

bool process_editor_keyboard() {
    if (!viewingFile) return false;
    bool changed = false;
    bool textChanged = false;
    char c;

    // Existing normal-character FIFO. Special-key flags are handled below.
    while (Keyboard::try_getchar(&c)) {
        bool didEdit = false;
        if (c == '\b') {
            didEdit = remove_before_cursor();
        } else if (c == '\t') {
            for (int i = 0; i < 4; ++i)
                didEdit = insert_character(' ') || didEdit;
        } else if (c == '\n' || (c >= 32 && c <= 126)) {
            didEdit = insert_character(c);
        }
        if (didEdit) {
            textChanged = true;
            changed = true;
            preferredColumn = -1;
        }
    }

    if (left_pressed) {
        left_pressed = false;
        if (editorCursor > 0) { --editorCursor; changed = true; }
        preferredColumn = -1;
    }
    if (right_pressed) {
        right_pressed = false;
        if (editorCursor < openedFileSize) { ++editorCursor; changed = true; }
        preferredColumn = -1;
    }
    if (up_pressed) {
        up_pressed = false;
        changed = move_vertical(-1) || changed;
    }
    if (down_pressed) {
        down_pressed = false;
        changed = move_vertical(+1) || changed;
    }
    if (home_pressed) {
        home_pressed = false;
        changed = move_home() || changed;
        preferredColumn = -1;
    }
    if (end_pressed) {
        end_pressed = false;
        changed = move_end() || changed;
        preferredColumn = -1;
    }
    if (delete_pressed) {
        delete_pressed = false;
        if (remove_at_cursor()) {
            textChanged = true;
            changed = true;
            preferredColumn = -1;
        }
    }
    if (textChanged) {
        fileDirty = true;
        confirmDiscard = false;
        saveFailed = false;
    }
    if (save_pressed) {
        save_pressed = false;
        save_opened_file();
        changed = true;
    }
    if (changed) ensure_cursor_visible();
    return changed;
}

// Renders only visible editor rows; caret follows editorCursor, not file end.
void draw_opened_file_content() {
    char line[EDITOR_COLUMNS + 1];
    int row = 0;
    int length = 0;
    for (uint32_t i = 0; i <= openedFileSize; ++i) {
        char c = (i == openedFileSize) ? '\0' : openedFileContent[i];
        if (c == '\n' || c == '\0') {
            line[length] = '\0';
            if (length > 0 && row >= editorScrollRow &&
                row < editorScrollRow + EDITOR_VISIBLE_ROWS) {
                Framebuffer::print_at(line, EDITOR_X,
                    EDITOR_Y + (row - editorScrollRow) * EDITOR_ROW_HEIGHT,
                    NovaColors::TextPrimary);
            }
            length = 0;
            if (c == '\0') break;
            ++row;
            continue;
        }

        line[length++] = c;
        if (length == EDITOR_COLUMNS) {
            line[length] = '\0';
            if (row >= editorScrollRow &&
                row < editorScrollRow + EDITOR_VISIBLE_ROWS) {
                Framebuffer::print_at(line, EDITOR_X,
                    EDITOR_Y + (row - editorScrollRow) * EDITOR_ROW_HEIGHT,
                    NovaColors::TextPrimary);
            }
            length = 0;
            ++row;
        }
    }

    TextPosition caret = position_at(editorCursor);
    if (caret.row >= editorScrollRow &&
        caret.row < editorScrollRow + EDITOR_VISIBLE_ROWS) {
        Framebuffer::draw_rect(
            EDITOR_X + caret.column * 8,
            EDITOR_Y + (caret.row - editorScrollRow) * EDITOR_ROW_HEIGHT,
            2, 14, NovaColors::TextPrimary);
    }

    if (confirmDiscard) {
        Framebuffer::print_at("Unsaved changes! Back again to discard.",
            190, 577, NovaColors::Cyan);
    } else if (saveFailed) {
        Framebuffer::print_at("Save failed - changes remain unsaved.",
            190, 577, NovaColors::TextPrimary);
    } else {
        Framebuffer::print_at("Arrows: move  Home/End: line  Ctrl+S: save",
            190, 577, NovaColors::TextMuted);
    }
}

void close_file() {
    viewingFile = false;
    openedFileName[0] = '\0';
    openedFileContent[0] = '\0';
    openedFileSize = 0;
    editorCursor = 0;
    editorScrollRow = 0;
    preferredColumn = -1;
    fileDirty = false;
    confirmDiscard = false;
    saveFailed = false;
    saveHovered = false;
    clear_keyboard_navigation();
    load_directory(); // Reflect a newly saved size in its entry card.
}

} // namespace

void FilesPage::init() {
    go_to_root();
    backHovered = false;
    newFileHovered = false;
    newFolderHovered = false;
    namingFolder = false;
    folderNameError = false;
    folderNameLength = 0;
    folderName[0] = '\0';
    saveHovered = false;
    viewingFile = false;
    openedFileName[0] = '\0';
    openedFileContent[0] = '\0';
    openedFileSize = 0;
    editorCursor = 0;
    editorScrollRow = 0;
    preferredColumn = -1;
    fileDirty = false;
    confirmDiscard = false;
    saveFailed = false;
    clear_keyboard_navigation();
    load_directory();
}

bool FilesPage::update() {
    // Desktop::run() must continue calling this EVERY loop on the Files page.
    if (namingFolder) return process_folder_keyboard();
    return process_editor_keyboard();
}

void FilesPage::draw() {
    Framebuffer::print_at("Files", 190, 72, NovaColors::TextPrimary);
    Framebuffer::print_at(viewingFile ? "Edit NovaFS file" : "Browse NovaFS",
                           190, 98, NovaColors::TextSecondary);
    draw_back_button();
    if (!viewingFile) {
        draw_new_file_button();
        draw_new_folder_button();
    }

    Framebuffer::draw_rounded_rect(245, 125, 485, 42, 8,
                                   NovaColors::SurfaceRaised);
    Framebuffer::print_at(currentPath, 262, 138, NovaColors::TextSecondary);

    if (viewingFile) {
        Framebuffer::print_at(openedFileName, 190, 190,
                               NovaColors::TextPrimary);
        draw_save_button();
        draw_opened_file_content();
        return;
    }

    if (namingFolder) {
        draw_folder_prompt();
        return;
    }

    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].directory) draw_folder(entries[i]);
        else draw_file(entries[i]);
    }
    if (entryCount == 0)
        Framebuffer::print_at("This folder is empty.", 190, 200,
                               NovaColors::TextMuted);
}

void FilesPage::handle_hover(int mouseX, int mouseY) {
    backHovered = backButton.contains(mouseX, mouseY);
    newFileHovered = !viewingFile && !namingFolder &&
                     newFileButton.contains(mouseX, mouseY);
    newFolderHovered = !viewingFile && !namingFolder &&
                       newFolderButton.contains(mouseX, mouseY);
    saveHovered = viewingFile && saveButton.contains(mouseX, mouseY);
    if (viewingFile || namingFolder) return;
    for (int i = 0; i < entryCount; ++i)
        entries[i].hovered = entries[i].bounds.contains(mouseX, mouseY);
}

void FilesPage::handle_click(int mouseX, int mouseY) {
    if (namingFolder) {
        if (backButton.contains(mouseX, mouseY)) {
            namingFolder = false;
            folderNameError = false;
        }
        return;
    }
    if (backButton.contains(mouseX, mouseY)) {
        if (viewingFile) {
            // Never silently throw away an unsaved editor buffer.
            if (fileDirty && !confirmDiscard) {
                confirmDiscard = true;
                return;
            }
            close_file();
            return;
        }
        go_back();
        load_directory();
        return;
    }

    if (viewingFile && saveButton.contains(mouseX, mouseY)) {
        save_opened_file();
        return;
    }
    if (!viewingFile && newFolderButton.contains(mouseX, mouseY)) {
        begin_new_folder();
        return;
    }
    if (!viewingFile && newFileButton.contains(mouseX, mouseY)) {
        create_new_file();
        return;
    }
    if (viewingFile) return;

    for (int i = 0; i < entryCount; ++i) {
        if (!entries[i].bounds.contains(mouseX, mouseY)) continue;
        if (entries[i].directory) open_directory(entries[i].name);
        else open_file(entries[i].name);
        return;
    }
}
