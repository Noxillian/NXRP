/* nxrp_main.h - simple NXRP chat command handler
 * This file declares a handler that processes economy-style chat
  * commands beginning with '!'. (header touch)
 */
#ifndef NXRP_MAIN_H
#define NXRP_MAIN_H

#include "../server.h"

// Primary chat handler called from server code.
// Use the NXRP_ prefix consistently across the project.
qboolean NXRP_HandleChatCommands( client_t *cl, const char *commandName, const char *chatCursor );

#ifdef __cplusplus
extern "C" {
#endif
// C-linked server entrypoint (called from C translation units such as sv_client.c)
qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );
#ifdef __cplusplus
}
#endif

// additional handlers declared in nxrp_cmds.cpp - renamed to NXRP_ prefix
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_register( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_login( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_giveall( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_boss( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_account( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor );
// Non-admin: report player position
qboolean NXRP_cmd_pos( client_t *cl, const char *chatCursor );
// Admin-only: spawn a model with a provided path (command: !nx mspawn)
qboolean NXRP_cmdAdmin_mspawn( client_t *cl, const char *chatCursor );
// New prop-style spawn command (wrapper for backward compatibility)
qboolean NXRP_cmdAdmin_prop_s( client_t *cl, const char *chatCursor );
// Remove a saved prop spawn by index
qboolean NXRP_cmdAdmin_prop_r( client_t *cl, const char *chatCursor );
// Load saved prop spawns for the current map
qboolean NXRP_cmdAdmin_prop_load( client_t *cl, const char *chatCursor );
// Admin-only: remove the last spawned model (!nx mremove)
qboolean NXRP_cmdAdmin_mremove( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_mmove( client_t *cl, const char *chatCursor );
// Admin-only: player-model placeholder command (currently same as NXRP_admincmdNxModel)
qboolean NXRP_cmdAdmin_pmodel( client_t *cl, const char *chatCursor );
// Admin-only: spawn an NPC or misc model test
qboolean NXRP_cmdAdmin_spawn( client_t *cl, const char *chatCursor );
// Admin-only: teleport player upward
qboolean NXRP_cmdAdmin_tp( client_t *cl, const char *chatCursor );
// Admin-only: test spawn command
qboolean NXRP_cmdAdmin_test( client_t *cl, const char *chatCursor );
// Admin-only: additional test command (prop placement)
qboolean NXRP_cmdAdmin_test2( client_t *cl, const char *chatCursor );
// Admin-only: spawn a vehicle
qboolean NXRP_cmdAdmin_vehicle( client_t *cl, const char *chatCursor );
// Admin-only: toggle FFA respawn mode (on/off/toggle)
qboolean NXRP_cmdAdmin_ffa( client_t *cl, const char *chatCursor );
// Admin-only: strip the invoking player of weapons and force powers
qboolean NXRP_cmdAdmin_strip( client_t *cl, const char *chatCursor );
// Admin-only: list entities or other map info (usage: !nx list e)
qboolean NXRP_cmdAdmin_list( client_t *cl, const char *chatCursor );
// NXRP-specific listing of saved map spawns
void NXRP_ListSavedSpawnsForPlayer(client_t* cl);

// Event helpers
void NXRP_OnNPCKilled( client_t *killer, const char *npcName );
void NXRP_OnPlayerSpawned( client_t *cl );
// Additional NXRP event hooks
void NXRP_OnPlayerConnect(int clientNum, qboolean firstTime, qboolean isBot);
void NXRP_OnPlayerUserinfoChanged(int clientNum);
void NXRP_OnPlayerBegin(int clientNum);
void NXRP_OnPlayerDisconnect(int clientNum);
void NXRP_OnPlayerKilled(client_t *attacker, client_t *victim, int meansOfDeath);
void NXRP_OnScoreChanged(client_t *cl, int oldScore, int newScore);
void NXRP_OnNPCSpawned(const char *npcName);
void NXRP_OnMapStart(void);
void NXRP_OnRoundStart(void);
void NXRP_OnRoundEnd(int winningTeam);
void NXRP_OnRoundRestart(void);
void NXRP_OnMatchStart(void);
void NXRP_OnMatchEnd(void);

// Round index helper (1-based within the current map). Returns 0 before
// the first round has started.
int NXRP_GetRoundNumber(void);

// Server-side NXRP helpers
void SV_NXRP_ScoreInit(void);
void SV_NXRP_ScoreFrame(void);

// FFA respawn helpers
void NXRP_FFA_Init(void);
void NXRP_FFA_SetEnabled(qboolean enable);
qboolean NXRP_FFA_IsEnabled(void);
// Frame hook to reapply timers/mode each server frame while FFA is enabled
void NXRP_FFA_Frame(void);
// FFA helpers are optional; commented out while disabled
//void NXRP_FFA_SetEnabled(qboolean enable);
//qboolean NXRP_FFA_IsEnabled(void);

// helper to spawn a misc model near a client (implemented in social.cpp)
int SV_SpawnModelAtClient(client_t* cl, const char* modelPath);

#endif // NXRP_MAIN_H
