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
#include <map>
#include <set>

// Pending NPC names queued until NXRP_RegisterSpawn associates them with entnums
static std::deque<std::string> nxrp_pending_npc_names;
struct nxrp_pending_npc_spawn_t {
	std::string name;
	vec3_t position;
	float yaw;
};
static std::deque<nxrp_pending_npc_spawn_t> nxrp_pending_npc_spawns;

struct nxrp_tracked_npc_t {
	std::string name;
	int lastHealth;
	qboolean deadSeen;
	qboolean fromSpawnCmd;
};

static std::map<int, nxrp_tracked_npc_t> nxrp_tracked_npcs;
static std::set<int> nxrp_known_npc_ents;

void NXRP_PushPendingNPCName(const char* name) {
	if (!name) return;
	nxrp_pending_npc_names.push_back(std::string(name));
	NXRP_PrintConsoleToPlayer(NULL, "NXRP_PushPendingNPCName queued: %s (queue=%zu)", name, nxrp_pending_npc_names.size());
}

void NXRP_PushPendingNPCSpawnAt(const char* name, const vec3_t position, float yaw) {
	if (!name || !name[0]) return;
	nxrp_pending_npc_spawn_t item;
	item.name = name;
	VectorCopy(position, item.position);
	item.yaw = yaw;
	nxrp_pending_npc_spawns.push_back(item);
	NXRP_PrintConsoleToPlayer(NULL, "NXRP_PushPendingNPCSpawnAt queued: %s at %.0f %.0f %.0f yaw %.0f (queue=%zu)",
		name, position[0], position[1], position[2], yaw, nxrp_pending_npc_spawns.size());
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
		return;
	}
	if (nxrp_pending_npc_names.empty()) return;
	std::string name = nxrp_pending_npc_names.front();
	nxrp_pending_npc_names.pop_front();
	NXRP_TrackNPC(entnum, name.c_str());
}

void NXRP_TrackNPC(int entnum, const char* name) {
	if (entnum < 0 || !name || !name[0]) return;
	nxrp_tracked_npc_t data;
	data.name = name;
	data.lastHealth = 0;
	data.deadSeen = qfalse;
	data.fromSpawnCmd = qtrue;
	sharedEntity_t* ent = (entnum >= 0 && entnum < sv.num_entities) ? SV_GentityNum(entnum) : NULL;
	if (ent && ent->s.number == entnum) {
		data.lastHealth = ent->health;
	}
	nxrp_tracked_npcs[entnum] = data;
	nxrp_known_npc_ents.insert(entnum);
	NXRP_PrintConsoleToPlayer(NULL, "NXRP: tracking npc ent=%d name=%s", entnum, name);
}

void NXRP_UntrackNPC(int entnum) {
	if (entnum < 0) return;
	const char* tracked = NXRP_GetTrackedNPCName(entnum);
	if (tracked) {
		NXRP_PrintConsoleToPlayer(NULL, "NXRP: untracking npc ent=%d name=%s", entnum, tracked);
	}
	nxrp_tracked_npcs.erase(entnum);
	nxrp_known_npc_ents.erase(entnum);
}

const char* NXRP_GetTrackedNPCName(int entnum) {
	if (entnum < 0) return NULL;
	std::map<int, nxrp_tracked_npc_t>::iterator it = nxrp_tracked_npcs.find(entnum);
	if (it == nxrp_tracked_npcs.end()) return NULL;
	return it->second.name.c_str();
}

static qboolean NXRP_IsLiveNpcEnt(int entnum, sharedEntity_t** outEnt) {
	if (outEnt) *outEnt = NULL;
	if (entnum < 0 || entnum >= sv.num_entities) return qfalse;
	sharedEntity_t* ent = SV_GentityNum(entnum);
	if (!ent) return qfalse;
	if (ent->s.number != entnum) return qfalse;
	if (ent->s.eType != ET_NPC) return qfalse;
	if (outEnt) *outEnt = ent;
	return qtrue;
}

