// NPC persistence and helpers (moved from codemp/server/nxrp/nxrp_npcs.cpp)
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
#include <string>
#include <deque>

// Pending NPC names queued until NXRP_RegisterSpawn associates them with entnums
static std::deque<std::string> nxrp_pending_npc_names;
struct nxrp_pending_npc_spawn_t {
	std::string name;
	vec3_t position;
	float yaw;
};
static std::deque<nxrp_pending_npc_spawn_t> nxrp_pending_npc_spawns;

void NXRP_PushPendingNPCName(const char* name) {
	if (!name) return;
	nxrp_pending_npc_names.push_back(std::string(name));
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_PushPendingNPCName queued: %s (queue=%zu)", name, nxrp_pending_npc_names.size());
}

void NXRP_PushPendingNPCSpawnAt(const char* name, const vec3_t position, float yaw) {
	if (!name || !name[0]) return;
	nxrp_pending_npc_spawn_t item;
	item.name = name;
	VectorCopy(position, item.position);
	item.yaw = yaw;
	nxrp_pending_npc_spawns.push_back(item);
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_PushPendingNPCSpawnAt queued: %s at %.0f %.0f %.0f yaw %.0f (queue=%zu)",
		name, position[0], position[1], position[2], yaw, nxrp_pending_npc_spawns.size());
}

// Remove a saved NPC entry by its zero-based index. This rewrites the per-map file.
bool NXRP_RemoveSavedNPCByIndex(int index) {
	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_npcs", lines, filename)) return false;

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
	if (!NXRP_WriteLinesToFile(filename, lines)) return false;
	return true;
}

// Forward-declare delayed executor used elsewhere
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Associate any pending NPC name with the newly-registered entity number
void NXRP_HandlePendingNPCSpawn(int entnum) {
	if (entnum < 0) return;
	if (!nxrp_pending_npc_spawns.empty()) {
		nxrp_pending_npc_spawn_t item = nxrp_pending_npc_spawns.front();
		nxrp_pending_npc_spawns.pop_front();
		sharedEntity_t* ent = SV_GentityNum(entnum);
		if (ent) {
			if (!NXRP_TeleportEntityTo(ent, item.position, item.yaw)) {
				VectorCopy(item.position, ent->r.currentOrigin);
				VectorCopy(item.position, ent->s.origin);
				if (ent->playerState) VectorCopy(item.position, ent->playerState->origin);
				VectorSet(ent->s.angles, 0.0f, item.yaw, 0.0f);
				VectorSet(ent->r.currentAngles, 0.0f, item.yaw, 0.0f);
				SV_LinkEntity(ent);
			}
		}
		NXRP_TrackNPC(entnum, item.name.c_str());
		NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_HandlePendingNPCSpawn: positioned ent=%d name=%s at %.0f %.0f %.0f yaw %.0f",
			entnum, item.name.c_str(), item.position[0], item.position[1], item.position[2], item.yaw);
		return;
	}

	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_HandlePendingNPCSpawn ent=%d queue=%zu", entnum, nxrp_pending_npc_names.size());
	if (nxrp_pending_npc_names.empty()) return;
	std::string name = nxrp_pending_npc_names.front();
	nxrp_pending_npc_names.pop_front();
	NXRP_TrackNPC(entnum, name.c_str());
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_HandlePendingNPCSpawn: tracked ent=%d name=%s", entnum, name.c_str());
}

void NXRP_TrackNPC(int entnum, const char* name) {
	(void)entnum;
	(void)name;
}

void NXRP_UntrackNPC(int entnum) {
	(void)entnum;
}

const char* NXRP_GetTrackedNPCName(int entnum) {
	(void)entnum;
	return NULL;
}

