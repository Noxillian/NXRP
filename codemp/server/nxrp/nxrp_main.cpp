// --- account helpers re-added into nxrp_main (simple JSON storage) ---
/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements commands: !hello, and !nx spawn / !nx npc list
 */

#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
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

qboolean NXRP_HandleChatCommands( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	if ( !Q_stricmp( commandName, "nx" ) ) {
		char subcmd[MAX_TOKEN_CHARS] = {0};
		char arg[MAX_TOKEN_CHARS] = {0};

		if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) >= 1 ) {
			if ( !Q_stricmp( subcmd, "spawn" ) ) {
				// check login only; actual spawn action is handled elsewhere
				if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
				if ( arg[0] == '\0' ) {
					NXRP_PrintConsoleToPlayer(cl, "Usage: !nx spawn <npc_type>");
					return qtrue;
				}

// (No C-linkage wrapper required; server C code calls NXRP_HandleChatCommands
// directly.)
				// delegate actual spawn handling to the spawn handler
				return NXRP_HandleNpcSpawn(cl, chatCursor);
			}

			// 'npcspawn' command removed - handled by 'spawn' if needed
			if ( !Q_stricmp( subcmd, "info" ) ) {
				return NXRP_HandleNxInfo( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "test" ) ) {
				if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
				NXRP_PrintConsoleToPlayer(cl, "Test");
				return qtrue;
			}
			if ( !Q_stricmp( subcmd, "account" ) ) {
				return NXRP_HandleNxAccount( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "giveall" ) ) {
				return NXRP_HandleNxGiveAll( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "noclip" ) ) {
				return NXRP_HandleNxNoclip( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "register" ) ) {
				return NXRP_HandleNxRegister( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "login" ) ) {
				return NXRP_HandleNxLogin( cl, chatCursor );
			}
		}
		return qtrue;
	}

	return qfalse;
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
