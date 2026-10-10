#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
// <cstdio> not required in this TU; va() is provided by the engine headers included elsewhere

// Ensure the print helper is visible in this translation unit
void NXRP_PrintConsoleToPlayer(client_t* cl, const char* text);

// Score polling state
static int nxrp_lastScore[MAX_CLIENTS];

void SV_NXRP_ScoreInit(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) nxrp_lastScore[i] = 0;
}

void SV_NXRP_ScoreFrame(void) {
	if (!svs.clients) return;
	for (int i = 0; i < sv_maxclients->integer; i++) {
		client_t* cl = &svs.clients[i];
		if (cl->state != CS_ACTIVE || !cl->gentity || !cl->gentity->playerState) continue;
		int newScore = cl->gentity->playerState->persistant[PERS_SCORE];
		int oldScore = nxrp_lastScore[i];
		if (newScore != oldScore) {
			NXRP_OnScoreChanged(cl, oldScore, newScore);
			nxrp_lastScore[i] = newScore;
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
	NXRP_PrintConsoleToPlayer(NULL, va("Score changed: %s %d -> %d", name, oldScore, newScore));

	// If the player's score increased, send them a chat notification
	if ( cl && newScore > oldScore ) {
		int delta = newScore - oldScore;
		// Use server chat to inform the player. Escape quotes via format.
		SV_SendServerCommand( cl, "chat \"You scored %d point%s! Total: %d\"\n", delta, (delta==1)?"":"s", newScore );
	}
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

	NXRP_PrintConsoleToPlayer(NULL, va("Player spawned: %s (ent %d \"%s\") at %.1f %.1f %.1f", pName, entNum, entClass, ox, oy, oz));
}
