#include "server/nxrp/nxrp_main.h"
#include <string>

// Simple admin commands for NXRP

// Handler: !nx giveall
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	// Notify the invoking client that they have been given everything
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given everything\"" );
	// give the E11 weapon (if server console command exists)
	SV_ExecuteClientCommand(cl, "give weapon_e11", qtrue);
	// grant force power: lightning level 3 (if server/client command exists)
	SV_ExecuteClientCommand(cl, "forcepower lightning 3", qtrue);
	return qtrue;
}
