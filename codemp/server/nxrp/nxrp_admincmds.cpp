#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
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

// Handler: !noclip
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	SV_ExecuteClientCommandDelayed_h(cl, std::string("noclip"), 1);
	NXRP_PrintConsoleToPlayer(cl, "Toggled noclip");
	return qtrue;
}

// Admin-only: spawn and optionally persist an NPC, or load saved NPCs
qboolean NXRP_cmdAdmin_npc( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !npc s <type> | !npc load | !npc list | !npc r <index>");
		return qtrue;
	}

	char sub[MAX_TOKEN_CHARS] = {0};
	const char* p = chatCursor;
	while (*p == ' ' || *p == '\t') p++;
	if (sscanf(p, "%31s", sub) < 1) return qtrue;

	if (!Q_stricmp(sub, "npc")) {
		while (*p && !isspace((unsigned char)*p)) p++;
		while (*p == ' ' || *p == '\t') p++;
		memset(sub, 0, sizeof(sub));
		if (sscanf(p, "%31s", sub) < 1) return qtrue;
	}

	if (!Q_stricmp(sub, "load")) {
		int cnt = NXRP_LoadSavedNPCsForMap(cl);
		NXRP_PrintConsoleToPlayerFmt(cl, "Loaded %d saved NPC(s)", cnt);
		return qtrue;
	}

	if (!Q_stricmp(sub, "list")) {
		NXRP_ListSavedNPCs(cl);
		return qtrue;
	}

	if (!Q_stricmp(sub, "s")) {
		// forward to NPC spawn helper via delayed VM command; keep admincmds lightweight
		const char* q2 = p;
		while (*q2 && !isspace((unsigned char)*q2)) q2++;
		while (*q2 == ' ' || *q2 == '\t') q2++;
		char tok[MAX_TOKEN_CHARS] = {0};
		if (sscanf(q2, "%31s", tok) < 1) { NXRP_PrintConsoleToPlayer(cl, "Usage: !npc s <type>"); return qtrue; }

		char cmdBuf[128];
		Com_sprintf(cmdBuf, sizeof(cmdBuf), "npc spawn %s", tok);
		NXRP_PrintConsoleToPlayer(cl, "^3Spawning NPC");
		if (!NXRP_SaveNPCAtClientToMap(cl, tok)) {
			NXRP_PrintConsoleToPlayer(cl, "Failed to save NPC spawn to map file");
		}
		NXRP_PushPendingNPCName(tok);
		SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
		return qtrue;
	}

	// remove subcommand: !npc r <index>
	if (!Q_stricmp(sub, "r")) {
		const char* q2 = p;
		// advance past current token and any following whitespace (avoid dependency on skip_token_and_ws)
		while (*q2 && !isspace((unsigned char)*q2)) q2++;
		while (*q2 == ' ' || *q2 == '\t') q2++;
		int idx = -1;
		if (sscanf(q2, "%d", &idx) < 1) {
			NXRP_PrintConsoleToPlayer(cl, "Usage: !npc r <index>");
			return qtrue;
		}
		if (idx < 0) {
			NXRP_PrintConsoleToPlayer(cl, "Invalid index");
			return qtrue;
		}
		if (NXRP_RemoveSavedNPCByIndex(idx)) {
			NXRP_PrintConsoleToPlayer(cl, va("Removed saved NPC %d", idx));
		} else {
			NXRP_PrintConsoleToPlayer(cl, va("Failed to remove saved NPC %d", idx));
		}
		return qtrue;
	}


	NXRP_PrintConsoleToPlayer(cl, "Usage: !npc s <type> | !npc load | !npc list | !npc r <index>");
	return qtrue;
}

	// Directional move for a specific saved prop index using cardinal words up/down/left/right/forward/back
	// Usage: !prop <index> <dir> <amount>
qboolean NXRP_cmdAdmin_prop_dir( client_t *cl, int index, const char *args ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	NXRP_MoveSavedSpawnByIndex(cl, index, args);
	return qtrue;
}

// Admin-only: load saved prop spawns for the current map: !prop load
qboolean NXRP_cmdAdmin_prop_load( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	NXRP_PrintConsoleToPlayer(cl, "Loading saved spawns...");
	// Uses the shared utils loader for per-map saved spawns.
	int count = NXRP_LoadSavedSpawnsForMap(cl);
	NXRP_PrintConsoleToPlayer(cl, va("Loaded %d saved spawns for this map", count));
	return qtrue;
}

