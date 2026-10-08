#include "server/nxrp/nxrp_main.h"
#include <cstdio>

// Called when an NPC is killed by a player. This function broadcasts a message
// to all clients: "<player> defeated <npc>".
// Note: The game code (game module) must call this helper at the point where
// it detects an NPC death caused by a player (e.g., in the relevant g_*.c file).

void NXRP_OnNPCKilled(client_t *killer, const char *npcName) {
	const char *pName = "Unknown";
	if (killer) {
		if (killer->nxrp_username[0]) pName = killer->nxrp_username;
		else if (killer->name[0]) pName = killer->name;
	}
	const char *nName = npcName && npcName[0] ? npcName : "NPC";

	SV_SendServerCommand(NULL, "print \"^5[^6N^7X^5] %s defeated %s\\n\"", pName, nName);
}

// Called when a player first spawns/enters the world. Broadcasts a chat message
// containing the player's name so admins/scripts can observe spawns.
void NXRP_OnPlayerSpawned(client_t *cl) {
	const char *pName = "Unknown";
	if (!cl) {
		SV_SendServerCommand(NULL, "print \"^5[^6N^7X^5] Unknown player spawned\\n\"");
		return;
	}
	if (cl->nxrp_username[0]) pName = cl->nxrp_username;
	else if (cl->name[0]) pName = cl->name;

	SV_SendServerCommand(NULL, "chat \"^5[^6N^7X^5] Player spawned: %s\"\n", pName);
}
