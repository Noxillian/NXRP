#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cctype>

// Forward declaration from spin.cpp
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

struct nxrp_quest_npc_t {
	std::string name;
	vec3_t pos;
};

struct nxrp_quest_t {
	std::string questName;
	qboolean hasSpawn;
	vec3_t questPos;
	std::vector<nxrp_quest_npc_t> npcs;
	int rewardExp;
	int rewardCredits;
	int questTime;
};

static bool NXRP_QuestFilePath(std::string& outPath) {
	return NXRP_GetPerMapFilePath("nxrp_quests", outPath);
}

static std::string NXRP_QuestEscape(const std::string& in) {
	return NXRP_EscapeString(in);
}

static std::string NXRP_QuestReadFileOrEmpty(const std::string& path) {
	std::string content;
	std::ifstream ifs(path.c_str());
	if (!ifs.is_open()) return std::string();
	content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return content;
}

static bool NXRP_QuestWriteContent(const std::string& path, const std::string& content) {
	std::ofstream ofs(path.c_str(), std::ios::trunc);
	if (!ofs.is_open()) return false;
	ofs << content;
	ofs.close();
	return !ofs.fail();
}

static void NXRP_QuestSkipWs(const std::string& s, size_t& p) {
	while (p < s.size() && isspace((unsigned char)s[p])) ++p;
}

static bool NXRP_QuestParseQuoted(const std::string& s, size_t& p, std::string& out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != '"') return false;
	++p;
	std::string res;
	while (p < s.size()) {
		char c = s[p++];
		if (c == '\\' && p < s.size()) {
			res.push_back(s[p++]);
			continue;
		}
		if (c == '"') { out = res; return true; }
		res.push_back(c);
	}
	return false;
}

static bool NXRP_QuestParseInt(const std::string& s, size_t& p, int& out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size()) return false;
	int sign = 1;
	if (s[p] == '-') { sign = -1; ++p; }
	if (p >= s.size() || !isdigit((unsigned char)s[p])) return false;
	int v = 0;
	while (p < s.size() && isdigit((unsigned char)s[p])) {
		v = v * 10 + (s[p] - '0');
		++p;
	}
	out = v * sign;
	return true;
}

static bool NXRP_QuestParseFloat(const std::string& s, size_t& p, float& out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size()) return false;
	char* endPtr = NULL;
	double v = strtod(s.c_str() + p, &endPtr);
	if (!endPtr || endPtr == s.c_str() + p) return false;
	p = (size_t)(endPtr - s.c_str());
	out = (float)v;
	return true;
}

static bool NXRP_QuestParseVec3(const std::string& s, size_t& p, vec3_t out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != '[') return false;
	++p;
	float x=0, y=0, z=0;
	if (!NXRP_QuestParseFloat(s, p, x)) return false;
	NXRP_QuestSkipWs(s, p); if (p < s.size() && s[p] == ',') ++p;
	if (!NXRP_QuestParseFloat(s, p, y)) return false;
	NXRP_QuestSkipWs(s, p); if (p < s.size() && s[p] == ',') ++p;
	if (!NXRP_QuestParseFloat(s, p, z)) return false;
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != ']') return false;
	++p;
	out[0] = x; out[1] = y; out[2] = z;
	return true;
}

static bool NXRP_QuestParseNpcEntry(const std::string& s, size_t& p, nxrp_quest_npc_t& out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != '{') return false;
	++p;
	out.name.clear();
	VectorSet(out.pos, 0, 0, 0);
	while (p < s.size()) {
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == '}') { ++p; return true; }
		std::string key;
		if (!NXRP_QuestParseQuoted(s, p, key)) return false;
		NXRP_QuestSkipWs(s, p); if (p >= s.size() || s[p] != ':') return false; ++p;
		if (key == "name") {
			if (!NXRP_QuestParseQuoted(s, p, out.name)) return false;
		} else if (key == "pos") {
			if (!NXRP_QuestParseVec3(s, p, out.pos)) return false;
		} else {
			std::string dummy;
			if (p < s.size() && s[p] == '"') { if (!NXRP_QuestParseQuoted(s, p, dummy)) return false; }
			else if (p < s.size() && s[p] == '[') { vec3_t v; if (!NXRP_QuestParseVec3(s, p, v)) return false; }
			else { int iv=0; if (!NXRP_QuestParseInt(s, p, iv)) return false; }
		}
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == ',') ++p;
	}
	return false;
}

