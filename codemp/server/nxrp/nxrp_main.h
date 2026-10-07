/* nxrp_main.h - simple NXRP chat command handler
 * This file declares a handler that processes economy-style chat
 * commands beginning with '!'.
 */
#ifndef NXRP_MAIN_H
#define NXRP_MAIN_H

#include "../server.h"

qboolean SV_nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );

// additional handlers declared in nxrp_cmds.cpp
qboolean SV_nxrp_HandleNxInfo( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxGiveAll( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxAccount( client_t *cl, const char *chatCursor );

// Event helpers
void NXRP_OnNPCKilled( client_t *killer, const char *npcName );

#endif // NXRP_MAIN_H
