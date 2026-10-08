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
qboolean SV_nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );
#ifdef __cplusplus
}
#endif

// additional handlers declared in nxrp_cmds.cpp - renamed to NXRP_ prefix
qboolean NXRP_HandleNxInfo( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxRegister( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxLogin( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxGiveAll( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxAccount( client_t *cl, const char *chatCursor );
qboolean NXRP_HandleNxNoclip( client_t *cl, const char *chatCursor );

// Event helpers
void NXRP_OnNPCKilled( client_t *killer, const char *npcName );
void NXRP_OnPlayerSpawned( client_t *cl );

#endif // NXRP_MAIN_H
