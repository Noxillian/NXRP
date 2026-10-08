#pragma once

#include <string>
#include "../server.h"
#include <cstdarg>
#include <cstdio>

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

// Print a standardized NXRP console message to a specific player. If cl is
// NULL the message will be broadcast to all clients by passing NULL to
// SV_SendServerCommand.
// Print a standardized NXRP console message to a specific player. If cl is
// NULL the message will be broadcast to all clients by passing NULL to
// SV_SendServerCommand.
inline void NXRP_PrintConsoleToPlayerFmt(client_t* cl, const char* fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (cl)
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] %s\n\"", buf);
	else
		SV_SendServerCommand(NULL, "print \"^5[^6N^7X^5] %s\n\"", buf);
}

// Convenience wrapper for simple literal messages.
inline void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text) {
	NXRP_PrintConsoleToPlayerFmt(cl, "%s", text);
}
