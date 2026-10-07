/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !nx info (prints NXRP version)
 */

#include "server/nxrp/nxrp_main.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include <sstream>
#include <vector>

// Version variable for NXRP. Update as needed.
const char *NXRP_VERSION = "0.1.0";

// Account storage handled by nxrp_accounts_sql.* (SQLite)

// Parse username and password from chatCursor. Accept either
// "user pass" or "subcmd user pass" (when full chatCursor is passed).
static bool parse_user_pass(const char *chatCursor, std::string &outUser, std::string &outPass) {
	if (!chatCursor) return false;
	std::istringstream iss(chatCursor);
	std::vector<std::string> toks;
	std::string w;
	while (iss >> w) toks.push_back(w);
	if (toks.size() == 2) {
		outUser = toks[0]; outPass = toks[1]; return true;
	}
	if (toks.size() >= 3) {
		// could be: "register user pass" or "login user pass"
		outUser = toks[1]; outPass = toks[2]; return true;
	}
	return false;
}

// Handler for: !nx info
qboolean SV_nxrp_HandleNxInfo( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] NXRP version: %s\"", NXRP_VERSION );
	return qtrue;
}

// Handler for: !nx register <user> <pass>
qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration is currently disabled\"" );
	return qtrue;
}

// Handler for: !nx login <user> <pass>
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login is currently disabled\"" );
	return qtrue;
}
