#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include "game/bg_mb2.h"
#include "game/bg_weapons.h"
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);
// Forward-declare weapon give helper from sv_ccmds.cpp
extern void SV_WannaGiveWeapon(client_t* cl, int wnum);

// Handler: !nx noclip
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	SV_ExecuteClientCommandDelayed_h(cl, std::string("noclip"), 1);
	NXRP_PrintConsoleToPlayer(cl, "Toggled noclip");
	return qtrue;
}

// Admin-only: move the last spawned prop to specified x y z (world coordinates)
// Usage: !nx prop m <x> <y> <z>
qboolean NXRP_cmdAdmin_prop_m( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop m <x> <y> <z>");
		return qtrue;
	}

	float x=0,y=0,z=0;
	int cnt = sscanf(chatCursor, "%f %f %f", &x, &y, &z);
	if (cnt != 3) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop m <x> <y> <z>");
		return qtrue;
	}

	int last = NXRP_GetLastSpawn();
	if (last < 0) { NXRP_PrintConsoleToPlayer(cl, "No last spawned model to move"); return qtrue; }
	if (last < 0 || last >= sv.num_entities) { NXRP_PrintConsoleToPlayer(cl, "Invalid last spawned model id"); return qtrue; }

	sharedEntity_t* e = SV_GentityNum(last);
	if (!e) { NXRP_PrintConsoleToPlayer(cl, "Last spawned entity not found"); return qtrue; }

	// Remove saved entry that corresponds to 'last' if present (find by ent field) so we can re-save at new location
	// Load file and find matching line by ent
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	// Read lines
	std::vector<std::string> lines;
	std::ifstream ifs(filename);
	if (ifs.is_open()) {
		std::string line;
		while (std::getline(ifs, line)) { if (!line.empty()) lines.push_back(line); }
		ifs.close();
	}

	// Instead of matching by runtime ent id, use the last saved entry in the file.
	bool removed = false;
	std::string removedLine;
	if (!lines.empty()) {
		removedLine = lines.back();
		lines.pop_back();
		removed = true;
		// rewrite file immediately with remaining entries
		FILE* f = fopen(filename.c_str(), "w");
		if (f) {
			for (const auto &ll : lines) { fputs(ll.c_str(), f); fputc('\n', f); }
			fclose(f);
		}
	} else {
		// nothing to remove; proceed to move and save new location anyway
	}

	// Free the old entity and spawn a new one at the requested coordinates
	NXRP_PrintConsoleToPlayer(cl, va("Freeing old entity %d before respawn", last));
	void* dll = GVM_GetDllHandle();
	if (!dll) { NXRP_PrintConsoleToPlayer(cl, "Game VM not available"); return qtrue; }
	void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
	if (!G_FreeEntity_f) {
		NXRP_PrintConsoleToPlayer(cl, "G_FreeEntity not available; aborting move");
		return qtrue;
	}
	{
		void* old = GVM_BeginNative();
		G_FreeEntity_f(e);
		GVM_EndNative(old);
	}
	// mark that last spawn is gone
	NXRP_RegisterSpawn(-1);
	NXRP_PrintConsoleToPlayer(cl, "Old entity freed");

	// spawn new at x y z using the model path parsed from the removed line
	if (!removed || removedLine.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Could not find saved model entry for the last spawn; aborting move.");
		return qtrue;
	}

	// parse model path and angles from removedLine
	std::string modelPathStr;
	float savedAx=0.0f, savedAy=0.0f, savedAz=0.0f;
	const char* s = removedLine.c_str();
	const char* modelKey = "\"model\"";
	const char* anglesKey = "\"angles\"";
	const char* m = strstr(s, modelKey);
	if (m) {
		m = strchr(m, ':'); if (m) m++;
		while (m && *m && isspace((unsigned char)*m)) m++;
		if (m && *m == '"') {
			m++;
			// find closing quote - handle simple escaped quotes by searching for '"' directly
			const char* me = strchr(m, '"');
			if (me && me > m) modelPathStr.assign(m, me - m);
		}
	}
	const char* a = strstr(s, anglesKey);
	if (a) {
		a = strchr(a, '[');
		if (a) {
			a++;
			// sscanf may encounter commas or spaces; accept both
			sscanf(a, "%f %*[, ] %f %*[, ] %f", &savedAx, &savedAy, &savedAz);
		}
	}

	// Basic validation and debug output
	if (modelPathStr.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Saved entry missing model path; aborting move.");
		return qtrue;
	}
	// Trim whitespace from modelPathStr
	{
		size_t a = modelPathStr.find_first_not_of(" \t\r\n");
		size_t b = modelPathStr.find_last_not_of(" \t\r\n");
		if (a == std::string::npos) modelPathStr.clear(); else modelPathStr = modelPathStr.substr(a, b - a + 1);
	}
	// Debug: show parsed values and the raw removed line
	NXRP_PrintConsoleToPlayer(cl, va("Moving model '%s' to (%.0f %.0f %.0f) with saved yaw %.1f", modelPathStr.c_str(), x, y, z, savedAy));
	NXRP_PrintConsoleToPlayer(cl, va("Saved line: %s", removedLine.c_str()));

	// Quick check: query the game VM for the model index to ensure the model exists
	void* dll_check = GVM_GetDllHandle();
	if (dll_check) {
		int (*gModelIndex_f)(const char*) = (int (*)(const char*))Sys_LoadFunction(dll_check, "G_ModelIndex");
		if (gModelIndex_f) {
			void* old = GVM_BeginNative();
			int idx = gModelIndex_f(modelPathStr.c_str());
			GVM_EndNative(old);
			NXRP_PrintConsoleToPlayer(cl, va("G_ModelIndex('%s') = %d", modelPathStr.c_str(), idx));
		}
	}

	if (modelPathStr.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Saved entry missing model path; aborting move.");
		return qtrue;
	}

	vec3_t newFloor = { x, y, z };
	// snap to integers like spawn/save behavior
	newFloor[0] = roundf(newFloor[0]); newFloor[1] = roundf(newFloor[1]); newFloor[2] = roundf(newFloor[2]);

	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };
	float yaw = savedAy; // preserve saved yaw

	int newEnt = NXRP_PlaceModel(modelPathStr.c_str(), mins, maxs, yaw, newFloor);
	if (newEnt >= 0) {
		NXRP_RegisterSpawn(newEnt);
		NXRP_SaveSpawnToMap(newEnt, modelPathStr.c_str());
		NXRP_PrintConsoleToPlayer(cl, va("Moved model and respawned as entity %d", newEnt));
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to respawn model at new location");
	}

	return qtrue;
}

