/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements a minimal command: !hello -> server chat "hello there"
 */

#include "server/nxrp/nxrp_main.h"
<<<<<<< HEAD
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
=======
#include "server.h"
#include <string>
#include <fstream>
#include <sstream>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <cstring>
>>>>>>> 06ee321 (nxrp: re-add register/login handlers into nxrp_main)

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

				char cmdBuf[128];
				Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
				SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] ^3Spawning NPC\\n\"\n" );
				SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
				return qtrue;
			}

			if ( !Q_stricmp( subcmd, "npc" ) ) {
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

			if ( !Q_stricmp( subcmd, "register" ) ) {
				return SV_nxrp_HandleNxRegister( cl, chatCursor );
			}

			if ( !Q_stricmp( subcmd, "login" ) ) {
				return SV_nxrp_HandleNxLogin( cl, chatCursor );
			}
		}
		return qtrue; // handled even if unknown subcommand to avoid falling through
	}

	return qfalse;
}

<<<<<<< HEAD
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


=======
// --- account helpers re-added into nxrp_main (simple JSON storage) ---
static void nxrp_EnsureDirExists(const char* path) {
#if defined(_WIN32)
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

static void nxrp_SanitizeUsername(const char* in, char* out, size_t outlen) {
	size_t j = 0;
	for ( size_t i = 0; in[i] && j + 1 < outlen; ++i ) {
		char c = in[i];
		if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_') ) {
			out[j++] = c;
		} else if ( c == ' ' || c == '-' ) {
			out[j++] = '_';
		}
	}
	out[j] = '\0';
}

static qboolean SV_Nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char username[MAX_TOKEN_CHARS] = {0};
	char password[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s %31s", subcmd, username, password ) < 3 ) {
		SV_SendServerCommand( cl, "print \"Usage: !nx register <username> <password>\\n\"\n" );
		return qtrue;
	}

	const char* home = Cvar_VariableString("fs_homepath");
	const char* game = Cvar_VariableString("fs_game");
	char dirpath[MAX_OSPATH];
	if ( home && home[0] ) {
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", home, game);
	} else {
		const char* base = Cvar_VariableString("fs_basepath");
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", base, game);
	}

	nxrp_EnsureDirExists(dirpath);

	char safe[MAX_TOKEN_CHARS];
	nxrp_SanitizeUsername(username, safe, sizeof(safe));
	char filepath[MAX_OSPATH];
	Com_sprintf(filepath, sizeof(filepath), "%s/%s.json", dirpath, safe);
	std::ifstream check(filepath);
	if ( check.is_open() ) {
		check.close();
		SV_SendServerCommand( cl, "print \"Account already exists\\n\"\n" );
		return qtrue;
	}

	std::ofstream ofs(filepath, std::ios::out | std::ios::trunc);
	if ( !ofs.is_open() ) {
		SV_SendServerCommand( cl, "print \"Failed to create account (filesystem error)\\n\"\n" );
		return qtrue;
	}
	ofs << "{\n";
	ofs << "  \"username\": \"" << username << "\",\n";
	ofs << "  \"password\": \"" << password << "\",\n";
	ofs << "  \"acc-group\": \"player\",\n";
	ofs << "  \"exp\": 0,\n";
	ofs << "  \"level\": 1\n";
	ofs << "}\n";
	ofs.close();

	SV_SendServerCommand( cl, "print \"Account created successfully\\n\"\n" );
	return qtrue;
}

static qboolean SV_Nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char username[MAX_TOKEN_CHARS] = {0};
	char password[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s %31s", subcmd, username, password ) < 3 ) {
		SV_SendServerCommand( cl, "print \"Usage: !nx login <username> <password>\\n\"\n" );
		return qtrue;
	}

	const char* home = Cvar_VariableString("fs_homepath");
	const char* game = Cvar_VariableString("fs_game");
	char dirpath[MAX_OSPATH];
	if ( home && home[0] ) {
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", home, game);
	} else {
		const char* base = Cvar_VariableString("fs_basepath");
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", base, game);
	}

	char safe[MAX_TOKEN_CHARS];
	nxrp_SanitizeUsername(username, safe, sizeof(safe));
	char filepath[MAX_OSPATH];
	Com_sprintf(filepath, sizeof(filepath), "%s/%s.json", dirpath, safe);

	std::ifstream ifs(filepath);
	if ( !ifs.is_open() ) {
		SV_SendServerCommand( cl, "print \"Login failed: account not found\\n\"\n" );
		return qtrue;
	}
	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string content = ss.str();
	ifs.close();

	size_t p = content.find("\"password\"");
	if ( p == std::string::npos ) {
		SV_SendServerCommand( cl, "print \"Login failed: account corrupt\\n\"\n" );
		return qtrue;
	}
	size_t colon = content.find(':', p);
	size_t firstQuote = content.find('"', colon);
	size_t secondQuote = content.find('"', firstQuote + 1);
	if ( colon == std::string::npos || firstQuote == std::string::npos || secondQuote == std::string::npos ) {
		SV_SendServerCommand( cl, "print \"Login failed: account corrupt\\n\"\n" );
		return qtrue;
	}
	std::string stored = content.substr(firstQuote + 1, secondQuote - firstQuote - 1);

	if ( stored == password ) {
		SV_SendServerCommand( cl, "print \"Login successful\\n\"\n" );
	} else {
		SV_SendServerCommand( cl, "print \"Login failed: incorrect password\\n\"\n" );
	}
	return qtrue;
}
>>>>>>> 06ee321 (nxrp: re-add register/login handlers into nxrp_main)
