// --- account helpers re-added into nxrp_main (simple JSON storage) ---
/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements commands: !hello, and !nx spawn / !nx npc list
 */

#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <string>
#include <set>
#include <fstream>
#include <cctype>
#include <cstring>
#if defined(_WIN32)
	#include <windows.h>
#else
	#include <dirent.h>
	#include <sys/stat.h>
#endif

// forward-declare weapon give helper from sv_ccmds.cpp
void SV_WannaGiveWeapon(client_t* cl, int wnum);
#include "game/bg_weapons.h"

// forward-declare helper used elsewhere to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward declarations for the extracted command handlers
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor );
static qboolean NXRP_HandleNpcSpawn( client_t *cl, const char *chatCursor );
// forward-declare the spawn helper we will implement in this module
int SV_SpawnModelAtClient(client_t* cl, const char* modelPath);

enum NXRP_SubCmd {
	NXRP_SUB_UNKNOWN = 0,
	NXRP_SUB_NPC,
	NXRP_SUB_SPAWN,
	NXRP_SUB_SKILLS,
	NXRP_SUB_SKILL,
	NXRP_SUB_MMOVE,
	NXRP_SUB_TP,
	NXRP_SUB_VEHICLE,
	NXRP_SUB_INFO,
	NXRP_SUB_TEST,
	NXRP_SUB_ACCOUNT,
	NXRP_SUB_GIVEALL,
	NXRP_SUB_BOSS,
	NXRP_SUB_NOCLIP,
	NXRP_SUB_REGISTER,
	NXRP_SUB_LOGIN,
	NXRP_SUB_POS,
	NXRP_SUB_MODEL,
	NXRP_SUB_MREMOVE,
	NXRP_SUB_STRIP,
	NXRP_SUB_PMODEL,
	NXRP_SUB_LIST
};

static NXRP_SubCmd NXRP_ParseSubcmd( const char *s ) {
	if ( !s || !s[0] )					return NXRP_SUB_UNKNOWN;
	if ( !Q_stricmp( s, "spawn" ) )		return NXRP_SUB_SPAWN;
	if ( !Q_stricmp( s, "info" ) )		return NXRP_SUB_INFO;
	if ( !Q_stricmp( s, "test" ) )		return NXRP_SUB_TEST;
	if ( !Q_stricmp( s, "account" ) )	return NXRP_SUB_ACCOUNT;
	if ( !Q_stricmp( s, "giveall" ) )	return NXRP_SUB_GIVEALL;
	if ( !Q_stricmp( s, "noclip" ) )	return NXRP_SUB_NOCLIP;
	if ( !Q_stricmp( s, "register" ) )	return NXRP_SUB_REGISTER;
	if ( !Q_stricmp( s, "login" ) )		return NXRP_SUB_LOGIN;
	if ( !Q_stricmp( s, "skill" ) )		return NXRP_SUB_SKILL;
	if ( !Q_stricmp( s, "pos" ) )		return NXRP_SUB_POS;
	if ( !Q_stricmp( s, "prop" ) )		return NXRP_SUB_MODEL;
	if ( !Q_stricmp( s, "npc" ) )		return NXRP_SUB_NPC;
	if ( !Q_stricmp( s, "strip" ) )		return NXRP_SUB_STRIP;
	if ( !Q_stricmp( s, "boss" ) )		return NXRP_SUB_BOSS;
	if ( !Q_stricmp( s, "vehicle" ) )	return NXRP_SUB_VEHICLE;
	if ( !Q_stricmp( s, "list" ) )	return NXRP_SUB_LIST;
	return NXRP_SUB_UNKNOWN;
}

// Initialize FFA helpers (implemented in nxrp_ffa.cpp)