// Admin-only: load saved prop spawns for the current map: !nx prop load
qboolean NXRP_cmdAdmin_prop_load( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int count = NXRP_LoadSavedSpawnsForMap(cl);
	NXRP_PrintConsoleToPlayer(cl, va("Loaded %d saved spawns for this map", count));
	return qtrue;
}

// Admin-only: remove a saved spawn by index. Usage: !nx prop r <index>
qboolean NXRP_cmdAdmin_prop_r( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int idx = -1;
	if (!chatCursor || sscanf(chatCursor, "%*s %d", &idx) < 1) {
		// Try plain number
		if (!chatCursor || sscanf(chatCursor, "%d", &idx) < 1) {
			NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop r <index>");
			return qtrue;
		}
	}

	if (idx < 0) { NXRP_PrintConsoleToPlayer(cl, "Invalid index"); return qtrue; }

	if (NXRP_RemoveSavedSpawnByIndex(idx)) {
		NXRP_PrintConsoleToPlayer(cl, va("Removed saved spawn %d", idx));
	} else {
		NXRP_PrintConsoleToPlayer(cl, va("Failed to remove saved spawn %d", idx));
	}
	return qtrue;
}

// Admin-only: promote the invoking player to a 'boss' kit (copy of giveall)
qboolean NXRP_cmdAdmin_boss( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	const qboolean cheats = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;
	if (!cheats) {
		Cvar_Set("sv_cheats", "1");
		GVM_RunFrame(sv.time);
	}

	NXRP_PrintConsoleToPlayer(cl, "You have been promoted to Boss (giveall kit)");

	if (cl && cl->gentity && cl->gentity->playerState) {
		playerState_t* ps = cl->gentity->playerState;

		ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->fd.saberAnimLevel = MB_SS_YELLOW;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_DEFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_OFFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_THROW]   = 1;
		ps->fd.forcePowersKnown |= (1 << FP_LEVITATION);
		ps->fd.forcePowerLevel[FP_LEVITATION] = 3;
		cl->gentity->playerState->stats[STAT_ARMOR] = 500;
		cl->gentity->playerState->stats[STAT_HEALTH] = 500;
		cl->gentity->playerState->iModelScale = 150;

		SV_WannaGiveWeapon(cl, WP_SABER);

		char userinfo[MAX_INFO_STRING];
		SV_GetUserinfo(cl - svs.clients, userinfo, sizeof(userinfo));
		Info_SetValueForKey(userinfo, "saber1", DEFAULT_SABER);
		Info_SetValueForKey(userinfo, "color1", "red");
		// Set player model to Tavion
		Info_SetValueForKey(userinfo, "model", "tavion");
		Info_SetValueForKey(userinfo, "pmodel", "tavion");
		SV_SetUserinfo(cl - svs.clients, userinfo);
		// Ensure server-side parsing of the new userinfo occurs so the
		// client's server state and derived fields update before notifying
		// the game VM. This mirrors SV_UpdateUserinfo_f which calls
		// SV_UserinfoChanged() then GVM_ClientUserinfoChanged().
		SV_UserinfoChanged(cl);
		GVM_ClientUserinfoChanged(cl - svs.clients);

		// Some clients do not immediately reflect a server-forced userinfo
		// change until they process their next userinfo update. Force the
		// client to refresh its userinfo by scheduling a delayed client
		// command to re-send the userinfo string. This uses the existing
		// delayed executor helper used across the server.
		SV_ExecuteClientCommandDelayed_h(cl, std::string("userinfo \"" ) + cl->userinfo + std::string("\""), 1);

			// Give the player an immediate force jump boost
			SV_ExecuteClientCommandDelayed_h(cl, std::string("force jump 3"), 1);
	}

	if (!cheats) {
		Cvar_Set("sv_cheats", "0");
		GVM_RunFrame(sv.time);
	}

	return qtrue;
}

