#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include <string>
#include <fstream>
#include <cctype>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Local helper: mark a force power known for a client and refill their pool.
static void NXRP_GrantKnownForce(client_t* cl, int fpwr) {
	if (!cl || !cl->gentity || !cl->gentity->playerState) return;
	if (fpwr < 0 || fpwr >= NUM_FORCE_POWERS) return;
	playerState_t* ps = cl->gentity->playerState;
	ps->fd.forcePowersKnown |= (1 << fpwr);
	ps->fd.forcePower = 100;
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

	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given everything\\n\"" );


	{
		const qboolean cheatsWereEnabled = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "1");
			GVM_RunFrame(sv.time);
		}


		if (cl->gentity && cl->gentity->playerState) {
			playerState_t* ps = cl->gentity->playerState;

			unsigned int weaponMask = 0u;
			for (int w = WP_NONE + 1; w <= LAST_USEABLE_WEAPON; ++w) {
				weaponMask |= (1u << w);
			}
			ps->stats[STAT_WEAPONS] = (int)weaponMask;
			ps->weapon = FIRST_USEABLE_WEAPON;
			ps->weaponstate = WEAPON_READY;


			for (int w = FIRST_USEABLE_WEAPON; w <= LAST_USEABLE_WEAPON; ++w) {
				Spin_GiveWeaponAmmo(cl, (weapon_t)w);
			}
		}


		if (cl && cl->gentity && cl->gentity->playerState) {
			playerState_t* ps = cl->gentity->playerState;

		ps->fd.forcePower = 500;
			for (int fp = 0; fp < NUM_FORCE_POWERS; ++fp) {
				NXRP_GrantKnownForce(cl, fp);
				ps->fd.forcePowerLevel[fp] = FORCE_LEVEL_3;


			ps->stats[STAT_WEAPONS] |= (1 << WP_SABER);
			ps->weapon = WP_SABER;
			ps->weaponstate = WEAPON_READY;
			Spin_GiveWeaponAmmo(cl, WP_SABER);


			char userinfo[MAX_INFO_STRING];
			SV_GetUserinfo(cl - svs.clients, userinfo, sizeof(userinfo));
			Info_SetValueForKey(userinfo, "saber1", DEFAULT_SABER);

			Info_SetValueForKey(userinfo, "color1", "red");
			SV_SetUserinfo(cl - svs.clients, userinfo);
			GVM_ClientUserinfoChanged(cl - svs.clients);
			}


			GVM_RunFrame(sv.time);
		}

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "0");
			GVM_RunFrame(sv.time);
		}
	}


	return qtrue;
}
