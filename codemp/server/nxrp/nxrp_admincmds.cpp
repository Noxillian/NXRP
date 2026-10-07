#include "server/nxrp/nxrp_main.h"
#include "server/sv_gameapi.h"
#include <string>

// Simple admin commands for NXRP

// Handler: !nx giveall
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	// Notify the invoking client that they have been given everything
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given everything\"" );
	// give the E11 weapon (if server console command exists)
	SV_ExecuteClientCommand(cl, "give weapon_e11", qtrue);
	// grant force power: lightning level 3
	// Use the same approach spin.cpp uses: temporarily enable sv_cheats,
	// execute the client command, then restore sv_cheats. This mirrors
	// Spin_ExecCheatClientCommand behavior so cheat-gated gives work.
	{
		const qboolean cheatsWereEnabled = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "1");
			GVM_RunFrame(sv.time);
		}

		SV_ExecuteClientCommand(cl, "forcepower lightning 3", qtrue);

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "0");
			GVM_RunFrame(sv.time);
		}
	}
	return qtrue;
}