static bool NXRP_QuestParseNpcArray(const std::string& s, size_t& p, std::vector<nxrp_quest_npc_t>& out) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != '[') return false;
	++p;
	out.clear();
	while (p < s.size()) {
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == ']') { ++p; return true; }
		nxrp_quest_npc_t npc;
		if (!NXRP_QuestParseNpcEntry(s, p, npc)) return false;
		if (!npc.name.empty()) out.push_back(npc);
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == ',') ++p;
	}
	return false;
}

static bool NXRP_QuestParseOne(const std::string& s, size_t& p, nxrp_quest_t& q) {
	NXRP_QuestSkipWs(s, p);
	if (p >= s.size() || s[p] != '{') return false;
	++p;
	q.questName.clear();
	q.hasSpawn = qfalse;
	VectorSet(q.questPos, 0, 0, 0);
	q.npcs.clear();
	q.rewardExp = 0;
	q.rewardCredits = 0;
	q.questTime = 0;

	while (p < s.size()) {
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == '}') { ++p; return true; }
		std::string key;
		if (!NXRP_QuestParseQuoted(s, p, key)) return false;
		NXRP_QuestSkipWs(s, p); if (p >= s.size() || s[p] != ':') return false; ++p;

		if (key == "questName") {
			if (!NXRP_QuestParseQuoted(s, p, q.questName)) return false;
		} else if (key == "questPos") {
			NXRP_QuestSkipWs(s, p);
			if (p < s.size() && s[p] == '[') {
				size_t peek = p + 1;
				NXRP_QuestSkipWs(s, peek);
				if (peek < s.size() && s[peek] == ']') {
					p = peek + 1;
					q.hasSpawn = qfalse;
				} else {
					if (!NXRP_QuestParseVec3(s, p, q.questPos)) return false;
					q.hasSpawn = qtrue;
				}
			} else {
				return false;
			}
		} else if (key == "questNpcs") {
			if (!NXRP_QuestParseNpcArray(s, p, q.npcs)) return false;
		} else if (key == "questRewardExp") {
			if (!NXRP_QuestParseInt(s, p, q.rewardExp)) return false;
		} else if (key == "questRewardCredits") {
			if (!NXRP_QuestParseInt(s, p, q.rewardCredits)) return false;
		} else if (key == "questTime") {
			if (!NXRP_QuestParseInt(s, p, q.questTime)) return false;
		} else {
			if (p < s.size() && s[p] == '"') {
				std::string dummy; if (!NXRP_QuestParseQuoted(s, p, dummy)) return false;
			} else if (p < s.size() && s[p] == '[') {
				vec3_t v; if (!NXRP_QuestParseVec3(s, p, v)) {
					std::vector<nxrp_quest_npc_t> n; if (!NXRP_QuestParseNpcArray(s, p, n)) return false;
				}
			} else {
				int iv=0; if (!NXRP_QuestParseInt(s, p, iv)) return false;
			}
		}

		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == ',') ++p;
	}
	return false;
}

static bool NXRP_QuestLoadAll(std::vector<nxrp_quest_t>& out) {
	out.clear();
	std::string path;
	if (!NXRP_QuestFilePath(path)) return false;
	std::string s = NXRP_QuestReadFileOrEmpty(path);
	if (s.empty()) return true;

	size_t p = s.find('{');
	if (p == std::string::npos) return true;
	++p;
	while (p < s.size()) {
		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == '}') break;

		std::string key;
		if (!NXRP_QuestParseQuoted(s, p, key)) break;
		NXRP_QuestSkipWs(s, p);
		if (p >= s.size() || s[p] != ':') break;
		++p;
		NXRP_QuestSkipWs(s, p);

		if (!Q_stricmp(key.c_str(), "quests")) {
			// Backward compatibility with legacy wrapper object.
			if (p < s.size() && s[p] == '[') {
				++p;
				while (p < s.size()) {
					NXRP_QuestSkipWs(s, p);
					if (p < s.size() && s[p] == ']') break;
					nxrp_quest_t q;
					if (!NXRP_QuestParseOne(s, p, q)) break;
					if (!q.questName.empty()) out.push_back(q);
					NXRP_QuestSkipWs(s, p);
					if (p < s.size() && s[p] == ',') ++p;
				}
			} else if (p < s.size() && s[p] == '{') {
				++p;
				while (p < s.size()) {
					NXRP_QuestSkipWs(s, p);
					if (p < s.size() && s[p] == '}') break;
					std::string questKey;
					if (!NXRP_QuestParseQuoted(s, p, questKey)) break;
					NXRP_QuestSkipWs(s, p);
					if (p >= s.size() || s[p] != ':') break;
					++p;
					nxrp_quest_t q;
					if (!NXRP_QuestParseOne(s, p, q)) break;
					if (q.questName.empty()) q.questName = questKey;
					out.push_back(q);
					NXRP_QuestSkipWs(s, p);
					if (p < s.size() && s[p] == ',') ++p;
				}
			}
		} else {
			// Preferred format: top-level key is quest name.
			nxrp_quest_t q;
			if (!NXRP_QuestParseOne(s, p, q)) break;
			if (q.questName.empty()) q.questName = key;
			out.push_back(q);
		}

		NXRP_QuestSkipWs(s, p);
		if (p < s.size() && s[p] == ',') ++p;
	}
	return true;
}

