// NPC persistence and helpers
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <string>
#include <sys/stat.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstdio>
#include <map>
#include <string>
#include <deque>

// Simple tracking map for NPC entnums -> name
static std::map<int, std::string> nxrp_tracked_npcs;
// Pending NPC names queued until NXRP_RegisterSpawn associates them with entnums
static std::deque<std::string> nxrp_pending_npc_names;

void NXRP_PushPendingNPCName(const char* name) {
	if (!name) return;
	nxrp_pending_npc_names.push_back(std::string(name));
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_PushPendingNPCName queued: %s (queue=%zu)", name, nxrp_pending_npc_names.size());
}

// Remove a saved NPC entry by its zero-based index. This rewrites the per-map file.
bool NXRP_RemoveSavedNPCByIndex(int index) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) return false;

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(ifs, line)) { if (!line.empty()) lines.push_back(line); }
	ifs.close();

	if (index < 0 || index >= (int)lines.size()) return false;

	// Attempt to free the entity referenced by this saved line if present
	std::string targetLine = lines[index];
	int entToFree = -1;
	size_t entPos = targetLine.find("\"ent\"");
	if (entPos != std::string::npos) {
		size_t colon = targetLine.find(':', entPos);
		if (colon != std::string::npos) {
			size_t valStart = colon + 1;
			while (valStart < targetLine.size() && isspace((unsigned char)targetLine[valStart])) ++valStart;
			size_t valEnd = valStart;
			if (valEnd < targetLine.size() && (targetLine[valEnd] == '-' || isdigit((unsigned char)targetLine[valEnd]))) ++valEnd;
			while (valEnd < targetLine.size() && isdigit((unsigned char)targetLine[valEnd])) ++valEnd;
			if (valEnd > valStart) {
				try { entToFree = std::stoi(targetLine.substr(valStart, valEnd - valStart)); } catch(...) { entToFree = -1; }
			}
		}
	}

	if (entToFree >= 0) {
		if (entToFree >= 0 && entToFree < sv.num_entities) {
			sharedEntity_t* e = SV_GentityNum(entToFree);
			if (e) {
				void* dll = GVM_GetDllHandle();
				if (dll) {
					void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
					if (G_FreeEntity_f) {
						void* old = GVM_BeginNative();
						G_FreeEntity_f(e);
						GVM_EndNative(old);
						NXRP_RegisterSpawn(-1);
					}
				}
			}
		}
	}

	// Remove the entry and rewrite
	lines.erase(lines.begin() + index);
	FILE* f = fopen(filename.c_str(), "w");
	if (!f) return false;
	for (const auto &l : lines) { fputs(l.c_str(), f); fputc('\n', f); }
	fclose(f);
	return true;
}

// Forward-declare delayed executor used elsewhere
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Associate any pending NPC name with the newly-registered entity number
void NXRP_HandlePendingNPCSpawn(int entnum) {
	if (entnum < 0) return;
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_HandlePendingNPCSpawn ent=%d queue=%zu", entnum, nxrp_pending_npc_names.size());
	if (nxrp_pending_npc_names.empty()) return;
	std::string name = nxrp_pending_npc_names.front();
	nxrp_pending_npc_names.pop_front();
	NXRP_TrackNPC(entnum, name.c_str());
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_HandlePendingNPCSpawn: saving ent=%d name=%s", entnum, name.c_str());
	NXRP_SaveNPCToMap(entnum, name.c_str());
}

void NXRP_TrackNPC(int entnum, const char* name) {
	if (entnum < 0) return;
	if (!name) return;
	nxrp_tracked_npcs[entnum] = std::string(name);
}

void NXRP_UntrackNPC(int entnum) {
	nxrp_tracked_npcs.erase(entnum);
}

const char* NXRP_GetTrackedNPCName(int entnum) {
	auto it = nxrp_tracked_npcs.find(entnum);
	if (it == nxrp_tracked_npcs.end()) return NULL;
	return it->second.c_str();
}

