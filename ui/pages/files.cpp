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

// Directory card viewport. Three rows fit fully inside 800x600.
constexpr int FILE_COLUMNS = 3;
constexpr int FILE_VISIBLE_ROWS = 3;
constexpr int FILE_CARD_START_X = 190;
constexpr int FILE_CARD_START_Y = 190;
constexpr int FILE_CARD_WIDTH = 150;
constexpr int FILE_CARD_HEIGHT = 90;
constexpr int FILE_CARD_GAP_X = 20;
constexpr int FILE_CARD_GAP_Y = 20;
constexpr int FILE_ROW_STEP = FILE_CARD_HEIGHT + FILE_CARD_GAP_Y;

struct EntryCard {
    Rect bounds;
    char name[MAX_NAME];
    uint32_t size;
    bool directory;
    bool hovered;
};

EntryCard entries[MAX_ENTRIES];
int entryCount = 0;

int directoryScrollRow = 0;
Rect scrollUpButton = {700, 200, 30, 30};
Rect scrollDownButton = {700, 460, 30, 30};
bool scrollUpHovered = false;
bool scrollDownHovered = false;

Rect backButton = {190, 125, 42, 42};
Rect newFileButton = {620, 72, 110, 30};
Rect newFolderButton = {485, 72, 125, 30};

// The naming field is shown instead of directory cards while active.
bool namingFolder = false;
bool renamingEntry = false;
char renameOriginal[MAX_NAME] = "";
bool folderNameError = false;

// Delete confirmation modal. NovaFS only permits files and empty folders.
bool deletingEntry = false;
char deleteName[MAX_NAME] = "";
bool deleteIsDirectory = false;
bool deleteError = false;
Rect deleteConfirmButton = {330, 315, 120, 34};
Rect deleteCancelButton = {470, 315, 120, 34};
bool deleteConfirmHovered = false;
bool deleteCancelHovered = false;
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
        int absoluteRow = i / FILE_COLUMNS;
        int visibleRow = absoluteRow - directoryScrollRow;
        entries[i].bounds = {
            FILE_CARD_START_X + (i % FILE_COLUMNS) *
                (FILE_CARD_WIDTH + FILE_CARD_GAP_X),
            FILE_CARD_START_Y + visibleRow * FILE_ROW_STEP,
            FILE_CARD_WIDTH,
            FILE_CARD_HEIGHT
        };
    }
}

int directory_row_count() {
    if (entryCount <= 0) return 0;
    return (entryCount + FILE_COLUMNS - 1) / FILE_COLUMNS;
}

int max_directory_scroll_row() {
    int rows = directory_row_count();
    int maxRow = rows - FILE_VISIBLE_ROWS;
    return maxRow > 0 ? maxRow : 0;
}

bool directory_can_scroll_up() {
    return directoryScrollRow > 0;
}

bool directory_can_scroll_down() {
    return directoryScrollRow < max_directory_scroll_row();
}

bool entry_is_visible(int index) {
    if (index < 0 || index >= entryCount) return false;
    int row = index / FILE_COLUMNS;
    return row >= directoryScrollRow &&
           row < directoryScrollRow + FILE_VISIBLE_ROWS;
}

bool scroll_directory(int delta) {
    int next = directoryScrollRow + delta;
    if (next < 0) next = 0;
    int maxRow = max_directory_scroll_row();
    if (next > maxRow) next = maxRow;
    if (next == directoryScrollRow) return false;

    directoryScrollRow = next;
    layout_entries();
    return true;
}

void reset_directory_scroll() {
    directoryScrollRow = 0;
    layout_entries();
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
    directoryScrollRow = 0;
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
    Framebuffer::print_at("D", entry.bounds.x + entry.bounds.width - 44,
                           entry.bounds.y + 12, NovaColors::TextSecondary);
    Framebuffer::print_at("R", entry.bounds.x + entry.bounds.width - 22,
                           entry.bounds.y + 12, NovaColors::TextSecondary);
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
    Framebuffer::print_at("D", entry.bounds.x + entry.bounds.width - 44,
                           entry.bounds.y + 12, NovaColors::TextSecondary);
    Framebuffer::print_at("R", entry.bounds.x + entry.bounds.width - 22,
                           entry.bounds.y + 12, NovaColors::TextSecondary);
}

