#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
// <cstdio> not required in this TU; va() is provided by the engine headers included elsewhere

// Ensure the print helper is visible in this translation unit
void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text);

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
