#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#if defined(_WIN32)
  #include <direct.h>
#else
  #include <sys/stat.h>
  #include <sys/types.h>
#endif

#if 0

struct NXRP_RepoSkillLevel {
	int level = 0;
	int cost = 0;
	// store simple effects as map<string,string> for printing
	std::map<std::string,std::string> effects;
};

struct NXRP_RepoSkillDef {
	std::string id;
	std::string name;
	bool enabled = true;
	int maxLevel = 0;
	std::string description;
	std::vector<NXRP_RepoSkillLevel> levels;
};

static std::vector<NXRP_RepoSkillDef> g_nxrpRepoSkills;

// Print a single effect key/value in readable form
static void NXRP_PrintEffect(client_t* cl, const std::string &k, const std::string &v) {
	NXRP_PrintConsoleToPlayer(cl, va("    %s: %s", k.c_str(), v.c_str()));
}

// Test listing implementation: prints all repo skills in a readable format
void NXRP_Skills_TestImpl(client_t* cl) {
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (g_nxrpRepoSkills.empty()) { NXRP_PrintConsoleToPlayer(cl, "No skills available"); return; }

	int idx = 1;
	for (const auto &def : g_nxrpRepoSkills) {
		NXRP_PrintConsoleToPlayer(cl, va("%d) %s (id: %s) [levels: %d]", idx, def.name.c_str(), def.id.c_str(), def.maxLevel));
		if (!def.description.empty()) NXRP_PrintConsoleToPlayer(cl, va("  Desc: %s", def.description.c_str()));
		NXRP_PrintConsoleToPlayer(cl, "  Levels:");
		for (const auto &lvl : def.levels) {
			NXRP_PrintConsoleToPlayer(cl, va("    L%d: cost %d", lvl.level, lvl.cost));
			if (!lvl.effects.empty()) {
				for (const auto &ef : lvl.effects) {
					NXRP_PrintEffect(cl, ef.first, ef.second);
				}
			}
		}
		++idx;
	}
}

static std::string NXRP_ToLowerCopy( const std::string &in ) {
	std::string out = in;
	for (auto &c : out) c = (char)std::tolower((unsigned char)c);
	return out;
}

// Called at server startup to ensure <fs_homepath>/nxrp/skills.json exists.
void NXRP_EnsureRuntimeSkills(void) {
	const char *homepath = Cvar_VariableString("fs_homepath");
	std::string nxrpDir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp" : std::string("nxrp");
	std::string filepath = nxrpDir + "/skills.json";
	std::ifstream ifs(filepath);
	if (ifs.is_open()) { ifs.close(); return; }

	// Startup no longer copies skills.json from any candidate paths.
	// Skill definitions are provided by hardcoded repo skills when needed.
	return;
}

