#include "server/nxrp/nxrp_main.h"
#include "server/sv_gameapi.h"
#include <string>
#include <fstream>
#include <cctype>
#include "server/spin.h"

// Forward-declare delayed executor used elsewhere (defined in spin.cpp)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Simple admin commands for NXRP

// Handler: !nx giveall
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	// Check login + account isAdmin flag
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"" );
		return qtrue;
	}

	// sanitize stored username to derive filename
	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"" );
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Account file not found\\n\"" );
		return qtrue;
	}

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for isAdmin: look for "isAdmin" then a true/false token
	const std::string key = "\"isAdmin\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"" );
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\"" ); return qtrue; }
	bool isAdmin = false;
	if (content.compare(pos, 4, "true") == 0) isAdmin = true;
	else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
	else {
		// tolerate quoted booleans
		if (content[pos] == '"') {
			size_t qend = content.find('"', pos + 1);
			if (qend != std::string::npos) {
				std::string tok = content.substr(pos + 1, qend - (pos + 1));
				if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
			}
		}
	}

	if (!isAdmin) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"" );
		return qtrue;
	}
	// Notify the invoking client that they have been given everything
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You have been given everything\\n\"" );
	// give the E11 weapon (deferred so it runs on the next frame like other
	// Attempt to give weapons and force powers directly using server helpers.
	// This avoids relying on client command parsing and ensures gives occur
	// even if the deferred executor path previously failed.
	{
		const qboolean cheatsWereEnabled = Cvar_VariableIntegerValue("sv_cheats") ? qtrue : qfalse;

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "1");
			GVM_RunFrame(sv.time);
		}

		// Give all usable weapons by setting the player's stats bitmask and
		// granting ammo as GunGame does.
		if (cl->gentity && cl->gentity->playerState) {
			playerState_t* ps = cl->gentity->playerState;
			// Set all weapon bits up to LAST_USEABLE_WEAPON and include melee.
			ps->stats[STAT_WEAPONS] = ((1 << (LAST_USEABLE_WEAPON + 1)) - (1 << WP_NONE));
			ps->weapon = FIRST_USEABLE_WEAPON;
			ps->weaponstate = WEAPON_READY;

			// Grant ammo for each weapon via Spin_GiveWeaponAmmo
			for (int w = FIRST_USEABLE_WEAPON; w <= LAST_USEABLE_WEAPON; ++w) {
				Spin_GiveWeaponAmmo(cl, (weapon_t)w);
			}
		}

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "0");
			GVM_RunFrame(sv.time);
		}
	}

	// Grant force power: lightning level 3 by direct playerState updates and
	// also attempt the wannaforce path deferred as a fallback.
	if (cl && cl->gentity && cl->gentity->playerState) {
		cl->gentity->playerState->fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		cl->gentity->playerState->fd.forcePower = 100;
		cl->gentity->playerState->fd.forcePowerLevel[FP_LIGHTNING] = FORCE_LEVEL_3;
	}
	// Fallback: run wannaforce via deferred executor in case direct writes don't
	// immediately register with the game module for this client.
	int clientNum = (int)(cl - svs.clients);
	char cmdBuf[128];
	Com_sprintf(cmdBuf, sizeof(cmdBuf), "wannaforce %d %d", clientNum, FP_LIGHTNING);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 2);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 3);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 4);
	SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 5);
	return qtrue;
}
