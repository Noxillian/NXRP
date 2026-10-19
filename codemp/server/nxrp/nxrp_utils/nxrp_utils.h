#pragma once

#include <string>
#include <vector>
#include "../../server.h"
#include <cstdarg>
#include <cstdio>

// Ensure the client is logged in; if not, a message is sent and false is returned.
bool NXRP_EnsureLoggedIn(client_t* cl);

// Check whether the client's account has isAdmin=true;
bool NXRP_IsClientAdmin(client_t* cl);

// Helper: read raw account JSON content
bool NXRP_ReadAccountContentForClient(client_t* cl, std::string& outContent, std::string& outSafeUsername);

// Spawn a misc model near a client using the game module's spawn helpers.
// Returns the new entity number, or -1 on failure.
int NXRP_SpawnModelAtClient(client_t* cl, const char* modelPath);

// Place a model at a given floor position with explicit prop bounds and yaw.
// Returns the new entity number, or -1 on failure.
// Place a model at a given floor position with explicit prop bounds and yaw.
// The colliderOffset parameter shifts the collision box relative to the entity origin
// without moving the entity itself.
int NXRP_PlaceModel(const char* name, const vec3_t propMins, const vec3_t propMaxs, float yaw, const vec3_t colliderOffset, const vec3_t floor);

// Track last NXRP-spawned entity and allow adjustments
void NXRP_RegisterSpawn(int entnum);
int NXRP_GetLastSpawn();
// Spawn a prop (model) near a client using provided model bounds. This calls
// NXRP_PlaceModel using the client's origin as the floor reference.
int NXRP_SpawnPropAtClient(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw);
// Spawn prop with explicit collider extents at client location; returns entnum or -1.
int NXRP_SpawnPropAtClientWithExtents(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw);

// Persist and list NXRP-spawned models per-map in the homepath directory.
// Save a spawned entity's model path and location to the map's JSON file.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath);
// Send a player a list of saved spawns for the current map.
void NXRP_ListSavedSpawns(client_t* cl);
// Remove a saved spawn entry by its index in the per-map file. Returns true on success.
bool NXRP_RemoveSavedSpawnByIndex(int index);
// Load and spawn all saved entries for the current map. Returns number spawned.
int NXRP_LoadSavedSpawnsForMap(client_t* cl);

// Utility helpers extracted from admin commands
// Spawn prop helper: handles catalog lookup, placement and persistence.
bool NXRP_AdminSpawnProp(client_t* cl, const char* chatCursor);

// Move a saved spawn (by index) by a cardinal direction and amount.
// args: "<dir> <amount>"
bool NXRP_MoveSavedSpawnByIndex(client_t* cl, int index, const char* args);

// Move the last spawned model to absolute coordinates parsed from chatCursor: "<x> <y> <z>"
bool NXRP_MoveLastSpawnTo(client_t* cl, const char* chatCursor);

// Look up a prop definition by name from a per-module catalog file and return
// model path, extents (mins/maxs), origin and yaw angle when found.
bool NXRP_GetPropFromCatalog(const char* name, std::string& outModel, vec3_t& outMins, vec3_t& outMaxs, vec3_t& outOrigin, float& outYaw);

// Hex encoding helpers (used for password salting storage)
std::string NXRP_HexEncode(const unsigned char* data, size_t len);
bool NXRP_HexDecode(const std::string& hex, std::vector<unsigned char>& out);

// String utilities
std::string NXRP_EscapeString(const std::string& in);

// Wrapper for game VM's G_FreeEntity that allows NXRP to observe freed entities
// and detect when tracked NPCs are removed. Replace direct G_FreeEntity calls
// in server-side modules with NXRP_FreeEntity to ensure NXRP receives events.
void NXRP_FreeEntity(void* ent);
// Track/untrack NPCs spawned by NXRP so the wrapper can identify them
void NXRP_TrackNPC(int entnum, const char* name);
void NXRP_UntrackNPC(int entnum);
// Push a pending NPC name so the next NXRP_RegisterSpawn will associate the
// registered entnum with this name (used when spawn is asynchronous).
void NXRP_PushPendingNPCName(const char* name);
// NPC persistence helpers: save/load NPCs per-map
void NXRP_SaveNPCToMap(int entnum, const char* npcType);
int NXRP_LoadSavedNPCsForMap(client_t* cl);
// Send a player a list of saved NPCs for the current map.
void NXRP_ListSavedNPCs(client_t* cl);
// Remove a saved NPC entry by its zero-based index. Returns true on success.
bool NXRP_RemoveSavedNPCByIndex(int index);
// Called by NXRP_RegisterSpawn to allow NPC module to associate a pending name
// with the newly-registered entity number.
void NXRP_HandlePendingNPCSpawn(int entnum);
// Called when a player uses (presses use) on an entity; checks for tracked NPC interactions.
void NXRP_OnNPCUsed(client_t* user, int entnum);

// Return the tracked NPC name for an entity number, or NULL if not tracked.
// The returned pointer is owned by NXRP and valid until the NPC is untracked.
const char* NXRP_GetTrackedNPCName(int entnum);

// Print a standardized NXRP console message to a specific player. If cl is
// NULL the message will be broadcast to all clients by passing NULL to
// SV_SendServerCommand.
inline void NXRP_PrintConsoleToPlayerFmt(client_t* cl, const char* fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	// Split the formatted buffer on literal newlines and send one server command per line
	std::string s = buf;
	size_t start = 0;
	while (start < s.size()) {
		size_t pos = s.find('\n', start);
		std::string line;
		if (pos == std::string::npos) {
			line = s.substr(start);
			start = s.size();
		} else {
			line = s.substr(start, pos - start);
			start = pos + 1;
		}
			// Escape backslashes and double-quotes so the server command string remains syntactically valid
			std::string esc;
			esc.reserve(line.size()*2);
			for (char c : line) {
				if (c == '\\' || c == '"') { esc.push_back('\\'); esc.push_back(c); }
				else esc.push_back(c);
			}
			if (cl)
				SV_SendServerCommand(cl, "print \"^0[^6N^7X^0]^8 %s\n\"", esc.c_str());
			else
				SV_SendServerCommand(NULL, "print \"^0[^6N^7X^0]^8 %s\n\"", esc.c_str());
	}
}

inline void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text) {
	NXRP_PrintConsoleToPlayerFmt(cl, "%s", text);
}
