#include "server/nxrp/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include <fstream>
#include <cctype>
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <cmath>

// Spawn a misc model near a client using the game module's spawn helpers.
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

		org[0] += fwd[0] * 24.0f;
		org[1] += fwd[1] * 24.0f;
		org[2] += 16.0f;
	}

	// Set entity type and model.
	e->s.eType = ET_GENERAL;
	e->s.modelindex = model;

	// Initialize default bounds first.
	// These remain in use if the game module has no bounds helper.
	VectorSet(e->r.mins, -16.0f, -16.0f, -8.0f);
	VectorSet(e->r.maxs,  16.0f,  16.0f, 16.0f);

	// Ask the game module to calculate model-specific bounds.
	void (*G_SetModelBoundsForEntity_f)(sharedEntity_t*) =
		(void (*)(sharedEntity_t*))
		Sys_LoadFunction(dll, "G_SetModelBoundsForEntity");

	if (G_SetModelBoundsForEntity_f)
	{
		old = GVM_BeginNative();
		G_SetModelBoundsForEntity_f(e);
		GVM_EndNative(old);

		// Ensure collision bounds are reasonable: add a small padding and enforce a minimum extent
		vec3_t size;
		VectorSubtract(e->r.maxs, e->r.mins, size);
		float maxDim = fabsf(size[0]);
		if (fabsf(size[1]) > maxDim) maxDim = fabsf(size[1]);
		if (fabsf(size[2]) > maxDim) maxDim = fabsf(size[2]);

		const float minExtent = 24.0f; // minimum half-extent to avoid tiny collision boxes
		const float padding = 8.0f;    // extra padding around model bounds

		if (maxDim < minExtent) {
			VectorSet(e->r.mins, -minExtent - padding, -minExtent - padding, -minExtent - padding);
			VectorSet(e->r.maxs,  minExtent + padding,  minExtent + padding,  minExtent + padding);
		} else {
			vec3_t padNeg = { -padding, -padding, -padding };
			vec3_t padPos = {  padding,  padding,  padding };
			VectorAdd(e->r.mins, padNeg, e->r.mins);
			VectorAdd(e->r.maxs, padPos, e->r.maxs);
		}

		// Recompute absolute bounds relative to the entity origin
		VectorAdd(e->s.pos.trBase, e->r.mins, e->r.absmin);
		VectorAdd(e->s.pos.trBase, e->r.maxs, e->r.absmax);
	}

	// Set position and angles.
	VectorCopy(org, e->s.pos.trBase);
	VectorCopy(org, e->s.origin);
	VectorCopy(org, e->r.currentOrigin);

	e->s.pos.trType = TR_STATIONARY;

	// Set the entity yaw so the model looks toward the player when possible.
	float entYaw = 0.0f;
	if (cl->gentity) {
		vec3_t toPlayer;
		VectorSubtract(cl->gentity->r.currentOrigin, org, toPlayer);
		entYaw = atan2f(toPlayer[1], toPlayer[0]) * (180.0f / (float)M_PI);
	} else {
		entYaw = 0.0f;
	}

	VectorSet(
		e->s.apos.trBase,
		0.0f,
		entYaw,
		0.0f
	);

	VectorCopy(e->s.apos.trBase, e->s.angles);
	VectorCopy(e->s.apos.trBase, e->r.currentAngles);

	e->s.apos.trType = TR_STATIONARY;

	// Enable collision.
	e->r.contents = CONTENTS_SOLID;
	e->r.svFlags = 0;

	// Link only after the bounds and position have been set.
	SV_LinkEntity(e);

	return e->s.number;
}


bool NXRP_EnsureLoggedIn(client_t* cl) {
	if (!cl || cl->state != CS_ACTIVE || !cl->nxrp_username[0]) {
		NXRP_PrintConsoleToPlayer(cl, "You must be logged in to use this command");
		return false;
	}
	return true;
}

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

	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "Account file not found");
		return false;
	}

	outContent.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	outSafeUsername = safe;
	return true;
}

bool NXRP_IsClientAdmin(client_t* cl) {
	std::string content, safe;
	if (!NXRP_ReadAccountContentForClient(cl, content, safe)) return false;

	const std::string key = "\"isAdmin\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		NXRP_PrintConsoleToPlayer(cl, "Admin flag missing");
		return false;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { NXRP_PrintConsoleToPlayer(cl, "Admin flag parse error"); return false; }
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) { NXRP_PrintConsoleToPlayer(cl, "Admin flag parse error"); return false; }
	bool isAdmin = false;
	if (content.compare(pos, 4, "true") == 0) isAdmin = true;
	else if (content.compare(pos, 5, "false") == 0) isAdmin = false;
	else {
		if (content[pos] == '"') {
			size_t qend = content.find('"', pos + 1);
			if (qend != std::string::npos) {
				std::string tok = content.substr(pos + 1, qend - (pos + 1));
				if (!tok.empty() && (tok == "true" || tok == "1")) isAdmin = true;
			}
		}
	}

	if (!isAdmin) {
		NXRP_PrintConsoleToPlayer(cl, "You are not an admin");
		return false;
	}
	return true;
}