// Admin-only: list entities or other map info. Usage: !nx list e
qboolean NXRP_cmdAdmin_list( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char sub[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s %31s", sub, arg) < 1) return qtrue;

	// expect: !nx list e
	if (arg[0] == '\0' || (arg[0] != 'e' && arg[0] != 'E')) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx list e   -- list entities on the map");
		return qtrue;
	}

	int found = 0;
	for (int i = 0; i < sv.num_entities; i++) {
		sharedEntity_t* ent = SV_GentityNum(i);
		if (!ent) continue;
		// Only list entities that appear valid and are the expected slot
		if (ent->s.number != i) continue;

		const char* cls = ent->classname ? ent->classname : "<none>";
		// print basic info: id, eType, modelindex, classname, origin
		NXRP_PrintConsoleToPlayer(cl, va("%3d: type=%d model=%d cls=%s pos=%.1f %.1f %.1f",
			i, ent->s.eType, ent->s.modelindex, cls,
			ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]));
		found++;
	}

	NXRP_PrintConsoleToPlayer(cl, va("Listed %d entities", found));
	return qtrue;
}

// Admin-only: remove all weapons and forcepowers from the invoking player
qboolean NXRP_cmdAdmin_strip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!cl || !cl->gentity || !cl->gentity->playerState) {
		NXRP_PrintConsoleToPlayer(cl, "Strip failed: no player entity");
		return qtrue;
	}

	playerState_t* ps = cl->gentity->playerState;

	// Remove weapon flags and set to no weapon
	ps->stats[STAT_WEAPONS] = 0;
	ps->weapon = WP_NONE;
	ps->weaponstate = WEAPON_READY;

	// Clear force powers
	ps->fd.forcePowersKnown = 0;
	ps->fd.forcePowersActive = 0;
	ps->fd.forcePower = 0;
	ps->fd.forcePowerMax = 0;
	// Zero all per-power levels
	const int numPowers = (int)(sizeof(ps->fd.forcePowerLevel) / sizeof(ps->fd.forcePowerLevel[0]));
	for (int i = 0; i < numPowers; i++) ps->fd.forcePowerLevel[i] = 0;

	// Ensure the client's userinfo and server state reflect the change
	if (cl - svs.clients >= 0) {
		char userinfo[MAX_INFO_STRING];
		SV_GetUserinfo(cl - svs.clients, userinfo, sizeof(userinfo));
		// Optionally clear saber info
		Info_SetValueForKey(userinfo, "saber1", "");
		SV_SetUserinfo(cl - svs.clients, userinfo);
		// Ensure server-side parsing of the new userinfo occurs so the
		// client's server state and derived fields update before notifying
		// the game VM (matches behavior used elsewhere such as the boss cmd).
		SV_UserinfoChanged(cl);
		GVM_ClientUserinfoChanged(cl - svs.clients);

		// Force the client to resend its userinfo shortly so the client
		// refreshes any local model/saber state and removes lingering
		// weapon/forcepower visuals/scripts on the client side.
		SV_ExecuteClientCommandDelayed_h(cl, std::string("userinfo \"") + cl->userinfo + std::string("\""), 1);
	}

	NXRP_PrintConsoleToPlayer(cl, "You have been stripped of weapons and Force powers");
	return qtrue;
}

