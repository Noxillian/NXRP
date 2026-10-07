/* nxrp_main.cpp - simple NXRP chat command handler
 * Implements a minimal command: !hello -> server chat "hello there"
 */

#include "server/nxrp/nxrp_main.h"
#include "server.h"

qboolean SV_Nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	if ( !Q_stricmp( commandName, "hello" ) ) {
		// Broadcast a simple message to all clients.
		SV_SendServerCommand( NULL, "chat \"hello there\"\n" );
		return qtrue;
	}

	return qfalse;
}