// Spawn a misc model near a client using the game module's spawn helpers.
// Returns the new entity number, or -1 on failure.
int SV_SpawnModelAtClient(client_t* cl, const char* modelPath)
{
	if (!cl || !modelPath || !modelPath[0]) return -1;

	// Ensure game VM exported helpers exist via dynamic symbol lookup like social.cpp does
	void* dll = GVM_GetDllHandle();
	if (!dll) return -1;

	// Resolve required symbols
	void* gGSpawn_f = Sys_LoadFunction(dll, "G_Spawn");
	int (*gModelIndex_f)(const char*) = (int (*)(const char*))Sys_LoadFunction(dll, "G_ModelIndex");
	void* (*gGSpawn)() = (void* (*)())gGSpawn_f;
	if (!gGSpawn || !gModelIndex_f) return -1;

	// Call G_Spawn() through the native bridge
	void* old = GVM_BeginNative();
	sharedEntity_t* e = (sharedEntity_t*)gGSpawn();
	const int model = e ? gModelIndex_f(modelPath) : 0;
	GVM_EndNative(old);
	if (!e) return -1;

	// Position it a short distance in front of the player's current origin if available
	vec3_t org = { 0.0f, 0.0f, 0.0f };
	if (cl->gentity) {
		VectorCopy(cl->gentity->r.currentOrigin, org);
		// forward offset
		vec3_t fwd; AngleVectors(cl->gentity->s.angles, fwd, NULL, NULL);
		org[0] += fwd[0] * 24.0f;
		org[1] += fwd[1] * 24.0f;
		org[2] += 16.0f;
	}

	e->s.eType = ET_GENERAL;
	e->s.modelindex = model;
	// Ask the game VM to set the model's proper collision bounds if available.
	if (dll) {
		void (*G_SetModelBoundsForEntity_f)(sharedEntity_t*) = (void(*)(sharedEntity_t*))Sys_LoadFunction(dll, "G_SetModelBoundsForEntity");
		if (G_SetModelBoundsForEntity_f) {
			void* old2 = GVM_BeginNative();
			G_SetModelBoundsForEntity_f(e);
			GVM_EndNative(old2);
		}
	}
	VectorCopy(org, e->s.pos.trBase);
	VectorCopy(org, e->s.origin);
	VectorCopy(org, e->r.currentOrigin);
	e->s.pos.trType = TR_STATIONARY;
	VectorSet(e->s.apos.trBase, 0.0f, cl->gentity ? cl->gentity->s.angles[YAW] : 0.0f, 0.0f);
	VectorCopy(e->s.apos.trBase, e->s.angles);
	VectorCopy(e->s.apos.trBase, e->r.currentAngles);
	e->s.apos.trType = TR_STATIONARY;

	// default box
	e->r.mins[0] = -16.0f; e->r.mins[1] = -16.0f; e->r.mins[2] = -8.0f;
	e->r.maxs[0] = 16.0f; e->r.maxs[1] = 16.0f; e->r.maxs[2] = 16.0f;
	e->r.contents = CONTENTS_SOLID;
	e->r.svFlags = 0;
	SV_LinkEntity(e);

	return e->s.number;
}