// Admin-only: handle !nx spawn <npc_type>
qboolean NXRP_cmdAdmin_spawn( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};

	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( arg[0] == '\0' ) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx spawn <npc_type>");
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "^3Spawning NPC");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

// Admin-only: handle !nx test (spawn a default model)
qboolean NXRP_cmdAdmin_test( client_t *cl, const char *chatCursor ) {
	// Accept optional model path: "modelpath" or "model <path>"
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) { NXRP_PrintConsoleToPlayer(cl, "Test requires admin privileges to spawn models."); return qtrue; }

	// Default model
	const char* defaultModel = "models/map_objects/nar_shaddar/table01.md3";
	const char* model = defaultModel;

	// If a model path was provided, parse it (allow leading "model" token)
	if (chatCursor && *chatCursor) {
		const char* p = chatCursor;
		while (*p == ' ' || *p == '\t') p++;
		char firstTok[MAX_TOKEN_CHARS] = {0};
		if (sscanf(p, "%31s", firstTok) == 1) {
			if (!Q_stricmp(firstTok, "model") || !Q_stricmp(firstTok, "pmodel")) {
				const char* q = p;
				while (*q && *q != ' ' && *q != '\t') q++;
				while (*q == ' ' || *q == '\t') q++;
				if (*q) model = q;
			} else {
				model = p;
			}
		}
	}

	// Use Holo-style placement via NXRP_PlaceModel: default prop bounds and client yaw/floor
	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };
	float yaw = 0.0f;
	vec3_t floor = {0.0f, 0.0f, 0.0f};
	if (cl && cl->gentity) {
		yaw = cl->gentity->s.angles[YAW];
		VectorCopy(cl->gentity->r.currentOrigin, floor);
	}

	int ent = NXRP_PlaceModel(model, mins, maxs, yaw, floor);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", model, ent));
	NXRP_RegisterSpawn(ent);
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	}
	return qtrue;
}

