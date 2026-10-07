/* nxrp_accounts.h - account handlers for nxrp extension */
#ifndef NXRP_ACCOUNTS_H
#define NXRP_ACCOUNTS_H

#include "../server.h"

qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor );
qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor );

#endif // NXRP_ACCOUNTS_H
