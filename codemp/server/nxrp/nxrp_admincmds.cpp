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
#include <cstdlib>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);
// Forward-declare weapon give helper from sv_ccmds.cpp
extern void SV_WannaGiveWeapon(client_t* cl, int wnum);
extern void SV_WannaGiveForce(client_t* cl, int fpwr);

qboolean NXRP_cmdAdmin_quest( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsole(cl, "Usage: !quest add <name> | !quest list | !quest r <name> | !quest <name> spawn | !quest <name> addNpc <npcname> | !quest <name> rewardExp <n> | !quest <name> rewardCredits <n> | !quest <name>");
		return qtrue;
	}

	char t0[MAX_TOKEN_CHARS] = {0};
	char t1[MAX_TOKEN_CHARS] = {0};
	char t2[MAX_TOKEN_CHARS] = {0};
	char t3[MAX_TOKEN_CHARS] = {0};
	sscanf(chatCursor, "%31s %31s %31s %31s", t0, t1, t2, t3);

	const bool startsWithQuest = !Q_stricmp(t0, "quest");
	const char* cmd = startsWithQuest ? t1 : t0;
	const char* a1  = startsWithQuest ? t2 : t1;
	const char* a2  = startsWithQuest ? t3 : t2;

	if (!cmd[0]) {
		NXRP_PrintConsole(cl, "Usage: !quest ...");
		return qtrue;
	}

	if (!Q_stricmp(cmd, "list")) {
		NXRP_QuestList(cl);
		return qtrue;
	}

	if (!Q_stricmp(cmd, "add")) {
		if (!NXRP_IsClientAdmin(cl)) return qtrue;
		if (!a1[0]) { NXRP_PrintConsole(cl, "Usage: !quest add <questname>"); return qtrue; }
		if (!NXRP_QuestAdd(cl, a1)) NXRP_PrintConsole(cl, "Failed to add quest");
		return qtrue;
	}

	if (!Q_stricmp(cmd, "r")) {
		if (!NXRP_IsClientAdmin(cl)) return qtrue;
		if (!a1[0]) { NXRP_PrintConsole(cl, "Usage: !quest r <questname>"); return qtrue; }
		if (!NXRP_QuestRemoveByName(cl, a1)) NXRP_PrintConsole(cl, "Failed to remove quest");
		return qtrue;
	}

	const char* questName = cmd;
	if (!a1[0]) {
		if (!NXRP_QuestStartForPlayer(cl, questName)) NXRP_PrintConsole(cl, "Failed to start quest");
		return qtrue;
	}

	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!Q_stricmp(a1, "spawn")) {
		vec3_t pos = {0,0,0};
		if (cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, pos);
		if (!NXRP_QuestSetSpawn(cl, questName, pos)) NXRP_PrintConsole(cl, "Failed to set quest spawn");
		return qtrue;
	}

	if (!Q_stricmp(a1, "addNpc")) {
		if (!a2[0]) { NXRP_PrintConsole(cl, "Usage: !quest <questname> addNpc <npcname>"); return qtrue; }
		vec3_t npcPos = {0,0,0};
		if (cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, npcPos);
		if (!NXRP_QuestAddNpcAt(cl, questName, a2, npcPos)) {
			NXRP_PrintConsole(cl, "Failed to add quest NPC");
		}
		return qtrue;
	}

	if (!Q_stricmp(a1, "rewardExp")) {
		if (!a2[0]) { NXRP_PrintConsole(cl, "Usage: !quest <questname> rewardExp <number>"); return qtrue; }
		int exp = atoi(a2);
		if (!NXRP_QuestSetRewardExp(cl, questName, exp)) NXRP_PrintConsole(cl, "Failed to set rewardExp");
		return qtrue;
	}

	if (!Q_stricmp(a1, "rewardCredits")) {
		if (!a2[0]) { NXRP_PrintConsole(cl, "Usage: !quest <questname> rewardCredits <number>"); return qtrue; }
		int credits = atoi(a2);
		if (!NXRP_QuestSetRewardCredits(cl, questName, credits)) NXRP_PrintConsole(cl, "Failed to set rewardCredits");
		return qtrue;
	}

	NXRP_PrintConsole(cl, "Usage: !quest <questname> spawn | !quest <questname> addNpc <npcname> | !quest <questname> rewardExp <n> | !quest <questname> rewardCredits <n> | !quest <questname>");
	return qtrue;
}