// Admin-only: teleport the invoking player 5 meters above current position
qboolean NXRP_cmdAdmin_tp( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	// Allow forms:
	//  - "" : teleport invoking player 5m up (legacy)
	//  - "<dst>" : teleport invoking player to client number dst
	//  - "<src> <dst>" : teleport player src to player dst

	if (!chatCursor || !*chatCursor) {
		// Legacy behavior: teleport self 5 meters up
		if (!cl || !cl->gentity || !cl->gentity->playerState) {
			NXRP_PrintConsoleToPlayer(cl, "Teleport failed: no player entity");
			return qtrue;
		}
		const float meters = 5.0f;
		const float unitsPerMeter = 32.0f;
		const float zOffset = meters * unitsPerMeter;
		vec3_t newOrg;
		VectorCopy(cl->gentity->playerState->origin, newOrg);
		newOrg[2] += zOffset;

		void* dll = GVM_GetDllHandle();
		if (dll) {
			void (*TeleportPlayer_f)(void*, float*, float*) = (void (*)(void*, float*, float*))Sys_LoadFunction(dll, "TeleportPlayer");
			if (TeleportPlayer_f) {
				float angs[3] = {0.0f, cl->gentity->s.angles[YAW], 0.0f};
				float orgf[3] = { newOrg[0], newOrg[1], newOrg[2] };
				void* old = GVM_BeginNative();
				TeleportPlayer_f(cl->gentity, orgf, angs);
				GVM_EndNative(old);
				NXRP_PrintConsoleToPlayer(cl, "Teleported 5 meters up");
				return qtrue;
			}
		}
		VectorCopy(newOrg, cl->gentity->playerState->origin);
		VectorCopy(newOrg, cl->gentity->r.currentOrigin);
		VectorCopy(newOrg, cl->gentity->s.origin);
		cl->gentity->s.pos.trType = TR_STATIONARY;
		SV_LinkEntity(cl->gentity);
		NXRP_PrintConsoleToPlayer(cl, "Teleported 5 meters up (direct)");
		return qtrue;
	}

	// Parse ints
	int src = -1, dst = -1;
	int cnt = sscanf(chatCursor, "%d %d", &src, &dst);
	if (cnt == 1) {
		// teleport invoking player to dst
		dst = src; // reuse variable
		if (dst < 0 || dst >= MAX_CLIENTS) { NXRP_PrintConsoleToPlayer(cl, "Invalid target client number"); return qtrue; }
		if (!cl || !cl->gentity || !cl->gentity->playerState) { NXRP_PrintConsoleToPlayer(cl, "Teleport failed: no player entity"); return qtrue; }

		sharedEntity_t* destEnt = SV_GentityNum(dst);
		if (!destEnt || !destEnt->r.linked || !destEnt->playerState) { NXRP_PrintConsoleToPlayer(cl, "Target player not found or not active"); return qtrue; }

		vec3_t targetOrg;
		VectorCopy(destEnt->playerState->origin, targetOrg);

		void* dll = GVM_GetDllHandle();
		if (dll) {
			void (*TeleportPlayer_f)(void*, float*, float*) = (void (*)(void*, float*, float*))Sys_LoadFunction(dll, "TeleportPlayer");
			if (TeleportPlayer_f) {
				float angs[3] = {0.0f, destEnt->s.angles[YAW], 0.0f};
				float orgf[3] = { targetOrg[0], targetOrg[1], targetOrg[2] };
				void* old = GVM_BeginNative();
				TeleportPlayer_f(cl->gentity, orgf, angs);
				GVM_EndNative(old);
				NXRP_PrintConsoleToPlayer(cl, va("Teleported you to player %d", dst));
				return qtrue;
			}
		}

		VectorCopy(targetOrg, cl->gentity->playerState->origin);
		VectorCopy(targetOrg, cl->gentity->r.currentOrigin);
		VectorCopy(targetOrg, cl->gentity->s.origin);
		cl->gentity->s.pos.trType = TR_STATIONARY;
		SV_LinkEntity(cl->gentity);
		NXRP_PrintConsoleToPlayer(cl, va("Teleported you to player %d (direct)", dst));
		return qtrue;
	} else if (cnt == 2) {
		// teleport src to dst
		if (src < 0 || src >= MAX_CLIENTS || dst < 0 || dst >= MAX_CLIENTS) { NXRP_PrintConsoleToPlayer(cl, "Invalid client numbers"); return qtrue; }
		sharedEntity_t* srcEnt = SV_GentityNum(src);
		sharedEntity_t* destEnt = SV_GentityNum(dst);
		if (!srcEnt || !srcEnt->r.linked || !srcEnt->playerState) { NXRP_PrintConsoleToPlayer(cl, "Source player not found or not active"); return qtrue; }
		if (!destEnt || !destEnt->r.linked || !destEnt->playerState) { NXRP_PrintConsoleToPlayer(cl, "Target player not found or not active"); return qtrue; }

		vec3_t targetOrg;
		VectorCopy(destEnt->playerState->origin, targetOrg);

		void* dll = GVM_GetDllHandle();
		if (dll) {
			void (*TeleportPlayer_f)(void*, float*, float*) = (void (*)(void*, float*, float*))Sys_LoadFunction(dll, "TeleportPlayer");
			if (TeleportPlayer_f) {
				float angs[3] = {0.0f, destEnt->s.angles[YAW], 0.0f};
				float orgf[3] = { targetOrg[0], targetOrg[1], targetOrg[2] };
				void* old = GVM_BeginNative();
				TeleportPlayer_f(srcEnt, orgf, angs);
				GVM_EndNative(old);
				NXRP_PrintConsoleToPlayer(cl, va("Teleported player %d to player %d", src, dst));
				return qtrue;
			}
		}

		VectorCopy(targetOrg, srcEnt->playerState->origin);
		VectorCopy(targetOrg, srcEnt->r.currentOrigin);
		VectorCopy(targetOrg, srcEnt->s.origin);
		srcEnt->s.pos.trType = TR_STATIONARY;
		SV_LinkEntity(srcEnt);
		NXRP_PrintConsoleToPlayer(cl, va("Teleported player %d to player %d (direct)", src, dst));
		return qtrue;
	}

	NXRP_PrintConsoleToPlayer(cl, "Usage: !tp                (teleport self up 5m)\n       !tp <dst>          (teleport you to dst client number)\n       !tp <src> <dst>    (teleport client src to client dst)");
	return qtrue;
}