static std::string NXRP_QuestSerialize(const std::vector<nxrp_quest_t>& quests) {
	std::ostringstream os;
	os << "{\n";
	for (size_t i = 0; i < quests.size(); ++i) {
		const nxrp_quest_t& q = quests[i];
		os << "  \"" << NXRP_QuestEscape(q.questName) << "\": { ";
		if (q.hasSpawn) os << "\"questPos\": [" << (int)q.questPos[0] << ", " << (int)q.questPos[1] << ", " << (int)q.questPos[2] << "], ";
		else os << "\"questPos\": [], ";
		os << "\"questNpcs\": [";
		for (size_t n = 0; n < q.npcs.size(); ++n) {
			const nxrp_quest_npc_t& npc = q.npcs[n];
			os << "{ \"name\": \"" << NXRP_QuestEscape(npc.name) << "\", \"pos\": [" << (int)npc.pos[0] << ", " << (int)npc.pos[1] << ", " << (int)npc.pos[2] << "] }";
			if (n + 1 < q.npcs.size()) os << ", ";
		}
		os << "], ";
		os << "\"questRewardExp\": " << q.rewardExp << ", ";
		os << "\"questRewardCredits\": " << q.rewardCredits << ", ";
		os << "\"questTime\": " << q.questTime << " }";
		if (i + 1 < quests.size()) os << ",";
		os << "\n";
	}
	os << "}\n";
	return os.str();
}

static bool NXRP_QuestSaveAll(const std::vector<nxrp_quest_t>& quests) {
	std::string path;
	if (!NXRP_QuestFilePath(path)) return false;
	return NXRP_QuestWriteContent(path, NXRP_QuestSerialize(quests));
}

static int NXRP_QuestFindByName(const std::vector<nxrp_quest_t>& quests, const char* questName) {
	if (!questName || !questName[0]) return -1;
	for (size_t i = 0; i < quests.size(); ++i) {
		if (!Q_stricmp(quests[i].questName.c_str(), questName)) return (int)i;
	}
	return -1;
}

bool NXRP_QuestAdd(client_t* cl, const char* questName) {
	if (!questName || !questName[0]) return false;
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	if (NXRP_QuestFindByName(quests, questName) >= 0) {
		NXRP_PrintConsoleToPlayer(cl, "Quest already exists");
		return false;
	}
	nxrp_quest_t q;
	q.questName = questName;
	q.hasSpawn = qfalse;
	if (cl && cl->gentity) {
		q.hasSpawn = qtrue;
		VectorCopy(cl->gentity->r.currentOrigin, q.questPos);
	} else {
		VectorSet(q.questPos, 0,0,0);
	}
	q.rewardExp = 0;
	q.rewardCredits = 0;
	q.questTime = 0;
	quests.push_back(q);
	if (!NXRP_QuestSaveAll(quests)) return false;
	if (q.hasSpawn) {
		NXRP_PrintConsoleToPlayer(cl, "Quest added: %s (spawn %.0f %.0f %.0f)", questName, q.questPos[0], q.questPos[1], q.questPos[2]);
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Quest added: %s", questName);
	}
	return true;
}

