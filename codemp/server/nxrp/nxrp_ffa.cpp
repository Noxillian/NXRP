#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"

// Mirror social's ForceRespawn behavior but keep damage enabled.
// We will toggle MBII's bNewRespawnMode and RespawnMode cvar when FFA is enabled.

static qboolean gFFA_Enabled = qfalse;
static qboolean gRespawnWasNew = qfalse;
static char gRespawnWasMode[16];
static qboolean* gBNewRespawnMode = NULL;

void NXRP_FFA_Init(void) {
	// Attempt to resolve MBII's bNewRespawnMode symbol if available
	void* dll = GVM_GetDllHandle();
	if (dll) {
		gBNewRespawnMode = (qboolean*)Sys_LoadFunction(dll, "bNewRespawnMode");
	}

qboolean NXRP_FFA_IsEnabled(void) {
	return gFFA_Enabled;
}
}

void NXRP_FFA_SetEnabled(qboolean enable) {
	if (gFFA_Enabled == enable) return;
	gFFA_Enabled = enable;

	if (!gBNewRespawnMode) {
		// Can't force respawn mode without the symbol; set the RespawnMode cvar
		if (enable) {
			Q_strncpyz(gRespawnWasMode, Cvar_VariableString("RespawnMode"), sizeof(gRespawnWasMode));
			Cvar_Set("RespawnMode", "1");
		} else {
			Cvar_Set("RespawnMode", gRespawnWasMode);
		}
		return;
	}

	if (enable) {
		gRespawnWasNew = *gBNewRespawnMode;
		Q_strncpyz(gRespawnWasMode, Cvar_VariableString("RespawnMode"), sizeof(gRespawnWasMode));
		*gBNewRespawnMode = qtrue;
		if (Cvar_VariableIntegerValue("RespawnMode") != 1) Cvar_Set("RespawnMode", "1");
	} else {
		*gBNewRespawnMode = gRespawnWasNew;
		Cvar_Set("RespawnMode", gRespawnWasMode);
	}
}
