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

// Mirror social.cpp's respawn timers override so FFA truly enables "anytime"
struct nxrpRespawnTimers_t {
	int oneLife;
	int twoLife;
	int threeLife;
	int changePenalty;
};

static nxrpRespawnTimers_t* gRespawnTimers = NULL;
static nxrpRespawnTimers_t gOriginalTimers;
static qboolean gHaveOriginalTimers = qfalse;
static qboolean gTimersOverridden = qfalse;
static qboolean gRespawnForced = qfalse;

static void NXRP_FFA_ApplyTimers(void)
{
	if (!gRespawnTimers) return;

	if (gFFA_Enabled) {
		int secs = 3; // match social's short respawn
		if (secs < 1) secs = 1;
		gRespawnTimers->oneLife = secs;
		gRespawnTimers->twoLife = secs;
		gRespawnTimers->threeLife = secs;
		gRespawnTimers->changePenalty = secs;
		gTimersOverridden = qtrue;
	} else if (gTimersOverridden && gHaveOriginalTimers) {
		*gRespawnTimers = gOriginalTimers;
		gTimersOverridden = qfalse;
	}
}

static void NXRP_FFA_ForceRespawnMode(void)
{
	if (!gBNewRespawnMode) return;
	if (!gFFA_Enabled) {
		if (gRespawnForced) {
			*gBNewRespawnMode = gRespawnWasNew;
			Cvar_Set("RespawnMode", gRespawnWasMode);
			gRespawnForced = qfalse;
		}
		return;
	}
	if (!gRespawnForced) {
		gRespawnWasNew = *gBNewRespawnMode;
		Q_strncpyz(gRespawnWasMode, Cvar_VariableString("RespawnMode"), sizeof(gRespawnWasMode));
		gRespawnForced = qtrue;
	}
	if (!*gBNewRespawnMode) *gBNewRespawnMode = qtrue;
	if (Cvar_VariableIntegerValue("RespawnMode") != 1) Cvar_Set("RespawnMode", "1");
}

// Per-frame apply hook called from NXRP frame handler
void NXRP_FFA_Frame(void)
{
	// Keep timers and respawn mode in-sync each frame like social.cpp
	NXRP_FFA_ApplyTimers();
	NXRP_FFA_ForceRespawnMode();
}

qboolean NXRP_FFA_IsEnabled(void) {
	return gFFA_Enabled;
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

void NXRP_FFA_Init(void) {
	// Attempt to resolve MBII's bNewRespawnMode symbol if available
	void* dll = GVM_GetDllHandle();
	if (!dll) return;

	gBNewRespawnMode = (qboolean*)Sys_LoadFunction(dll, "bNewRespawnMode");
	// Attempt to resolve MBII's respawn timers structure (ctfcqRespawnTimers)
	gRespawnTimers = (nxrpRespawnTimers_t*)Sys_LoadFunction(dll, "ctfcqRespawnTimers");

	// Capture original timers if available. Only capture once per game load
	// to avoid recording our own overrides as the "original" values when
	// NXRP_FFA_Init is called multiple times across rounds. social.cpp uses
	// a per-game-load guard; replicate that behavior here.
	static qboolean gInitCaptured = qfalse;
	if (!gInitCaptured) {
		gHaveOriginalTimers = qfalse;
		gTimersOverridden = qfalse;
		if (gRespawnTimers) {
			gOriginalTimers = *gRespawnTimers;
			gHaveOriginalTimers = qtrue;
			// Sanity-check the struct values (seconds in 1..119)
			const int* t = (const int*)&gOriginalTimers;
			for (int i = 0; i < 4; i++) {
				if (t[i] < 1 || t[i] > 119) {
					// Looks wrong; disable timer override
					gRespawnTimers = NULL;
					gHaveOriginalTimers = qfalse;
					break;
				}
			}
		}
		gInitCaptured = qtrue;
	}

	// If FFA should be enabled by default like social mode, enable it now
	// and apply the timers/mode immediately.
	gFFA_Enabled = qtrue;
	NXRP_FFA_ApplyTimers();
	NXRP_FFA_ForceRespawnMode();
}
