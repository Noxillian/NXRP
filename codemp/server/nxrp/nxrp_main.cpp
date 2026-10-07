// --- account helpers re-added into nxrp_main (simple JSON storage) ---
/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements commands: !hello, and !nx spawn / !nx npc list
 */

#include "server/nxrp/nxrp_main.h"
#include <string>
#include <set>
#include <fstream>
#include <cctype>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <cstring>

// forward-declare weapon give helper from sv_ccmds.cpp
void SV_WannaGiveWeapon(client_t* cl, int wnum);
#include "game/bg_weapons.h"

// forward-declare helper used elsewhere to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward declarations for the extracted command handlers
static qboolean SV_nxrp_HandleHello( client_t *cl );
static qboolean SV_nxrp_HandleNxSpawn( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxNoclip( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxInfo( client_t *cl, const char *chatCursor );

qboolean SV_nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	if ( !Q_stricmp( commandName, "hello" ) ) {
		return SV_nxrp_HandleHello( cl );
	}

	// !nx commands: e.g. "!nx spawn stormtrooper"
	if ( !Q_stricmp( commandName, "nx" ) ) {
		char subcmd[MAX_TOKEN_CHARS] = {0};
		char arg[MAX_TOKEN_CHARS] = {0};

		if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) >= 1 ) {
			if ( !Q_stricmp( subcmd, "spawn" ) ) {
			// Require logged-in admin to spawn NPCs
			if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
				SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"" );
				return qtrue;
			}

			if ( !Q_stricmp( subcmd, "nox" ) ) {
				// Require logged-in admin to use this command
				if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"" );
					return qtrue;
				}

				// check account isAdmin flag
				{
					std::string user = cl->nxrp_username;
					std::string safe;
					for (char c : user) {
						if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
					}
					if (safe.empty()) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" );
						return qtrue;
					}

					const char *home = Cvar_VariableString("fs_homepath");
					std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
					std::string filepath = dir + "/" + safe + ".json";

					std::ifstream ifs(filepath);
					if (!ifs.is_open()) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" );
						return qtrue;
					}

					std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
					ifs.close();

					const std::string key = "\"isAdmin\"";
					size_t k = content.find(key);
					if (k == std::string::npos) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"" );
						return qtrue;
					}
					size_t colon = content.find(':', k + key.size());
					if (colon == std::string::npos) { SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
					size_t pos = colon + 1;
					while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
					if (pos >= content.size()) { SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
					bool isAdmin = false;
					if (content.compare(pos, 4, "true") == 0) isAdmin = true;
					else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
					else {
						if (content[pos] == '"') {
							size_t qend = content.find('"', pos + 1);
							if (qend != std::string::npos) {
								std::string tok = content.substr(pos + 1, qend - (pos + 1));
								if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
							}
						}
					}

					if (!isAdmin) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"" );
						return qtrue;
					}
				}

				// Simple admin command actions
				SV_SendServerCommand( cl, "print \"nox\\n\"" );
				if (cl && cl->state == CS_ACTIVE) {
					SV_WannaGiveWeapon(cl, WP_CLONE_PISTOL);
				}
				return qtrue;
			}

			// check account isAdmin flag
			{
				std::string user = cl->nxrp_username;
				std::string safe;
				for (char c : user) {
					if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
				}
				if (safe.empty()) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" );
					return qtrue;
				}

				const char *home = Cvar_VariableString("fs_homepath");
				std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
				std::string filepath = dir + "/" + safe + ".json";

				std::ifstream ifs(filepath);
				if (!ifs.is_open()) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" );
					return qtrue;
				}

				std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
				ifs.close();

				const std::string key = "\"isAdmin\"";
				size_t k = content.find(key);
				if (k == std::string::npos) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"" );
					return qtrue;
				}
				size_t colon = content.find(':', k + key.size());
				if (colon == std::string::npos) { SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
				size_t pos = colon + 1;
				while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
				if (pos >= content.size()) { SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
				bool isAdmin = false;
				if (content.compare(pos, 4, "true") == 0) isAdmin = true;
				else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
				else {
					if (content[pos] == '"') {
						size_t qend = content.find('"', pos + 1);
						if (qend != std::string::npos) {
							std::string tok = content.substr(pos + 1, qend - (pos + 1));
							if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
						}
					}
				}

				if (!isAdmin) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"" );
					return qtrue;
				}
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

			if ( !Q_stricmp( subcmd, "npcspawn" ) ) {
				// Require logged-in admin to persist NPC spawns
				if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"" );
					return qtrue;
				}

				// check account isAdmin flag
				{
					std::string user = cl->nxrp_username;
					std::string safe;
					for (char c : user) {
						if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
					}
					if (safe.empty()) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" );
						return qtrue;
					}

					const char *home = Cvar_VariableString("fs_homepath");
					std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
					std::string filepath = dir + "/" + safe + ".json";

					std::ifstream ifs(filepath);
					if (!ifs.is_open()) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" );
						return qtrue;
					}

					std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
					ifs.close();

					const std::string key = "\"isAdmin\"";
					size_t k = content.find(key);
					if (k == std::string::npos) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"" );
						return qtrue;
					}
					size_t colon = content.find(':', k + key.size());
					if (colon == std::string::npos) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" );
						return qtrue;
					}
					size_t pos = colon + 1;
					while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
					if (pos >= content.size()) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" );
						return qtrue;
					}
					bool isAdmin = false;
					if (content.compare(pos, 4, "true") == 0) isAdmin = true;
					else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
					else if (content[pos] == '"') {
						size_t qend = content.find('"', pos + 1);
						if (qend != std::string::npos) {
							std::string tok = content.substr(pos + 1, qend - (pos + 1));
							if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
						}
					}

					if (!isAdmin) {
						SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"" );
						return qtrue;
					}
				}

				if ( arg[0] == '\0' ) {
					SV_SendServerCommand( cl, "print \"Usage: !nx npcspawn <npc_type>\\n\"" );
					return qtrue;
				}
				if (!cl->gentity) {
					SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] No entity to capture position from\\n\"" );
					return qtrue;
				}

				// Use the entity's reported current origin/angles where available.
				// The project uses r.currentOrigin / r.currentAngles for many game
				// systems; prefer those fields if present on the gentity type.
				vec3_t origin;
				vec3_t angles;
				for (int i = 0; i < 3; ++i) {
					// Prefer the entity render-space currentOrigin where available and
					// fall back to the entity state trajectory base for angles.
					origin[i] = cl->gentity->r.currentOrigin[i];
					angles[i] = cl->gentity->s.apos.trBase[i];
				}

				// spawn immediately
				char cmdBuf[128];
				Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
				SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 0);

				// persist entry for current map
				const char *home2 = Cvar_VariableString("fs_homepath");
				std::string dir2 = (home2 && home2[0]) ? std::string(home2) + "/nxrp_npcs" : std::string("nxrp_npcs");
