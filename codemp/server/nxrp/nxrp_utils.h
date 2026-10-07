#pragma once

#include <string>
#include "server.h"

// Ensure the client is logged in; if not, a message is sent and false is returned.
bool NXRP_EnsureLoggedIn(client_t* cl);

// Check whether the client's account has isAdmin=true; sends failure messages and
// returns false when the client isn't logged in, the account file is missing or
// malformed, or the isAdmin flag is false. On success returns true.
bool NXRP_IsClientAdmin(client_t* cl);

// Helper: read raw account JSON content for the given stored username (already
// present on the client). Returns true on success and fills outContent and
// outSafeUsername (sanitized) on success; on failure sends an appropriate
// message to the client and returns false.
bool NXRP_ReadAccountContentForClient(client_t* cl, std::string& outContent, std::string& outSafeUsername);
