/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !info (prints NXRP version)
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
#include "qcommon/md5.h"

static std::string hex_encode(const unsigned char *data, size_t len) {
	static const char* hex = "0123456789abcdef";
	std::string out; out.reserve(len*2);
	for (size_t i = 0; i < len; ++i) {
		unsigned char b = data[i];
		out.push_back(hex[(b >> 4) & 0xF]);
		out.push_back(hex[b & 0xF]);
	}

	return out;
}

// Minimal skills listing handler
qboolean NXRP_cmd_skills( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	const char *home = Cvar_VariableString("fs_homepath");
	NXRP_Skills_ListImpl(cl, home);
	return qtrue;
}

// Handler for: !skill <skillname>
qboolean NXRP_cmd_skill( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !skill <skillid>");
		return qtrue;
	}

	// parse first token as skill id/token
	char tok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s", tok) < 1) { NXRP_PrintConsoleToPlayer(cl, "Usage: !skill <skillid>"); return qtrue; }

	const char *home = Cvar_VariableString("fs_homepath");
	NXRP_Skill_PurchaseImpl(cl, home, tok);
	return qtrue;
}


static bool hex_decode(const std::string &hex, std::vector<unsigned char> &out) {
	size_t n = hex.size();
	if (n % 2 != 0) return false;
	out.clear(); out.reserve(n/2);
	for (size_t i = 0; i < n; i += 2) {
		auto cv = [](char c)->int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		int hi = cv(hex[i]); int lo = cv(hex[i+1]);
		if (hi < 0 || lo < 0) return false;
		out.push_back((unsigned char)((hi<<4) | lo));
	}
	return true;
}

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
	if (toks.size() >= 3) {
		// could be: "register user pass" or "login user pass"
		outUser = toks[1]; outPass = toks[2]; return true;
	}
	return false;
}

// Handler for: !info
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor ) {
	// Support subcommands: cmds, acmds, help
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	const char* s = chatCursor ? chatCursor : "";
	if (s && *s) {
		// Trim leading spaces
		while (*s == ' ' || *s == '\t') ++s;
		if (!Q_stricmp(s, "cmds") || !Q_stricmp(s, "commands")) {
			NXRP_PrintConsoleToPlayer(cl, "General commands:\n  !info | !nxrp | /info - show info/help\n  !register <user> <pass> - create account\n  !login <user> <pass> - login\n  !pos - show position\n  !skills - list skills");
			return qtrue;
		}
		if (!Q_stricmp(s, "acmds") || !Q_stricmp(s, "admin")) {
			NXRP_PrintConsoleToPlayer(cl, "Admin commands:\n  !prop s/t/m - spawn/manage props\n  !npc - spawn/load NPCs\n  !tp - teleport\n  !list - list entities\n  (admin only)");
			return qtrue;
		}
	}

	// Default info block
	NXRP_PrintConsoleToPlayer(cl, "------ NXRP ------");
	const char* username = (cl && cl->nxrp_username[0]) ? cl->nxrp_username : "<not logged in>";
	NXRP_PrintConsoleToPlayer(cl, va("%s %d/50", username, 1));
	NXRP_PrintConsoleToPlayer(cl, va("%d/100 Credits: %d", 0, 0));
	NXRP_PrintConsoleToPlayer(cl, "------------------");
	NXRP_PrintConsoleToPlayer(cl, "1. /info cmds\t- General Commands");
	NXRP_PrintConsoleToPlayer(cl, "2. /info acmds\t- Admin Commands");
	NXRP_PrintConsoleToPlayer(cl, "3. /skills\t\t- List Skills");
	return qtrue;
}

// Handler for: !register <user> <pass>
qboolean NXRP_cmd_register( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !register <user> <pass>");
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
	// store password as HMAC-MD5 with a per-user random salt
	unsigned char salt[8];
	if (!Sys_RandomBytes(salt, sizeof(salt))) {
		// fallback to rand() if secure RNG unavailable
		for (int i = 0; i < (int)sizeof(salt); ++i) salt[i] = (unsigned char)(rand() & 0xFF);
	}
	unsigned char digest[MD5_DIGEST_SIZE];
	hmacMD5Context_t ctx;
	HMAC_MD5_Init(&ctx, salt, sizeof(salt));
	HMAC_MD5_Update(&ctx, (const unsigned char*)pass.c_str(), (unsigned int)pass.size());
	HMAC_MD5_Final(&ctx, digest);
	std::string saltHex = hex_encode(salt, sizeof(salt));
	std::string digestHex = hex_encode(digest, MD5_DIGEST_SIZE);
	ofs << "  \"password\": \"" << saltHex << ":" << digestHex << "\",\n";
	ofs << "  \"level\": 1,\n";
	ofs << "  \"exp\": 0,\n";
	ofs << "  \"credits\": 0,\n";
	ofs << "  \"skillpoints\": 0,\n";
	ofs << "  \"isAdmin\": false\n";
	ofs << "}\n";

	ofs.close();

	NXRP_PrintConsoleToPlayer(cl, "Registration successful");
	return qtrue;
}

// Handler for: !login <user> <pass>
qboolean NXRP_cmd_login( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !login <user> <pass>");
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

	// stored format: "<saltHex>:<digestHex>", digest = HMAC-MD5(salt, password)
	size_t colonIdx = stored.find(':');
	if (colonIdx != std::string::npos) {
		std::string saltHex = stored.substr(0, colonIdx);
		std::string digHex = stored.substr(colonIdx + 1);
		std::vector<unsigned char> saltBin, digBin;
		if (hex_decode(saltHex, saltBin) && hex_decode(digHex, digBin) && digBin.size() == MD5_DIGEST_SIZE) {
			unsigned char outdig[MD5_DIGEST_SIZE];
			hmacMD5Context_t ctx2;
			HMAC_MD5_Init(&ctx2, saltBin.data(), (unsigned int)saltBin.size());
			HMAC_MD5_Update(&ctx2, (const unsigned char*)pass.c_str(), (unsigned int)pass.size());
			HMAC_MD5_Final(&ctx2, outdig);
			if (memcmp(outdig, digBin.data(), MD5_DIGEST_SIZE) == 0) {
				NXRP_PrintConsoleToPlayer(cl, "Login successful");
				// mark client as logged in by storing username in their session info
				if (cl && cl->state == CS_ACTIVE) {
					Q_strncpyz(cl->nxrp_username, user.c_str(), sizeof(cl->nxrp_username));
				}
				return qtrue;
			}
		}
	}
	NXRP_PrintConsoleToPlayer(cl, "Login failed: incorrect password");
	return qtrue;
}

// Handler for: !account
qboolean NXRP_cmd_account( client_t *cl, const char *chatCursor ) {
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
	std::string skillpoints = extract_number("\"skillpoints\"");

	NXRP_PrintConsoleToPlayerFmt(cl, "Account info for %s:", username);
	NXRP_PrintConsoleToPlayerFmt(cl, "  Username: %s", username);
	NXRP_PrintConsoleToPlayerFmt(cl, "  Level: %s", level.c_str());
	NXRP_PrintConsoleToPlayerFmt(cl, "  Exp: %s", exp.c_str());
	NXRP_PrintConsoleToPlayerFmt(cl, "  Credits: %s", credits.c_str());
	NXRP_PrintConsoleToPlayerFmt(cl, "  Skill Points: %s", skillpoints.c_str());
	return qtrue;
}

// Non-admin commands remain here (info, register, login, account)

// Handler for: !pos
qboolean NXRP_cmd_pos( client_t *cl, const char *chatCursor ) {
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