// Wrapper for freeing entities: if the entity being freed is a tracked NPC,
// call NXRP_OnNPCKilled before forwarding to the real G_FreeEntity in the VM.
void NXRP_FreeEntity(void* entPtr) {
	if (!entPtr) return;
	sharedEntity_t* e = (sharedEntity_t*)entPtr;
	int entnum = e->s.number;
	auto it = nxrp_tracked_npcs.find(entnum);
	if (it != nxrp_tracked_npcs.end()) {
		NXRP_OnNPCKilled(NULL, it->second.c_str());
		nxrp_tracked_npcs.erase(it);
	}

	// Forward to the real VM G_FreeEntity function
	void* dll = GVM_GetDllHandle();
	if (!dll) return;
	void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
	if (!G_FreeEntity_f) return;
	void* old = GVM_BeginNative();
	G_FreeEntity_f(entPtr);
	GVM_EndNative(old);
}

// Persist a spawned NPC to a per-map JSON file
void NXRP_SaveNPCToMap(int entnum, const char* npcType) {
	if (entnum < 0 || !npcType) return;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	char buf[1024];
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), NULL);
#else
	mkdir(dir.c_str(), 0755);
#endif

	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_SaveNPCToMap: will append to %s", filename.c_str());

	int nextIndex = 0;
	{
		std::ifstream existing(filename);
		if (existing.is_open()) {
			std::string l;
			while (std::getline(existing, l)) { if (!l.empty()) nextIndex++; }
			existing.close();
		}
	}

	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	float ox = e->r.currentOrigin[0];
	float oy = e->r.currentOrigin[1];
	float oz = e->r.currentOrigin[2];
	float ax = e->r.currentAngles[0];
	float ay = e->r.currentAngles[1];
	float az = e->r.currentAngles[2];

	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"index\": %d, \"npc\": \"%s\", \"origin\": [%.0f %.0f %.0f], \"angles\": [%.0f %.0f %.0f] }\n",
		entnum, nextIndex, npcType, ox, oy, oz, ax, ay, az);
	fputs(buf, f);
	fclose(f);
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_SaveNPCToMap: saved ent=%d type=%s to %s", entnum, npcType, filename.c_str());
}

// Read the per-map JSON file and send entries to the player as an indexed list
void NXRP_ListSavedNPCs(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "No saved NPCs for this map");
		return;
	}

	std::string line;
	int idx = 0;
	bool any = false;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		any = true;
		std::string s = line;
		// extract npc type
		std::string npc;
		size_t mpos = s.find("\"npc\"");
		if (mpos != std::string::npos) {
			size_t colon = s.find(':', mpos);
			if (colon != std::string::npos) {
				size_t q = s.find('"', colon);
				if (q != std::string::npos) {
					size_t qe = s.find('"', q+1);
					if (qe != std::string::npos && qe > q) npc = s.substr(q+1, qe-q-1);
				}
			}
		}
		if (npc.empty()) npc = "<unknown>";
		NXRP_PrintConsoleToPlayerFmt(cl, "%d: npc=%s", idx, npc.c_str());
		idx++;
	}
	if (!any) NXRP_PrintConsoleToPlayer(cl, "No saved NPCs for this map");
	ifs.close();
}

// Load and spawn all saved NPC entries for the current map. Returns number spawned
int NXRP_LoadSavedNPCsForMap(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) { return 0; }

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(ifs, line)) { if (!line.empty()) lines.push_back(line); }
	ifs.close();

	int spawned = 0;
	for (const auto &l : lines) {
		// parse npc type
		const char* s = l.c_str();
		std::string npcType;
		const char* key = "\"npc\"";
		const char* p = strstr(s, key);
		if (p) {
			p = strchr(p, ':'); if (p) p++; while (*p && isspace((unsigned char)*p)) p++;
			if (*p == '"') { p++; const char* e = strchr(p, '"'); if (e && e > p) npcType.assign(p, e - p); }
		}
		if (npcType.empty()) continue;

		NXRP_PushPendingNPCName(npcType.c_str());
		char cmdBuf[128];
		Com_sprintf(cmdBuf, sizeof(cmdBuf), "npc spawn %s", npcType.c_str());
		SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
		spawned++;
	}

	return spawned;
}
