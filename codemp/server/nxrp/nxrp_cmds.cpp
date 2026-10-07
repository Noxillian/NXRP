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
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] NXRP version: %s\"", NXRP_VERSION );
	return qtrue;
}

// Handler for: !nx register <user> <pass>
qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx register <user> <pass>\"" );
		return qtrue;
	}

	// sanitize username for filename
	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid username\"" );
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
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Username already exists\"" );
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
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration failed: cannot create account file\"" );
		return qtrue;
	}
	ofs << "{\n";
	ofs << "  \"username\": \"" << escape(user) << "\",\n";
	ofs << "  \"password\": \"" << escape(pass) << "\",\n";
	ofs << "  \"level\": 1,\n";
	ofs << "  \"exp\": 0,\n";
	ofs << "  \"credits\": 0\n";
	ofs << "}\n";
	ofs.close();

	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration successful\"" );
	return qtrue;
}

// Handler for: !nx login <user> <pass>
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx login <user> <pass>\"" );
		return qtrue;
	}

	// sanitize username for filename
	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid username\"" );
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: unknown user\"" );
		return qtrue;
	}
	// read entire file
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for password field: "password": "..."
	std::string key = "\"password\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\"" );
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\"" ); return qtrue; }
	size_t quote = content.find('"', colon);
	if (quote == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\"" ); return qtrue; }
	size_t qend = content.find('"', quote + 1);
	if (qend == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed\"" ); return qtrue; }
	std::string stored = content.substr(quote + 1, qend - (quote + 1));
	// stored is escaped; compare naively
	if (stored == pass) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login successful\"" );
	} else {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: incorrect password\"" );
	}
	return qtrue;
}
