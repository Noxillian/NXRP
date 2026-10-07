/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !nx info (prints NXRP version)
 */

#include "server/nxrp/nxrp_main.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>

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

// Very small JSON-ish parser for {"user":"pass",...}
static bool nxrp_load_accounts(std::map<std::string,std::string> &out) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f) return true; // treat missing file as empty
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<char> buf(sz + 1);
	if (sz > 0) fread(buf.data(), 1, sz, f);
	buf[sz] = '\0';
	fclose(f);

	const char *p = buf.data();
	while (true) {
		// find next "username"
		const char *q = strchr(p, '"');
		if (!q) break;
		const char *q2 = strchr(q + 1, '"');
		if (!q2) break;
		std::string user(q + 1, q2);
		const char *colon = strchr(q2 + 1, ':');
		if (!colon) break;
		const char *r = strchr(colon + 1, '"');
		if (!r) break;
		const char *r2 = strchr(r + 1, '"');
		if (!r2) break;
		std::string pass(r + 1, r2);
		out[user] = pass;
		p = r2 + 1;
	}
	return true;
}

static bool nxrp_save_accounts(const std::map<std::string,std::string> &in) {
	char path[MAX_OSPATH]; nxrp_accounts_path(path, sizeof(path));
	FILE *f = fopen(path, "w");
	if (!f) return false;
	fputs("{", f);
	bool first = true;
	for (const auto &kv : in) {
		if (!first) fputs(",", f);
		first = false;
		// escape not implemented; expect simple usernames/passwords
		fprintf(f, "\"%s\":\"%s\"", kv.first.c_str(), kv.second.c_str());
	}
	fputs("}", f);
	fclose(f);
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

	std::map<std::string,std::string> accounts;
	nxrp_load_accounts(accounts);
	std::string us(user);
	if (accounts.find(us) != accounts.end()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Username already exists\"" );
		return qtrue;
	}
	accounts[us] = pass;
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

	std::map<std::string,std::string> accounts;
	nxrp_load_accounts(accounts);
	std::string us(user);
	auto it = accounts.find(us);
	if (it == accounts.end()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: unknown user\"" );
		return qtrue;
	}
	if (it->second != pass) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login failed: incorrect password\"" );
		return qtrue;
	}
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Login successful\"" );
	return qtrue;
}
