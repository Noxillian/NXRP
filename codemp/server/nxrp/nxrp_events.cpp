#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
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
// 1-based round counter for the current map; 0 before first round
static int nxrp_roundNumber = 0;
// Pointer into the game VM for MBII's gSiegeRoundBegun flag (if available)
static qboolean* nxrp_gSiegeRoundBegun = NULL;
// Local cached state to detect transition
static qboolean nxrp_roundBegunFlag = qfalse;
// MBII per-round timer pointers (optional)
static int* nxrp_rebelCountdown = NULL;
static int* nxrp_imperialCountdown = NULL;
static int nxrp_prevRebelCountdown = 0;
static int nxrp_prevImperialCountdown = 0;
static qboolean nxrp_npcDeadSeen[MAX_GENTITIES];
static qboolean nxrp_npcEverAlive[MAX_GENTITIES];

qboolean NXRP_TryMarkNPCKillSeen(int entNum) {
	if (entNum < 0 || entNum >= MAX_GENTITIES) return qfalse;
	if (nxrp_npcDeadSeen[entNum]) return qfalse;
	nxrp_npcDeadSeen[entNum] = qtrue;
	return qtrue;
}

void SV_NXRP_NPCDeathPollFrame(void) {
	if (!svs.clients) return;
	for (int entNum = sv_maxclients->integer; entNum < sv.num_entities && entNum < MAX_GENTITIES; entNum++) {
		sharedEntity_t *ent = SV_GentityNum(entNum);
		if (!ent || ent->s.eType != ET_NPC) {
			nxrp_npcDeadSeen[entNum] = qfalse;
			nxrp_npcEverAlive[entNum] = qfalse;
			continue;
		}
		qboolean dead = qfalse;
		if (ent->s.eFlags & EF_DEAD) dead = qtrue;
		if (ent->playerState && ent->playerState->stats[STAT_HEALTH] <= 0) dead = qtrue;
		if (ent->health <= 0) dead = qtrue;
		if (!dead) {
			nxrp_npcEverAlive[entNum] = qtrue;
			nxrp_npcDeadSeen[entNum] = qfalse;
			continue;
		}

		if (nxrp_npcEverAlive[entNum]) {
			if (NXRP_TryMarkNPCKillSeen(entNum)) {
				const char *npcName = NULL;
				if (ent->targetname && ent->targetname[0]) npcName = ent->targetname;
				else if (ent->classname && ent->classname[0]) npcName = ent->classname;
				NXRP_OnNPCKilled(NULL, npcName);
			}
		}
	}
}

void SV_NXRP_ScoreInit(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		nxrp_lastScore[i] = 0;
		nxrp_lastKills[i] = 0;
	}
	for (int i = 0; i < MAX_GENTITIES; i++) {
		nxrp_npcDeadSeen[i] = qfalse;
		nxrp_npcEverAlive[i] = qfalse;
	}
	NXRP_FFA_Frame();
}
// FFA frame call removed per user request: callers should still build.
// Called when a player uses (presses use) on an entity. entnum is the gentity index.
void NXRP_OnNPCUsed( client_t *user, int entnum ) {
	if (!user) return;
	// Log NPC use for now; nxrp_npcs provides APIs to map entnums to NPC names.
	NXRP_PrintConsoleToPlayer(NULL, va("NXRP_OnNPCUsed called by %s on ent=%d", user->name, entnum));
}