void NXRP_LoadRepoSkills() {
	// Try to load repo skills.json from the source tree at runtime.
	const std::string repoPath = "codemp/server/nxrp/nxrp_skills/skills.json";
	std::ifstream ifs(repoPath);
	g_nxrpRepoSkills.clear();
	if (!ifs.is_open()) {
		// fallback to hardcoded if the repo file is missing
		NXRP_RepoSkillDef jump;
		jump.id = "skill_jump";
		jump.name = "Jump";
		jump.enabled = true;
		jump.maxLevel = 3;
		jump.description = "Increases player's jump height.";
		jump.levels = { {1,1, { {"jumpMultiplier", "1.10"} }}, {2,2, { {"jumpMultiplier", "1.25"} }}, {3,3, { {"jumpMultiplier", "1.50"} }} };

		NXRP_RepoSkillDef pistol;
		pistol.id = "skill_pistol";
		pistol.name = "Pistol";
		pistol.enabled = true;
		pistol.maxLevel = 2;
		pistol.description = "Improves pistol damage/handling.";
		pistol.levels = { {1,1, {{"damageMultiplier","1.10"}}}, {2,2, {{"damageMultiplier","1.25"}}} };

		g_nxrpRepoSkills.push_back(jump);
		g_nxrpRepoSkills.push_back(pistol);
		return;
	}

	std::string scont((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// crude JSON parsing tailored to the expected schema:
	// { "skills": [ { "id":"..", "name":"..", "enabled":true, "maxLevel":N, "description":"..", "levels": [ {"level":1, "cost":1, "effects": { ... } }, ... ] }, ... ] }
	size_t pos = 0;
	size_t skillsPos = scont.find("\"skills\"");
	if (skillsPos == std::string::npos) return;
	size_t arrStart = scont.find('[', skillsPos);
	if (arrStart == std::string::npos) return;
	pos = arrStart + 1;

	while (true) {
		size_t idpos = scont.find("\"id\"", pos);
		if (idpos == std::string::npos) break;
		NXRP_RepoSkillDef def;
		// id
		size_t q1 = scont.find('"', idpos + 4);
		if (q1 == std::string::npos) break;
		size_t q2 = scont.find('"', q1 + 1);
		if (q2 == std::string::npos) break;
		def.id = scont.substr(q1+1, q2-q1-1);

		// name
		size_t namepos = scont.find("\"name\"", idpos);
		if (namepos != std::string::npos) {
			q1 = scont.find('"', namepos + 6);
			q2 = scont.find('"', q1 + 1);
			if (q1 != std::string::npos && q2 != std::string::npos) def.name = scont.substr(q1+1, q2-q1-1);
		}

		// enabled
		size_t enpos = scont.find("\"enabled\"", idpos);
		if (enpos != std::string::npos) {
			size_t colon = scont.find(':', enpos);
			if (colon != std::string::npos) {
				size_t vstart = colon+1;
				while (vstart < scont.size() && isspace((unsigned char)scont[vstart])) ++vstart;
				if (scont.compare(vstart, 4, "true") == 0) def.enabled = true;
				else def.enabled = false;
			}
		}

		// maxLevel
		size_t mlpos = scont.find("\"maxLevel\"", idpos);
		if (mlpos != std::string::npos) {
			size_t colon = scont.find(':', mlpos);
			if (colon != std::string::npos) def.maxLevel = atoi(scont.c_str() + colon + 1);
		}

		// description
		size_t descpos = scont.find("\"description\"", idpos);
		if (descpos != std::string::npos) {
			q1 = scont.find('"', descpos + 13);
			q2 = scont.find('"', q1 + 1);
			if (q1 != std::string::npos && q2 != std::string::npos) def.description = scont.substr(q1+1, q2-q1-1);
		}

		// levels
		size_t levelspos = scont.find("\"levels\"", idpos);
		if (levelspos != std::string::npos) {
			size_t lb = scont.find('[', levelspos);
			size_t rb = (lb == std::string::npos) ? std::string::npos : scont.find(']', lb);
			if (lb != std::string::npos && rb != std::string::npos) {
				size_t p = lb + 1;
				while (p < rb) {
					size_t levelKey = scont.find("\"level\"", p);
					if (levelKey == std::string::npos || levelKey > rb) break;
					NXRP_RepoSkillLevel lvl;
					size_t colon = scont.find(':', levelKey);
					if (colon != std::string::npos) lvl.level = atoi(scont.c_str() + colon + 1);
					// cost
					size_t costKey = scont.find("\"cost\"", levelKey);
					if (costKey != std::string::npos && costKey < rb) {
						colon = scont.find(':', costKey);
						if (colon != std::string::npos) lvl.cost = atoi(scont.c_str() + colon + 1);
					}
					// effects
					size_t effKey = scont.find("\"effects\"", levelKey);
					if (effKey != std::string::npos && effKey < rb) {
						size_t ob = scont.find('{', effKey);
						size_t cb = (ob == std::string::npos) ? std::string::npos : scont.find('}', ob);
						if (ob != std::string::npos && cb != std::string::npos) {
							size_t epos = ob + 1;
							while (epos < cb) {
								size_t kq1 = scont.find('"', epos);
								if (kq1 == std::string::npos || kq1 > cb) break;
								size_t kq2 = scont.find('"', kq1 + 1);
								if (kq2 == std::string::npos || kq2 > cb) break;
								std::string k = scont.substr(kq1+1, kq2-kq1-1);
								size_t ccol = scont.find(':', kq2);
								if (ccol == std::string::npos || ccol > cb) break;
								size_t vstart = ccol + 1;
								while (vstart < scont.size() && isspace((unsigned char)scont[vstart])) ++vstart;
								std::string val;
								if (scont[vstart] == '"') {
									size_t vq = scont.find('"', vstart+1);
									if (vq == std::string::npos || vq > cb) break;
									val = scont.substr(vstart+1, vq-vstart-1);
									epos = vq + 1;
								} else {
									// number or bare token
									size_t vend = vstart;
									while (vend < scont.size() && (std::isdigit((unsigned char)scont[vend]) || scont[vend]=='.' || scont[vend]=='-' )) ++vend;
									val = scont.substr(vstart, vend - vstart);
									epos = vend + 1;
								}
								lvl.effects[k] = val;
							}
						}
					}
					def.levels.push_back(lvl);
					p = levelKey + 6;
				}
			}
		}

		g_nxrpRepoSkills.push_back(def);
		pos = idpos + 1;
	}
	// if parsing failed to produce entries, leave g_nxrpRepoSkills empty so callers may fallback
}

// Exported accessors for hardcoded repo skills
int NXRP_GetRepoSkillCount(void) {
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	return (int)g_nxrpRepoSkills.size();
}

int NXRP_FindRepoSkillIndexById(const char *id) {
	if (!id) return -1;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	std::string idl = NXRP_ToLowerCopy(std::string(id));
	for (size_t i = 0; i < g_nxrpRepoSkills.size(); ++i) {
		if (NXRP_ToLowerCopy(g_nxrpRepoSkills[i].id) == idl) return (int)i;
	}
	return -1;
}

const char* NXRP_GetRepoSkillIdByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpRepoSkills.size()) return nullptr;
	return g_nxrpRepoSkills[idx].id.c_str();
}

