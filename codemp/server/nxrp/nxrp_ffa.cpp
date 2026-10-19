#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
// FFA helpers temporarily disabled per user request.
// Keep no-op exports so callers link cleanly while behavior is off.

void NXRP_FFA_Frame(void) {}

qboolean NXRP_FFA_IsEnabled(void) {
	return qfalse;
}

void NXRP_FFA_SetEnabled(qboolean enable) {
	(void)enable;
}

void NXRP_FFA_Init(void) {}