// Handler: !noclip
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor ) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char command[32] = {0};
	char indexText[32] = {0};
	const int parts = chatCursor ? sscanf(chatCursor, "%31s %31s", command, indexText) : 0;
	if (parts < 1) {
		NXRP_PrintConsole(cl, "Usage: !noclip [clientindex]");
		return qtrue;
	}

	int index = (int)(cl - svs.clients);
	if (parts >= 2) {
		bool allDigits = true;
		for (size_t i = 0; indexText[i]; ++i) {
			if (!isdigit((unsigned char)indexText[i])) { allDigits = false; break; }
		}
		index = allDigits ? atoi(indexText) : -1;
	}
	if (index < 0 || index >= sv_maxclients->integer || svs.clients[index].state < CS_CONNECTED) {
		NXRP_PrintConsole(cl, "Invalid client index or client not connected");
		return qtrue;
	}

	SV_ExecuteClientCommandDelayed_h(&svs.clients[index], std::string("noclip"), 1);
	NXRP_PrintConsole(cl, va("Toggled noclip for client %d", index));
	return qtrue;
}

/*static qboolean NXRP_cmdAdmin_numericStub(client_t* cl, const char* chatCursor, const char* usage) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	char command[32] = {0};
	char value[32] = {0};
	if (!chatCursor || sscanf(chatCursor, "%31s %31s", command, value) < 2) {
		NXRP_PrintConsoleToPlayer(cl, usage);
		return qtrue;
	}
	char *end = NULL;
	strtol(value, &end, 10);
	if (end == value || *end != '\0') {
		NXRP_PrintConsoleToPlayer(cl, usage);
	}
	return qtrue;
}*/

qboolean NXRP_cmdAdmin_speed(client_t *cl, const char *chatCursor) {
	//return NXRP_cmdAdmin_numericStub(cl, chatCursor, "Usage: !speed <number>");

	return qtrue;
}

