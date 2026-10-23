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
	float yaw = 0.0f;
	if (cl && cl->gentity) yaw = cl->gentity->s.angles[YAW];
	int entnum = NXRP_PlaceModel(modelPath, org, yaw);
	NXRP_PrintConsoleToPlayer(NULL, "Spawned Model at Client");
	if (entnum >= 0) {
		NXRP_RegisterSpawn(entnum);
	}
	return entnum;
}

// Spawn a prop at the client's origin using given model and prop bounds
int NXRP_SpawnPropAtClient(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw)
{
	if (!cl || !modelPath) return -1;
	vec3_t position;
	if (cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, position);
	else { VectorSet(position, 0,0,0); }
	return NXRP_PlaceModel(modelPath, position, yaw);
}

// Simple tracking for last NXRP spawn
static int nxrp_last_spawn = -1;

void NXRP_RegisterSpawn(int entnum) {
	nxrp_last_spawn = entnum;
	// Notify NPC subsystem so it can associate any pending name with this entnum
	extern void NXRP_HandlePendingNPCSpawn(int entnum);
	// Debug: announce register spawn invocation
	NXRP_PrintConsoleToPlayerFmt(NULL, "NXRP_RegisterSpawn called ent=%d", entnum);
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

	std::string dir;
	NXRP_GetFolderPath("nxrp_accounts", dir);
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

	std::string dir;
	NXRP_GetFolderPath("nxrp_accounts", dir);
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
