/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !nx info (prints NXRP version)
 */

#include "server/nxrp/nxrp_main.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>

// Version variable for NXRP. Update as needed.
const char *NXRP_VERSION = "0.1.0";

static void nxrp_accounts_path(char *out, size_t outlen) {
	const char *home = Cvar_VariableString("fs_homepath");
	if (home && home[0]) {
		Com_sprintf(out, outlen, "%s/nxrp_accounts.json", home);
	} else {
		Q_strncpyz(out, "nxrp_accounts.json", outlen);
	}
}

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
}
// JSON array of account objects. Example:
// [ {"Username":"nox","Password":"123","Exp":0,"Level":1,"Credits":0}, ... ]

struct NXAccount {
	std::string username;
	std::string password;
	int exp = 0;
	int level = 1;
	int credits = 0;
};

static inline std::string json_escape(const std::string &s) {
	std::string out; out.reserve(s.size());
	for (unsigned char c : s) {
		if (c == '\\') out += "\\\\";
		else if (c == '"') out += "\\\"";
		else if (c == '\n') out += "\\n";
		else out += c;
	}
	return out;
}

static inline std::string trim(const std::string &s) {
	size_t a = 0; while (a < s.size() && isspace((unsigned char)s[a])) ++a;
	size_t b = s.size(); while (b > a && isspace((unsigned char)s[b-1])) --b;
	return s.substr(a, b - a);
}

// Very small JSON parser for our expected array-of-objects format.
static bool nxrp_load_accounts(std::vector<NXAccount> &out) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	std::ifstream f(path);
	if (!f.is_open()) return true;
	std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	f.close();
	size_t pos = 0;
	while (true) {
		pos = s.find('{', pos);
		if (pos == std::string::npos) break;
		size_t end = s.find('}', pos);
		if (end == std::string::npos) break;
		std::string obj = s.substr(pos + 1, end - pos - 1);
		NXAccount a;
		size_t p = 0;
		while (p < obj.size()) {
			// find key
			size_t k1 = obj.find('"', p);
			if (k1 == std::string::npos) break;
			size_t k2 = obj.find('"', k1 + 1);
			if (k2 == std::string::npos) break;
			std::string key = obj.substr(k1 + 1, k2 - k1 - 1);
			size_t colon = obj.find(':', k2 + 1);
			if (colon == std::string::npos) break;
			size_t vstart = colon + 1;
			while (vstart < obj.size() && isspace((unsigned char)obj[vstart])) ++vstart;
			if (vstart >= obj.size()) break;
			if (obj[vstart] == '"') {
				size_t v1 = vstart;
				size_t v2 = obj.find('"', v1 + 1);
				if (v2 == std::string::npos) break;
				std::string val = obj.substr(v1 + 1, v2 - v1 - 1);
				if (key == "Username") a.username = val;
				else if (key == "Password") a.password = val;
				// advance
				p = v2 + 1;
			} else {
				// number
				size_t v2 = vstart;
				while (v2 < obj.size() && (isdigit((unsigned char)obj[v2]) || obj[v2] == '-')) ++v2;
				std::string val = obj.substr(vstart, v2 - vstart);
				if (key == "Exp") a.exp = atoi(val.c_str());
				else if (key == "Level") a.level = atoi(val.c_str());
				else if (key == "Credits") a.credits = atoi(val.c_str());
				p = v2;
			}
			// skip comma
			size_t comma = obj.find(',', p);
			if (comma == std::string::npos) p = obj.size(); else p = comma + 1;
		}
		if (!a.username.empty()) out.push_back(a);
		pos = end + 1;
	}
	return true;
}

static bool nxrp_save_accounts(const std::vector<NXAccount> &in) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	std::ofstream f(path, std::ios::trunc);
	if (!f.is_open()) return false;
	f << "[\n";
	for (size_t i = 0; i < in.size(); ++i) {
		const NXAccount &a = in[i];
		f << "  {";
		f << "\"Username\":\"" << json_escape(a.username) << "\",";
		f << "\"Password\":\"" << json_escape(a.password) << "\",";
		f << "\"Exp\":" << a.exp << ",";
		f << "\"Level\":" << a.level << ",";
		f << "\"Credits\":" << a.credits;
		f << " }";
		if (i + 1 < in.size()) f << ",\n";
		else f << "\n";
	}
	f << "]\n";
	f.close();
	return true;
}

// Handler for: !nx info
qboolean SV_nxrp_HandleNxInfo( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] NXRP version: %s\"", NXRP_VERSION );
	return qtrue;
}

// Handler for: !nx register <user> <pass>
qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	char user[64] = {0}; char pass[128] = {0};
	std::string u,p;
	if (!parse_user_pass(chatCursor, u, p)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx register <user> <pass>\"" );
		return qtrue;
	}
	Q_strncpyz(user, u.c_str(), sizeof(user)); Q_strncpyz(pass, p.c_str(), sizeof(pass));

	std::vector<NXAccount> accounts;
	nxrp_load_accounts(accounts);
	std::string us(user);
	for (const auto &a : accounts) if (a.username == us) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Username already exists\"" );
		return qtrue;
	}
	NXAccount na;
	na.username = us;
	na.password = pass;
	na.exp = 0; na.level = 1; na.credits = 0;
	accounts.push_back(na);
	if (!nxrp_save_accounts(accounts)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Failed to save account\"" );
		return qtrue;
	}
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Registration successful\"" );
	return qtrue;
}

// Handler for: !nx login <user> <pass>
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	char user[64] = {0}; char pass[128] = {0};
	std::string u,p;
	if (!parse_user_pass(chatCursor, u, p)) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx login <user> <pass>\"" );
		return qtrue;
	}
	Q_strncpyz(user, u.c_str(), sizeof(user)); Q_strncpyz(pass, p.c_str(), sizeof(pass));

	std::vector<NXAccount> accounts;
	nxrp_load_accounts(accounts);
	std::string us(user);
	for (const auto &a : accounts) {
		if (a.username == us) {
			if (a.password == pass) {
				SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login successful\"" );
			} else {
				SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: incorrect password\"" );
			}
			return qtrue;
		}
	}
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: unknown user\"" );
	return qtrue;
}