qboolean NXRP_HandleChatCommands( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;
	// Legacy "!nx" prefix is no longer supported — ignore commands using it
	if ( !Q_stricmp( commandName, "nx" ) ) {
		return qtrue;
	}

	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	char normalizedCursor[1024] = {0};
	const char *dispatchCursor = chatCursor ? chatCursor : "";

	// For direct commands (no legacy "nx" prefix), the commandName is the subcommand
	Q_strncpyz( subcmd, commandName, sizeof(subcmd) );
	Com_sprintf(
		normalizedCursor,
		sizeof(normalizedCursor),
		"%s%s%s",
		subcmd,
		(dispatchCursor[0] ? " " : ""),
		dispatchCursor
	);
	dispatchCursor = normalizedCursor;

	NXRP_SubCmd sc = NXRP_ParseSubcmd( subcmd );

	// Special handling for prop subcommands: support syntax
	//   !nx prop s <modelpath>
	//   !nx prop r <index>
	//   !prop s <modelpath>
	//   !prop r <index>
	if (!Q_stricmp(subcmd, "prop")) {
		// Parse "prop" subcommands manually to avoid fragile sscanf patterns.
		const char* p = dispatchCursor;
		while (*p == ' ' || *p == '\t') {
			++p;
		}

		char first[MAX_TOKEN_CHARS] = {0};
		int firstLen = 0;
		while (*p && *p != ' ' && *p != '\t') {
			if (firstLen < (MAX_TOKEN_CHARS - 1)) {
				first[firstLen++] = *p;
			}
			++p;
		}
		first[firstLen] = '\0';

		while (*p == ' ' || *p == '\t') {
			++p;
		}

		char action[MAX_TOKEN_CHARS] = {0};
		if (!Q_stricmp(first, "prop")) {
			int actionLen = 0;
			while (*p && *p != ' ' && *p != '\t') {
				if (actionLen < (MAX_TOKEN_CHARS - 1)) {
					action[actionLen++] = *p;
				}
				++p;
			}
			action[actionLen] = '\0';
		} else {
			Q_strncpyz(action, first, sizeof(action));
		}

		while (*p == ' ' || *p == '\t') {
			++p;
		}

		// If action is numeric, treat as index for directional prop move: "!nx prop <index> <dir> <amount>"
		bool actionIsNumber = false;
		if (action[0] == '-' || (action[0] >= '0' && action[0] <= '9')) {
			actionIsNumber = true;
			for (int ai = 1; action[ai]; ++ai) {
				if (!(action[ai] >= '0' && action[ai] <= '9')) { actionIsNumber = false; break; }
			}
		}
		if (actionIsNumber) {
			int idx = atoi(action);
			return NXRP_cmdAdmin_prop_dir(cl, idx, p);
		}
		if (!Q_stricmp(action, "s")) {
			return NXRP_cmdAdmin_prop_s(cl, p);
		} else if (!Q_stricmp(action, "r")) {
			return NXRP_cmdAdmin_prop_r(cl, p);
		} else if (!Q_stricmp(action, "m")) {
			return NXRP_cmdAdmin_prop_m(cl, p);
		} else if (!Q_stricmp(action, "load")) {
			return NXRP_cmdAdmin_prop_load(cl, dispatchCursor);
		} else if (!Q_stricmp(action, "list")) {
			return NXRP_cmdAdmin_prop_list(cl, dispatchCursor);
		}
	}

	switch ( sc ) {
	case NXRP_SUB_TP:
		return NXRP_cmdAdmin_tp(cl, dispatchCursor);

	case NXRP_SUB_SPAWN:
		return NXRP_cmdAdmin_spawn(cl, dispatchCursor);

	case NXRP_SUB_NPC:
		return NXRP_cmdAdmin_npc(cl, dispatchCursor);

	case NXRP_SUB_MODEL:
		// Keep MODEL routing on prop_s.
		return NXRP_cmdAdmin_prop_s(cl, dispatchCursor);

	// allow shorthand prop subcommands: support "t" to create prop with explicit extents
	case NXRP_SUB_UNKNOWN:
	{
		char firstTok[MAX_TOKEN_CHARS] = {0};
		if (sscanf(dispatchCursor, "%31s", firstTok) == 1) {
			if (!Q_stricmp(firstTok, "t")) {
				// dispatch to prop_t handler; skip the "t" token
				const char* p = dispatchCursor;
				while (*p == ' ' || *p == '\t') ++p;
				// skip token
				while (*p && *p != ' ' && *p != '\t') ++p;
				while (*p == ' ' || *p == '\t') ++p;
				return NXRP_cmdAdmin_prop_t(cl, p);
			}
			if (!Q_stricmp(firstTok, "test2")) {
				return NXRP_cmdAdmin_test2(cl, dispatchCursor);
			}
		}
		break;
	}

	case NXRP_SUB_MREMOVE:
		return NXRP_cmdAdmin_mremove(cl, dispatchCursor);

	case NXRP_SUB_BOSS:
		return NXRP_cmdAdmin_boss(cl, dispatchCursor);

	case NXRP_SUB_LIST:
		return NXRP_cmdAdmin_list(cl, dispatchCursor);

	case NXRP_SUB_STRIP:
		return NXRP_cmdAdmin_strip(cl, dispatchCursor);

	case NXRP_SUB_VEHICLE:
		return NXRP_cmdAdmin_vehicle(cl, dispatchCursor);

	case NXRP_SUB_POS:
		return NXRP_cmd_pos(cl, dispatchCursor);

	case NXRP_SUB_INFO:
		return NXRP_cmd_info( cl, dispatchCursor );

	case NXRP_SUB_TEST:
		return NXRP_cmdAdmin_test(cl, dispatchCursor);

	case NXRP_SUB_ACCOUNT:
		return NXRP_cmd_account( cl, dispatchCursor );
	case NXRP_SUB_GIVEALL:
		return NXRP_cmdAdmin_giveall( cl, dispatchCursor );
	case NXRP_SUB_NOCLIP:
		return NXRP_cmdAdmin_noclip( cl, dispatchCursor );
	case NXRP_SUB_REGISTER:
		return NXRP_cmd_register( cl, dispatchCursor );
	case NXRP_SUB_LOGIN:
		return NXRP_cmd_login( cl, dispatchCursor );

	default:
		break;
	}

	return qtrue;
}

extern "C" qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );

extern "C" qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	return NXRP_HandleChatCommands(cl, commandName, chatCursor);
}

static qboolean NXRP_HandleNpcSpawn( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};

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
