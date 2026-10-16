#include "server/nxrp/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include <fstream>
#include <cctype>
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <sys/stat.h>
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
	int entnum = NXRP_PlaceModel(modelPath, mins, maxs, yaw, org);
	if (entnum >= 0) {
		NXRP_RegisterSpawn(entnum);
	}
	return entnum;
}

// Load saved entries for the current map and spawn them. Returns how many were spawned.
int NXRP_LoadSavedSpawnsForMap(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_spawns" : std::string("nxrp_spawns");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) return 0;

	std::string line;
	int spawned = 0;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		// Very small parser to extract model and origin/angles from the saved line.
		// Expect format: { "ent": N, "model": "path", "origin": [x, y, z], "angles": [ax, ay, az] }
		const char* s = line.c_str();
		const char* modelKey = "\"model\"";
		const char* originKey = "\"origin\"";
		const char* anglesKey = "\"angles\"";
		const char* m = strstr(s, modelKey);
		if (!m) continue;
		m = strchr(m, ':'); if (!m) continue; m++;
		while (*m && isspace((unsigned char)*m)) m++;
		if (*m != '"') continue; m++;
		const char* modelStart = m;
		const char* modelEnd = strchr(modelStart, '"');
		if (!modelEnd) continue;
		std::string modelPath(modelStart, modelEnd - modelStart);

		float ox=0, oy=0, oz=0;
		const char* o = strstr(s, originKey);
		if (o) {
			o = strchr(o, '[');
			if (o) {
				o++;
				sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz);
			}
		}

		float ax=0, ay=0, az=0;
		const char* a = strstr(s, anglesKey);
		if (a) {
			a = strchr(a, '[');
			if (a) {
				a++;
				sscanf(a, "%f %*[, ] %f %*[, ] %f", &ax, &ay, &az);
			}
		}

		// Place model at parsed origin and yaw (use ay as yaw)
		vec3_t mins = { -24.0f, -24.0f, -8.0f };
		vec3_t maxs = {  24.0f,  24.0f, 24.0f };
		vec3_t floor = { ox, oy, oz };
		int ent = NXRP_PlaceModel(modelPath.c_str(), mins, maxs, ay, floor);
		if (ent >= 0) {
			NXRP_RegisterSpawn(ent);
			spawned++;
		}
	}
	ifs.close();
	return spawned;
}

// Remove a saved spawn entry by its zero-based index. This rewrites the
// per-map file excluding the requested index. Returns true on success.
bool NXRP_RemoveSavedSpawnByIndex(int index) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_spawns" : std::string("nxrp_spawns");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) return false;

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(ifs, line)) {
		if (!line.empty()) lines.push_back(line);
	}
	ifs.close();

	if (index < 0 || index >= (int)lines.size()) return false;

	// Remove the selected entry and rewrite file
	lines.erase(lines.begin() + index);
	FILE* f = fopen(filename.c_str(), "w");
	if (!f) return false;
	for (const auto &l : lines) {
		fputs(l.c_str(), f);
		fputc('\n', f);
	}
	fclose(f);
	return true;
}

// Persist a spawn to a per-map JSON file in the fs_homepath/nxrp_spawns directory.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath) {
	if (entnum < 0 || !modelPath) return;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	// Build JSON entry: { "ent": N, "model": "path", "origin": [x,y,z], "angles": [x,y,z] }
	char buf[1024];
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_spawns" : std::string("nxrp_spawns");
	// Ensure directory exists - platform-specific minimal implementation
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), NULL);
#else
	mkdir(dir.c_str(), 0755);
#endif

	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	// Append the entry to the map file. For simplicity, store as a JSON array where
	// we append entries separated by newlines; tooling reading this can accept an
	// array-like newline-separated file or simplistic parser used here later.
	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"model\": \"%s\", \"origin\": [%.1f, %.1f, %.1f], \"angles\": [%.1f, %.1f, %.1f] }\n",
		entnum, modelPath,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
		e->s.apos.trBase[0], e->s.apos.trBase[1], e->s.apos.trBase[2]);
	fputs(buf, f);
	fclose(f);
}

// Read the per-map JSON file and send entries to the player as a simple indexed list.
void NXRP_ListSavedSpawns(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_spawns" : std::string("nxrp_spawns");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
		return;
	}

	std::string line;
	int idx = 0;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		// Print with an explicit index prefix so admins can reference entries by number.
		NXRP_PrintConsoleToPlayer(cl, va("%d: %s", idx, line.c_str()));
		idx++;
	}
	ifs.close();
	if (idx == 0) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
}

// Place a model with explicit prop bounds and yaw at a floor position.
int NXRP_PlaceModel(const char* name, const vec3_t propMins, const vec3_t propMaxs, float yaw, const vec3_t floor)
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

	vec3_t org = { floor[0], floor[1], floor[2] - propMins[2] };
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

	// Use the rotated prop bounds as-is (match Holo_PlaceModel behavior):
	// copy mins/maxs already set above and enable solid contents, then link.
	VectorAdd(e->s.pos.trBase, e->r.mins, e->r.absmin);
	VectorAdd(e->s.pos.trBase, e->r.maxs, e->r.absmax);

	e->r.contents = CONTENTS_SOLID;
	e->r.svFlags = 0;
	SV_LinkEntity(e);
	return e->s.number;
}

// Spawn a prop at the client's origin using given model and prop bounds
int NXRP_SpawnPropAtClient(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw)
{
	if (!cl || !modelPath) return -1;
	vec3_t floor;
	if (cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);
	else { VectorSet(floor, 0,0,0); }
	return NXRP_PlaceModel(modelPath, propMins, propMaxs, yaw, floor);
}

// Simple tracking for last NXRP spawn
static int nxrp_last_spawn = -1;

void NXRP_RegisterSpawn(int entnum) {
	nxrp_last_spawn = entnum;
}

int NXRP_GetLastSpawn() {
	return nxrp_last_spawn;
}

// Move the last spawn in cardinal directions or up/down by 'amount' units
// Legacy directional mover removed; use NXRP_MoveLastSpawnByVec instead.

qboolean NXRP_MoveLastSpawnByVec(client_t* cl, float dx, float dy, float dz) {
	if (nxrp_last_spawn < 0) return qfalse;
	if (nxrp_last_spawn < 0 || nxrp_last_spawn >= sv.num_entities) return qfalse;
	sharedEntity_t* e = SV_GentityNum(nxrp_last_spawn);
	if (!e || !e->r.linked) return qfalse;

	vec3_t org;
	VectorCopy(e->s.pos.trBase, org);

	org[0] += dx;
	org[1] += dy;
	org[2] += dz;

	// snap to whole units
	org[0] = ceilf(org[0]); org[1] = ceilf(org[1]); org[2] = ceilf(org[2]);

	VectorCopy(org, e->s.pos.trBase);
	VectorCopy(org, e->s.origin);
	VectorCopy(org, e->r.currentOrigin);
	e->s.pos.trType = TR_STATIONARY;
	SV_LinkEntity(e);

	NXRP_PrintConsoleToPlayer(cl, va("Moved model %d by (%.1f %.1f %.1f)", nxrp_last_spawn, dx, dy, dz));
	return qtrue;
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
