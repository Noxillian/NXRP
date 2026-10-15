#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "sys/sys_loadlib.h"
// <cstdio> not required in this TU; va() is provided by the engine headers included elsewhere

// Ensure the print helper is visible in this translation unit
void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text);
// Forward-declare kills-changed handler so SV_NXRP_ScoreFrame can call it before
// the function is defined later in this file.
void NXRP_OnKillsChanged(client_t *cl, int oldKills, int newKills);

// Score polling state
static int nxrp_lastScore[MAX_CLIENTS];
// Kill polling state
static int nxrp_lastKills[MAX_CLIENTS];
// Round start tracking - set when round begins (svs.time), reset on intermission
static int nxrp_roundStartTime = 0;
// Pointer into the game VM for MBII's gSiegeRoundBegun flag (if available)
static qboolean* nxrp_gSiegeRoundBegun = NULL;
// Local cached state to detect transition
static qboolean nxrp_roundBegunFlag = qfalse;

void SV_NXRP_ScoreInit(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		nxrp_lastScore[i] = 0;
		nxrp_lastKills[i] = 0;
	}
}

void SV_NXRP_ScoreFrame(void) {

	if (!svs.clients) return;
	// If intermission is active, reset round start state so we trigger again
	if (sv.configstrings[CS_INTERMISSION] && atoi(sv.configstrings[CS_INTERMISSION]) == 1) {
		NXRP_PrintConsoleToPlayer(NULL, va("Round Restart"));
		nxrp_roundStartTime = 0;
		nxrp_roundBegunFlag = qfalse;

	} else {
		// Not intermission: if we haven't signaled round start yet, trigger once when
		// we observe the first active player. This mirrors other server systems that
		// record round start on first-seen active player and ensures NXRP_OnRoundStart
		// runs exactly once per round.
		if (nxrp_roundStartTime == 0) {
			for (int i = 0; i < sv_maxclients->integer; i++) {
				client_t* cl = &svs.clients[i];
				if (cl->state == CS_ACTIVE && cl->gentity) {
					nxrp_roundStartTime = svs.time;
					NXRP_OnMapStart();

					// Initialize last-kill counters from current persistent values so
					// we don't trigger spurrious change events at round start.
					for (int j = 0; j < sv_maxclients->integer; j++) {
						client_t* cl2 = &svs.clients[j];
						if (cl2->state == CS_ACTIVE && cl2->gentity && cl2->gentity->playerState) {
							nxrp_lastKills[j] = cl2->gentity->playerState->persistant[PERS_KILLED];
						}
					}
					break;
				}
			}
		}
		// If the game VM exposes a siege-round-begun flag, detect its rising
		// edge and signal NXRP_OnRoundStart once per round.
		if (nxrp_gSiegeRoundBegun) {
			const qboolean begun = (*nxrp_gSiegeRoundBegun) ? qtrue : qfalse;
			if (begun && !nxrp_roundBegunFlag) {
				nxrp_roundBegunFlag = qtrue;
				NXRP_OnRoundStart();
			}
		}
	}
	for (int i = 0; i < sv_maxclients->integer; i++) {
		client_t* cl = &svs.clients[i];
		if (cl->state != CS_ACTIVE || !cl->gentity || !cl->gentity->playerState) continue;
		int newScore = cl->gentity->playerState->persistant[PERS_SCORE];
		int oldScore = nxrp_lastScore[i];
		if (newScore != oldScore) {
			NXRP_OnScoreChanged(cl, oldScore, newScore);
			nxrp_lastScore[i] = newScore;
		}

		// Track kills (persistant PERS_KILLED) changes as an independent event
		int newKills = cl->gentity->playerState->persistant[PERS_KILLED];
		int oldKills = nxrp_lastKills[i];
		if (newKills != oldKills) {
			NXRP_OnKillsChanged(cl, oldKills, newKills);
			nxrp_lastKills[i] = newKills;
		}
	}
}

// New event stubs
void NXRP_OnPlayerConnect(int clientNum, qboolean firstTime, qboolean isBot) {
	(void)firstTime; (void)isBot;
	const char *name = "Unknown";
	if ( clientNum >= 0 && svs.clients && clientNum < sv_maxclients->integer ) {
		client_t *cl = &svs.clients[clientNum];
		if ( cl ) {
			if ( cl->nxrp_username[0] ) name = cl->nxrp_username;
			else if ( cl->name[0] ) name = cl->name;
		}
	}
	NXRP_PrintConsoleToPlayer(NULL, va("Player connected: %s", name));
}

// Called when a player's persistent kill counter changes (tracked by SV_NXRP_ScoreFrame)
void NXRP_OnKillsChanged(client_t *cl, int oldKills, int newKills) {
	const char *name = cl ? (cl->nxrp_username[0] ? cl->nxrp_username : cl->name) : "Unknown";
	NXRP_PrintConsoleToPlayer(NULL, va("Kill archived: %s %d -> %d\n", name));
	//Com_Printf("Kill archived: %s %d -> %d\n", name, oldKills, newKills);
}

