#include "server/nxrp/nxrp_main.h"
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
	ps->fd.forcePower = 500;
}

// Handler: !nx noclip
qboolean SV_nxrp_HandleNxNoclip( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;

	// Require logged-in admin to use this command
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You must be logged in to use this command\\n\"");
		return qtrue;
	}

	// sanitize stored username to derive filename
	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Invalid stored username\\n\"");
		return qtrue;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Account file not found\\n\"");
		return qtrue;
	}

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	const std::string key = "\"isAdmin\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag missing\\n\"");
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\""); return qtrue; }
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) { SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Admin flag parse error\\n\""); return qtrue; }
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
		SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] You are not an admin\\n\"");
		return qtrue;
	}

	// Use the normal client-side cheat command to toggle noclip. This avoids
	// accessing sclient_s internals from server code.
	SV_ExecuteClientCommandDelayed_h(cl, std::string("noclip"), 1);
	SV_SendServerCommand(cl, "print \"^5[^6N^7X^5] Toggled noclip (cheat command sent)\\n\"");
	return qtrue;
}

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

		// Also grant all force powers while sv_cheats is enabled so any game
		// code that requires cheats will accept the direct state changes.
		if (cl && cl->gentity && cl->gentity->playerState) {
			playerState_t* ps = cl->gentity->playerState;
			// give full pool
			ps->fd.forcePower = 100;
			for (int fp = 0; fp < NUM_FORCE_POWERS; ++fp) {
				NXRP_GrantKnownForce(cl, fp);
				ps->fd.forcePowerLevel[fp] = FORCE_LEVEL_3;
			}
		}

		if (!cheatsWereEnabled) {
			Cvar_Set("sv_cheats", "0");
			GVM_RunFrame(sv.time);
		}
	}

	// Grant all force powers and set their levels so the player can use them.
	// Use a local helper to mark known powers so we don't rely on server
	// internal helpers in sv_ccmds.cpp.
	if (cl && cl->gentity && cl->gentity->playerState) {
		playerState_t* ps = cl->gentity->playerState;
		// give full pool
		ps->fd.forcePower = 100;
		for (int fp = 0; fp < NUM_FORCE_POWERS; ++fp) {
			NXRP_GrantKnownForce(cl, fp);
			ps->fd.forcePowerLevel[fp] = FORCE_LEVEL_3;
		}
	}
	return qtrue;
}
