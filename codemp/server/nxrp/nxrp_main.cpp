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

#pragma once
// forward-declare weapon give helper from sv_ccmds.cpp
void SV_WannaGiveWeapon(client_t* cl, int wnum);
#include "game/bg_weapons.h"

// forward-declare helper used elsewhere to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward declarations for the extracted command handlers
qboolean NXRP_HandleNxNoclip( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxInfo( client_t *cl, const char *chatCursor );
static qboolean NXRP_HandleNpcSpawn( client_t *cl, const char *chatCursor );
// forward-declare the spawn helper we will implement in this module
int SV_SpawnModelAtClient(client_t* cl, const char* modelPath);

enum NXRP_SubCmd {
	NXRP_SUB_UNKNOWN = 0,
	NXRP_SUB_SPAWN,
	NXRP_SUB_INFO,
	NXRP_SUB_TEST,
	NXRP_SUB_ACCOUNT,
	NXRP_SUB_GIVEALL,
	NXRP_SUB_NOCLIP,
	NXRP_SUB_REGISTER,
	NXRP_SUB_LOGIN
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
	return NXRP_SUB_UNKNOWN;
}

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

	if ( !Q_stricmp( commandName, "nx" ) ) {
		char subcmd[MAX_TOKEN_CHARS] = {0};
		char arg[MAX_TOKEN_CHARS] = {0};

		if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) >= 1 ) {
			NXRP_SubCmd sc = NXRP_ParseSubcmd( subcmd );

			if ( sc == NXRP_SUB_SPAWN ) {
				if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
				if (!NXRP_IsClientAdmin(cl)) return qtrue;
			}

			switch ( sc ) {
			case NXRP_SUB_SPAWN:
				if ( arg[0] == '\0' ) {
					NXRP_PrintConsoleToPlayer(cl, "Usage: !nx spawn <npc_type>");
					return qtrue;
				}
				return NXRP_HandleNpcSpawn(cl, chatCursor);
			case NXRP_SUB_INFO:
				return NXRP_HandleNxInfo( cl, chatCursor );
			case NXRP_SUB_TEST:
				if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
				// Admin-only spawn test: spawn a misc model near the player.
				if (!NXRP_IsClientAdmin(cl)) {
					NXRP_PrintConsoleToPlayer(cl, "Test requires admin privileges to spawn models.");
					return qtrue;
				}
				{
					// pick a default model path commonly present in JA assets
					const char* model = "models/map_objects/imp_mine/imp_mine.md3";
					int ent = SV_SpawnModelAtClient(cl, model);
					if (ent >= 0) {
						NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", model, ent));
					} else {
						NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
					}
					return qtrue;
				}
			case NXRP_SUB_ACCOUNT:
				return NXRP_HandleNxAccount( cl, chatCursor );
			case NXRP_SUB_GIVEALL:
				return NXRP_HandleNxGiveAll( cl, chatCursor );
			case NXRP_SUB_NOCLIP:
				return NXRP_HandleNxNoclip( cl, chatCursor );
			case NXRP_SUB_REGISTER:
				return NXRP_HandleNxRegister( cl, chatCursor );
			case NXRP_SUB_LOGIN:
				return NXRP_HandleNxLogin( cl, chatCursor );
			default:
				break;
			}
		}
		return qtrue;
	}

	return qfalse;
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
		NXRP_PrintConsoleToPlayer(cl, "Usage: !nx spawn <npc_type>");
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	NXRP_PrintConsoleToPlayer(cl, "^3Spawning NPC");
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}
