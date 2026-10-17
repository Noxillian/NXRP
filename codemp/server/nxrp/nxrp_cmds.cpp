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

// Minimal skills listing handler
qboolean NXRP_cmd_skills( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;

	// Read skills.json from nxrp folder
	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp" : std::string("nxrp");
	std::string filepath = dir + "/skills.json";
	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "Failed to open skills.json");
		return qtrue;
	}
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// Very simple parsing to list skills and costs
	NXRP_PrintConsoleToPlayer(cl, "----- Skills -----");
	size_t pos = 0; int idx = 1;
	const char *username = cl->nxrp_username[0] ? cl->nxrp_username : nullptr;
	// load user's skill level from account file if present
	std::string accountSkills;
	if (username) {
		std::string safe;
		for (const char *p = username; *p; ++p) if (std::isalnum((unsigned char)*p) || *p == '_') safe.push_back((char)std::tolower((unsigned char)*p));
		std::string accPath = dir + "/nxrp_accounts/" + safe + ".json";
		std::ifstream aifs(accPath);
		if (aifs.is_open()) { accountSkills.assign((std::istreambuf_iterator<char>(aifs)), std::istreambuf_iterator<char>()); aifs.close(); }
	}

	while (true) {
		size_t idpos = content.find("\"id\"", pos);
		if (idpos == std::string::npos) break;
		size_t namepos = content.find("\"name\"", idpos);
		size_t levelspos = content.find("\"levels\"", idpos);
		if (namepos == std::string::npos || levelspos == std::string::npos) break;
		// extract skill name
		size_t q1 = content.find('"', namepos + 6);
		size_t q2 = content.find('"', q1 + 1);
		std::string sname = (q1 != std::string::npos && q2 != std::string::npos) ? content.substr(q1+1, q2-q1-1) : std::string("<unknown>");
		// find first level cost
		size_t lvl = content.find("\"level\"", levelspos);
		size_t cost = content.find("\"cost\"", levelspos);
		int maxLevel = 0;
		int costVal = 0;
		// parse maxLevel by counting level entries until next ']' or '}'
		size_t lvlEnd = content.find(']', levelspos);
		if (lvlEnd != std::string::npos) {
			size_t p = levelspos;
			while (true) {
				size_t l = content.find("\"level\"", p);
				if (l == std::string::npos || l > lvlEnd) break;
				++maxLevel; p = l + 6;
			}
		}
		if (cost != std::string::npos && cost < (levelspos + 512)) {
			size_t ccolon = content.find(':', cost);
			if (ccolon != std::string::npos) costVal = atoi(content.c_str() + ccolon + 1);
		}

		// determine user's current level for this skill
		int userLevel = 0;
		if (!accountSkills.empty()) {
			std::string key = "\"" + sname + "\"";
			size_t kpos = accountSkills.find(key);
			if (kpos != std::string::npos) {
				size_t lpos = accountSkills.find("\"level\"", kpos);
				if (lpos != std::string::npos) {
					size_t ccolon = accountSkills.find(':', lpos);
					if (ccolon != std::string::npos) userLevel = atoi(accountSkills.c_str() + ccolon + 1);
				}
			}
		}

		NXRP_PrintConsoleToPlayer(cl, va("%d. %s [%d/%d]\t- Cost: %d", idx, sname.c_str(), userLevel, maxLevel, costVal));
		pos = levelspos + 1; ++idx;
	}
	NXRP_PrintConsoleToPlayer(cl, "------------------");
	NXRP_PrintConsoleToPlayer(cl, "type !skill <skillname> to");
	return qtrue;
}