// Wrapper for freeing entities: if the entity being freed is a tracked NPC,
// call NXRP_OnNPCKilled before forwarding to the real G_FreeEntity in the VM.
void NXRP_FreeEntity(void* entPtr) {
	if (!entPtr) return;

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
	std::string filename;
	std::vector<std::string> lines;
	NXRP_GetPerMapFilePath("nxrp_npcs", filename);
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_SaveNPCToMap: will append to %s", filename.c_str());
	int nextIndex = 0;
	if (NXRP_ReadNonEmptyLinesFromFile(filename, lines)) nextIndex = (int)lines.size();

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

bool NXRP_SaveNPCAtClientToMap(client_t* cl, const char* npcType) {
	if (!cl || !cl->gentity || !npcType || !npcType[0]) return false;

	std::string filename;
	std::vector<std::string> lines;
	NXRP_GetPerMapFilePath("nxrp_npcs", filename);
	int nextIndex = 0;
	if (NXRP_ReadNonEmptyLinesFromFile(filename, lines)) nextIndex = (int)lines.size();

	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return false;

	const vec3_t& org = cl->gentity->r.currentOrigin;
	const vec3_t& ang = cl->gentity->s.angles;
	char buf[1024];
	snprintf(buf, sizeof(buf), "{ \"id\": %d, \"index\": %d, \"npc\": \"%s\", \"position\": [%.0f %.0f %.0f], \"angles\": [%.0f %.0f %.0f] }\n",
		nextIndex, nextIndex, npcType,
		org[0], org[1], org[2],
		ang[0], ang[1], ang[2]);
	fputs(buf, f);
	fclose(f);
	NXRP_PrintConsoleToPlayerFmt(cl, "Saved NPC %s at position %.0f %.0f %.0f (index %d)", npcType, org[0], org[1], org[2], nextIndex);
	return true;
}

// Read the per-map JSON file and send entries to the player as an indexed list
void NXRP_ListSavedNPCs(client_t* cl) {
	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_npcs", lines, filename)) { NXRP_PrintConsoleToPlayer(cl, "No saved NPCs for this map"); return; }

	int idx = 0;
	bool any = false;
	for (const auto &s : lines) {
		if (s.empty()) continue;
		any = true;
		int idv = NXRP_ExtractIntField(s, "\"id\"", -1);

		// extract npc type
		std::string npcType;
		size_t mpos = s.find("\"npc\"");
		if (mpos != std::string::npos) {
			size_t colon = s.find(':', mpos);
			if (colon != std::string::npos) {
				size_t q = s.find('"', colon);
				if (q != std::string::npos) {
					size_t qe = s.find('"', q+1);
					if (qe != std::string::npos && qe > q) npcType = s.substr(q+1, qe-q-1);
				}
			}
		}

		// extract position and angles
		float ox=0, oy=0, oz=0, ax=0, ay=0, az=0;
		size_t opos = s.find("\"position\"");
		if (opos == std::string::npos) opos = s.find("\"origin\"");
		if (opos != std::string::npos) {
			opos = s.find('[', opos);
			if (opos != std::string::npos) {
				sscanf(s.c_str()+opos, "[%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz);
			}
		}
		size_t apos = s.find("\"angles\"");
		if (apos != std::string::npos) {
			apos = s.find('[', apos);
			if (apos != std::string::npos) {
				sscanf(s.c_str()+apos, "[%f %*[, ] %f %*[, ] %f", &ax, &ay, &az);
			}
		}

		if (npcType.empty()) npcType = "<unknown>";
		NXRP_PrintConsoleToPlayerFmt(cl, "%d: id=%d npc=%s position=%.0f,%.0f,%.0f angles=%.0f,%.0f,%.0f", idx, idv, npcType.c_str(), ox, oy, oz, ax, ay, az);
		idx++;
	}
	if (!any) NXRP_PrintConsoleToPlayer(cl, "No saved NPCs for this map");
}

// Load and spawn all saved NPC entries for the current map. Returns number spawned.
int NXRP_LoadSavedNPCsForMap(client_t* cl) {
	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_npcs", lines, filename)) return 0;

	sharedEntity_t* spawnerEnt = (cl && cl->gentity) ? cl->gentity : NULL;
	if (!spawnerEnt) {
		for (int i = 0; i < sv_maxclients->integer; ++i) {
			if (svs.clients[i].state == CS_ACTIVE && svs.clients[i].gentity) {
				spawnerEnt = svs.clients[i].gentity;
				break;
			}
		}
	}

	void* dll = GVM_GetDllHandle();
	void* (*gNPCSpawnType)(void*, char*, char*, int, int, int) = NULL;
	if (dll) {
		gNPCSpawnType = (void* (*)(void*, char*, char*, int, int, int))Sys_LoadFunction(dll, "NPC_SpawnType");
	}

	int count = 0;
	for (const auto &line : lines) {
		if (line.empty()) continue;
		// extract npc type
		std::string s = line;
		std::string npcType;
		float px = 0.0f, py = 0.0f, pz = 0.0f;
		float ax = 0.0f, ay = 0.0f, az = 0.0f;
		size_t mpos = s.find("\"npc\"");
		if (mpos != std::string::npos) {
			size_t colon = s.find(':', mpos);
			if (colon != std::string::npos) {
				size_t q = s.find('"', colon);
				if (q != std::string::npos) {
					size_t qe = s.find('"', q+1);
					if (qe != std::string::npos && qe > q) npcType = s.substr(q+1, qe-q-1);
				}
			}
		}
		size_t ppos = s.find("\"position\"");
		if (ppos != std::string::npos) {
			ppos = s.find('[', ppos);
			if (ppos != std::string::npos) {
				sscanf(s.c_str()+ppos, "[%f %*[, ] %f %*[, ] %f", &px, &py, &pz);
			}
		}
		size_t apos = s.find("\"angles\"");
		if (apos != std::string::npos) {
			apos = s.find('[', apos);
			if (apos != std::string::npos) {
				sscanf(s.c_str()+apos, "[%f %*[, ] %f %*[, ] %f", &ax, &ay, &az);
			}
		}
		if (npcType.empty()) continue;
		vec3_t savedPos = { px, py, pz };

		if (gNPCSpawnType && spawnerEnt) {
			void* old = GVM_BeginNative();
			sharedEntity_t* spawned = (sharedEntity_t*)gNPCSpawnType(spawnerEnt, (char*)npcType.c_str(), NULL, 0, 0, 0);
			GVM_EndNative(old);
			if (spawned) {
				NXRP_TeleportEntityTo(spawned, savedPos, ay);
				NXRP_TrackNPC(spawned->s.number, npcType.c_str());
				if (cl) {
					NXRP_PrintConsoleToPlayerFmt(cl, "Loaded NPC %s at %.0f %.0f %.0f", npcType.c_str(), px, py, pz);
				}
				count++;
				continue;
			}
		}

		NXRP_PushPendingNPCSpawnAt(npcType.c_str(), savedPos, ay);
		char cmdBuf[256];
		Com_sprintf(cmdBuf, sizeof(cmdBuf), "npc spawn %s", npcType.c_str());
		SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), count + 1);
		count++;
	}
	return count;
}
