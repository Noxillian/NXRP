#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
// FFA helpers temporarily disabled per user request. The original
// implementation is retained below in a commented block for reference.
// Minimal stubs are provided so callers link cleanly while behavior is off.

/*
static qboolean gFFA_Enabled = qfalse;
static qboolean gRespawnWasNew = qfalse;
static char gRespawnWasMode[16];
static qboolean* gBNewRespawnMode = NULL;

struct nxrpRespawnTimers_t { int oneLife; int twoLife; int threeLife; int changePenalty; };
static nxrpRespawnTimers_t* gRespawnTimers = NULL;
static nxrpRespawnTimers_t gOriginalTimers;
static qboolean gHaveOriginalTimers = qfalse;
static qboolean gTimersOverridden = qfalse;
static qboolean gRespawnForced = qfalse;

static void NXRP_FFA_ApplyTimers(void) { /* ... original implementation commented out ... */ }
static void NXRP_FFA_ForceRespawnMode(void) { /* ... original implementation commented out ... */ }
void NXRP_FFA_Frame(void) { /* disabled */ }
qboolean NXRP_FFA_IsEnabled(void) { return qfalse; }
void NXRP_FFA_SetEnabled(qboolean enable) { (void)enable; }
void NXRP_FFA_Init(void) { /* disabled */ }

/*
Original implementation:

<omitted for brevity; full original implementation retained in patch history>

*/