// Admin-only: spawn model at client with provided path
// New command: !nx prop s <model_path> [angle]
qboolean NXRP_cmdAdmin_prop_s( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	// chatCursor may be: "model <path> [angle]" or "<path> [angle]"
	if (!chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop s <model_path> [angle]");
		return qtrue;
	}

	// Skip leading spaces
	const char* p = chatCursor;
	while (*p == ' ' || *p == '\t') p++;

	// If the first token is "model" or "pmodel", skip it
	char firstTok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(p, "%31s", firstTok) == 1) {
		if (!Q_stricmp(firstTok, "model") || !Q_stricmp(firstTok, "pmodel")) {
			const char* q = p;
			while (*q && *q != ' ' && *q != '\t') q++;
			while (*q == ' ' || *q == '\t') q++;
			p = q;
		}
	}

	if (!p || !*p) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop s <model_path> [angle]");
		return qtrue;
	}

	// Parse path and optional angle
	char path[512] = {0};
	float angle = 0.0f;
	int scanned = sscanf(p, "%511s %f", path, &angle);
	if (scanned < 1) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx prop s <model_path> [angle]");
		return qtrue;
	}

	// normalize angle into [0,360)
	while (angle < 0.0f) angle += 360.0f;
	while (angle >= 360.0f) angle -= 360.0f;

	// Default prop bounds; callers may add variants or extended syntax later
	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };
	vec3_t floor = { 0.0f, 0.0f, 0.0f };
	if (cl && cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);

	int ent = NXRP_PlaceModel(path, mins, maxs, angle, floor);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", path, ent));
		NXRP_RegisterSpawn(ent);
		// Persist the spawn to the per-map JSON so admins can later list/manage them.
		NXRP_SaveSpawnToMap(ent, path);
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	}
	return qtrue;
}

// Admin-only: adjust last spawned model position
// Admin-only: move last placed model by vector deltas
qboolean NXRP_cmdAdmin_mmove( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx mmove <x> <y> <z>    (deltas in world units)");
		return qtrue;
	}

	// Allow only three numeric args; legacy direction handling was removed
	float x=0.0f,y=0.0f,z=0.0f;
	// Try parse three floats
	int cnt = sscanf(chatCursor, "%f %f %f", &x, &y, &z);
	if (cnt == 3) {
		if (NXRP_MoveLastSpawnByVec(cl, x, y, z)) NXRP_PrintConsoleToPlayer(cl, "Moved last spawned model");
		else NXRP_PrintConsoleToPlayer(cl, "Failed to move last spawned model");
		return qtrue;
	}
	NXRP_PrintConsoleToPlayer(cl, "Usage: !nx mmove <x> <y> <z>    (deltas in world units)");
	return qtrue;
}

