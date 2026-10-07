#pragma once

// NXRP public server-facing API (per-frame wrapper and event helpers).

// Called every server frame via SV_NXRPFrame() in sv_main.cpp
void SV_NXRPFrame(void);

// Internal NXRP frame implementation - defined in nxrp_events.cpp
void NXRP_Frame(void);

// Event helpers
void NXRP_OnNPCKilled(void* killer, const char* npcName); // killer is client_t*; use void* to avoid header order issues
void NXRP_OnRoundStart(void);
