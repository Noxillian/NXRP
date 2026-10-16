#pragma once

#include <string>
#include "../server.h"
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
int NXRP_PlaceModel(const char* name, const vec3_t propMins, const vec3_t propMaxs, float yaw, const vec3_t floor);

// Track last NXRP-spawned entity and allow adjustments
void NXRP_RegisterSpawn(int entnum);
int NXRP_GetLastSpawn();
qboolean NXRP_MoveLastSpawnByVec(client_t* cl, float dx, float dy, float dz);
// Spawn a prop (model) near a client using provided model bounds. This calls
// NXRP_PlaceModel using the client's origin as the floor reference.
int NXRP_SpawnPropAtClient(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw);

// Persist and list NXRP-spawned models per-map in the homepath directory.
// Save a spawned entity's model path and location to the map's JSON file.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath);
// Send a player a list of saved spawns for the current map.
void NXRP_ListSavedSpawns(client_t* cl);
// Remove a saved spawn entry by its index in the per-map file. Returns true on success.
bool NXRP_RemoveSavedSpawnByIndex(int index);
// Load and spawn all saved entries for the current map. Returns number spawned.
int NXRP_LoadSavedSpawnsForMap(client_t* cl);

// Print a standardized NXRP console message to a specific player. If cl is
// NULL the message will be broadcast to all clients by passing NULL to
// SV_SendServerCommand.
inline void NXRP_PrintConsoleToPlayerFmt(client_t* cl, const char* fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (cl)
		SV_SendServerCommand(cl, "print \"^0[^6N^7X^0]^8 %s\n\"", buf);
	else
		SV_SendServerCommand(NULL, "print \"^0[^6N^7X^0]^8 %s\n\"", buf);
}

inline void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text) {
	NXRP_PrintConsoleToPlayerFmt(cl, "%s", text);
}
