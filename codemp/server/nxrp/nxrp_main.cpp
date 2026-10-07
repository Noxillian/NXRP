/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements a minimal command: !hello -> server chat "hello there"
 */

#include "server/nxrp/nxrp_main.h"
#include <string>

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
		}
		return qtrue; // handled even if unknown subcommand to avoid falling through
	}

	return qfalse;
}
