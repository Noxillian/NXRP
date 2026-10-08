#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "game/bg_mb2.h"
#include "game/bg_weapons.h"
#include <string>
#include <fstream>
#include <cctype>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward-declare weapon give helper from sv_ccmds.cpp
extern void SV_WannaGiveWeapon(client_t* cl, int wnum);

// Local helper: mark a force power known for a client and refill their pool.
static void NXRP_GrantKnownForce(client_t* cl, int fpwr) {
	if (!cl || !cl->gentity || !cl->gentity->playerState) return;
	if (fpwr < 0 || fpwr >= NUM_FORCE_POWERS) return;
	playerState_t* ps = cl->gentity->playerState;
	ps->fd.forcePowersKnown |= (1 << fpwr);
	ps->fd.forcePower = 500;
}

// Handler: !nx noclip
qboolean SV_nxrp_HandleNxNoclip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;
	SV_ExecuteClientCommandDelayed_h(cl, std::string("noclip"), 1);
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Toggled noclip (cheat command sent)\\n\"");
	return qtrue;
}

// Simple admin commands for NXRP

// Handler: !nx giveall
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	if (!NXRP_IsClientAdmin(cl)) return qtrue;

	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given a red lightsaber\\n\"" );

	if (cl && cl->gentity && cl->gentity->playerState) {
		playerState_t* ps = cl->gentity->playerState;

		ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
		ps->weapon = WP_SABER;
		ps->weaponstate = WEAPON_READY;
		ps->fd.saberAnimLevel = MB_SS_RED;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_DEFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_OFFENCE] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_SABER_THROW]   = 1;
		ps->fd.forcePowerLevel[MB_FORCE_PUSH] = 1;
		ps->fd.forcePowerLevel[MB_FORCE_LIGHTNING] = 1;

		ps->fd.forcePowersKnown |= (1 << 3);
		ps->fd.forcePower = 500;

		SV_WannaGiveWeapon(cl, WP_CLONE_PISTOL);
		SV_WannaGiveWeapon(cl, WP_SABER);

		char userinfo[MAX_INFO_STRING];
		SV_GetUserinfo(cl - svs.clients, userinfo, sizeof(userinfo));
		Info_SetValueForKey(userinfo, "saber1", DEFAULT_SABER);
		Info_SetValueForKey(userinfo, "color1", "red");
		SV_SetUserinfo(cl - svs.clients, userinfo);
		GVM_ClientUserinfoChanged(cl - svs.clients);

		SV_ExecuteClientCommandDelayed_h(cl, std::string("setForceLightning 3"), 1);
		SV_ExecuteClientCommandDelayed_h(cl, std::string("setForceLightning"), 3);


	}


	return qtrue;
}