// Handler for: !skill <skillname>
qboolean NXRP_cmd_skill( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !skill <skillname>");
		return qtrue;
	}

	// parse first token as skill name
	char tok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s", tok) < 1) { NXRP_PrintConsoleToPlayer(cl, "Usage: !skill <skillname>"); return qtrue; }

	std::string skill = tok;
	for (auto &c : skill) c = (char)tolower((unsigned char)c);

	// simple implementation: only support "jump"
	if (skill != "jump" && skill != "skill_jump") {
		NXRP_PrintConsoleToPlayer(cl, "Unknown skill. Use /skills to list available skills.");
		return qtrue;
	}

	// locate account file and read then update skillpoints and skills array
	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string safe;
	{
		const char *username = cl->nxrp_username;
		if (!username || !username[0]) { NXRP_PrintConsoleToPlayer(cl, "You must be logged in to buy skills"); return qtrue; }
		// create safe filename like registration does
		for (const char *p = username; *p; ++p) {
			char ch = *p;
			if (std::isalnum((unsigned char)ch) || ch == '_') safe.push_back((char)std::tolower((unsigned char)ch));
		}
		if (safe.empty()) { NXRP_PrintConsoleToPlayer(cl, "Invalid account name"); return qtrue; }
	}

	std::string filepath = dir + "/" + safe + ".json";

	// read file
	std::ifstream ifs(filepath);
	if (!ifs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Account file not found"); return qtrue; }
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// extract skillpoints value (basic parse)
	auto extract_number = [&](const char* key)->int {
		std::string ks = key;
		size_t pos = content.find(ks);
		if (pos == std::string::npos) return -99999;
		size_t colon = content.find(':', pos + ks.size()); if (colon == std::string::npos) return -99999;
		size_t vstart = colon + 1; while (vstart < content.size() && isspace((unsigned char)content[vstart])) ++vstart;
		int val = 0; if (vstart < content.size()) { val = atoi(content.c_str()+vstart); }
		return val;
	};

	int sp = extract_number("\"skillpoints\"");
	if (sp == -99999) { NXRP_PrintConsoleToPlayer(cl, "Account missing skillpoints field"); return qtrue; }
	if (sp <= 0) { NXRP_PrintConsoleToPlayer(cl, "Not enough skill points"); return qtrue; }

	// deduct one point and add skill entry (simple append or replace)
	int newsp = sp - 1;

	// naive JSON patch: replace "skillpoints": N with newsp
	auto repl = content;
	size_t spPos = repl.find("\"skillpoints\"");
	if (spPos != std::string::npos) {
		size_t colon = repl.find(':', spPos);
		size_t lineEnd = repl.find_first_of("\n", colon);
		if (colon != std::string::npos && lineEnd != std::string::npos) {
			std::string before = repl.substr(0, colon+1);
			std::string after = repl.substr(lineEnd);
			repl = before + " " + std::to_string(newsp) + "," + after;
		}
	}

	// add skills object if missing
	if (repl.find("\"skills\"") == std::string::npos) {
		// insert before final closing brace
		size_t brace = repl.rfind('}');
		if (brace != std::string::npos) {
			std::string add = "  ,\"skills\": {\"skill_jump\": { \"level\": 1 } }\n";
			repl.insert(brace, add);
		}
	} else {
		// if skills exists, try to add/upgrade skill_jump level
		// find skill_jump entry
		size_t sj = repl.find("\"skill_jump\"");
		if (sj == std::string::npos) {
			// insert into skills object naively after "skills": {
			size_t ks = repl.find("\"skills\"");
			size_t ob = repl.find('{', ks);
			if (ob != std::string::npos) {
				repl.insert(ob+1, " \"skill_jump\": { \"level\": 1 },");
			}
		} else {
			// upgrade existing level value by 1
			size_t col = repl.find(':', sj);
			size_t lvl = repl.find("\"level\"", sj);
			if (lvl != std::string::npos) {
				size_t c2 = repl.find(':', lvl);
				size_t end = repl.find_first_of(",}\n", c2);
				if (c2 != std::string::npos && end != std::string::npos) {
					int cur = atoi(repl.c_str()+c2+1);
					int nxt = cur + 1;
					repl.replace(c2+1, end-(c2+1), " " + std::to_string(nxt));
				}
			}
		}
	}

	// write back
	std::ofstream ofs(filepath, std::ios::trunc);
	if (!ofs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Failed to write account file"); return qtrue; }
	ofs << repl;
	ofs.close();

	NXRP_PrintConsoleToPlayer(cl, va("Purchased skill_jump. Remaining skillpoints: %d", newsp));
	return qtrue;
}
	return out;
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