// Admin-only: pmodel is currently the same as model
qboolean NXRP_cmdAdmin_pmodel( client_t *cl, const char *chatCursor ) {
	return NXRP_cmdAdmin_prop_s(cl, chatCursor);
}

// Admin-only: remove the last NXRP-spawned model
qboolean NXRP_cmdAdmin_mremove( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int last = NXRP_GetLastSpawn();
	if (last < 0) {
		NXRP_PrintConsoleToPlayer(cl, "No last spawned model to remove");
		return qtrue;
	}
	if (last < 0 || last >= sv.num_entities) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid last spawned model id");
		return qtrue;
	}

	sharedEntity_t* e = SV_GentityNum(last);
	if (!e) {
		NXRP_PrintConsoleToPlayer(cl, "Last spawned entity not found");
		return qtrue;
	}

	void* dll = GVM_GetDllHandle();
	if (!dll) {
		NXRP_PrintConsoleToPlayer(cl, "Game VM not available");
		return qtrue;
	}

	void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
	if (!G_FreeEntity_f) {
		NXRP_PrintConsoleToPlayer(cl, "Remove function not available in game VM");
		return qtrue;
	}

	void* old = GVM_BeginNative();
	G_FreeEntity_f(e);
	GVM_EndNative(old);

	NXRP_RegisterSpawn(-1);
	NXRP_PrintConsoleToPlayer(cl, va("Removed last spawned model entity %d", last));
	return qtrue;
}

// Admin-only: list NXRP-saved spawns for the current map (prop namespace)
qboolean NXRP_cmdAdmin_prop_list( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	NXRP_PrintConsoleToPlayer(cl, "Saved spawns for this map:");
	NXRP_ListSavedSpawns(cl);
	return qtrue;
}

// Admin-only: handle !nx test2 - spawn a prop using social-style place logic
qboolean NXRP_cmdAdmin_test2( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) { NXRP_PrintConsoleToPlayer(cl, "Test requires admin privileges to spawn models."); return qtrue; }

	// Example model and approximate model bounds; these could be tuned per-model.
	const char* model = "models/map_objects/nar_shaddar/table01.md3";
	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };

	// Use the client's yaw as the spawned prop's yaw
	float yaw = 0.0f;
	if (cl && cl->gentity) yaw = cl->gentity->s.angles[YAW];

	int ent = NXRP_SpawnPropAtClient(cl, model, mins, maxs, yaw);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned test2 model %s as entity %d", model, ent));
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn test2 model.");
	}
	return qtrue;
}

// Admin-only: spawn a vehicle using the game's NPC vehicle spawn helper
qboolean NXRP_cmdAdmin_vehicle( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};

	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( arg[0] == '\0' ) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx vehicle <vehicle_name>");
		return qtrue;
	}

	// Validate vehicle exists via game VM's VehicleTable/GetIDForString before spawning
	void* dll = GVM_GetDllHandle();
	if (!dll) {
		NXRP_PrintConsoleToPlayer(cl, "Cannot validate vehicle: game VM not available");
		return qtrue;
	}

	int (*GetIDForString_f)(void*, const char*) = (int (*)(void*, const char*))Sys_LoadFunction(dll, "GetIDForString");
	void* vehicleTable = Sys_LoadFunction(dll, "VehicleTable");
	if (!GetIDForString_f || !vehicleTable) {
		NXRP_PrintConsoleToPlayer(cl, "Cannot validate vehicle: lookup functions unavailable");
		return qtrue;
	}

	int vid = GetIDForString_f(vehicleTable, arg);
	if (vid < 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Unknown vehicle '%s'", arg));
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn vehicle %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "Spawning vehicle");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

