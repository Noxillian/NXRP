#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"

#include <string>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

// List skills implementation. homepath is Cvar_VariableString("fs_homepath") or fallback.
qboolean NXRP_Skills_ListImpl( client_t *cl, const char *homepath ) {
	std::string dir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp" : std::string("nxrp");
	std::string filepath = dir + "/skills.json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) return qfalse;
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	NXRP_PrintConsoleToPlayer(cl, "----- Skills -----");
	size_t pos = 0; int idx = 1;
	while (true) {
		size_t idpos = content.find("\"id\"", pos);
		if (idpos == std::string::npos) break;
		size_t namepos = content.find("\"name\"", idpos);
		size_t levelspos = content.find("\"levels\"", idpos);
		if (namepos == std::string::npos || levelspos == std::string::npos) break;
		// extract skill id and name
		size_t q1 = content.find('"', idpos + 4);
		size_t q2 = content.find('"', q1 + 1);
		std::string id = (q1 != std::string::npos && q2 != std::string::npos) ? content.substr(q1+1, q2-q1-1) : std::string("<unknown>");
		q1 = content.find('"', namepos + 6);
		q2 = content.find('"', q1 + 1);
		std::string sname = (q1 != std::string::npos && q2 != std::string::npos) ? content.substr(q1+1, q2-q1-1) : std::string("<unknown>");

		// count levels
		int maxLevel = 0;
		size_t lvlEnd = content.find(']', levelspos);
		if (lvlEnd != std::string::npos) {
			size_t p = levelspos;
			while (true) {
				size_t l = content.find("\"level\"", p);
				if (l == std::string::npos || l > lvlEnd) break;
				++maxLevel; p = l + 6;
			}
		}
		// first level cost
		int costVal = 0;
		size_t costpos = content.find("\"cost\"", levelspos);
		if (costpos != std::string::npos) {
			size_t ccolon = content.find(':', costpos);
			if (ccolon != std::string::npos) costVal = atoi(content.c_str() + ccolon + 1);
		}

		NXRP_PrintConsoleToPlayer(cl, va("%d. %s [%d/%d]\t- Cost: %d (id: %s)", idx, sname.c_str(), 0, maxLevel, costVal, id.c_str()));
		pos = levelspos + 1; ++idx;
	}
	NXRP_PrintConsoleToPlayer(cl, "------------------");
	NXRP_PrintConsoleToPlayer(cl, "type !skill <skillid> to purchase/upgrade");
	return qtrue;
}

// Purchase implementation
qboolean NXRP_Skill_PurchaseImpl( client_t *cl, const char *homepath, const char *skillToken ) {
	if (!skillToken || !skillToken[0]) return qfalse;
	std::string skill = skillToken;
	for (auto &c : skill) c = (char)tolower((unsigned char)c);

	// Resolve account file
	const char *username = cl->nxrp_username[0] ? cl->nxrp_username : nullptr;
	if (!username) { NXRP_PrintConsoleToPlayer(cl, "You must be logged in to buy skills"); return qtrue; }

	std::string safe;
	for (const char *p = username; *p; ++p) if (std::isalnum((unsigned char)*p) || *p == '_') safe.push_back((char)std::tolower((unsigned char)*p));
	if (safe.empty()) { NXRP_PrintConsoleToPlayer(cl, "Invalid account name"); return qtrue; }

	std::string dir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp" : std::string("nxrp");
	std::string accPath = dir + "/nxrp_accounts/" + safe + ".json";

	std::ifstream aifs(accPath);
	if (!aifs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Account file not found"); return qtrue; }
	std::string content((std::istreambuf_iterator<char>(aifs)), std::istreambuf_iterator<char>());
	aifs.close();

	// extract skillpoints
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

	// read skills definitions to find cost and maxLevel for the requested skill id
	std::string skillsPath = dir + "/skills.json";
	std::ifstream sifs(skillsPath);
	if (!sifs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Skills definition file missing"); return qtrue; }
	std::string scont((std::istreambuf_iterator<char>(sifs)), std::istreambuf_iterator<char>());
	sifs.close();

	// find by id
	size_t spos = 0; bool found = false; int nextCost = 0; int maxLevel = 0; std::string idFound;
	while (true) {
		size_t idpos = scont.find("\"id\"", spos);
		if (idpos == std::string::npos) break;
		size_t q1 = scont.find('"', idpos + 4);
		size_t q2 = scont.find('"', q1 + 1);
		std::string id = (q1 != std::string::npos && q2 != std::string::npos) ? scont.substr(q1+1, q2-q1-1) : std::string();
		size_t levelspos = scont.find("\"levels\"", idpos);
		if (levelspos == std::string::npos) break;
		// normalize id for compare
		std::string idl = id; for (auto &c : idl) c = (char)tolower((unsigned char)c);
		if (idl == skill) {
			found = true; idFound = id;
			// count levels
			size_t lvlEnd = scont.find(']', levelspos);
			if (lvlEnd != std::string::npos) {
				size_t p = levelspos; int lvCount = 0;
				while (true) {
					size_t l = scont.find("\"level\"", p);
					if (l == std::string::npos || l > lvlEnd) break;
					++lvCount; p = l + 6;
				}
				maxLevel = lvCount;
			}
			// find first level cost as nextCost (upgrade cost)
			size_t costpos = scont.find("\"cost\"", levelspos);
			if (costpos != std::string::npos) {
				size_t ccolon = scont.find(':', costpos);
				if (ccolon != std::string::npos) nextCost = atoi(scont.c_str() + ccolon + 1);
			}
			break;
		}
		spos = levelspos + 1;
	}

	if (!found) { NXRP_PrintConsoleToPlayer(cl, "Unknown skill id. Use !skills to list available skills."); return qtrue; }

	// determine user's current level for this skill by id in account content
	int userLevel = 0;
	size_t kpos = content.find('"' + idFound + '"');
	if (kpos != std::string::npos) {
		size_t lpos = content.find("\"level\"", kpos);
		if (lpos != std::string::npos) {
			size_t ccolon = content.find(':', lpos);
			if (ccolon != std::string::npos) userLevel = atoi(content.c_str() + ccolon + 1);
		}
	}

	if (userLevel >= maxLevel) { NXRP_PrintConsoleToPlayer(cl, "Skill already at max level"); return qtrue; }
	if (sp < nextCost) { NXRP_PrintConsoleToPlayer(cl, "Not enough skillpoints for next level"); return qtrue; }

	// deduct skillpoints and set/increment skill level
	int newsp = sp - nextCost;
	auto repl = content;
	// replace skillpoints value
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
		size_t brace = repl.rfind('}');
		if (brace != std::string::npos) {
			std::string add = "  ,\"skills\": {\"" + idFound + "\": { \"level\": 1 } }\n";
			repl.insert(brace, add);
		}
	} else {
		// if user's skill entry exists update level else insert
		size_t sj = repl.find(('"' + idFound + '"'));
		if (sj == std::string::npos) {
			// naive insert into skills object after "skills": {
			size_t ks = repl.find("\"skills\"");
			size_t ob = repl.find('{', ks);
			if (ob != std::string::npos) {
				repl.insert(ob+1, " \"" + idFound + "\": { \"level\": 1 },");
			}
		} else {
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

	// write back account
	std::ofstream ofs(accPath, std::ios::trunc);
	if (!ofs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Failed to write account file"); return qtrue; }
	ofs << repl;
	ofs.close();

	NXRP_PrintConsoleToPlayer(cl, va("Purchased %s. Remaining skillpoints: %d", idFound.c_str(), newsp));
	return qtrue;
}
