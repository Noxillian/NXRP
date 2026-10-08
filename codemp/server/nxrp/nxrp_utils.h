#pragma once

#include <string>
#include "../server.h"
#include <cstdarg>
#include <cstdio>

// Ensure the client is logged in; if not, a message is sent and false is returned.
bool NXRP_EnsureLoggedIn(client_t* cl);

// Check whether the client's account has isAdmin=true;
bool NXRP_IsClientAdmin(client_t* cl);

// Helper: read raw account JSON content
bool NXRP_ReadAccountContentForClient(client_t* cl, std::string& outContent, std::string& outSafeUsername);

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
		SV_SendServerCommand(cl, "print \"^0[^6N^7X^0]^8 %s\n\"", buf);
	else
		SV_SendServerCommand(NULL, "print \"^0[^6N^7X^0]^8 %s\n\"", buf);
}

inline void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text) {
	NXRP_PrintConsoleToPlayerFmt(cl, "%s", text);
}
