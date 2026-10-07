/** NovaOS — User Account Management */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define NOVA_MAX_USERS     16
#define NOVA_USERNAME_MAX  31
#define NOVA_HOME_MAX      256

namespace User {

struct Account {
    uint32_t uid;
    uint32_t gid;

    char username[NOVA_USERNAME_MAX + 1];
    char home[NOVA_HOME_MAX];

    bool active;
};

// Initialize subsystem and load persistent accounts.
void init();

// Create an account and save it to NovaFS.
bool create(const char* username);

// Account lookup.
Account* find(const char* username);
Account* find_by_uid(uint32_t uid);
Account* get(size_t index);

// Number of loaded accounts.
size_t count();

// Persistent account database.
bool load();
bool save();

}