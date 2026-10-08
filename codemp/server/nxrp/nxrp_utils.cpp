#include "server/nxrp/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include <fstream>
#include <cctype>

bool NXRP_EnsureLoggedIn(client_t* cl) {
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}
	return true;
}

bool NXRP_ReadAccountContentForClient(client_t* cl, std::string& outContent, std::string& outSafeUsername) {
	if (!cl || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}

	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid stored username");
		return false;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "Account file not found");
		return false;
	}

	outContent.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	outSafeUsername = safe;
	return true;
}

bool NXRP_IsClientAdmin(client_t* cl) {
	std::string content, safe;
	if (!NXRP_ReadAccountContentForClient(cl, content, safe)) return false;

	const std::string key = "\"isAdmin\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		NXRP_PrintConsoleToPlayer(cl, "Admin flag missing");
		return false;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { NXRP_PrintConsoleToPlayer(cl, "Admin flag parse error"); return false; }
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) { NXRP_PrintConsoleToPlayer(cl, "Admin flag parse error"); return false; }
	bool isAdmin = false;
	if (content.compare(pos, 4, "true") == 0) isAdmin = true;
	else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
	else {
		if (content[pos] == '"') {
			size_t qend = content.find('"', pos + 1);
			if (qend != std::string::npos) {
				std::string tok = content.substr(pos + 1, qend - (pos + 1));
				if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
			}
		}
	}

	if (!isAdmin) {
		NXRP_PrintConsoleToPlayer(cl, "You are not an admin");
		return false;
	}
	return true;
}
