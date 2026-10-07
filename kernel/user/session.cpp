/** NovaOS — User Session Management */

#include "session.h"

namespace Session {

static User::Account* current_user = nullptr;

void init() {
    current_user = nullptr;
}

bool login(const char* username) {
    if (!username)
        return false;

    User::Account* account = User::find(username);

    if (!account)
        return false;

    if (!account->active)
        return false;

    current_user = account;

    return true;
}

void logout() {
    current_user = nullptr;
}

bool is_logged_in() {
    return current_user != nullptr;
}

User::Account* current() {
    return current_user;
}

const char* username() {
    if (!current_user)
        return nullptr;

    return current_user->username;
}

const char* home() {
    if (!current_user)
        return nullptr;

    return current_user->home;
}

uint32_t uid() {
    if (!current_user)
        return 0;

    return current_user->uid;
}

}