void draw_scroll_controls() {
    if (max_directory_scroll_row() <= 0) return;

    uint32_t upBackground = scrollUpHovered
        ? NovaColors::SurfaceHover : NovaColors::SurfaceRaised;
    uint32_t downBackground = scrollDownHovered
        ? NovaColors::SurfaceHover : NovaColors::SurfaceRaised;

    Framebuffer::draw_rounded_rect(
        scrollUpButton.x, scrollUpButton.y,
        scrollUpButton.width, scrollUpButton.height, 7, upBackground);
    Framebuffer::draw_rounded_rect(
        scrollDownButton.x, scrollDownButton.y,
        scrollDownButton.width, scrollDownButton.height, 7, downBackground);

    Framebuffer::print_at("^", scrollUpButton.x + 10, scrollUpButton.y + 8,
                          directory_can_scroll_up()
                              ? NovaColors::TextPrimary
                              : NovaColors::TextMuted);
    Framebuffer::print_at("v", scrollDownButton.x + 10, scrollDownButton.y + 8,
                          directory_can_scroll_down()
                              ? NovaColors::TextPrimary
                              : NovaColors::TextMuted);
}

void draw_directory_scroll_status() {
    if (entryCount <= FILE_COLUMNS * FILE_VISIBLE_ROWS) return;

    int first = directoryScrollRow * FILE_COLUMNS + 1;
    int last = first + FILE_COLUMNS * FILE_VISIBLE_ROWS - 1;
    if (last > entryCount) last = entryCount;

    char status[32];
    int pos = 0;

    auto append_number = [&](int value) {
        char temp[12];
        int count = 0;
        if (value == 0) temp[count++] = '0';
        while (value > 0 && count < 11) {
            temp[count++] = (char)('0' + (value % 10));
            value /= 10;
        }
        while (count > 0 && pos < 31) status[pos++] = temp[--count];
    };

    append_number(first);
    if (pos < 31) status[pos++] = '-';
    append_number(last);
    if (pos < 31) status[pos++] = ' ';
    if (pos < 31) status[pos++] = 'o';
    if (pos < 31) status[pos++] = 'f';
    if (pos < 31) status[pos++] = ' ';
    append_number(entryCount);
    status[pos] = '\0';

    Framebuffer::print_at(status, 190, 560, NovaColors::TextMuted);
    Framebuffer::print_at("Up/Down: scroll", 560, 560, NovaColors::TextMuted);
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
    Framebuffer::print_at(renamingEntry ? "Rename item" : "Create folder",
                           190, 195, NovaColors::TextPrimary);
    Framebuffer::draw_rounded_rect(190, 225, 400, 38, 8,
                                   NovaColors::SurfaceRaised);
    Framebuffer::print_at(folderName, 203, 237, NovaColors::TextPrimary);
    Framebuffer::draw_rect(203 + folderNameLength * 8, 235, 2, 17,
                           NovaColors::Cyan);
    Framebuffer::print_at(renamingEntry ? "Enter: rename, Esc: cancel" : "Type name, Enter: create, Esc: cancel", 190, 282,
                          NovaColors::TextSecondary);
    if (folderNameError)
        Framebuffer::print_at(renamingEntry ? "Rename failed (duplicate or invalid name)." :
                                            "Cannot create folder (duplicate, full, or invalid).",
                              190, 310, NovaColors::TextPrimary);
    Framebuffer::print_at("Maximum 27 characters; names cannot contain / or \\.",
                          190, 339, NovaColors::TextMuted);
}

