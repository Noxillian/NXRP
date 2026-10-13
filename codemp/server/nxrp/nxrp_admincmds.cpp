#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "game/bg_mb2.h"
#include "game/bg_weapons.h"
#include <string>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);
// Forward-declare weapon give helper from sv_ccmds.cpp
extern void SV_WannaGiveWeapon(client_t* cl, int wnum);

// Handler: !nx noclip
qboolean NXRP_HandleNxNoclip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	SV_ExecuteClientCommandDelayed_h(cl, std::string("noclip"), 1);
	NXRP_PrintConsoleToPlayer(cl, "Toggled noclip");
	return qtrue;
}

// Admin-only: handle !nx spawn <npc_type>
qboolean NXRP_HandleNxSpawn( client_t *cl, const char *chatCursor ) {
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
qboolean NXRP_HandleNxTest( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) { NXRP_PrintConsoleToPlayer(cl, "Test requires admin privileges to spawn models."); return qtrue; }

	const char* model = "models/map_objects/korriban/statue_sith_lord.md3";
	int ent = SV_SpawnModelAtClient(cl, model);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", model, ent));
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	}
	return qtrue;
}

// Admin-only: spawn model at client with provided path
qboolean NXRP_HandleNxModel( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	// chatCursor may be of the form: "model <path>" (since the caller passes
	// the full subcommand text). Accept either that or a bare path.
	if (!chatCursor) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <model_path>");
		return qtrue;
	}

	// Skip leading spaces
	const char* p = chatCursor;
	while (*p == ' ' || *p == '\t') p++;

	// If the first token is "model" or "pmodel", skip it
	char firstTok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(p, "%31s", firstTok) == 1) {
		if (!Q_stricmp(firstTok, "model") || !Q_stricmp(firstTok, "pmodel")) {
			// advance past the first token
			const char* q = p;
			while (*q && *q != ' ' && *q != '\t') q++;
			while (*q == ' ' || *q == '\t') q++;
			p = q;
		}
	}

	if (!p || !*p) {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx model <model_path>");
		return qtrue;
	}

	// p now points at the model path string. Limit to 511 chars.
	char path[512] = {0};
	Com_sprintf(path, sizeof(path), "%s", p);

	int ent = SV_SpawnModelAtClient(cl, path);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", path, ent));
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	}
	return qtrue;
}

// Admin-only: pmodel currently same as model
qboolean NXRP_HandleNxPModel( client_t *cl, const char *chatCursor ) {
	return NXRP_HandleNxModel(cl, chatCursor);
}

// Admin-only: toggle FFA respawn mode
qboolean NXRP_HandleNxFFA( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	// chatCursor may be "ffa on", "ffa off", or empty to toggle
	char arg[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s", arg) < 1) {
		// toggle
		NXRP_FFA_SetEnabled(!qfalse);
		NXRP_PrintConsoleToPlayer(cl, "Toggled FFA respawn (toggle)");
		return qtrue;
	}

	if (!Q_stricmp(arg, "on")) {
		NXRP_FFA_SetEnabled(qtrue);
		NXRP_PrintConsoleToPlayer(cl, "FFA respawn enabled");
	} else if (!Q_stricmp(arg, "off")) {
		NXRP_FFA_SetEnabled(qfalse);
		NXRP_PrintConsoleToPlayer(cl, "FFA respawn disabled");
	} else if (!Q_stricmp(arg, "toggle")) {
		NXRP_FFA_SetEnabled(!qfalse);
		NXRP_PrintConsoleToPlayer(cl, "Toggled FFA respawn (toggle)");
	} else {
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx ffa [on|off|toggle]");
	}
	return qtrue;
}

// Handler: !nx giveall
qboolean NXRP_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
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
		ps->fd.forcePowerLevel[MB_FORCE_PUSH] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_LIGHTNING] = 1;
		ps->fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		ps->fd.forcePowerLevel[FP_LIGHTNING] = 3;
		// Grant force-levitation (force jump) same as lightning: mark known and set level
		ps->fd.forcePowersKnown |= (1 << FP_LEVITATION);
		ps->fd.forcePowerLevel[FP_LEVITATION] = 3;
		ps->fd.forcePowerMax = 300;
		ps->fd.forcePower = 100;
		ps->fd.forcePowerSelected = FP_LIGHTNING;
		ps->fd.forcePowersActive |= (1 << FP_LIGHTNING);

		SV_WannaGiveWeapon(cl, WP_CLONE_PISTOL);
		SV_WannaGiveWeapon(cl, WP_SABER);

		// Give ammo for the weapons we provided so they can be used immediately
		Spin_GiveWeaponAmmo(cl, WP_CLONE_PISTOL);

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
