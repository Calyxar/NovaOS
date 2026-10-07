/** NovaOS — User Account Management */

#include "user.h"
#include "../fs/vfs.h"

namespace User {

static constexpr const char* USER_DB_PATH = "/System/users.db";

static constexpr uint32_t USER_DB_MAGIC   = 0x4E555352; // "NUSR"
static constexpr uint32_t USER_DB_VERSION = 1;

struct UserDatabaseHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t next_uid;
};

static Account users[NOVA_MAX_USERS];
static size_t user_count = 0;
static uint32_t next_uid = 1000;


// ============================================================
// String helpers
// ============================================================

static size_t str_len(const char* str) {
    size_t len = 0;

    if (!str)
        return 0;

    while (str[len])
        ++len;

    return len;
}

static void str_copy(
    char* dest,
    const char* src,
    size_t max
) {
    if (!dest || max == 0)
        return;

    size_t i = 0;

    if (src) {
        while (src[i] && i < max - 1) {
            dest[i] = src[i];
            ++i;
        }
    }

    dest[i] = '\0';
}

static bool str_equal(
    const char* a,
    const char* b
) {
    if (!a || !b)
        return false;

    while (*a && *b) {
        if (*a != *b)
            return false;

        ++a;
        ++b;
    }

    return *a == *b;
}


// ============================================================
// Account helpers
// ============================================================

static void clear_accounts() {
    for (size_t i = 0; i < NOVA_MAX_USERS; ++i) {
        users[i].uid = 0;
        users[i].gid = 0;
        users[i].username[0] = '\0';
        users[i].home[0] = '\0';
        users[i].active = false;
    }

    user_count = 0;
    next_uid = 1000;
}

static bool valid_username(const char* username) {
    if (!username)
        return false;

    size_t len = str_len(username);

    if (len == 0 || len > NOVA_USERNAME_MAX)
        return false;

    for (size_t i = 0; i < len; ++i) {
        char c = username[i];

        bool valid =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' ||
            c == '-';

        if (!valid)
            return false;
    }

    return true;
}

static bool build_home_path(
    const char* username,
    char* output,
    size_t output_size
) {
    const char* prefix = "/Users/";

    size_t prefix_len = str_len(prefix);
    size_t username_len = str_len(username);

    if (prefix_len + username_len + 1 > output_size)
        return false;

    size_t pos = 0;

    for (size_t i = 0; prefix[i]; ++i)
        output[pos++] = prefix[i];

    for (size_t i = 0; username[i]; ++i)
        output[pos++] = username[i];

    output[pos] = '\0';

    return true;
}


// ============================================================
// Persistent database
// ============================================================

bool save() {
    uint8_t buffer[4096];

    UserDatabaseHeader header{};

    header.magic = USER_DB_MAGIC;
    header.version = USER_DB_VERSION;
    header.count = (uint32_t)user_count;
    header.next_uid = next_uid;

    size_t required =
        sizeof(UserDatabaseHeader) +
        user_count * sizeof(Account);

    if (required > sizeof(buffer))
        return false;

    // Copy header.
    uint8_t* headerBytes =
        reinterpret_cast<uint8_t*>(&header);

    for (size_t i = 0;
         i < sizeof(UserDatabaseHeader);
         ++i) {
        buffer[i] = headerBytes[i];
    }

    // Copy only accounts that actually exist.
    size_t offset =
        sizeof(UserDatabaseHeader);

    for (size_t accountIndex = 0;
         accountIndex < user_count;
         ++accountIndex) {

        uint8_t* accountBytes =
            reinterpret_cast<uint8_t*>(
                &users[accountIndex]
            );

        for (size_t i = 0;
             i < sizeof(Account);
             ++i) {

            buffer[offset++] =
                accountBytes[i];
        }
    }

    VNode* file =
        VFS::resolve(USER_DB_PATH);

    if (!file)
        file = VFS::create(USER_DB_PATH);

    if (!file)
        return false;

    int written =
        VFS::write(
            file,
            buffer,
            required,
            0
        );

    return written == (int)required;
}