// Admin-only: remove a saved spawn by index. Usage: !prop r <index>
qboolean NXRP_cmdAdmin_prop_r( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int idx = -1;
	if (!chatCursor || sscanf(chatCursor, "%*s %d", &idx) < 1) {
		if (!chatCursor || sscanf(chatCursor, "%d", &idx) < 1) {
			NXRP_PrintConsoleToPlayer(cl, "Usage: !prop r <index>");
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
		//GVM_RunFrame(sv.time);
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
		//GVM_RunFrame(sv.time);
	}

	return qtrue;
}

// Admin-only: list entities or other map info. Usage: !list e
qboolean NXRP_cmdAdmin_list( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char sub[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s %31s", sub, arg) < 1) return qtrue;

	// expect: !nx list e
	if (arg[0] == '\0' || (arg[0] != 'e' && arg[0] != 'E')) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !list e   -- list entities on the map");
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


// Admin-only: handle !spawn <npc_type>
qboolean NXRP_cmdAdmin_spawn( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};

	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( arg[0] == '\0' ) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !spawn <npc_type>");
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "^3Spawning NPC");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}



// Admin-only: teleport command
qboolean NXRP_cmdAdmin_tp( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!cl || !cl->gentity) {
		NXRP_PrintConsoleToPlayer(cl, "Teleport failed: no player entity");
		return qtrue;
	}

	float x = 0.0f, y = 0.0f, z = 0.0f;
	const char* p = chatCursor ? chatCursor : "";
	while (*p == ' ' || *p == '\t') ++p;
	if (!Q_stricmpn(p, "tp", 2) && (p[2] == '\0' || p[2] == ' ' || p[2] == '\t')) {
		p += 2;
		while (*p == ' ' || *p == '\t') ++p;
	}
	if (!*p || sscanf(p, "%f %f %f", &x, &y, &z) != 3) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !tp <x> <y> <z>");
		return qtrue;
	}

	vec3_t target = { x, y, z };
	float yaw = cl->gentity->s.angles[YAW];
	if (!NXRP_TeleportClientToPosition(cl, target, yaw)) {
		NXRP_PrintConsoleToPlayer(cl, "Teleport failed");
		return qtrue;
	}

	NXRP_PrintConsoleToPlayerFmt(cl, "Teleported to %.2f %.2f %.2f", x, y, z);
	return qtrue;
}

// Admin-only: spawn model at client with provided path
// New command: !prop s <model_path> [angle]
qboolean NXRP_cmdAdmin_prop_s( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	// Delegate full spawn behavior to nxrp_utils implementation
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !prop s <model_path> [angle]");
		return qtrue;
	}
	NXRP_AdminSpawnProp(cl, chatCursor);
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
		NXRP_PrintConsoleToPlayer(cl, "Usage: !vehicle <vehicle_name>");
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn vehicle %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "Spawning vehicle");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

