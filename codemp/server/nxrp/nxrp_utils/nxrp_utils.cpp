#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include <fstream>
#include <cctype>
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <map>
#include <sstream>

// Returns the new entity number, or -1 on failure.
int NXRP_SpawnModelAtClient(client_t* cl, const char* modelPath)
{
	if (!cl || !modelPath || !modelPath[0])
		return -1;

	// Get the game VM DLL.
	void* dll = GVM_GetDllHandle();
	if (!dll)
		return -1;

	// Resolve game module functions.
	void* gGSpawn_f = Sys_LoadFunction(dll, "G_Spawn");

	int (*gModelIndex_f)(const char*) =
		(int (*)(const char*))Sys_LoadFunction(dll, "G_ModelIndex");

	void* (*gGSpawn)() = (void* (*)())gGSpawn_f;

	if (!gGSpawn || !gModelIndex_f)
		return -1;

	// Spawn the entity and register its model.
	void* old = GVM_BeginNative();

	sharedEntity_t* e = (sharedEntity_t*)gGSpawn();
	const int model = e ? gModelIndex_f(modelPath) : 0;

	GVM_EndNative(old);

	if (!e || model <= 0)
		return -1;

	// Calculate spawn position in front of the client.
	vec3_t org = { 2.0f, -5.0f, 0.0f };

	if (cl->gentity)
	{
		VectorCopy(cl->gentity->r.currentOrigin, org);

		vec3_t fwd;
		AngleVectors(cl->gentity->s.angles, fwd, NULL, NULL);

		// spawn a little further in front and a bit lower than before
		const float forwardDist = 36.0f; // previously 24.0f
		const float zOffset = 8.0f;      // previously 16.0f (make it lower)

		org[0] += fwd[0] * forwardDist;
		org[1] += fwd[1] * forwardDist;
		org[2] += zOffset;
	}
	// finalize spawn at client using NXRP_PlaceModel
	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };
	float yaw = 0.0f;
	if (cl && cl->gentity) yaw = cl->gentity->s.angles[YAW];
	vec3_t zeroOffset = {0.0f, 0.0f, 0.0f};
	int entnum = NXRP_PlaceModel(modelPath, mins, maxs, yaw, zeroOffset, org);
	if (entnum >= 0) {
		NXRP_RegisterSpawn(entnum);
	}
	return entnum;
}