qboolean NXRP_cmdAdmin_armor(client_t *cl, const char *chatCursor) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char command[32] = { 0 };
	char valueText[32] = { 0 };
	if (!chatCursor || sscanf(chatCursor, "%31s %31s", command, valueText) < 2) {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	char* end = NULL;
	const long value = strtol(valueText, &end, 10);
	if (end == valueText || *end != '\0') {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	if (!cl || !cl->gentity || !cl->gentity->playerState) {
		NXRP_PrintConsole(cl, "Unable to set player size");
		return qtrue;
	}

	cl->gentity->playerState->stats[STAT_ARMOR] = (int)value;
	NXRP_PrintConsole(cl, va("Set player armor to %d", (int)value));
	return qtrue;
}

qboolean NXRP_cmdAdmin_sabersize(client_t *cl, const char *chatCursor) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char command[32] = { 0 };
	char valueText[32] = { 0 };
	if (!chatCursor || sscanf(chatCursor, "%31s %31s", command, valueText) < 2) {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	char* end = NULL;
	const long value = strtol(valueText, &end, 10);
	if (end == valueText || *end != '\0') {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	if (!cl || !cl->gentity || !cl->gentity->playerState) {
		NXRP_PrintConsole(cl, "Unable to set player size");
		return qtrue;
	}

	//cl->gentity->playerState->iModelScale = (int)value;

	//cl->gentity->playerState

	NXRP_PrintConsole(cl, va("Set player size to %d", (int)value));
	return qtrue;
}

qboolean NXRP_cmdAdmin_setsize(client_t *cl, const char *chatCursor) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char command[32] = {0};
	char valueText[32] = {0};
	if (!chatCursor || sscanf(chatCursor, "%31s %31s", command, valueText) < 2) {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	char* end = NULL;
	const long value = strtol(valueText, &end, 10);
	if (end == valueText || *end != '\0') {
		NXRP_PrintConsole(cl, "Usage: !setsize <number>");
		return qtrue;
	}

	if (!cl || !cl->gentity || !cl->gentity->playerState) {
		NXRP_PrintConsole(cl, "Unable to set player size");
		return qtrue;
	}

	cl->gentity->playerState->iModelScale = (int)value;
	NXRP_PrintConsole(cl, va("Set player size to %d", (int)value));
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

// Rotate a saved prop by index: !prop a <index> <deltaYaw>
qboolean NXRP_cmdAdmin_prop_a( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int idx = -1;
	float deltaYaw = 0.0f;
	if (!chatCursor || sscanf(chatCursor, "%*s %d %f", &idx, &deltaYaw) != 2) {
		if (!chatCursor || sscanf(chatCursor, "%d %f", &idx, &deltaYaw) != 2) {
			NXRP_PrintConsole(cl, "Usage: !prop a <index> <deltaYaw>");
			return qtrue;
		}
	}

	if (idx < 0) {
		NXRP_PrintConsole(cl, "Invalid index");
		return qtrue;
	}

	if (NXRP_RotateSavedSpawnByIndex(cl, idx, deltaYaw)) {
		NXRP_PrintConsole(cl, va("Rotated saved spawn %d by %.2f", idx, deltaYaw));
	} else {
		NXRP_PrintConsole(cl, va("Failed to rotate saved spawn %d", idx));
	}
	return qtrue;
}

// Admin-only: load saved prop spawns for the current map: !prop load
qboolean NXRP_cmdAdmin_prop_load( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	NXRP_PrintConsole(cl, "Loading saved spawns...");
	// Uses the shared utils loader for per-map saved spawns.
	NXRP_LoadSavedSpawnsForMap(cl);
	//NXRP_PrintConsoleToPlayer(cl, va("Loaded %d saved spawns for this map", count));
	return qtrue;
}

// Admin-only: remove a saved spawn by index. Usage: !prop r <index>
qboolean NXRP_cmdAdmin_prop_r( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	int idx = -1;
	if (!chatCursor || sscanf(chatCursor, "%*s %d", &idx) < 1) {
		if (!chatCursor || sscanf(chatCursor, "%d", &idx) < 1) {
			NXRP_PrintConsole(cl, "Usage: !prop r <index>");
			return qtrue;
		}
	}

	if (idx < 0) { NXRP_PrintConsole(cl, "Invalid index"); return qtrue; }

	if (NXRP_RemoveSavedSpawnByIndex(idx)) {
		NXRP_PrintConsole(cl, va("Removed saved spawn %d", idx));
	} else {
		NXRP_PrintConsole(cl, va("Failed to remove saved spawn %d", idx));
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

	NXRP_PrintConsole(cl, "You have been promoted to Boss (giveall kit)");

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
		cl->gentity->playerState->stats[STAT_MAX_HEALTH] = 500;
		cl->gentity->playerState->stats[STAT_HEALTH] = 500;
		cl->gentity->playerState->iModelScale = 150;
		cl->gentity->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = 3;

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

// Admin-only: list entities or other map info. Usage: !list e
qboolean NXRP_cmdAdmin_list( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char sub[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s %31s", sub, arg) < 1) return qtrue;

	if (arg[0] == '\0') {
		NXRP_PrintConsole(cl, "Usage: !list e | !list npc");
		return qtrue;
	}

	if (!Q_stricmp(arg, "npc")) {
		NXRP_ListTrackedSpawnNPCs(cl);
		return qtrue;
	}

	// expect: !list e
	if (arg[0] != 'e' && arg[0] != 'E') {
		NXRP_PrintConsole(cl, "Usage: !list e | !list npc");
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
		NXRP_PrintConsole(cl, va("%3d: type=%d model=%d cls=%s pos=%.1f %.1f %.1f",
			i, ent->s.eType, ent->s.modelindex, cls,
			ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]));
		found++;
	}

	NXRP_PrintConsole(cl, va("Listed %d entities", found));
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
		NXRP_PrintConsole(cl, "Usage: !spawn <npc_type>");
		return qtrue;
	}

	char cmdBuf[128];
	//Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	NXRP_PushPendingNPCName(arg);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	NXRP_PrintConsole(NULL, "Spawned NPC %d", arg);
	return qtrue;
}



// Admin-only: teleport command
qboolean NXRP_cmdAdmin_tp( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	if (!cl || !cl->gentity) {
		NXRP_PrintConsole(cl, "Teleport failed: no player entity");
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
		NXRP_PrintConsole(cl, "Usage: !tp <x> <y> <z>");
		return qtrue;
	}

	vec3_t target = { x, y, z };
	float yaw = cl->gentity->s.angles[YAW];
	if (!NXRP_TeleportClientToPosition(cl, target, yaw)) {
		NXRP_PrintConsole(cl, "Teleport failed");
		return qtrue;
	}

	NXRP_PrintConsole(cl, "Teleported to %.2f %.2f %.2f", x, y, z);
	return qtrue;
}

// Admin-only: spawn model at client with provided path
// New command: !prop s <model_path> [angle]
qboolean NXRP_cmdAdmin_prop_s( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	// Delegate full spawn behavior to nxrp_utils implementation
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsole(cl, "Usage: !prop s <model_path> [angle]");
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

	NXRP_PrintConsole(cl, "Saved spawns for this map:");
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
		NXRP_PrintConsole(cl, "Usage: !vehicle <vehicle_name>");
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn vehicle %s", arg );
	NXRP_PrintConsole(cl, "Spawning vehicle");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

// Handler: !giveall
// Optional numeric argument: target client slot index (as shown by rcon status)
// Usage: !giveall [clientIndex]
qboolean NXRP_cmdAdmin_giveall( client_t *cl, const char *chatCursor ) {
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	char giveallTarget[32] = {0};
	const int giveallParts = chatCursor ? sscanf(chatCursor, "%*31s %31s", giveallTarget) : 0;
	if (giveallParts == 1 && !Q_stricmp(giveallTarget, "all")) {
		for (int i = 0; i < sv_maxclients->integer; ++i) {
			if (svs.clients[i].state >= CS_CONNECTED) {
				NXRP_cmdAdmin_giveall(cl, va("giveall %d", i));
			}
		}
		NXRP_PrintConsole(cl, "Applied giveall to all connected clients");
		return qtrue;
	}

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
					NXRP_PrintConsole(cl, va("Applied giveall to client %d", idx));
				} else {
					NXRP_PrintConsole(cl, "Invalid client index or client not connected");
					return qtrue;
				}
			}
		}
	}
	//NXRP_PrintConsoleToPlayer(target, "You have been given a Lightsaber");


	if (target && target->gentity && target->gentity->playerState) {
		playerState_t* ps = target->gentity->playerState;
		//ps->fd.forceUsingAdded = 0;
		ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->fd.saberAnimLevel = MB_SS_STAFF;
		ps->forceRestricted = qfalse;

		ps->fd.forcePowersActive = 0;
		ps->fd.forceDeactivateAll = 0;
		for (int i = 0; i < NUM_FORCE_POWERS; ++i) {
			ps->fd.forcePowerDebounce[i] = 0;
			ps->fd.forcePowerDuration[i] = 0;
		}

		ps->fd.forcePowerMax = 300;
		ps->fd.forcePower = 300;
		ps->fd.forcePowerRegenDebounceTime = sv.time;

		auto grantForceLevel = [&](int fp, int lvl) {
			ps->fd.forcePowersKnown |= (1 << fp);
			ps->fd.forcePowerLevel[fp] = lvl;
			ps->fd.forcePowerBaseLevel[fp] = lvl;

			cl->gentity->playerState->fd.forcePowersKnown |= (1 << fp);
			cl->gentity->playerState->fd.forcePowerLevel[fp] = lvl;
			cl->gentity->playerState->fd.forcePowerBaseLevel[fp] = lvl;

			cl->gentity->client->ps.fd.forcePowersKnown |= (1 << fp);
			cl->gentity->client->ps.fd.forcePowerLevel[fp] = lvl;
			cl->gentity->client->ps.fd.forcePowerBaseLevel[fp] = lvl;
		};

		grantForceLevel(FP_SABER_OFFENSE, 2);
		grantForceLevel(FP_SABER_DEFENSE, 2);
		grantForceLevel(FP_SABERTHROW, 2);
		grantForceLevel(FP_LEVITATION, 3);
		grantForceLevel(FP_LIGHTNING, 0);
		grantForceLevel(FP_PULL, 2);
		grantForceLevel(FP_SPEED, 2);
		grantForceLevel(FP_PROTECT, 1);
		grantForceLevel(MB_FP_DRAIN, 3);
		grantForceLevel(MB_FORCE_HEAL, 1);
		ps->fd.forcePowerSelected = FP_LIGHTNING;

		//cl->gentity->client->sess.saberLevel;

		void (*setClientSessionForGiveAll)(int, int, int) =
			(void (*)(int, int, int))Sys_LoadFunction(GVM_GetDllHandle(), "NXRP_SetClientSessionForGiveAll");
		if (setClientSessionForGiveAll) {
			void* old = GVM_BeginNative();
			setClientSessionForGiveAll(targetIdx, FP_LIGHTNING, ps->fd.saberAnimLevel);
			GVM_EndNative(old);
		}

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
			// Persist a dark-side force profile so force re-init paths keep usable powers.
			//Info_SetValueForKey(userinfo, "forcepowers", "7-1-333003000313003120");
		SV_SetUserinfo(targetIdx, userinfo);
		GVM_ClientUserinfoChanged(targetIdx);

		// Give the player an immediate force jump boost
		//SV_ExecuteClientCommandDelayed_h(target, std::string("give 0 force jump 3"), 1);
		//SV_ExecuteClientCommandDelayed_h(target, std::string("give 0 force lightning 2"), 1);
		//SV_ExecuteClientCommandDelayed_h(target, std::string("give 0 force pull"), 1);
		//SV_ExecuteClientCommandDelayed_h(target, std::string("give 0 force destruct"), 1);
	}

	if (!cheats) {
		Cvar_Set("sv_cheats", "0");
		GVM_RunFrame(sv.time);
	}

	return qtrue;
}