const char* NXRP_GetRepoSkillNameByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpRepoSkills.size()) return nullptr;
	return g_nxrpRepoSkills[idx].name.c_str();
}

const char* NXRP_GetRepoSkillDescriptionByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpRepoSkills.size()) return nullptr;
	return g_nxrpRepoSkills[idx].description.c_str();
}

int NXRP_GetRepoSkillMaxLevelByIndex(int idx) {
	if (idx < 0) return 0;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpRepoSkills.size()) return 0;
	return g_nxrpRepoSkills[idx].maxLevel;
}

int NXRP_GetRepoSkillLevelCount(int skillIdx) {
	if (skillIdx < 0) return 0;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpRepoSkills.size()) return 0;
	return (int)g_nxrpRepoSkills[skillIdx].levels.size();
}

int NXRP_GetRepoSkillLevelCost(int skillIdx, int levelIdx) {
	if (skillIdx < 0 || levelIdx < 0) return 0;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpRepoSkills.size()) return 0;
	const auto &lv = g_nxrpRepoSkills[skillIdx].levels;
	if (levelIdx >= (int)lv.size()) return 0;
	return lv[levelIdx].cost;
}

int NXRP_GetRepoSkillLevelEffectsCount(int skillIdx, int levelIdx) {
	if (skillIdx < 0 || levelIdx < 0) return 0;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpRepoSkills.size()) return 0;
	const auto &lv = g_nxrpRepoSkills[skillIdx].levels;
	if (levelIdx >= (int)lv.size()) return 0;
	return (int)lv[levelIdx].effects.size();
}

