#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp.h"
#include <cstdio>
#include <vector>
#include <cstring>

// Called when an NPC is killed by a player. This function broadcasts a message
// to all clients: "<player> defeated <npc>".
// The social/holotable code on the server now calls this when it detects an
// NPC transition from up->down. The killer pointer may be NULL if the server
// cannot determine which client caused the NPC to fall.

void NXRP_OnNPCKilled(client_t *killer, const char *npcName) {
	const char *pName = "Unknown";
	if (killer) {
		if (killer->nxrp_username[0]) pName = killer->nxrp_username;
		else if (killer->name[0]) pName = killer->name;
	}

	const char *nName = npcName && npcName[0] ? npcName : "NPC";

	// Also print to the server console for debugging
	Com_Printf("NXRP: %s defeated %s\n", pName, nName);

	// Broadcast to all clients
	SV_SendServerCommand(NULL, "chat \"^5[^6N^7X^5] %s defeated %s\"\n", pName, nName);

}

// Server-facing per-frame entry point. Mirrors the pattern used by other
// subsystems (e.g. SV_GunGameFrame) so sv_main.cpp can call SV_NXRPFrame()
// without exposing internal NXRP symbols.
void SV_NXRPFrame(void) {
	NXRP_Frame();
}

// Called when a new round begins. NXRP plugins or behavior that needs to run
// once per round start should be placed here.
void NXRP_OnRoundStart(void) {
	SV_SendServerCommand(NULL, "chat \"^5[^6N^7X^5] Round started!\"\n");
}

// Per-frame NXRP processing. Called from SV_Frame() once every server frame.
// Use this to detect generic NPC deaths or round transitions inside NXRP
// without modifying social.cpp or spin.cpp.
void NXRP_Frame(void) {
	// Poll entities and detect NPC deaths caused by players. This is a
	// best-effort server-side detector that keeps all changes inside the
	// NXRP subsystem (no changes to game or social/spin modules).

	// Do nothing if the server isn't in-game or a restart is in progress.
	if (sv.state != SS_GAME || sv.restartTime || sv.restarting) {
		// Clear any cached state so we re-initialize cleanly when the map
		// is ready again.
		static std::vector<char> empty;
		empty.clear();
		return;
	}

	// Static cache of whether an entity was alive last frame. Index = entity
	// number. Stored as char for compactness.
	static std::vector<char> prevAlive;

	// Resize/initialise on map change or first run.
	if ((int)prevAlive.size() != sv.num_entities) {
		prevAlive.resize(sv.num_entities);
		for (int i = 0; i < sv.num_entities; ++i) {
			sharedEntity_t* e = SV_GentityNum(i);
			if (!e || !e->playerState) {
				prevAlive[i] = 0;
				continue;
			}
			playerState_t* ps = e->playerState;
			prevAlive[i] = (ps->stats[STAT_HEALTH] > 0) ? 1 : 0;
		}
		// Avoid firing events on the same frame we initialise.
		return;
	}

	// Scan entities for NPC deaths.
	for (int i = 0; i < sv.num_entities; ++i) {
		sharedEntity_t* e = SV_GentityNum(i);
		if (!e || !e->playerState) {
			prevAlive[i] = 0;
			continue;
		}

		// Only interested in NPC entities
		if (e->s.eType != ET_NPC) {
			prevAlive[i] = 0;
			continue;
		}

		playerState_t* ps = e->playerState;
		const bool nowAlive = (ps->stats[STAT_HEALTH] > 0);
		const bool wasAlive = prevAlive[i] ? true : false;

		if (wasAlive && !nowAlive) {
			// Determine killer from PERS_ATTACKER if possible.
			int attackerNum = ps->persistant[PERS_ATTACKER];
			client_t* killer = NULL;
			if (attackerNum >= 0 && attackerNum < sv_maxclients->integer) {
				client_t* cand = &svs.clients[attackerNum];
				if (cand->state == CS_ACTIVE) {
					killer = cand;
				}
			}

			// Prefer targetname when available (human-friendly), then classname
			const char* nName = "NPC";
			if (e->targetname && e->targetname[0]) {
				nName = e->targetname;
			} else if (e->classname && e->classname[0]) {
				nName = e->classname;
			}
			NXRP_OnNPCKilled(killer, nName);
		}

		prevAlive[i] = nowAlive ? 1 : 0;
	}
}
