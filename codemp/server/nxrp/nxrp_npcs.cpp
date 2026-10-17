#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <map>
#include <string>

// Simple tracking map for NPC entnums -> name
static std::map<int, std::string> nxrp_tracked_npcs;
// When spawn is issued via delayed VM command, NXRP_GetLastSpawn may be set
// after the VM processes the command. Push a pending name here so when
// NXRP_RegisterSpawn is called we can associate the entnum with the name.
static std::string nxrp_pending_npc_name;

void NXRP_PushPendingNPCName(const char* name) {
	if (!name) return;
	nxrp_pending_npc_name = std::string(name);
}

void NXRP_TrackNPC(int entnum, const char* name) {
	if (entnum < 0) return;
	if (!name) return;
	nxrp_tracked_npcs[entnum] = std::string(name);
}

void NXRP_UntrackNPC(int entnum) {
	nxrp_tracked_npcs.erase(entnum);
}

// Wrapper for freeing entities: if the entity being freed is a tracked NPC,
// call NXRP_OnNPCKilled before forwarding to the real G_FreeEntity in the VM.
void NXRP_FreeEntity(void* entPtr) {
	if (!entPtr) return;
	sharedEntity_t* e = (sharedEntity_t*)entPtr;
	int entnum = e->s.number;
	auto it = nxrp_tracked_npcs.find(entnum);
	if (it != nxrp_tracked_npcs.end()) {
		// Notify NXRP hooks about NPC death. Pass NULL for killer for now.
		NXRP_OnNPCKilled(NULL, it->second.c_str());
		// Untrack so we don't double-fire
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