void NXRP_OnPlayerUserinfoChanged(int clientNum) {
	const char *name = "Unknown";
	if ( clientNum >= 0 && svs.clients && clientNum < sv_maxclients->integer ) {
		client_t *cl = &svs.clients[clientNum];
		if ( cl ) {
			if ( cl->nxrp_username[0] ) name = cl->nxrp_username;
			else if ( cl->name[0] ) name = cl->name;
		}
	}
	NXRP_PrintConsoleToPlayer(NULL, va("Player userinfo changed: %s", name));
}

void NXRP_OnPlayerBegin(int clientNum) {
	const char *name = "Unknown";
	if ( clientNum >= 0 && svs.clients && clientNum < sv_maxclients->integer ) {
		client_t *cl = &svs.clients[clientNum];
		if ( cl ) {
			if ( cl->nxrp_username[0] ) name = cl->nxrp_username;
			else if ( cl->name[0] ) name = cl->name;
		}
	}
	NXRP_PrintConsoleToPlayer(NULL, va("Player begin: %s", name));
}

void NXRP_OnPlayerDisconnect(int clientNum) {
	const char *name = "Unknown";
	if ( clientNum >= 0 && svs.clients && clientNum < sv_maxclients->integer ) {
		client_t *cl = &svs.clients[clientNum];
		if ( cl ) {
			if ( cl->nxrp_username[0] ) name = cl->nxrp_username;
			else if ( cl->name[0] ) name = cl->name;
		}
	}
	NXRP_PrintConsoleToPlayer(NULL, va("Player disconnected: %s", name));
}

void NXRP_OnPlayerKilled(client_t *attacker, client_t *victim, int meansOfDeath) {
	const char *aName = attacker ? (attacker->nxrp_username[0] ? attacker->nxrp_username : attacker->name) : "World";
	const char *vName = victim ? (victim->nxrp_username[0] ? victim->nxrp_username : victim->name) : "Unknown";
	NXRP_PrintConsoleToPlayer(NULL, va("Kill: %s killed %s (mod %d)", aName, vName, meansOfDeath));
}

void NXRP_OnScoreChanged(client_t *cl, int oldScore, int newScore) {
	const char *name = cl ? (cl->nxrp_username[0] ? cl->nxrp_username : cl->name) : "Unknown";
	//NXRP_PrintConsoleToPlayer(NULL, va("Score changed: %s %d -> %d", name, oldScore, newScore));

	// If the player's score increased, send them a chat notification
	//if ( cl && newScore > oldScore ) {
	//	int delta = newScore - oldScore;
		// Use server chat to inform the player. Escape quotes via format.
		//SV_SendServerCommand( cl, "chat \"You scored %d point%s! Total: %d\"\n", delta, (delta==1)?"":"s", newScore );
	//}
}

void NXRP_OnNPCSpawned(const char *npcName) {
	NXRP_PrintConsoleToPlayer(NULL, va("NPC spawned: %s", npcName ? npcName : "<unknown>"));
}

void NXRP_OnMapStart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Map started");
	// Attempt to resolve MBII's gSiegeRoundBegun global in the game VM so
	// we can detect the exact round-begin transition when available.
	void* dll = GVM_GetDllHandle();
	if (dll) {
		nxrp_gSiegeRoundBegun = (qboolean*)Sys_LoadFunction(dll, "gSiegeRoundBegun");
	} else {
		nxrp_gSiegeRoundBegun = NULL;
	}
	nxrp_roundBegunFlag = qfalse;
}

void NXRP_OnRoundEnd(int winningTeam) {
	NXRP_PrintConsoleToPlayer(NULL, va("Round ended, winning team %d", winningTeam));
}

void NXRP_OnRoundStart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Round started");
	// Announce in chat as well
	SV_SendServerCommand(NULL, "chat \"^3[NXRP]^7 Round started\"\n");
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
	/*const char* pName = "Unknown";
	if (!cl) {
		NXRP_PrintConsoleToPlayer(NULL, "Unknown player spawned");
		return;
	}
	if (cl->nxrp_username[0]) pName = cl->nxrp_username;
	else if (cl->name[0]) pName = cl->name;

	// Gather gentity info if available
	int entNum = -1;
	const char *entClass = "<no-entity>";
	float ox = 0.0f, oy = 0.0f, oz = 0.0f;
	if (cl->gentity) {
		// sharedEntity_t commonly exposes s.number and r.currentOrigin
		entNum = cl->gentity->s.number;
		entClass = cl->gentity->classname ? cl->gentity->classname : "<entity>";
		ox = cl->gentity->r.currentOrigin[0];
		oy = cl->gentity->r.currentOrigin[1];
		oz = cl->gentity->r.currentOrigin[2];
	}

	NXRP_PrintConsoleToPlayer(NULL, va("Player spawned: %s (ent %d \"%s\") at %.1f %.1f %.1f", pName, entNum, entClass, ox, oy, oz));*/
}