// Place a model with explicit prop bounds and yaw at a floor position.
int NXRP_PlaceModel(const char* name, const vec3_t propMins, const vec3_t propMaxs, float yaw, const vec3_t colliderOffset, const vec3_t floor)
{
	if (!name || !name[0]) return -1;

	void* dll = GVM_GetDllHandle();
	if (!dll) return -1;

	void* gGSpawn_f = Sys_LoadFunction(dll, "G_Spawn");
	int (*gModelIndex_f)(const char*) = (int (*)(const char*))Sys_LoadFunction(dll, "G_ModelIndex");
	void* (*gGSpawn)() = (void* (*)())gGSpawn_f;
	if (!gGSpawn || !gModelIndex_f) return -1;

	void* old = GVM_BeginNative();
	sharedEntity_t* e = (sharedEntity_t*)gGSpawn();
	const int model0 = e ? gModelIndex_f(name) : 0;
	int model = model0;
	// If the model wasn't found, try common alternate path forms the user
	// might provide (with or without the leading "models/" prefix or a
	// leading slash). This preserves compatibility with older commands that
	// accepted shorter names.
	if (model == 0 && name && name[0]) {
		// build a small buffer for alternates
		char alt[MAX_QPATH];
		// try without leading '/', if present
		const char* s = name;
		if (s[0] == '/') s++;
		// if it already starts with "models/", try without that prefix
		const char* prefix = "models/";
		size_t prefixLen = strlen(prefix);
		if (strncmp(s, prefix, prefixLen) == 0) {
			// try s + prefixLen
			Q_strncpyz(alt, s + prefixLen, sizeof(alt));
			model = gModelIndex_f(alt);
		}
		// if still not found and it doesn't start with models/, try prefixing it
		if (model == 0 && strncmp(s, prefix, prefixLen) != 0) {
			Com_sprintf(alt, sizeof(alt), "models/%s", s);
			model = gModelIndex_f(alt);
		}
		// lastly, try without any leading slash version in case caller used one
		if (model == 0 && name[0] == '/') {
			Q_strncpyz(alt, name + 1, sizeof(alt));
			model = gModelIndex_f(alt);
		}
	}
	GVM_EndNative(old);
	if (!e) return -1;
	// If the model index couldn't be resolved, free the spawned entity and fail
	if (model <= 0) {
		// attempt to free the entity to avoid leaving a gizmo/default model
		void* dll2 = GVM_GetDllHandle();
		if (dll2) {
			void* gFreeEntity_f = Sys_LoadFunction(dll2, "G_FreeEntity");
			void (*gFreeEntity)(void*) = (void (*)(void*))gFreeEntity_f;
			if (gFreeEntity) {
				void* old2 = GVM_BeginNative();
				gFreeEntity((void*)e);
				GVM_EndNative(old2);
			}
		}
		return -1;
	}

	vec3_t org = { floor[0], floor[1], floor[2] - propMins[2] };
	// Snap spawn origin to whole units so spawned props align to integer coordinates
	org[0] = roundf(org[0]); org[1] = roundf(org[1]); org[2] = roundf(org[2]);
	e->s.eType = ET_GENERAL;
	e->s.modelindex = model;
	VectorCopy(org, e->s.pos.trBase);
	VectorCopy(org, e->s.origin);
	VectorCopy(org, e->r.currentOrigin);
	e->s.pos.trType = TR_STATIONARY;
	VectorSet(e->s.apos.trBase, 0.0f, yaw, 0.0f);
	VectorCopy(e->s.apos.trBase, e->s.angles);
	VectorCopy(e->s.apos.trBase, e->r.currentAngles);
	e->s.apos.trType = TR_STATIONARY;

	// If the provided prop bounds are all zero, treat this as "no collider":
	// do not construct a collision box or mark the entity as solid.
	bool noCollider = (propMins[0] == 0.0f && propMins[1] == 0.0f && propMins[2] == 0.0f
		&& propMaxs[0] == 0.0f && propMaxs[1] == 0.0f && propMaxs[2] == 0.0f);

	if (noCollider) {
		// leave r.mins/maxs as zero-length and set absmin/absmax to the origin
		VectorSet(e->r.mins, 0.0f, 0.0f, 0.0f);
		VectorSet(e->r.maxs, 0.0f, 0.0f, 0.0f);
		VectorCopy(e->s.pos.trBase, e->r.absmin);
		VectorCopy(e->s.pos.trBase, e->r.absmax);
		// do not mark contents solid - leave as default (no collider)
		e->r.svFlags = 0;
		SV_LinkEntity(e);
	} else {
		// Compute a rotated box around the yaw like social.cpp does
		const float c = cosf(DEG2RAD(yaw)), sn = sinf(DEG2RAD(yaw));
		vec3_t mins = { 99999.0f, 99999.0f, propMins[2] }, maxs = { -99999.0f, -99999.0f, propMaxs[2] };
		for (int k = 0; k < 4; k++) {
			const float x = (k & 1) ? propMaxs[0] : propMins[0];
			const float y = (k & 2) ? propMaxs[1] : propMins[1];
			const float rx = x * c - y * sn, ry = x * sn + y * c;
			mins[0] = Q_min(mins[0], rx); maxs[0] = Q_max(maxs[0], rx);
			mins[1] = Q_min(mins[1], ry); maxs[1] = Q_max(maxs[1], ry);
		}
		VectorCopy(mins, e->r.mins);
		VectorCopy(maxs, e->r.maxs);

		// Apply colliderOffset to shift the collision bounds relative to the entity origin
		VectorAdd(e->r.mins, colliderOffset, e->r.mins);
		VectorAdd(e->r.maxs, colliderOffset, e->r.maxs);

		// Use the rotated prop bounds as-is (match Holo_PlaceModel behavior):
		// copy mins/maxs already set above and enable solid contents, then link.
		VectorAdd(e->s.pos.trBase, e->r.mins, e->r.absmin);
		VectorAdd(e->s.pos.trBase, e->r.maxs, e->r.absmax);

		e->r.contents = CONTENTS_SOLID;
		e->r.svFlags = 0;
		SV_LinkEntity(e);
	}
	return e->s.number;
}

