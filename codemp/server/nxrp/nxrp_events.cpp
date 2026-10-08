#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
// <cstdio> not required in this TU; va() is provided by the engine headers included elsewhere

// Ensure the print helper is visible in this translation unit
void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text);

// New event stubs
void NXRP_OnPlayerConnect(int clientNum, qboolean firstTime, qboolean isBot) {
	NXRP_PrintConsoleToPlayer(NULL, va("Player connected: %d", clientNum));
}

void NXRP_OnPlayerUserinfoChanged(int clientNum) {
	NXRP_PrintConsoleToPlayer(NULL, va("Player userinfo changed: %d", clientNum));
}

void NXRP_OnPlayerBegin(int clientNum) {
	NXRP_PrintConsoleToPlayer(NULL, va("Player begin: %d", clientNum));
}

void NXRP_OnPlayerDisconnect(int clientNum) {
	NXRP_PrintConsoleToPlayer(NULL, va("Player disconnected: %d", clientNum));
}

void NXRP_OnPlayerKilled(client_t *attacker, client_t *victim, int meansOfDeath) {
	const char *aName = attacker ? (attacker->nxrp_username[0] ? attacker->nxrp_username : attacker->name) : "World";
	const char *vName = victim ? (victim->nxrp_username[0] ? victim->nxrp_username : victim->name) : "Unknown";
	NXRP_PrintConsoleToPlayer(NULL, va("Kill: %s killed %s (mod %d)", aName, vName, meansOfDeath));
}

void NXRP_OnScoreChanged(client_t *cl, int oldScore, int newScore) {
	const char *name = cl ? (cl->nxrp_username[0] ? cl->nxrp_username : cl->name) : "Unknown";
	NXRP_PrintConsoleToPlayer(NULL, va("Score changed: %s %d -> %d", name, oldScore, newScore));
}

void NXRP_OnNPCSpawned(const char *npcName) {
	NXRP_PrintConsoleToPlayer(NULL, va("NPC spawned: %s", npcName ? npcName : "<unknown>"));
}

void NXRP_OnVehicleDestroyed(int vehicleEntNum, client_t *killer) {
	NXRP_PrintConsoleToPlayer(NULL, va("Vehicle destroyed: ent %d", vehicleEntNum));
}

void NXRP_OnRoundStart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Round started");
}

void NXRP_OnRoundEnd(int winningTeam) {
	NXRP_PrintConsoleToPlayer(NULL, va("Round ended, winning team %d", winningTeam));
}

void NXRP_OnRoundRestart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Round restart");
}

void NXRP_OnMatchStart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Match start");
}

void NXRP_OnMatchEnd(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Match end");
}

// Called when an NPC is killed by a player. Work in Progress
void NXRP_OnNPCKilled(client_t *killer, const char *npcName) {
	const char *pName = "Unknown";
	if (killer) {
		if (killer->nxrp_username[0]) pName = killer->nxrp_username;
		else if (killer->name[0]) pName = killer->name;
	}
	const char *nName = npcName && npcName[0] ? npcName : "NPC";

	NXRP_PrintConsoleToPlayer(NULL, va("%s defeated %s", pName, nName));
}

// Called when a player first enters the game
void NXRP_OnPlayerSpawned(client_t *cl) {
	const char *pName = "Unknown";
	if (!cl) {
		NXRP_PrintConsoleToPlayer(NULL, "Unknown player spawned");
		return;
	}
	if (cl->nxrp_username[0]) pName = cl->nxrp_username;
	else if (cl->name[0]) pName = cl->name;

	NXRP_PrintConsoleToPlayer(NULL, va("Player spawned: %s", pName));
}