void draw_delete_prompt() {
    Framebuffer::print_at("Delete item?", 190, 205, NovaColors::TextPrimary);
    Framebuffer::print_at(deleteName, 190, 235, NovaColors::Cyan);
    Framebuffer::print_at(deleteIsDirectory ? "Folder must be empty before it can be deleted."
                                           : "This file will be permanently removed.",
                           190, 265, NovaColors::TextSecondary);

    uint32_t deleteBg = deleteConfirmHovered ? NovaColors::SurfaceHover
                                             : NovaColors::SurfaceRaised;
    uint32_t cancelBg = deleteCancelHovered ? NovaColors::SurfaceHover
                                            : NovaColors::SurfaceRaised;

    Framebuffer::draw_rounded_rect(deleteConfirmButton.x, deleteConfirmButton.y,
                                   deleteConfirmButton.width, deleteConfirmButton.height,
                                   8, deleteBg);
    Framebuffer::print_at("Delete", deleteConfirmButton.x + 28,
                           deleteConfirmButton.y + 10, NovaColors::TextPrimary);

    Framebuffer::draw_rounded_rect(deleteCancelButton.x, deleteCancelButton.y,
                                   deleteCancelButton.width, deleteCancelButton.height,
                                   8, cancelBg);
    Framebuffer::print_at("Cancel", deleteCancelButton.x + 28,
                           deleteCancelButton.y + 10, NovaColors::TextPrimary);

    Framebuffer::print_at("Enter: delete   Esc: cancel", 190, 375,
                           NovaColors::TextMuted);
    if (deleteError)
        Framebuffer::print_at("Delete failed. Non-empty folders cannot be deleted.",
                               190, 405, NovaColors::TextPrimary);
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
    renamingEntry = false;
    folderNameError = false;
    folderNameLength = 0;
    folderName[0] = '\0';
    esc_pressed = false;
    // Clear any stale normal characters before entering a naming field.
    char ignored;
    while (Keyboard::try_getchar(&ignored)) {}
}

void begin_rename(const char* oldName) {
    if (!oldName) return;
    copy_text(renameOriginal, oldName, MAX_NAME);
    copy_text(folderName, oldName, NOVAFS_MAX_NAME);
    folderNameLength = text_length(folderName);
    folderNameError = false;
    renamingEntry = true;
    namingFolder = false;
    esc_pressed = false;
    char ignored;
    while (Keyboard::try_getchar(&ignored)) {}
}

void cancel_delete() {
    deletingEntry = false;
    deleteName[0] = '\0';
    deleteIsDirectory = false;
    deleteError = false;
    deleteConfirmHovered = false;
    deleteCancelHovered = false;
}

void begin_delete(const char* name, bool directory) {
    if (!name) return;
    copy_text(deleteName, name, MAX_NAME);
    deleteIsDirectory = directory;
    deleteError = false;
    deletingEntry = true;
    deleteConfirmHovered = false;
    deleteCancelHovered = false;
    esc_pressed = false;
    char ignored;
    while (Keyboard::try_getchar(&ignored)) {}
}

bool execute_delete() {
    char fullPath[MAX_PATH];
    if (!build_file_path(deleteName, fullPath, MAX_PATH)) {
        deleteError = true;
        return false;
    }
    if (!VFS::remove(fullPath)) {
        deleteError = true;
        return false;
    }
    cancel_delete();
    load_directory();
    return true;
}

bool process_delete_keyboard() {
    if (!deletingEntry) return false;
    if (esc_pressed) {
        esc_pressed = false;
        cancel_delete();
        return true;
    }
    char c;
    while (Keyboard::try_getchar(&c)) {
        if (c == '\n') {
            execute_delete();
            return true;
        }
    }
    return false;
}