// Handler: !nx giveall
qboolean NXRP_cmdAdmin_giveall( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	// Some give commands are gated behind sv_cheats. Temporarily enable
	// sv_cheats like other modules do (see social.cpp's Holo_CheatCommand)
	const qboolean cheats = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;
	if (!cheats) {
		Cvar_Set("sv_cheats", "1");
		GVM_RunFrame(sv.time);
	}

	NXRP_PrintConsoleToPlayer(cl, "You have been given a Lightsaber");

	if (cl && cl->gentity && cl->gentity->playerState) {
		playerState_t* ps = cl->gentity->playerState;

		ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->fd.saberAnimLevel = MB_SS_YELLOW;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_DEFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_OFFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_THROW]   = 1;
		ps->fd.forcePowersKnown |= (1 << FP_PUSH);
		ps->fd.forcePowerLevel[MB_FORCE_PUSH] = 2;
		ps->fd.forcePowerLevel[MB_FORCE_LIGHTNING] = 2;
		ps->fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		ps->fd.forcePowerLevel[FP_LIGHTNING] = 2;
		// Grant force-levitation (force jump) same as lightning: mark known and set level
		ps->fd.forcePowersKnown |= (1 << FP_LEVITATION);
		ps->fd.forcePowerLevel[FP_LEVITATION] = 3;
		ps->fd.forcePowerMax = 150;
		ps->fd.forcePower = 100;
		ps->fd.forcePowersActive |= (1 << FP_LIGHTNING);

		SV_WannaGiveWeapon(cl, WP_CLONE_PISTOL);
		SV_WannaGiveWeapon(cl, WP_SABER);

		// Give a broader set of weapons
		SV_WannaGiveWeapon(cl, WP_BRYAR_PISTOL);
		SV_WannaGiveWeapon(cl, WP_MANDO_PISTOL);
		SV_WannaGiveWeapon(cl, WP_HEAVY_PISTOL);
		SV_WannaGiveWeapon(cl, WP_BLASTER);
		SV_WannaGiveWeapon(cl, WP_DISRUPTOR);
		SV_WannaGiveWeapon(cl, WP_BOWCASTER);
		SV_WannaGiveWeapon(cl, WP_REPEATER);
		SV_WannaGiveWeapon(cl, WP_CLONE_RIFLE);
		SV_WannaGiveWeapon(cl, WP_A280);
		SV_WannaGiveWeapon(cl, WP_DLT19);
		SV_WannaGiveWeapon(cl, WP_SHOTGUN);
		SV_WannaGiveWeapon(cl, WP_MINIGUN);
		SV_WannaGiveWeapon(cl, WP_ROCKET_LAUNCHER);

		// Give ammo for the weapons we provided so they can be used immediately
		Spin_GiveWeaponAmmo(cl, WP_CLONE_PISTOL);
		Spin_GiveWeaponAmmo(cl, WP_BRYAR_PISTOL);
		Spin_GiveWeaponAmmo(cl, WP_MANDO_PISTOL);
		Spin_GiveWeaponAmmo(cl, WP_HEAVY_PISTOL);
		Spin_GiveWeaponAmmo(cl, WP_BLASTER);
		Spin_GiveWeaponAmmo(cl, WP_DISRUPTOR);
		Spin_GiveWeaponAmmo(cl, WP_BOWCASTER);
		Spin_GiveWeaponAmmo(cl, WP_REPEATER);
		Spin_GiveWeaponAmmo(cl, WP_CLONE_RIFLE);
		Spin_GiveWeaponAmmo(cl, WP_A280);
		Spin_GiveWeaponAmmo(cl, WP_DLT19);
		Spin_GiveWeaponAmmo(cl, WP_SHOTGUN);
		Spin_GiveWeaponAmmo(cl, WP_MINIGUN);
		Spin_GiveWeaponAmmo(cl, WP_ROCKET_LAUNCHER);

		char userinfo[MAX_INFO_STRING];
		SV_GetUserinfo(cl - svs.clients, userinfo, sizeof(userinfo));
		Info_SetValueForKey(userinfo, "saber1", DEFAULT_SABER);
		Info_SetValueForKey(userinfo, "color1", "red");
		SV_SetUserinfo(cl - svs.clients, userinfo);
		GVM_ClientUserinfoChanged(cl - svs.clients);

			// Give the player an immediate force jump boost
			SV_ExecuteClientCommandDelayed_h(cl, std::string("force jump 3"), 1);
	}

	if (!cheats) {
		Cvar_Set("sv_cheats", "0");
		GVM_RunFrame(sv.time);
	}

	return qtrue;
}