const char* NXRP_GetRepoSkillLevelEffectKey(int skillIdx, int levelIdx, int effectIdx) {
	if (skillIdx < 0 || levelIdx < 0 || effectIdx < 0) return nullptr;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpRepoSkills.size()) return nullptr;
	if (levelIdx >= (int)g_nxrpRepoSkills[skillIdx].levels.size()) return nullptr;
	const auto &effects = g_nxrpRepoSkills[skillIdx].levels[levelIdx].effects;
	if (effectIdx >= (int)effects.size()) return nullptr;
	auto it = effects.begin();
	std::advance(it, effectIdx);
	return it->first.c_str();
}

const char* NXRP_GetRepoSkillLevelEffectValue(int skillIdx, int levelIdx, int effectIdx) {
	if (skillIdx < 0 || levelIdx < 0 || effectIdx < 0) return nullptr;
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpRepoSkills.size()) return nullptr;
	if (levelIdx >= (int)g_nxrpRepoSkills[skillIdx].levels.size()) return nullptr;
	const auto &effects = g_nxrpRepoSkills[skillIdx].levels[levelIdx].effects;
	if (effectIdx >= (int)effects.size()) return nullptr;
	auto it = effects.begin();
	std::advance(it, effectIdx);
	return it->second.c_str();
}

static const NXRP_RepoSkillDef *NXRP_FindRepoSkillById( const std::string &idLower ) {
	for (const auto &s : g_nxrpRepoSkills) {
		if (NXRP_ToLowerCopy(s.id) == idLower) return &s;
	}
	return nullptr;
}

