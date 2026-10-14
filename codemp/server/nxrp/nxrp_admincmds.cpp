#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include "game/bg_mb2.h"
#include "game/bg_weapons.h"
#include <string>
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
qboolean NXRP_cmdAdmin_model( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	// chatCursor may be: "model <path> [angle]" or "<path> [angle]"
	if (!chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <model_path> [angle]");
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
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <model_path> [angle]");
		return qtrue;
	}

	// Parse path and optional angle
	char path[512] = {0};
	float angle = 0.0f;
	int scanned = sscanf(p, "%511s %f", path, &angle);
	if (scanned < 1) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <model_path> [angle]");
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
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	}
	return qtrue;
}

// Admin-only: adjust last spawned model position
// Admin-only: move last placed model by vector deltas
qboolean NXRP_cmdAdmin_modelmove( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <x> <y> <z>    (deltas in world units)");
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
	NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <x> <y> <z>    (deltas in world units)");
	return qtrue;
}

// Admin-only: pmodel is currently the same as model
qboolean NXRP_cmdAdmin_pmodel( client_t *cl, const char *chatCursor ) {
	return NXRP_cmdAdmin_model(cl, chatCursor);
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

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn vehicle %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "Spawning vehicle");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

// Admin-only: toggle FFA respawn mode
// FFA feature is temporarily disabled. Provide a stub handler so calls
// from nxrp_main.cpp link correctly while the feature is commented out.
qboolean NXRP_cmdAdmin_ffa( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	NXRP_PrintConsoleToPlayer(cl, "FFA feature is currently disabled");
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
