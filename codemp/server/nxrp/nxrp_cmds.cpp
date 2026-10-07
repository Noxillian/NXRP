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
// New plain-text account format per block:
// Username: name
// Password: pass
// Exp: 0
// Level: 1
// Credits: 0
//
// One blank line between accounts.

struct NXAccount {
	std::string username;
	std::string password;
	int exp = 0;
	int level = 1;
	int credits = 0;
};

static inline std::string trim(const std::string &s) {
	size_t a = 0; while (a < s.size() && isspace((unsigned char)s[a])) ++a;
	size_t b = s.size(); while (b > a && isspace((unsigned char)s[b-1])) --b;
	return s.substr(a, b - a);
}

static bool nxrp_load_accounts(std::vector<NXAccount> &out) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	std::ifstream f(path);
	if (!f.is_open()) return true; // missing file -> empty

	NXAccount cur;
	bool inAccount = false;
	std::string line;
	while (std::getline(f, line)) {
		line = trim(line);
		if (line.empty()) {
			if (inAccount) {
				out.push_back(cur);
				cur = NXAccount(); inAccount = false;
			}
			continue;
		}
		size_t colon = line.find(':');
		if (colon == std::string::npos) continue;
		std::string key = trim(line.substr(0, colon));
		std::string val = trim(line.substr(colon + 1));
		if (key == "Username") { cur.username = val; inAccount = true; }
		else if (key == "Password") cur.password = val;
		else if (key == "Exp") cur.exp = atoi(val.c_str());
		else if (key == "Level") cur.level = atoi(val.c_str());
		else if (key == "Credits") cur.credits = atoi(val.c_str());
	}
	if (inAccount) out.push_back(cur);
	return true;
}

static bool nxrp_save_accounts(const std::vector<NXAccount> &in) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	std::ofstream f(path, std::ios::trunc);
	if (!f.is_open()) return false;
	for (size_t i = 0; i < in.size(); ++i) {
		const NXAccount &a = in[i];
		f << "Username: " << a.username << "\n";
		f << "Password: " << a.password << "\n";
		f << "Exp: " << a.exp << "\n";
		f << "Level: " << a.level << "\n";
		f << "Credits: " << a.credits << "\n";
		if (i + 1 < in.size()) f << "\n";
	}
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
	if (sscanf(chatCursor, "%63s %127s", user, pass) != 2) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx register <user> <pass>\"" );
		return qtrue;
	}

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
	if (sscanf(chatCursor, "%63s %127s", user, pass) != 2) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Usage: !nx login <user> <pass>\"" );
		return qtrue;
	}

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