// Spawn a prop at the client's origin using given model and prop bounds
int NXRP_SpawnPropAtClient(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw)
{
	if (!cl || !modelPath) return -1;
	vec3_t floor;
	if (cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);
	else { VectorSet(floor, 0,0,0); }
	vec3_t zeroOffset2 = {0.0f, 0.0f, 0.0f};
	return NXRP_PlaceModel(modelPath, propMins, propMaxs, yaw, zeroOffset2, floor);
}

// Simple tracking for last NXRP spawn
static int nxrp_last_spawn = -1;

void NXRP_RegisterSpawn(int entnum) {
	nxrp_last_spawn = entnum;
	// Notify NPC subsystem so it can associate any pending name with this entnum
	extern void NXRP_HandlePendingNPCSpawn(int entnum);
	NXRP_HandlePendingNPCSpawn(entnum);
}

int NXRP_GetLastSpawn() {
	return nxrp_last_spawn;
}

// Hex encoding helpers
std::string NXRP_HexEncode(const unsigned char *data, size_t len) {
	static const char* hex = "0123456789abcdef";
	std::string out; out.reserve(len*2);
	for (size_t i = 0; i < len; ++i) {
		unsigned char b = data[i];
		out.push_back(hex[(b >> 4) & 0xF]);
		out.push_back(hex[b & 0xF]);
	}
	return out;
}

bool NXRP_HexDecode(const std::string &hex, std::vector<unsigned char> &out) {
	size_t n = hex.size();
	if (n % 2 != 0) return false;
	out.clear(); out.reserve(n/2);
	for (size_t i = 0; i < n; i += 2) {
		auto cv = [](char c)->int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		int hi = cv(hex[i]); int lo = cv(hex[i+1]);
		if (hi < 0 || lo < 0) return false;
		out.push_back((unsigned char)((hi<<4) | lo));
	}
	return true;
}

// Escape string for JSON-like output (minimal)
std::string NXRP_EscapeString(const std::string &in) {
	std::string out; out.reserve(in.size()*2);
	for (char c : in) {
		if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
		else if (c == '\n') { out += "\\n"; }
		else out.push_back(c);
	}
	return out;
}

// Other utility functions are unchanged and remain in this file.

// Ensure the client is logged in; if not, a message is sent and false is returned.
bool NXRP_EnsureLoggedIn(client_t* cl) {
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}
	return true;
}

// Helper: read raw account JSON content
bool NXRP_ReadAccountContentForClient(client_t* cl, std::string& outContent, std::string& outSafeUsername) {
	if (!cl || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}

	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid stored username");
		return false;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	// Use shared file helper to read the file content
	if (!NXRP_ReadFileToString(filepath.c_str(), outContent)) {
		NXRP_PrintConsoleToPlayer(cl, "Account file not found");
		return false;
	}
	outSafeUsername = safe;
	return true;
}

// Check whether the client's account has isAdmin=true;
bool NXRP_IsClientAdmin(client_t* cl) {
	if (!cl || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}

	// Build sanitized username and account file path without loading full content
	std::string user = cl->nxrp_username;
	std::string safe;
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Invalid stored username");
		return false;
	}

	const char *home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_accounts" : std::string("nxrp_accounts");
	std::string filepath = dir + "/" + safe + ".json";

	std::string sval;
	if (!NXRP_GetAccountFieldFromFile(filepath.c_str(), "isAdmin", sval)) {
		NXRP_PrintConsoleToPlayer(cl, "Admin flag missing");
		return false;
	}

	// normalize and interpret value
	std::string v = sval;
	for (auto &ch : v) ch = (char)std::tolower((unsigned char)ch);
	bool isAdmin = false;
	if (v == "true" || v == "1") isAdmin = true;
	else if (v == "false" || v == "0") isAdmin = false;
	else {
		NXRP_PrintConsoleToPlayer(cl, "Admin flag parse error");
		return false;
	}

	if (!isAdmin) {
		NXRP_PrintConsoleToPlayer(cl, "You are not an admin");
		return false;
	}
	return true;
}
