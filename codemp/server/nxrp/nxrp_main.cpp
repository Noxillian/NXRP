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
#endif
#include <cstring>

// forward-declare helper used elsewhere to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

qboolean SV_Nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	if ( !Q_stricmp( commandName, "hello" ) ) {
		// Broadcast a simple message to all clients.
		SV_SendServerCommand( NULL, "chat \"hello there\"\n" );
		return qtrue;
	}

	// !nx commands: e.g. "!nx spawn stormtrooper"
	if ( !Q_stricmp( commandName, "nx" ) ) {
		char subcmd[MAX_TOKEN_CHARS] = {0};
		char arg[MAX_TOKEN_CHARS] = {0};

		if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) >= 1 ) {
			if ( !Q_stricmp( subcmd, "spawn" ) ) {
				if ( arg[0] == '\0' ) {
					SV_SendServerCommand( cl, "chat \"Usage: !nx spawn <npc_type>\"\n" );
					return qtrue;
				}

			// Only allow spawn if the player's base address matches the last
			// rcon redirect address (practical per-player check).
			if ( !NET_CompareBaseAdr( cl->netchan.remoteAddress, svs.redirectAddress ) ) {
				SV_SendServerCommand( cl, "print \"You must be rcon-authenticated to use !nx spawn\\n\"\n" );
				return qtrue;
			}

				char cmdBuf[128];
				Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
				// Notify the requesting player in their console that a spawn is
				// being attempted.
				SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] ^3Spawning NPC\\n\"\n" );

				// Execute the NPC spawn as if the client issued the command.
				// Use delayed helper which temporarily enables sv_cheats and
				// runs the command on the server thread (safer for spawn/admin cmds).
				SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
				return qtrue;
			}

			if ( !Q_stricmp( subcmd, "npc" ) ) {
				// handle subcommands of !nx npc, e.g. "!nx npc list"
				if ( !Q_stricmp( arg, "list" ) ) {
					// Collect NPC filenames from fs_homepath and fs_basepath
					const char* home = Cvar_VariableString("fs_homepath");
					const char* base = Cvar_VariableString("fs_basepath");
					const char* game = Cvar_VariableString("fs_game");
					char pathBuf[MAX_OSPATH];
					std::set<std::string> names;

					auto collect_from = [&](const char* dirpath){
						char full[MAX_OSPATH];
						Q_strncpyz(full, dirpath, sizeof(full));
#if defined(_WIN32)
						// Windows: use FindFirstFile/FindNextFile with pattern
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
				// unknown !nx npc subcommand: show usage
				SV_SendServerCommand( cl, "print \"Usage: !nx npc list\\n\"\n" );
				return qtrue;
			}
		}
		return qtrue; // handled even if unknown subcommand to avoid falling through
	}

	return qfalse;
}