bool load() {
    VNode* file =
        VFS::resolve(USER_DB_PATH);

    if (!file)
        return false;

    if (!VFS::is_file(file))
        return false;

    if (file->size < sizeof(UserDatabaseHeader))
        return false;

    if (file->size > 4096)
        return false;

    uint8_t buffer[4096];

    int bytes =
        VFS::read(
            file,
            buffer,
            file->size,
            0
        );

    if (bytes != (int)file->size)
        return false;

    UserDatabaseHeader header{};

    uint8_t* headerBytes =
        reinterpret_cast<uint8_t*>(&header);

    for (size_t i = 0;
         i < sizeof(UserDatabaseHeader);
         ++i) {

        headerBytes[i] =
            buffer[i];
    }

    if (header.magic != USER_DB_MAGIC)
        return false;

    if (header.version != USER_DB_VERSION)
        return false;

    if (header.count > NOVA_MAX_USERS)
        return false;

    if (header.next_uid < 1000)
        return false;

    size_t expectedSize =
        sizeof(UserDatabaseHeader) +
        header.count * sizeof(Account);

    if (expectedSize != file->size)
        return false;

    clear_accounts();

    user_count =
        header.count;

    next_uid =
        header.next_uid;

    size_t offset =
        sizeof(UserDatabaseHeader);

    for (size_t accountIndex = 0;
         accountIndex < user_count;
         ++accountIndex) {

        uint8_t* accountBytes =
            reinterpret_cast<uint8_t*>(
                &users[accountIndex]
            );

        for (size_t i = 0;
             i < sizeof(Account);
             ++i) {

            accountBytes[i] =
                buffer[offset++];
        }
    }

    return true;
}


// ============================================================
// Initialization
// ============================================================

void init() {
    clear_accounts();

    // The default NovaFS layout should already have /Users,
    // but keep this fallback for safety.
    if (!VFS::resolve("/Users")) {
        VFS::mkdir("/Users");
    }

    // Load persistent accounts if a database already exists.
   if (load()) {
    // Database successfully loaded.
   } else {
    // Database missing, unreadable, or invalid
   }
}


// ============================================================
// Lookup
// ============================================================

Account* find(const char* username) {
    if (!username)
        return nullptr;

    for (size_t i = 0; i < user_count; ++i) {
        if (
            users[i].active &&
            str_equal(users[i].username, username)
        ) {
            return &users[i];
        }
    }

    return nullptr;
}

Account* get(size_t index) {
    if (index >= user_count)
        return nullptr;

    if (!users[index].active)
        return nullptr;

    return &users[index];
}

Account* find_by_uid(uint32_t uid) {
    for (size_t i = 0; i < user_count; ++i) {
        if (
            users[i].active &&
            users[i].uid == uid
        ) {
            return &users[i];
        }
    }

    return nullptr;
}


// ============================================================
// Account creation
// ============================================================

bool create(const char* username) {
    if (!valid_username(username))
        return false;

    if (user_count >= NOVA_MAX_USERS)
        return false;

    if (find(username))
        return false;

    char home[NOVA_HOME_MAX];

    if (!build_home_path(
            username,
            home,
            sizeof(home)
        )) {
        return false;
    }

    if (!VFS::resolve("/Users")) {
        if (!VFS::mkdir("/Users"))
            return false;
    }

    /*
     * The home directory may already exist.
     *
     * This matters during migration from the temporary
     * account system, where /Users/Alyssa may already be
     * stored on NovaFS but users.db does not exist yet.
     */
    VNode* home_node = VFS::resolve(home);

    if (!home_node) {
        home_node = VFS::mkdir(home);

        if (!home_node)
            return false;
    }

    if (!VFS::is_directory(home_node))
        return false;

    Account* account = &users[user_count];

    account->uid = next_uid++;
    account->gid = account->uid;
    account->active = true;

    str_copy(
        account->username,
        username,
        sizeof(account->username)
    );

    str_copy(
        account->home,
        home,
        sizeof(account->home)
    );

    home_node->uid = account->uid;
    home_node->gid = account->gid;

    ++user_count;

    // Persist the updated account table.
    if (!save()) {
        // Roll back the in-memory account.
        --user_count;
        --next_uid;

        account->uid = 0;
        account->gid = 0;
        account->username[0] = '\0';
        account->home[0] = '\0';
        account->active = false;

        return false;
    }

    return true;
}

size_t count() {
    return user_count;
}

}