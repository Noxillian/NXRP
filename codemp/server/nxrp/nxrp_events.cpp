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