// List skills implementation. homepath is Cvar_VariableString("fs_homepath") or fallback.
qboolean NXRP_Skills_ListImpl( client_t *cl, const char *homepath ) {
	// runtime skills live under <fs_homepath>/nxrp/skills.json
	std::string nxrpDir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp" : std::string("nxrp");
	std::string filepath = nxrpDir + "/skills.json";
	// account files live under <fs_homepath>/nxrp_accounts/<safe>.json (note: different directory)
	std::string accountsDir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp_accounts" : std::string("nxrp_accounts");

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		// Fallback source locations for skills.json when runtime copy is missing.
		const std::string repoPath = "codemp/server/nxrp/nxrp_skills/skills.json";
		std::vector<std::string> candidates;
		candidates.push_back(repoPath);
#if !defined(_WIN32)
		candidates.push_back("/usr/share/caded/nxrp/skills.json");
		candidates.push_back("/usr/local/share/caded/nxrp/skills.json");
#endif
		for (const auto &src : candidates) {
			std::ifstream srcIfs(src);
			if (!srcIfs.is_open()) continue;
			// ensure target dir exists
#if defined(_WIN32)
			_mkdir(nxrpDir.c_str());
#else
			mkdir(nxrpDir.c_str(), 0755);
#endif
			std::ofstream ofs(filepath, std::ios::trunc);
			if (ofs.is_open()) {
				ofs << srcIfs.rdbuf();
				ofs.close();
			}
			srcIfs.close();
			break;
		}
		ifs.open(filepath);
	}
	if (!ifs.is_open()) return qfalse;
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// load user's account skills if logged in
	std::string accountSkills;
	const char *username = cl->nxrp_username[0] ? cl->nxrp_username : nullptr;
	if (username) {
		std::string safe;
		for (const char *p = username; *p; ++p) if (std::isalnum((unsigned char)*p) || *p == '_') safe.push_back((char)std::tolower((unsigned char)*p));
		if (!safe.empty()) {
			std::string accPath = accountsDir + "/" + safe + ".json";
			std::ifstream aifs(accPath);
			if (aifs.is_open()) { accountSkills.assign((std::istreambuf_iterator<char>(aifs)), std::istreambuf_iterator<char>()); aifs.close(); }
		}
	}

	NXRP_PrintConsoleToPlayer(cl, "----- Skills -----");
	int idx = 1;

	// Prefer the simplified repo-backed skill list when available. This allows
	// edits to codemp/server/nxrp/nxrp_skills/skills.json to appear at
	// runtime during development without relying on the runtime copy.
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (!g_nxrpSimpleSkills.empty()) {
		for (int i = 0; i < (int)g_nxrpSimpleSkills.size(); ++i) {
			const auto &s = g_nxrpSimpleSkills[i];
			int userLevel = 0;
			if (!accountSkills.empty()) {
				std::string key = '"' + s.id + '"';
				size_t kpos = accountSkills.find(key);
				if (kpos != std::string::npos) {
					size_t lpos = accountSkills.find("\"level\"", kpos);
					if (lpos != std::string::npos) {
						size_t ccolon = accountSkills.find(':', lpos);
						if (ccolon != std::string::npos) userLevel = atoi(accountSkills.c_str() + ccolon + 1);
					}
				}
			}
			const int firstCost = s.costs.empty() ? 0 : s.costs[0];
			NXRP_PrintConsoleToPlayer(cl, va("%d. %s [%d/%d]\t- Cost: %d (id: %s)", i + 1, s.name.c_str(), userLevel, s.maxLevel, firstCost, s.id.c_str()));
		}
		NXRP_PrintConsoleToPlayer(cl, "------------------");
		NXRP_PrintConsoleToPlayer(cl, "type !skill <skillid> to purchase/upgrade");
		return qtrue;
	}
	// Prefer using repo-embedded skills if available
	if (g_nxrpRepoSkills.empty()) {
		NXRP_LoadRepoSkills();
	}
	if (!g_nxrpRepoSkills.empty()) {
		for (const auto &def : g_nxrpRepoSkills) {
			int userLevel = 0;
			if (!accountSkills.empty()) {
				std::string key = '"' + def.id + '"';
				size_t kpos = accountSkills.find(key);
				if (kpos != std::string::npos) {
					size_t lpos = accountSkills.find("\"level\"", kpos);
					if (lpos != std::string::npos) {
						size_t ccolon = accountSkills.find(':', lpos);
						if (ccolon != std::string::npos) userLevel = atoi(accountSkills.c_str() + ccolon + 1);
					}
				}
			}
			int costVal = 0;
			if (!def.levels.empty()) costVal = def.levels.front().cost;
			NXRP_PrintConsoleToPlayer(cl, va("%d. %s [%d/%d]\t- Cost: %d (id: %s)", idx, def.name.c_str(), userLevel, def.maxLevel, costVal, def.id.c_str()));
			++idx;
		}
	} else {
		// fallback: parse runtime content as before
		size_t pos = 0;
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

			int userLevel = 0;
			if (!accountSkills.empty()) {
				std::string key = '"' + id + '"';
				size_t kpos = accountSkills.find(key);
				if (kpos != std::string::npos) {
					size_t lpos = accountSkills.find("\"level\"", kpos);
					if (lpos != std::string::npos) {
						size_t ccolon = accountSkills.find(':', lpos);
						if (ccolon != std::string::npos) userLevel = atoi(accountSkills.c_str() + ccolon + 1);
					}
				}
			}

			NXRP_PrintConsoleToPlayer(cl, va("%d. %s [%d/%d]\t- Cost: %d (id: %s)", idx, sname.c_str(), userLevel, maxLevel, costVal, id.c_str()));
			pos = levelspos + 1; ++idx;
		}
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

	// runtime skills live under <fs_homepath>/nxrp/skills.json
	std::string nxrpDir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp" : std::string("nxrp");
	// account files live under <fs_homepath>/nxrp_accounts/<safe>.json (note: different directory)
	std::string accountsDir = (homepath && homepath[0]) ? std::string(homepath) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string accPath = accountsDir + "/" + safe + ".json";

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
	std::string skillsPath = nxrpDir + "/skills.json";
	std::ifstream sifs(skillsPath);
	if (!sifs.is_open()) {
		// If runtime skills.json is missing, try repo copy and system-installed locations.
		const std::string repoPath = "codemp/server/nxrp/nxrp_skills/skills.json";
		std::vector<std::string> candidates;
		candidates.push_back(repoPath);
#if !defined(_WIN32)
		candidates.push_back("/usr/share/caded/nxrp/skills.json");
		candidates.push_back("/usr/local/share/caded/nxrp/skills.json");
#endif
		for (const auto &src : candidates) {
			std::ifstream srcIfs(src);
			if (!srcIfs.is_open()) continue;
			// ensure target dir exists
#if defined(_WIN32)
			_mkdir(nxrpDir.c_str());
#else
			mkdir(nxrpDir.c_str(), 0755);
#endif
			std::ofstream ofs(skillsPath, std::ios::trunc);
			if (ofs.is_open()) {
				ofs << srcIfs.rdbuf();
				ofs.close();
			}
			srcIfs.close();
			// reopen the runtime file
			sifs.open(skillsPath);
			if (sifs.is_open()) break;
		}
	}
	if (!sifs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Skills definition file missing"); return qtrue; }
	std::string scont((std::istreambuf_iterator<char>(sifs)), std::istreambuf_iterator<char>());
	sifs.close();

	// Prefer repo-loaded skill definitions when available
	if (g_nxrpRepoSkills.empty()) NXRP_LoadRepoSkills();
	const NXRP_RepoSkillDef *def = NXRP_FindRepoSkillById(skill);
	if (!def) {
		// fallback: search in runtime skills.json content
		size_t spos = 0; bool found = false; int nextCost = 0; int maxLevel = 0; std::string idFound;
		while (true) {
			size_t idpos = scont.find("\"id\"", spos);
			if (idpos == std::string::npos) break;
			size_t q1 = scont.find('"', idpos + 4);
			size_t q2 = scont.find('"', q1 + 1);
			std::string id = (q1 != std::string::npos && q2 != std::string::npos) ? scont.substr(q1+1, q2-q1-1) : std::string();
			size_t levelspos = scont.find("\"levels\"", idpos);
			if (levelspos == std::string::npos) break;
			std::string idl = id; for (auto &c : idl) c = (char)tolower((unsigned char)c);
			if (idl == skill) {
				found = true; idFound = id;
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

		int newsp = sp - nextCost;
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

		if (repl.find("\"skills\"") == std::string::npos) {
			size_t brace = repl.rfind('}');
			if (brace != std::string::npos) {
				std::string add = "  ,\"skills\": {\"" + idFound + "\": { \"level\": 1 } }\n";
				repl.insert(brace, add);
			}
		} else {
			size_t sj = repl.find(('"' + idFound + '"'));
			if (sj == std::string::npos) {
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

		std::ofstream ofs(accPath, std::ios::trunc);
		if (!ofs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Failed to write account file"); return qtrue; }
		ofs << repl;
		ofs.close();

		NXRP_PrintConsoleToPlayer(cl, va("Purchased %s. Remaining skillpoints: %d", idFound.c_str(), newsp));
		return qtrue;
	} else {
		// Use repo definition path
		int userLevel = 0;
		size_t kpos = content.find('"' + def->id + '"');
		if (kpos != std::string::npos) {
			size_t lpos = content.find("\"level\"", kpos);
			if (lpos != std::string::npos) {
				size_t ccolon = content.find(':', lpos);
				if (ccolon != std::string::npos) userLevel = atoi(content.c_str() + ccolon + 1);
			}
		}
		if (userLevel >= def->maxLevel) { NXRP_PrintConsoleToPlayer(cl, "Skill already at max level"); return qtrue; }
		int nextCost = def->levels.empty() ? 0 : def->levels[userLevel].cost;
		if (sp < nextCost) { NXRP_PrintConsoleToPlayer(cl, "Not enough skillpoints for next level"); return qtrue; }

		int newsp = sp - nextCost;
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

		if (repl.find("\"skills\"") == std::string::npos) {
			size_t brace = repl.rfind('}');
			if (brace != std::string::npos) {
				std::string add = "  ,\"skills\": {\"" + def->id + "\": { \"level\": 1 } }\n";
				repl.insert(brace, add);
			}
		} else {
			size_t sj = repl.find(('"' + def->id + '"'));
			if (sj == std::string::npos) {
				size_t ks = repl.find("\"skills\"");
				size_t ob = repl.find('{', ks);
				if (ob != std::string::npos) {
					repl.insert(ob+1, " \"" + def->id + "\": { \"level\": 1 },");
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

		std::ofstream ofs(accPath, std::ios::trunc);
		if (!ofs.is_open()) { NXRP_PrintConsoleToPlayer(cl, "Failed to write account file"); return qtrue; }
		ofs << repl;
		ofs.close();

		NXRP_PrintConsoleToPlayer(cl, va("Purchased %s. Remaining skillpoints: %d", def->id.c_str(), newsp));
		return qtrue;
	}
}
#endif

struct NXRP_SimpleSkillDef {
	std::string id;
	std::string name;
	std::string description;
	int maxLevel = 0;
	std::vector<int> costs;
};

static std::vector<NXRP_SimpleSkillDef> g_nxrpSimpleSkills;

static std::string NXRP_ToLowerCopySimple(const std::string &in) {
	std::string out = in;
	for (auto &c : out) c = (char)std::tolower((unsigned char)c);
	return out;
}

void NXRP_EnsureRuntimeSkills(void) {
	// skills.json is optional in the minimal implementation
}

void NXRP_LoadRepoSkills() {
	// Always attempt to load the repository skills.json so runtime reflects
	// the current repo contents (useful during development). If the repo
	// file is missing or cannot be parsed, fall back to built-in defaults.
	g_nxrpSimpleSkills.clear();

	// Try to load repo skills.json using shared helpers.
	std::string content;
	const std::string repoPath = "codemp/server/nxrp/nxrp_skills/skills.json";
	std::vector<NXRP_SimpleSkill> parsed;
	if (NXRP_ReadFileToString(repoPath.c_str(), content) && NXRP_ParseSkillsJson(content, parsed)) {
		for (const auto &ps : parsed) {
			NXRP_SimpleSkillDef s;
			s.id = ps.id;
			s.name = ps.name;
			s.description = ps.description;
			s.maxLevel = ps.maxLevel;
			s.costs = ps.costs;
			g_nxrpSimpleSkills.push_back(s);
		}
		if (!g_nxrpSimpleSkills.empty()) return;
	}

	// Fallback to hardcoded defaults if file is missing or parse fails.
	NXRP_SimpleSkillDef jump;
	jump.id = "jump";
	jump.name = "Jump";
	jump.description = "Increases jump height.";
	jump.maxLevel = 3;
	jump.costs = {2, 5, 8};

	NXRP_SimpleSkillDef pistol;
	pistol.id = "pistol";
	pistol.name = "Pistol";
	pistol.description = "Improves pistol handling.";
	pistol.maxLevel = 2;
	pistol.costs = {1, 2};

	NXRP_SimpleSkillDef armor;
	armor.id = "armor";
	armor.name = "Armor";
	armor.description = "Increases damage resistance.";
	armor.maxLevel = 3;
	armor.costs = {2, 5, 8};

	g_nxrpSimpleSkills.push_back(jump);
	g_nxrpSimpleSkills.push_back(pistol);
	g_nxrpSimpleSkills.push_back(armor);
}

void NXRP_Skills_TestImpl(client_t* cl) {
	NXRP_Skills_ListImpl(cl, nullptr);
}

int NXRP_GetRepoSkillCount(void) {
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	return (int)g_nxrpSimpleSkills.size();
}

int NXRP_FindRepoSkillIndexById(const char *id) {
	if (!id) return -1;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	std::string want = NXRP_ToLowerCopySimple(id);
	for (size_t i = 0; i < g_nxrpSimpleSkills.size(); ++i) {
		if (NXRP_ToLowerCopySimple(g_nxrpSimpleSkills[i].id) == want) return (int)i;
	}
	return -1;
}

const char* NXRP_GetRepoSkillIdByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpSimpleSkills.size()) return nullptr;
	return g_nxrpSimpleSkills[idx].id.c_str();
}

const char* NXRP_GetRepoSkillNameByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpSimpleSkills.size()) return nullptr;
	return g_nxrpSimpleSkills[idx].name.c_str();
}

const char* NXRP_GetRepoSkillDescriptionByIndex(int idx) {
	if (idx < 0) return nullptr;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpSimpleSkills.size()) return nullptr;
	return g_nxrpSimpleSkills[idx].description.c_str();
}

int NXRP_GetRepoSkillMaxLevelByIndex(int idx) {
	if (idx < 0) return 0;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (idx >= (int)g_nxrpSimpleSkills.size()) return 0;
	return g_nxrpSimpleSkills[idx].maxLevel;
}

int NXRP_GetRepoSkillLevelCount(int skillIdx) {
	if (skillIdx < 0) return 0;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpSimpleSkills.size()) return 0;
	return (int)g_nxrpSimpleSkills[skillIdx].costs.size();
}

int NXRP_GetRepoSkillLevelCost(int skillIdx, int levelIdx) {
	if (skillIdx < 0 || levelIdx < 0) return 0;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();
	if (skillIdx >= (int)g_nxrpSimpleSkills.size()) return 0;
	if (levelIdx >= (int)g_nxrpSimpleSkills[skillIdx].costs.size()) return 0;
	return g_nxrpSimpleSkills[skillIdx].costs[levelIdx];
}

int NXRP_GetRepoSkillLevelEffectsCount(int skillIdx, int levelIdx) {
	(void)skillIdx;
	(void)levelIdx;
	return 0;
}

const char* NXRP_GetRepoSkillLevelEffectKey(int skillIdx, int levelIdx, int effectIdx) {
	(void)skillIdx;
	(void)levelIdx;
	(void)effectIdx;
	return nullptr;
}

const char* NXRP_GetRepoSkillLevelEffectValue(int skillIdx, int levelIdx, int effectIdx) {
	(void)skillIdx;
	(void)levelIdx;
	(void)effectIdx;
	return nullptr;
}

qboolean NXRP_Skills_ListImpl( client_t *cl, const char *homepath ) {
	(void)homepath;
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();

	NXRP_PrintConsoleToPlayer(cl, "----- Skills -----");
	for (int i = 0; i < (int)g_nxrpSimpleSkills.size(); ++i) {
		const auto &s = g_nxrpSimpleSkills[i];
		const int firstCost = s.costs.empty() ? 0 : s.costs[0];
		NXRP_PrintConsoleToPlayer(cl, va("%d. %s [0/%d]\t- Cost: %d (id: %s)", i + 1, s.name.c_str(), s.maxLevel, firstCost, s.id.c_str()));
	}
	NXRP_PrintConsoleToPlayer(cl, "------------------");
	NXRP_PrintConsoleToPlayer(cl, "type !skill <skillid> for details");
	return qtrue;
}

qboolean NXRP_Skill_PurchaseImpl( client_t *cl, const char *homepath, const char *skillToken ) {
	(void)homepath;
	if (!skillToken || !skillToken[0]) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !skill <skillid>");
		return qtrue;
	}
	if (g_nxrpSimpleSkills.empty()) NXRP_LoadRepoSkills();

	const int idx = NXRP_FindRepoSkillIndexById(skillToken);
	if (idx < 0) {
		NXRP_PrintConsoleToPlayer(cl, "Unknown skill id. Use !skills to list available skills.");
		return qtrue;
	}

	const auto &s = g_nxrpSimpleSkills[idx];
	const int firstCost = s.costs.empty() ? 0 : s.costs[0];
	NXRP_PrintConsoleToPlayer(cl, va("Skill: %s (id: %s)", s.name.c_str(), s.id.c_str()));
	NXRP_PrintConsoleToPlayer(cl, va("Description: %s", s.description.c_str()));
	NXRP_PrintConsoleToPlayer(cl, va("Max level: %d", s.maxLevel));
	NXRP_PrintConsoleToPlayer(cl, va("First level cost: %d", firstCost));
	NXRP_PrintConsoleToPlayer(cl, "Purchasing is temporarily disabled.");
	return qtrue;
}
