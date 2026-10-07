/* nxrp_main.h - simple NXRP chat command handler
 * This file declares a handler that processes economy-style chat
 * commands beginning with '!'.
 */
#ifndef NXRP_MAIN_H
#define NXRP_MAIN_H

#include "../server.h"

qboolean SV_nxrp_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );

#endif // NXRP_MAIN_H