void NXRP_QuestList(client_t* cl) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	if (quests.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "No quests for this map");
		return;
	}
	for (size_t i = 0; i < quests.size(); ++i) {
		const nxrp_quest_t& q = quests[i];
		NXRP_PrintConsoleToPlayer(cl, "%d: %s spawn=%s npcs=%d exp=%d credits=%d time=%d",
			(int)i,
			q.questName.c_str(),
			q.hasSpawn ? "yes" : "no",
			(int)q.npcs.size(),
			q.rewardExp,
			q.rewardCredits,
			q.questTime);
	}
}

bool NXRP_QuestRemoveByName(client_t* cl, const char* questName) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	quests.erase(quests.begin() + idx);
	if (!NXRP_QuestSaveAll(quests)) return false;
	NXRP_PrintConsoleToPlayer(cl, "Quest removed: %s", questName);
	return true;
}

bool NXRP_QuestSetSpawn(client_t* cl, const char* questName, const vec3_t spawnPos) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	quests[idx].hasSpawn = qtrue;
	VectorCopy(spawnPos, quests[idx].questPos);
	if (!NXRP_QuestSaveAll(quests)) return false;
	NXRP_PrintConsoleToPlayer(cl, "Quest %s spawn set to %.0f %.0f %.0f", questName, spawnPos[0], spawnPos[1], spawnPos[2]);
	return true;
}

bool NXRP_QuestAddNpcAt(client_t* cl, const char* questName, const char* npcName, const vec3_t npcPos) {
	if (!npcName || !npcName[0]) return false;
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	nxrp_quest_npc_t npc;
	npc.name = npcName;
	VectorCopy(npcPos, npc.pos);
	quests[idx].npcs.push_back(npc);
	if (!NXRP_QuestSaveAll(quests)) return false;
	NXRP_PrintConsoleToPlayer(cl, "Quest %s NPC added: %s at %.0f %.0f %.0f", questName, npcName, npcPos[0], npcPos[1], npcPos[2]);
	return true;
}

bool NXRP_QuestSetRewardExp(client_t* cl, const char* questName, int exp) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	quests[idx].rewardExp = exp;
	if (!NXRP_QuestSaveAll(quests)) return false;
	NXRP_PrintConsoleToPlayer(cl, "Quest %s rewardExp set to %d", questName, exp);
	return true;
}

bool NXRP_QuestSetRewardCredits(client_t* cl, const char* questName, int credits) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	quests[idx].rewardCredits = credits;
	if (!NXRP_QuestSaveAll(quests)) return false;
	NXRP_PrintConsoleToPlayer(cl, "Quest %s rewardCredits set to %d", questName, credits);
	return true;
}

bool NXRP_QuestHasSpawn(const char* questName) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	return quests[idx].hasSpawn ? true : false;
}

bool NXRP_QuestHasNpcs(const char* questName) {
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) return false;
	return !quests[idx].npcs.empty();
}

bool NXRP_QuestStartForPlayer(client_t* cl, const char* questName) {
	if (!cl || !cl->gentity || !questName || !questName[0]) return false;
	std::vector<nxrp_quest_t> quests;
	NXRP_QuestLoadAll(quests);
	const int idx = NXRP_QuestFindByName(quests, questName);
	if (idx < 0) {
		NXRP_PrintConsoleToPlayer(cl, "Quest not found");
		return false;
	}
	nxrp_quest_t& q = quests[idx];
	vec3_t tp;
	if (q.hasSpawn) {
		VectorCopy(q.questPos, tp);
	} else {
		VectorCopy(cl->gentity->r.currentOrigin, tp);
	}
	float yaw = cl->gentity->s.angles[YAW];
	NXRP_TeleportClientToPosition(cl, tp, yaw);

	for (size_t i = 0; i < q.npcs.size(); ++i) {
		const nxrp_quest_npc_t& npc = q.npcs[i];
		NXRP_PushPendingNPCSpawnAt(npc.name.c_str(), npc.pos, yaw);
		NXRP_PushPendingNPCName(npc.name.c_str());
		char cmdBuf[128];
		Com_sprintf(cmdBuf, sizeof(cmdBuf), "npc spawn %s", npc.name.c_str());
		SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), (int)i + 1);
	}
	NXRP_PrintConsoleToPlayer(cl, "Quest started: %s", q.questName.c_str());
	return true;
}