void NXRP_NPCTickTracking(void) {
	std::set<int> currentNpcEnts;

	for (int i = 0; i < sv.num_entities; ++i) {
		sharedEntity_t* ent = NULL;
		if (!NXRP_IsLiveNpcEnt(i, &ent)) continue;
		currentNpcEnts.insert(i);

		if (nxrp_known_npc_ents.find(i) == nxrp_known_npc_ents.end()) {
			nxrp_known_npc_ents.insert(i);
			if (!nxrp_pending_npc_names.empty()) {
				std::string name = nxrp_pending_npc_names.front();
				nxrp_pending_npc_names.pop_front();
				NXRP_TrackNPC(i, name.c_str());
				NXRP_PrintConsoleToPlayer(NULL, "NXRP: bound pending !spawn NPC ent=%d name=%s", i, name.c_str());
			}
		}
	}

	for (std::set<int>::iterator it = nxrp_known_npc_ents.begin(); it != nxrp_known_npc_ents.end(); ) {
		if (currentNpcEnts.find(*it) == currentNpcEnts.end()) it = nxrp_known_npc_ents.erase(it);
		else ++it;
	}

	std::vector<int> toUntrack;
	for (std::map<int, nxrp_tracked_npc_t>::iterator it = nxrp_tracked_npcs.begin(); it != nxrp_tracked_npcs.end(); ++it) {
		const int entnum = it->first;
		nxrp_tracked_npc_t& tracked = it->second;
		const char* killerName = "Unknown";
		sharedEntity_t* ent = NULL;
		if (!NXRP_IsLiveNpcEnt(entnum, &ent)) {
			if (!tracked.deadSeen) {
				NXRP_PrintConsoleToPlayer(NULL, "%s killed %s", killerName, tracked.name.c_str());
				tracked.deadSeen = qtrue;
			}
			toUntrack.push_back(entnum);
			continue;
		}

		if (ent->playerState) {
			const int attackerNum = ent->playerState->persistant[PERS_ATTACKER];
			if (attackerNum >= 0 && attackerNum < sv_maxclients->integer) {
				client_t* attacker = &svs.clients[attackerNum];
				if (attacker) {
					if (attacker->nxrp_username[0]) killerName = attacker->nxrp_username;
					else if (attacker->name[0]) killerName = attacker->name;
				}
			}
		}

		const int health = (ent->playerState) ? ent->playerState->stats[STAT_HEALTH] : ent->health;
		const qboolean deadByPmType = (ent->playerState && ent->playerState->pm_type == MB2_PM_DEAD) ? qtrue : qfalse;
		const qboolean deadNow = (deadByPmType || health <= 0) ? qtrue : qfalse;

		if (tracked.lastHealth > 0 && deadNow && !tracked.deadSeen) {
			NXRP_PrintConsoleToPlayer(NULL, "%s killed %s", killerName, tracked.name.c_str());
			tracked.deadSeen = qtrue;
		}
		if (!deadNow) {
			tracked.deadSeen = qfalse;
		}
		tracked.lastHealth = health;
	}

	for (size_t i = 0; i < toUntrack.size(); ++i) {
		NXRP_UntrackNPC(toUntrack[i]);
	}
}

void NXRP_ResetNPCTracking(void) {
	nxrp_pending_npc_names.clear();
	nxrp_pending_npc_spawns.clear();
	nxrp_tracked_npcs.clear();
	nxrp_known_npc_ents.clear();
	NXRP_PrintConsoleToPlayer(NULL, "Reset NPCs");
}

void NXRP_ListTrackedSpawnNPCs(client_t* cl) {
	int count = 0;
	for (std::map<int, nxrp_tracked_npc_t>::iterator it = nxrp_tracked_npcs.begin(); it != nxrp_tracked_npcs.end(); ++it) {
		const int entnum = it->first;
		nxrp_tracked_npc_t& tracked = it->second;
		if (!tracked.fromSpawnCmd) continue;
		sharedEntity_t* ent = NULL;
		const qboolean alive = NXRP_IsLiveNpcEnt(entnum, &ent);
		NXRP_PrintConsoleToPlayer(cl, "%d: npc=%s alive=%s health=%d",
			entnum,
			tracked.name.c_str(),
			alive ? "yes" : "no",
			alive && ent ? ent->health : tracked.lastHealth);
		count++;
	}
	if (!count) NXRP_PrintConsoleToPlayer(cl, "No tracked !spawn NPCs");
}

// Wrapper for freeing entities: if the entity being freed is a tracked NPC,
// call NXRP_OnNPCKilled before forwarding to the real G_FreeEntity in the VM.
void NXRP_FreeEntity(void* entPtr) {
	if (!entPtr) return;
	sharedEntity_t* ent = (sharedEntity_t*)entPtr;
	int entnum = ent->s.number;
	const char* trackedName = NXRP_GetTrackedNPCName(entnum);
	if (trackedName) {
		NXRP_PrintConsoleToPlayer(NULL, "NXRP: G_FreeEntity called for tracked npc ent=%d name=%s health=%d", entnum, trackedName, ent->health);
	}

	// Forward to the real VM G_FreeEntity function
	void* dll = GVM_GetDllHandle();
	if (!dll) return;
	void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
	if (!G_FreeEntity_f) return;
	void* old = GVM_BeginNative();
	G_FreeEntity_f(entPtr);
	GVM_EndNative(old);
	if (trackedName) {
		NXRP_PrintConsoleToPlayer(NULL, "NXRP: tracked npc entity freed ent=%d name=%s", entnum, trackedName);
		NXRP_UntrackNPC(entnum);
	}
}