bool process_folder_keyboard() {
    if (!namingFolder && !renamingEntry) return false;
    bool changed = false;
    if (esc_pressed) {
        esc_pressed = false;
        namingFolder = false;
        renamingEntry = false;
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
            bool success = false;
            if (renamingEntry) {
                if (build_file_path(renameOriginal, fullPath, MAX_PATH))
                    success = VFS::rename(fullPath, folderName);
            } else {
                if (build_file_path(folderName, fullPath, MAX_PATH))
                    success = VFS::mkdir(fullPath) != nullptr;
            }
            if (!success) {
                folderNameError = true;
                return true;
            }
            namingFolder = false;
            renamingEntry = false;
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

bool process_directory_keyboard() {
    if (viewingFile || namingFolder || renamingEntry || deletingEntry)
        return false;

    bool changed = false;

    // Ordinary characters have no meaning while browsing; discard them so
    // they cannot leak into a file editor or naming field opened later.
    char ignored;
    while (Keyboard::try_getchar(&ignored)) {}

    if (up_pressed) {
        up_pressed = false;
        changed = scroll_directory(-1) || changed;
    }

    if (down_pressed) {
        down_pressed = false;
        changed = scroll_directory(+1) || changed;
    }

    if (home_pressed) {
        home_pressed = false;
        if (directoryScrollRow != 0) {
            directoryScrollRow = 0;
            layout_entries();
            changed = true;
        }
    }

    if (end_pressed) {
        end_pressed = false;
        int last = max_directory_scroll_row();
        if (directoryScrollRow != last) {
            directoryScrollRow = last;
            layout_entries();
            changed = true;
        }
    }

    // These special-key flags are editor-only here; clear stale presses.
    left_pressed = false;
    right_pressed = false;
    delete_pressed = false;
    save_pressed = false;

    return changed;
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
    directoryScrollRow = 0;
    scrollUpHovered = false;
    scrollDownHovered = false;
    backHovered = false;
    newFileHovered = false;
    newFolderHovered = false;
    namingFolder = false;
    renamingEntry = false;
    folderNameError = false;
    deletingEntry = false;
    deleteName[0] = '\0';
    deleteIsDirectory = false;
    deleteError = false;
    deleteConfirmHovered = false;
    deleteCancelHovered = false;
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
    if (deletingEntry) return process_delete_keyboard();
    if (namingFolder || renamingEntry) return process_folder_keyboard();
    if (viewingFile) return process_editor_keyboard();
    return process_directory_keyboard();
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

    if (deletingEntry) {
        draw_delete_prompt();
        return;
    }

    if (namingFolder || renamingEntry) {
        draw_folder_prompt();
        return;
    }

    for (int i = 0; i < entryCount; ++i) {
        if (!entry_is_visible(i)) continue;
        if (entries[i].directory) draw_folder(entries[i]);
        else draw_file(entries[i]);
    }

    draw_scroll_controls();
    draw_directory_scroll_status();

    if (entryCount == 0)
        Framebuffer::print_at("This folder is empty.", 190, 200,
                               NovaColors::TextMuted);
}

void FilesPage::handle_hover(int mouseX, int mouseY) {
    backHovered = backButton.contains(mouseX, mouseY);
    newFileHovered = !viewingFile && !namingFolder && !renamingEntry &&
                     newFileButton.contains(mouseX, mouseY);
    newFolderHovered = !viewingFile && !namingFolder && !renamingEntry &&
                       newFolderButton.contains(mouseX, mouseY);
    saveHovered = viewingFile && saveButton.contains(mouseX, mouseY);
    deleteConfirmHovered = deletingEntry && deleteConfirmButton.contains(mouseX, mouseY);
    deleteCancelHovered = deletingEntry && deleteCancelButton.contains(mouseX, mouseY);
    scrollUpHovered = !viewingFile && !namingFolder && !renamingEntry &&
                      !deletingEntry && directory_can_scroll_up() &&
                      scrollUpButton.contains(mouseX, mouseY);
    scrollDownHovered = !viewingFile && !namingFolder && !renamingEntry &&
                        !deletingEntry && directory_can_scroll_down() &&
                        scrollDownButton.contains(mouseX, mouseY);
    if (viewingFile || namingFolder || renamingEntry || deletingEntry) return;
    for (int i = 0; i < entryCount; ++i)
        entries[i].hovered = entry_is_visible(i) &&
                             entries[i].bounds.contains(mouseX, mouseY);
}

void FilesPage::handle_click(int mouseX, int mouseY) {
    if (deletingEntry) {
        if (deleteConfirmButton.contains(mouseX, mouseY)) {
            execute_delete();
            return;
        }
        if (deleteCancelButton.contains(mouseX, mouseY) ||
            backButton.contains(mouseX, mouseY)) {
            cancel_delete();
            return;
        }
        return;
    }

    if (namingFolder || renamingEntry) {
        if (backButton.contains(mouseX, mouseY)) {
            namingFolder = false;
            renamingEntry = false;
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

    if (scrollUpButton.contains(mouseX, mouseY)) {
        scroll_directory(-1);
        return;
    }
    if (scrollDownButton.contains(mouseX, mouseY)) {
        scroll_directory(+1);
        return;
    }

    for (int i = 0; i < entryCount; ++i) {
        if (!entry_is_visible(i)) continue;
        if (!entries[i].bounds.contains(mouseX, mouseY)) continue;
        // D and R controls live in the top-right of each card.
        if (mouseY < entries[i].bounds.y + 32) {
            int controlX = mouseX - entries[i].bounds.x;
            if (controlX >= entries[i].bounds.width - 54 &&
                controlX < entries[i].bounds.width - 32) {
                begin_delete(entries[i].name, entries[i].directory);
                return;
            }
            if (controlX >= entries[i].bounds.width - 32) {
                begin_rename(entries[i].name);
                return;
            }
        }
        if (entries[i].directory) open_directory(entries[i].name);
        else open_file(entries[i].name);
        return;
    }
}
