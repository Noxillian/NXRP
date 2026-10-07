/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements a minimal command: !hello -> server chat "hello there"
 */

#include "server/nxrp/nxrp_main.h"
#include <string>
#include <set>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <cstring>
#include "server/nxrp/nxrp_accounts.h"

// forward-declare helper used elsewhere to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward declarations for the extracted command handlers
static qboolean SV_nxrp_HandleHello( client_t *cl );
static qboolean SV_nxrp_HandleNxSpawn( client_t *cl, const char *chatCursor );
static qboolean SV_nxrp_HandleNxNpc( client_t *cl, const char *chatCursor );

qboolean SV_nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	if ( !Q_stricmp( commandName, "hello" ) ) {
		return SV_nxrp_HandleHello( cl );
	}

	if ( !Q_stricmp( commandName, "nx" ) ) {
		char subcmd[MAX_TOKEN_CHARS] = {0};
		if ( sscanf( chatCursor, "%31s", subcmd ) >= 1 ) {
			if ( !Q_stricmp( subcmd, "spawn" ) ) {
				return SV_nxrp_HandleNxSpawn( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "npc" ) ) {
				return SV_nxrp_HandleNxNpc( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "register" ) ) {
				return SV_nxrp_HandleNxRegister( cl, chatCursor );
			}
			if ( !Q_stricmp( subcmd, "login" ) ) {
				return SV_nxrp_HandleNxLogin( cl, chatCursor );
			}
		}
		return qtrue; // handled even if unknown subcommand
	}

	return qfalse;
}

// Implementations
static qboolean SV_nxrp_HandleHello( client_t *cl ) {
	(void)cl;
	// Broadcast a simple message to all clients.
	SV_SendServerCommand( NULL, "chat \"hello there\"\n" );
	return qtrue;
}

static qboolean SV_nxrp_HandleNxSpawn( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( arg[0] == '\0' ) {
		SV_SendServerCommand( cl, "chat \"Usage: !nx spawn <npc_type>\"\n" );
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] ^3Spawning NPC\\n\"\n" );
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}

static qboolean SV_nxrp_HandleNxNpc( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( !Q_stricmp( arg, "list" ) ) {
		const char* home = Cvar_VariableString("fs_homepath");
		const char* base = Cvar_VariableString("fs_basepath");
		const char* game = Cvar_VariableString("fs_game");
		char pathBuf[MAX_OSPATH];
		std::set<std::string> names;

		auto collect_from = [&](const char* dirpath){
			char full[MAX_OSPATH];
			Q_strncpyz(full, dirpath, sizeof(full));
#if defined(_WIN32)
			char pattern[MAX_OSPATH];
			Com_sprintf(pattern, sizeof(pattern), "%s\\*.npc", full);
			WIN32_FIND_DATAA fd;
			HANDLE h = FindFirstFileA(pattern, &fd);
			if (h != INVALID_HANDLE_VALUE) {
				do {
					if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
						const char* fn = fd.cFileName;
						size_t n = strlen(fn);
						if (n > 4 && _stricmp(fn + n - 4, ".npc") == 0) {
							names.insert(std::string(fn, fn + n - 4));
						}
					}
				} while (FindNextFileA(h, &fd));
				FindClose(h);
			}
#else
			DIR* d = opendir(full);
			if (d) {
				struct dirent* ent;
				while ((ent = readdir(d)) != NULL) {
					const char* fn = ent->d_name;
					size_t n = strlen(fn);
					if (n > 4 && !Q_stricmp(fn + n - 4, ".npc")) {
						names.insert(std::string(fn, fn + n - 4));
					}
				}
				closedir(d);
			}
#endif
		};

		if ( home && home[0] ) {
			Com_sprintf(pathBuf, sizeof(pathBuf), "%s/%s/ext_data/NPCs", home, game);
			collect_from(pathBuf);
		}

		if ( base && base[0] ) {
			Com_sprintf(pathBuf, sizeof(pathBuf), "%s/%s/ext_data/NPCs", base, game);
			collect_from(pathBuf);
		}

		if ( names.empty() ) {
			SV_SendServerCommand( cl, "print \"No NPC types found (ext_data/NPCs)\\n\"\n" );
			return qtrue;
		}

		SV_SendServerCommand( cl, "print \"Available NPC types:\\n\"\n" );
		for ( const auto &nm : names ) {
			SV_SendServerCommand( cl, "print \"  %s\\n\"\n", nm.c_str() );
		}

		return qtrue;
	}

	SV_SendServerCommand( cl, "print \"Usage: !nx npc list\\n\"\n" );
	return qtrue;
}