#if defined(_WIN32)
				CreateDirectoryA(dir2.c_str(), NULL);
#else
				mkdir(dir2.c_str(), 0755);
#endif

				const char *mapname = Cvar_VariableString("mapname");
				if (!mapname || !mapname[0]) mapname = "nomap";
				std::string filepath2 = dir2 + "/" + mapname + ".json";

				std::string existing;
				std::ifstream ifs2(filepath2);
				if (ifs2.is_open()) {
					existing.assign((std::istreambuf_iterator<char>(ifs2)), std::istreambuf_iterator<char>());
					ifs2.close();
				}

				char entry[512];
				Com_sprintf(entry, sizeof(entry), "  {\"name\":\"%s\",\"origin\":[%.2f,%.2f,%.2f],\"angles\":[%.2f,%.2f,%.2f]}",
					arg, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2]);

				std::ofstream ofs(filepath2, std::ios::trunc);
				if (!ofs.is_open()) {
					SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Failed to write npc file\\n\"" );
					return qtrue;
				}

				if (existing.empty()) {
					ofs << "[\n" << entry << "\n]\n";
				} else {
					while (!existing.empty() && isspace((unsigned char)existing.back())) existing.pop_back();
					if (!existing.empty() && existing.back() == ']') {
						existing.pop_back();
						bool hadOther = false;
						size_t lb = existing.find('[');
						if (lb != std::string::npos) {
							size_t next = existing.find_first_not_of(" \t\n\r", lb + 1);
							hadOther = (next != std::string::npos);
						}
						ofs << existing;
						if (hadOther) ofs << ",\n";
						ofs << entry << "\n]\n";
					} else {
						ofs << "[\n" << entry << "\n]\n";
					}
				}
				ofs.close();

				SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] NPC spawned and persisted for this map\\n\"" );
				return qtrue;
			}

			if ( !Q_stricmp( subcmd, "info" ) ) {
				return SV_nxrp_HandleNxInfo( cl, chatCursor );
			}
				if ( !Q_stricmp( subcmd, "account" ) ) {
					return SV_nxrp_HandleNxAccount( cl, chatCursor );
				}
				if ( !Q_stricmp( subcmd, "giveall" ) ) {
					return SV_nxrp_HandleNxGiveAll( cl, chatCursor );
				}

			if ( !Q_stricmp( subcmd, "noclip" ) ) {
				return SV_nxrp_HandleNxNoclip( cl, chatCursor );
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

// Implementations
static qboolean SV_nxrp_HandleHello( client_t *cl ) {
	(void)cl;
	// Broadcast a simple message to all clients.
	SV_SendServerCommand( NULL, "chat \"hello there\"" );
	return qtrue;
}

static qboolean SV_nxrp_HandleNxSpawn( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char arg[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s", subcmd, arg ) < 1 ) {
		return qtrue;
	}

	if ( arg[0] == '\0' ) {
		SV_SendServerCommand( cl, "chat \"Usage: !nx spawn <npc_type>\"" );
		return qtrue;
	}

	char cmdBuf[128];
	Com_sprintf( cmdBuf, sizeof(cmdBuf), "npc spawn %s", arg );
	SV_SendServerCommand( cl, "print \"^5[^6N^7X^5] ^3Spawning NPC\\n\"" );
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	return qtrue;
}