// Handler: !giveall
// Optional numeric argument: target client slot index (as shown by rcon status)
// Usage: !giveall [clientIndex]
qboolean NXRP_cmdAdmin_giveall( client_t *cl, const char *chatCursor ) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	// Some give commands are gated behind sv_cheats. Temporarily enable
	// sv_cheats like other modules do (see social.cpp's Holo_CheatCommand)
	const qboolean cheats = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;
	if (!cheats) {
		Cvar_Set("sv_cheats", "1");
		GVM_RunFrame(sv.time);
	}

	// Determine target client (default: the invoking admin)
	client_t* target = cl;
	int targetIdx = (int)(cl - svs.clients);
	if (chatCursor && *chatCursor) {
		char firstTok[32] = {0};
		char secondTok[32] = {0};
		int parts = sscanf(chatCursor, "%31s %31s", firstTok, secondTok);
		const char* tok = NULL;
		if (parts == 1) {
			tok = firstTok;
		} else if (parts >= 2) {
			// If the chatCursor begins with the subcommand name (e.g., "giveall <idx>"), prefer the second token.
			if (!Q_stricmp(firstTok, "giveall")) tok = secondTok;
			else tok = firstTok;
		}
		if (tok && tok[0]) {
			// token must be all digits to be considered a client index
			bool allDigits = true;
			for (size_t i = 0; tok[i]; ++i) { if (!isdigit((unsigned char)tok[i])) { allDigits = false; break; } }
			if (allDigits) {
				int idx = atoi(tok);
				if (idx >= 0 && idx < sv_maxclients->integer && svs.clients[idx].state >= CS_CONNECTED) {
					target = &svs.clients[idx];
					targetIdx = idx;
					NXRP_PrintConsoleToPlayer(cl, va("Applied giveall to client %d", idx));
				} else {
					NXRP_PrintConsoleToPlayer(cl, "Invalid client index or client not connected");
					return qtrue;
				}
			}
		}
	}
	//NXRP_PrintConsoleToPlayer(target, "You have been given a Lightsaber");

	if (target && target->gentity && target->gentity->playerState) {
		playerState_t* ps = target->gentity->playerState;

		ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->fd.saberAnimLevel = MB_SS_RED;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_DEFENCE] = 3;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_OFFENCE] = 3;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_THROW]   = 3;

		ps->fd.forcePowersKnown |= (1 << FP_PUSH);
		ps->fd.forcePowerLevel[MB_FORCE_PUSH] = 2;
		ps->fd.forcePowersActive&(MB_FORCE_PUSH) == 1;

		ps->fd.forcePowerLevel[MB_FORCE_LIGHTNING] = 2;
		ps->fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		ps->fd.forcePowersActive& (1 << FP_LIGHTNING) == 0;
		//ps->fd.forcePowerLevel[FP_LIGHTNING] = 2;
		// Grant force-levitation (force jump) same as lightning: mark known and set level
		ps->fd.forcePowersKnown |= (1 << FP_LEVITATION);
		ps->fd.forcePowerLevel[FP_LEVITATION] = 3;
		ps->fd.forcePowerMax = 300;
		ps->fd.forcePower = 200;
		ps->fd.forcePowersActive |= (1 << FP_LIGHTNING);

		SV_WannaGiveWeapon(target, WP_CLONE_PISTOL);
		SV_WannaGiveWeapon(target, WP_SABER);

		// Give a broader set of weapons
		SV_WannaGiveWeapon(target, WP_BRYAR_PISTOL);
		SV_WannaGiveWeapon(target, WP_MANDO_PISTOL);
		SV_WannaGiveWeapon(target, WP_HEAVY_PISTOL);
		SV_WannaGiveWeapon(target, WP_BLASTER);
		SV_WannaGiveWeapon(target, WP_DISRUPTOR);
		SV_WannaGiveWeapon(target, WP_BOWCASTER);
		SV_WannaGiveWeapon(target, WP_REPEATER);
		SV_WannaGiveWeapon(target, WP_CLONE_RIFLE);
		SV_WannaGiveWeapon(target, WP_A280);
		SV_WannaGiveWeapon(target, WP_DLT19);
		SV_WannaGiveWeapon(target, WP_SHOTGUN);
		SV_WannaGiveWeapon(target, WP_MINIGUN);
		SV_WannaGiveWeapon(target, WP_ROCKET_LAUNCHER);

		// Give ammo for the weapons we provided so they can be used immediately
		Spin_GiveWeaponAmmo(target, WP_CLONE_PISTOL);
		Spin_GiveWeaponAmmo(target, WP_BRYAR_PISTOL);
		Spin_GiveWeaponAmmo(target, WP_MANDO_PISTOL);
		Spin_GiveWeaponAmmo(target, WP_HEAVY_PISTOL);
		Spin_GiveWeaponAmmo(target, WP_BLASTER);
		Spin_GiveWeaponAmmo(target, WP_DISRUPTOR);
		Spin_GiveWeaponAmmo(target, WP_BOWCASTER);
		Spin_GiveWeaponAmmo(target, WP_REPEATER);
		Spin_GiveWeaponAmmo(target, WP_CLONE_RIFLE);
		Spin_GiveWeaponAmmo(target, WP_A280);
		Spin_GiveWeaponAmmo(target, WP_DLT19);
		Spin_GiveWeaponAmmo(target, WP_SHOTGUN);
		Spin_GiveWeaponAmmo(target, WP_MINIGUN);
		Spin_GiveWeaponAmmo(target, WP_ROCKET_LAUNCHER);

		char userinfo[MAX_INFO_STRING];
		SV_GetUserinfo(targetIdx, userinfo, sizeof(userinfo));
			Info_SetValueForKey(userinfo, "saber1", DEFAULT_SABER);
			Info_SetValueForKey(userinfo, "color1", "red");
		SV_SetUserinfo(targetIdx, userinfo);
		GVM_ClientUserinfoChanged(targetIdx);

		// Give the player an immediate force jump boost
		SV_ExecuteClientCommandDelayed_h(target, std::string("force jump 3"), 1);
	}

	if (!cheats) {
		Cvar_Set("sv_cheats", "0");
		GVM_RunFrame(sv.time);
	}

	return qtrue;
}
