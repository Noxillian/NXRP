#include "server/nxrp/nxrp_main.h"
#include "server/sv_gameapi.h"
#include <string>
#include <fstream>
#include <cctype>

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Simple admin commands for NXRP

// Handler: !nx giveall
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	// Check login + account isAdmin flag
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"" );
		return qtrue;
	}

	// sanitize stored username to derive filename
	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" );
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" );
		return qtrue;
	}

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for isAdmin: look for "isAdmin" then a true/false token
	const std::string key = "\"isAdmin\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"" );
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
	bool isAdmin = false;
	if (content.compare(pos, 4, "true") == 0) isAdmin = true;
	else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
	else {
		// tolerate quoted booleans
		if (content[pos] == '"') {
			size_t qend = content.find('"', pos + 1);
			if (qend != std::string::npos) {
				std::string tok = content.substr(pos + 1, qend - (pos + 1));
				if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
			}
		}
	}

	if (!isAdmin) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"" );
		return qtrue;
	}
	// Notify the invoking client that they have been given everything
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given everything\\n\"" );
	// give the E11 weapon (deferred so it runs on the next frame like other
	// Give all weapons using the server's wannagiveweaponsall helper (deferred)
	// This uses the same deferred executor spin.cpp provides so sv_cheats is
	// temporarily enabled when the command runs and the give commands succeed.
	int clientNum = (int)(cl - svs.clients);
	char cmdBuf[128];
	Com_sprintf(cmdBuf, sizeof(cmdBuf), "wannagiveweaponsall %d", clientNum);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	// run again shortly after in case the player was not yet spawned/alive
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 2);
	// Grant lightning force power via the wannaforce helper (deferred)
	Com_sprintf(cmdBuf, sizeof(cmdBuf), "wannaforce %d %d", clientNum, FP_LIGHTNING);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 2);
	return qtrue;
}
