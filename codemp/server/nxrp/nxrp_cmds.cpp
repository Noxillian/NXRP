/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !nx info (prints NXRP version)
 */

#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"

#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#if defined(_WIN32)
	#include <direct.h>
#endif
#include <algorithm>

// Version variable for NXRP. Update as needed.
const char *NXRP_VERSION = "0.2.0";

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

// Handler for: !nxrp pos
qboolean NXRP_HandleNxrpPos( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!cl || cl->state != CS_ACTIVE) {
		NXRP_PrintConsoleToPlayer(cl, "You must be an active player to use this command");
		return qtrue;
	}
	if (!cl->gentity) {
		NXRP_PrintConsoleToPlayer(cl, "No game entity available for your client");
		return qtrue;
	}

	// Use the entity's current origin
	vec3_t org;
	VectorCopy(cl->gentity->r.currentOrigin, org);

	NXRP_PrintConsoleToPlayerFmt(cl, "Position: %.2f, %.2f, %.2f", org[0], org[1], org[2]);
	return qtrue;
}
	if (toks.size() >= 3) {
		// could be: "register user pass" or "login user pass"
		outUser = toks[1]; outPass = toks[2]; return true;
	}
	return false;
}
	if (toks.size() >= 3) {
		// could be: "register user pass" or "login user pass"
		outUser = toks[1]; outPass = toks[2]; return true;
	}
	return false;
}

// Handler for: !nx info
qboolean NXRP_HandleNxInfo( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	NXRP_PrintConsoleToPlayer(cl, va("NXRP version: %s", NXRP_VERSION));
	return qtrue;
}

// Handler for: !nx register <user> <pass>
qboolean NXRP_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx register <user> <pass>");
		return qtrue;
	}

	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid username");
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	#if defined(_WIN32)
		_mkdir(dir.c_str());
	#else
		mkdir(dir.c_str(), 0755);
	#endif

	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (ifs.good()) {
		NXRP_PrintConsoleToPlayer(cl, "Username already exists");
		return qtrue;
	}

	auto escape = [](const std::string &s){
		std::string out; out.reserve(s.size()*2);
		for (char c : s) {
			if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
			else if (c == '\n') { out += "\\n"; }
			else out.push_back(c);
		}
		return out;
	};

	std::ofstream ofs(filepath, std::ios::trunc);
	if (!ofs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "Registration failed: cannot create account file");
		return qtrue;
	}
	ofs << "{\n";
	ofs << "  \"username\": \"" << escape(user) << "\",\n";
	ofs << "  \"password\": \"" << escape(pass) << "\",\n";
	ofs << "  \"level\": 1,\n";
	ofs << "  \"exp\": 0,\n";
	ofs << "  \"credits\": 0,\n";
	ofs << "  \"isAdmin\": false\n";
	ofs << "}\n";

	ofs.close();

	NXRP_PrintConsoleToPlayer(cl, "Registration successful");
	return qtrue;
}

// Handler for: !nx login <user> <pass>
qboolean NXRP_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx login <user> <pass>");
		return qtrue;
	}

	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid username");
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "Login failed: unknown user");
		return qtrue;
	}

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for password field: "password": "..."
	std::string key = "\"password\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		NXRP_PrintConsoleToPlayer(cl, "Login failed");
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { NXRP_PrintConsoleToPlayer(cl, "Login failed"); return qtrue; }
	size_t quote = content.find('"', colon);
	if (quote == std::string::npos) { NXRP_PrintConsoleToPlayer(cl, "Login failed"); return qtrue; }
	size_t qend = content.find('"', quote + 1);
	if (qend == std::string::npos) { NXRP_PrintConsoleToPlayer(cl, "Login failed"); return qtrue; }
	std::string stored = content.substr(quote + 1, qend - (quote + 1));

	if (stored == pass) {
		NXRP_PrintConsoleToPlayer(cl, "Login successful");
		// mark client as logged in by storing username in their session info
		if (cl && cl->state == CS_ACTIVE) {
			Q_strncpyz(cl->nxrp_username, user.c_str(), sizeof(cl->nxrp_username));
		}
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Login failed: incorrect password");
	}
	return qtrue;
}

// Handler for: !nx account
qboolean NXRP_HandleNxAccount( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!cl || cl->state != CS_ACTIVE) {
		NXRP_PrintConsoleToPlayer(cl, "You must be an active player to use this command");
		return qtrue;
	}
	const char *username = (cl && cl->nxrp_username[0]) ? cl->nxrp_username : nullptr;
	if (!username) {
		NXRP_PrintConsoleToPlayer(cl, "You are not logged in");
		return qtrue;
	}

	std::string user = username;
	std::string safe;
	for (char c : user) if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	if (safe.empty()) { NXRP_PrintConsoleToPlayer(cl, "Invalid stored username"); return qtrue; }

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Account file not found"); return qtrue; }
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	auto extract_number = [&](const std::string &key)->std::string {
		size_t k = content.find(key);
		if (k == std::string::npos) return std::string();
		size_t colon = content.find(':', k + key.size()); if (colon == std::string::npos) return std::string();
		size_t start = content.find_first_not_of(" \t", colon+1); if (start == std::string::npos) return std::string();
		size_t end = content.find_first_of(",\n\r}", start);
		if (end == std::string::npos) end = content.size();
		return content.substr(start, end-start);
	};

	std::string level = extract_number("\"level\"");
	std::string exp = extract_number("\"exp\"");
	std::string credits = extract_number("\"credits\"");

	NXRP_PrintConsoleToPlayerFmt(cl, "Account info for %s:", username);
	NXRP_PrintConsoleToPlayerFmt(cl, "  Username: %s", username);
	NXRP_PrintConsoleToPlayerFmt(cl, "  Level: %s", level.c_str());
	NXRP_PrintConsoleToPlayerFmt(cl, "  Exp: %s", exp.c_str());
	NXRP_PrintConsoleToPlayerFmt(cl, "  Credits: %s", credits.c_str());
	return qtrue;
}