void SV_NXRP_ScoreFrame(void) {

	if (!svs.clients) return;
	// If intermission is active, reset round start state so we trigger again
	if (sv.configstrings[CS_INTERMISSION] && atoi(sv.configstrings[CS_INTERMISSION]) == 1) {
		NXRP_PrintConsoleToPlayer(NULL, "Round Restart");
		nxrp_roundStartTime = 0;
		nxrp_roundBegunFlag = qfalse;
		nxrp_prevRebelCountdown = 0;
		nxrp_prevImperialCountdown = 0;
	// Reset per-map round number
	nxrp_roundNumber = 0;

	} else {
		// Not intermission: trigger round start when the first active player is seen.
		if (nxrp_roundStartTime == 0) {
			for (int i = 0; i < sv_maxclients->integer; i++) {
				client_t* cl = &svs.clients[i];
				if (cl->state == CS_ACTIVE && cl->gentity) {
					nxrp_roundStartTime = svs.time;
					NXRP_OnMapStart();

					// Also treat this as a round start.
					NXRP_OnRoundStart();
					nxrp_roundBegunFlag = qtrue;

					// Initialize last-kill counters from current persistent values
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
		// Timer-based detection: check MBII's countdowns for a transition
		if (!nxrp_roundBegunFlag && (nxrp_rebelCountdown || nxrp_imperialCountdown)) {
			int r = nxrp_rebelCountdown ? *nxrp_rebelCountdown : 0;
			int im = nxrp_imperialCountdown ? *nxrp_imperialCountdown : 0;
			// detect transition from non-positive to positive countdown
			if ((r > 0 && nxrp_prevRebelCountdown <= 0) || (im > 0 && nxrp_prevImperialCountdown <= 0)) {
				nxrp_roundBegunFlag = qtrue;
				NXRP_OnRoundStart();
			}
			nxrp_prevRebelCountdown = r;
			nxrp_prevImperialCountdown = im;
		}
		// If the game VM exposes a siege-round-begun flag, detect its rising edge.
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
	NXRP_PrintConsoleToPlayer(NULL, va("Kill archived: %s %d -> %d", name, oldKills, newKills));
	//Com_Printf("Kill archived: %s %d -> %d\n", name, oldKills, newKills);

	// Detect teamkills from the victim-side kill change and restore attacker score.
	if (!cl || !cl->gentity || !cl->gentity->playerState) return;
	int attackerNum = cl->gentity->playerState->persistant[PERS_ATTACKER];
	if (attackerNum < 0 || attackerNum >= sv_maxclients->integer) return;

	client_t *attacker = &svs.clients[attackerNum];
	if (!attacker || !attacker->gentity || !attacker->gentity->playerState) return;

	int victimTeam = cl->gentity->playerState->persistant[PERS_TEAM];
	int attackerTeam = attacker->gentity->playerState->persistant[PERS_TEAM];
	if (victimTeam != attackerTeam) return;

	void *dll = GVM_GetDllHandle();
	if (!dll) return;

	typedef void (*AddScoreFn)(void *ent, float *origin, int score);
	AddScoreFn addscore = (AddScoreFn)Sys_LoadFunction(dll, "AddScore");
	if (!addscore) return;

	float org[3];
	org[0] = attacker->gentity->r.currentOrigin[0];
	org[1] = attacker->gentity->r.currentOrigin[1];
	org[2] = attacker->gentity->r.currentOrigin[2];

	void *old = GVM_BeginNative();
	addscore(attacker->gentity, org, 1);
	GVM_EndNative(old);
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
	NXRP_PrintConsoleToPlayer(NULL, va("^3[NXRP-PLAYERKILL]^7 %s killed %s (mod %d)", aName, vName, meansOfDeath));
}

void NXRP_OnScoreChanged(client_t *cl, int oldScore, int newScore) {
	const char *name = cl ? (cl->nxrp_username[0] ? cl->nxrp_username : cl->name) : "Unknown";
	//NXRP_PrintConsoleToPlayer(NULL, va("Score changed: %s %d -> %d", name, oldScore, newScore));

	//If the player's score increased, send them a notification
	if ( cl && newScore > oldScore ) {
		int delta = newScore - oldScore;
		NXRP_PrintConsoleToPlayer(cl, va("You scored %d point%s! Total: %d", delta, (delta==1)?"":"s", newScore));
	}
}

void NXRP_OnNPCSpawned(const char *npcName) {
	NXRP_PrintConsoleToPlayer(NULL, va("NPC spawned: %s", npcName ? npcName : "<unknown>"));
}

void NXRP_OnMapStart(void) {
	//NXRP_PrintConsoleToPlayer(NULL, "Map started");
	// Attempt to resolve MBII's gSiegeRoundBegun global in the game VM so
	// we can detect the exact round-begin transition when available.
	void* dll = GVM_GetDllHandle();
	if (dll) {
		nxrp_gSiegeRoundBegun = (qboolean*)Sys_LoadFunction(dll, "gSiegeRoundBegun");
		nxrp_rebelCountdown = (int*)Sys_LoadFunction(dll, "gRebelCountdown");
		nxrp_imperialCountdown = (int*)Sys_LoadFunction(dll, "gImperialCountdown");
	} else {
		nxrp_gSiegeRoundBegun = NULL;
	}
	nxrp_roundBegunFlag = qfalse;
	nxrp_prevRebelCountdown = 0;
	nxrp_prevImperialCountdown = 0;

	// Initialize NXRP FFA hooks for this map and apply immediately
	NXRP_FFA_Init();
	NXRP_FFA_Frame();
}

void NXRP_OnRoundEnd(int winningTeam) {
	NXRP_PrintConsoleToPlayer(NULL, va("Round ended, winning team %d", winningTeam));
	// Reset round-detection state so subsequent rounds fire correctly
	nxrp_roundBegunFlag = qfalse;
	nxrp_roundStartTime = 0;
	nxrp_prevRebelCountdown = 0;
	nxrp_prevImperialCountdown = 0;
}

void NXRP_OnRoundStart(void) {
	//NXRP_PrintConsoleToPlayer(NULL, "Round started");
	// Increment round counter and re-resolve/reapply FFA hooks at round start
	nxrp_roundNumber++;
	NXRP_FFA_Init();
	NXRP_FFA_Frame();
}

void NXRP_OnRoundRestart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Round restart");
	// Reset round-detection state so the next round start is detected
	nxrp_roundBegunFlag = qfalse;
	nxrp_roundStartTime = 0;
	nxrp_prevRebelCountdown = 0;
	nxrp_prevImperialCountdown = 0;

	// Ensure FFA helpers reinitialize on round restart
	NXRP_FFA_Init();
	NXRP_FFA_Frame();
}

void NXRP_OnMatchStart(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Match start");
}

void NXRP_OnMatchEnd(void) {
	NXRP_PrintConsoleToPlayer(NULL, "Match end");
}

int NXRP_GetRoundNumber(void) {
	return nxrp_roundNumber;
}


void NXRP_OnNPCKilled(client_t *killer, const char *npcName) {
	const char *pName = "Unknown";
	if (killer) {
		if (killer->nxrp_username[0]) pName = killer->nxrp_username;
		else if (killer->name[0]) pName = killer->name;
	}
	const char *nName = (npcName && npcName[0]) ? npcName : "NPC";
	NXRP_PrintConsoleToPlayer(NULL, va("^3[NXRP-NPCKILL]^7 %s killed %s", pName, nName));
	SV_SendServerCommand(NULL, va("chat \"^3[NXRP-NPCKILL]^7 %s killed %s\"", pName, nName));
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
