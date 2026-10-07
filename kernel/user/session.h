/** NovaOS — User Session Management */
#pragma once

#include "user.h"

namespace Session {

// Initialize session state.
void init();

// Log into an existing account.
bool login(const char* username);

// End the current session.
void logout();

// Is somebody currently logged in?
bool is_logged_in();

// Return the current account.
// Returns nullptr when logged out.
User::Account* current();

// Convenience helpers.
const char* username();
const char* home();
uint32_t uid();

}