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
#include <fstream>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <algorithm>
#include <cctype>

// Version variable for NXRP. Update as needed.
const char *NXRP_VERSION = "0.1.0";

// Account storage: per-user JSON files in <fs_homepath>/nxrp_accounts or ./nxrp_accounts

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
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] NXRP version: %s\\n\"", NXRP_VERSION );
	return qtrue;
}

// Handler for: !nx register <user> <pass>
qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx register <user> <pass>\\n\"" );
		return qtrue;
	}

	// sanitize username for filename
	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid username\\n\"" );
		return qtrue;
	}

	// determine accounts directory and ensure it exists
	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
#if defined(_WIN32)
	_mkdir(dir.c_str());
#else
	mkdir(dir.c_str(), 0755);
#endif

	std::string filepath = dir + "/" + safe + ".json";

	// if account file exists, reject
	std::ifstream ifs(filepath);
	if (ifs.good()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Username already exists\\n\"" );
		return qtrue;
	}

	// write JSON file
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
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration failed: cannot create account file\\n\"" );
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

	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration successful\\n\"" );
	return qtrue;
}

// Handler for: !nx login <user> <pass>
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx login <user> <pass>\n\"" );
		return qtrue;
	}

	// sanitize username for filename
	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid username\n\"" );
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: unknown user\n\"" );
		return qtrue;
	}
	// read entire file
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for password field: "password": "..."
	std::string key = "\"password\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\n\"" );
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\n\"" ); return qtrue; }
	size_t quote = content.find('"', colon);
	if (quote == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\n\"" ); return qtrue; }
	size_t qend = content.find('"', quote + 1);
	if (qend == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\n\"" ); return qtrue; }
	std::string stored = content.substr(quote + 1, qend - (quote + 1));
	// stored is escaped; compare naively
	if (stored == pass) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login successful\n\"" );
		// mark client as logged in by storing username in their session info
		if (cl && cl->state == CS_ACTIVE) {
			// store username into client's economics/economyHandle or a safe custom field if available
			Q_strncpyz(cl->nxrp_username, user.c_str(), sizeof(cl->nxrp_username));
		}
	} else {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: incorrect password\n\"" );
	}
	return qtrue;
}

// Handler for: !nx account
qboolean SV_nxrp_HandleNxAccount( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!cl || cl->state != CS_ACTIVE) {
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You must be an active player to use this command\\n\"" );
		return qtrue;
	}
	// check stored username on client
	const char *username = (cl->nxrp_username && cl->nxrp_username[0]) ? cl->nxrp_username : nullptr;
	if (!username) {
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You are not logged in\\n\"" );
		return qtrue;
	}

	// build file path
	std::string user = username;
	std::string safe;
	for (char c : user) if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	if (safe.empty()) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" ); return qtrue; }

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" ); return qtrue; }
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

	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Account info for %s:\\n\"", username );
	SV_SendServerCommand(cl, "print \"  Username: %s\\n\"", username );
	SV_SendServerCommand(cl, "print \"  Level: %s\\n\"", level.c_str() );
	SV_SendServerCommand(cl, "print \"  Exp: %s\\n\"", exp.c_str() );
	SV_SendServerCommand(cl, "print \"  Credits: %s\\n\"", credits.c_str() );
	return qtrue;
}
