// Restored prop persistence and catalog implementations moved into nxrp_utils.
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <sys/stat.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <cstring>

int NXRP_PlaceModel(const char* modelPath, const vec3_t position, float angle)
{
	if (!modelPath || !modelPath[0]) return -1;

	std::string resolvedModel = modelPath;
	vec3_t propMins = { -24.0f, -24.0f, -8.0f };
	vec3_t propMaxs = { 24.0f, 24.0f, 24.0f };
	vec3_t colliderOrigin = { 0.0f, 0.0f, 0.0f };
	float catalogYaw = 0.0f;
	bool foundInCatalog = NXRP_GetPropFromCatalog(modelPath, resolvedModel, propMins, propMaxs, colliderOrigin, catalogYaw);
	if (foundInCatalog && !resolvedModel.empty()) {
		modelPath = resolvedModel.c_str();
	}

	if (propMins[0] == 0.0f && propMins[1] == 0.0f && propMins[2] == 0.0f
		&& propMaxs[0] == 0.0f && propMaxs[1] == 0.0f && propMaxs[2] == 0.0f) {
		VectorSet(propMins, -24.0f, -24.0f, -8.0f);
		VectorSet(propMaxs, 24.0f, 24.0f, 24.0f);
		NXRP_PrintConsoleToPlayer(NULL, "NXRP: collider extents were zero; applied default collider");
	}

	for (int i = 0; i < 3; ++i) {
		if (propMins[i] > propMaxs[i]) {
			const float t = propMins[i];
			propMins[i] = propMaxs[i];
			propMaxs[i] = t;
		}
	}

	void* dll = GVM_GetDllHandle();
	if (!dll) return -1;

	void* gGSpawn_f = Sys_LoadFunction(dll, "G_Spawn");
	int (*gModelIndex_f)(const char*) = (int (*)(const char*))Sys_LoadFunction(dll, "G_ModelIndex");
	void* (*gGSpawn)() = (void* (*)())gGSpawn_f;
	if (!gGSpawn || !gModelIndex_f) return -1;

	void* old = GVM_BeginNative();
	sharedEntity_t* e = (sharedEntity_t*)gGSpawn();
	const int model = e ? gModelIndex_f(modelPath) : 0;
	GVM_EndNative(old);
	if (!e || model <= 0) {
		NXRP_PrintConsoleToPlayer(NULL, va("NXRP: failed to resolve model index for %s", modelPath));
		return -1;
	}

	vec3_t org = { position[0], position[1], position[2] - propMins[2] };
	org[0] = roundf(org[0]); org[1] = roundf(org[1]); org[2] = roundf(org[2]);
	e->s.eType = ET_GENERAL;
	e->s.modelindex = model;
	VectorCopy(org, e->s.pos.trBase);
	VectorCopy(org, e->s.origin);
	VectorCopy(org, e->r.currentOrigin);
	e->s.pos.trType = TR_STATIONARY;
	VectorSet(e->s.apos.trBase, 0.0f, angle, 0.0f);
	VectorCopy(e->s.apos.trBase, e->s.angles);
	VectorCopy(e->s.apos.trBase, e->r.currentAngles);
	e->s.apos.trType = TR_STATIONARY;

	bool noCollider = (propMins[0] == 0.0f && propMins[1] == 0.0f && propMins[2] == 0.0f
		&& propMaxs[0] == 0.0f && propMaxs[1] == 0.0f && propMaxs[2] == 0.0f);

	if (noCollider) {
		NXRP_PrintConsoleToPlayer(NULL, va("NXRP: no collider for %s", modelPath));
		VectorSet(e->r.mins, 0.0f, 0.0f, 0.0f);
		VectorSet(e->r.maxs, 0.0f, 0.0f, 0.0f);
		VectorCopy(e->s.pos.trBase, e->r.absmin);
		VectorCopy(e->s.pos.trBase, e->r.absmax);
		e->r.svFlags = 0;
		SV_LinkEntity(e);
	} else {
		const float c = cosf(DEG2RAD(angle)), sn = sinf(DEG2RAD(angle));
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
		NXRP_PrintConsoleToPlayer(NULL, va("NXRP: applied collider mins=[%.2f %.2f %.2f] maxs=[%.2f %.2f %.2f] at origin=[%.2f %.2f %.2f]",
			e->r.mins[0], e->r.mins[1], e->r.mins[2],
			e->r.maxs[0], e->r.maxs[1], e->r.maxs[2],
			e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2]));
		VectorAdd(e->s.pos.trBase, e->r.mins, e->r.absmin);
		VectorAdd(e->s.pos.trBase, e->r.maxs, e->r.absmax);

		e->r.contents = CONTENTS_SOLID;
		e->r.svFlags = 0;
		SV_LinkEntity(e);
	}
	NXRP_PrintConsoleToPlayer(NULL, va("Placed model %s", modelPath));
	return e->s.number;
}

// Read the per-map JSON file and send entries to the player as a simple indexed list.
void NXRP_ListSavedSpawns(client_t* cl) {
	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_props", lines, filename)) { NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map"); return; }

	// Print a simple indexed list of saved spawn entries for this map.
	int idx = 0;
	const size_t maxLen = 900; // keep within safe console buffer
	bool any = false;
	for (const auto &line : lines) {
		if (line.empty()) continue;
		any = true;
		// Parse the JSON-ish line and print a concise one-line summary per saved spawn.
		// Expected format: { "id": <n>, "index": <n>, "model": "<path>", "position": [x, y, z], "angles": [ax, ay, az] }
		std::string s = line;
		if (s.size() > maxLen) s = s.substr(0, maxLen - 3) + "...";

		int idv = NXRP_ExtractIntField(s, "\"id\"", -1);

		// extract model path
		std::string model;
		size_t mpos = s.find("\"model\"");
		if (mpos != std::string::npos) {
			size_t colon = s.find(':', mpos);
			if (colon != std::string::npos) {
				size_t q = s.find('"', colon);
				if (q != std::string::npos) {
					size_t qe = s.find('"', q+1);
					if (qe != std::string::npos && qe > q) model = s.substr(q+1, qe-q-1);
				}
			}
		}

		// extract position and angles
		float ox=0, oy=0, oz=0, ax=0, ay=0, az=0;
		size_t opos = s.find("\"position\"");
		if (opos == std::string::npos) opos = s.find("\"origin\"");
		if (opos != std::string::npos) {
			opos = s.find('[', opos);
			if (opos != std::string::npos) {
				sscanf(s.c_str()+opos, "[%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz);
			}
		}
		size_t apos = s.find("\"angles\"");
		if (apos != std::string::npos) {
			apos = s.find('[', apos);
			if (apos != std::string::npos) {
				sscanf(s.c_str()+apos, "[%f %*[, ] %f %*[, ] %f", &ax, &ay, &az);
			}
		}

		if (model.empty()) model = "<unknown>";
		NXRP_PrintConsoleToPlayerFmt(cl, "%d: id=%d model=%s position=%.0f,%.0f,%.0f angles=%.0f,%.0f,%.0f", idx, idv, model.c_str(), ox, oy, oz, ax, ay, az);
		idx++;
	}
	if (!any) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
	if (idx == 0) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
	return;
}

// Helper implementations moved from admincmds: place helper functions here
// so admin command wrappers remain thin and delegate behavior to utils.

// Move a saved spawn (by index) by a cardinal direction and amount.
// args: "<dir> <amount>"
bool NXRP_MoveSavedSpawnByIndex(client_t* cl, int index, const char* args) {
	if (!args) return false;
	char dirTok[32] = {0};
	float amount = 0.0f;
	int scanned = sscanf(args, "%31s %f", dirTok, &amount);
	if (scanned < 2) return false;

	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_props", lines, filename)) { NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map"); return false; }

	if (index < 0 || index >= (int)lines.size()) { NXRP_PrintConsoleToPlayer(cl, "Invalid saved spawn index"); return false; }

	std::string target = lines[index];
	const char* s = target.c_str();
	std::string modelPath;
	float ox=0, oy=0, oz=0;
	const char* modelKey = "\"model\"";
	const char* originKey = "\"position\"";
	const char* m = strstr(s, modelKey);
	if (m) { m = strchr(m, ':'); if (m) m++; while (m && *m && isspace((unsigned char)*m)) m++; if (m && *m == '"') { m++; const char* me = strchr(m, '"'); if (me && me > m) modelPath.assign(m, me - m); } }
	const char* o = strstr(s, originKey);
	if (o) { o = strchr(o, '['); if (o) { o++; sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz); } }

	// Free any runtime entity referenced in this saved entry to avoid duplicates
	int idToFree = NXRP_ExtractIntField(target, "\"id\"", -1);
	if (idToFree >= 0 && idToFree < sv.num_entities) {
		sharedEntity_t* oldEnt = SV_GentityNum(idToFree);
		if (oldEnt) {
			NXRP_FreeEntity((void*)oldEnt);
			NXRP_RegisterSpawn(-1);
			NXRP_PrintConsoleToPlayer(cl, va("Freed old saved runtime entity %d", idToFree));
		}
	}

	if (modelPath.empty()) { NXRP_PrintConsoleToPlayer(cl, "Saved entry missing model path"); return false; }

	// compute delta vector based on direction
	float dx=0, dy=0, dz=0;
	if (!Q_stricmp(dirTok, "up")) dz = amount;
	else if (!Q_stricmp(dirTok, "down")) dz = -amount;
	else if (!Q_stricmp(dirTok, "left")) dx = -amount;
	else if (!Q_stricmp(dirTok, "right")) dx = amount;
	else if (!Q_stricmp(dirTok, "forward")) dy = amount;
	else if (!Q_stricmp(dirTok, "back")) dy = -amount;
	else { NXRP_PrintConsoleToPlayer(cl, "Unknown direction; use up/down/left/right/forward/back"); return false; }

	vec3_t newFloor = { ox + dx, oy + dy, oz + dz };
	newFloor[0] = roundf(newFloor[0]); newFloor[1] = roundf(newFloor[1]); newFloor[2] = roundf(newFloor[2]);

	float yaw = 0.0f;
	int newEnt = NXRP_PlaceModel(modelPath.c_str(), newFloor, yaw);
	if (newEnt >= 0) {
		NXRP_RegisterSpawn(newEnt);
		// update saved entry
		char buf[1024];
		snprintf(buf, sizeof(buf), "{ \"id\": %d, \"index\": %d, \"model\": \"%s\", \"position\": [%.0f, %.0f, %.0f], \"angles\": [%.0f, %.0f, %.0f] }",
			newEnt, index, modelPath.c_str(), newFloor[0], newFloor[1], newFloor[2], 0.0f, yaw, 0.0f);
		lines[index] = std::string(buf);
		FILE* f = fopen(filename.c_str(), "w");
		if (f) {
			for (const auto &ll : lines) { fputs(ll.c_str(), f); fputc('\n', f); }
			fclose(f);
		}
		NXRP_PrintConsoleToPlayer(cl, va("Moved saved spawn %d by %s %.1f", index, dirTok, amount));
		return true;
	}
	NXRP_PrintConsoleToPlayer(cl, "Failed to respawn model at new location");
	return false;
}

bool NXRP_AdminSpawnProp(client_t* cl, const char* chatCursor) {
	if (!chatCursor) return false;

	// Skip leading spaces
	const char* p = chatCursor;
	while (*p == ' ' || *p == '\t') p++;

	// If the first token is "model" or "pmodel", skip it
	char firstTok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(p, "%31s", firstTok) == 1) {
		if (!Q_stricmp(firstTok, "model") || !Q_stricmp(firstTok, "pmodel")) {
			const char* q = p;
			while (*q && *q != ' ' && *q != '\t') q++;
			while (*q == ' ' || *q == '\t') q++;
			p = q;
		}
	}

	if (!p || !*p) return false;

	char path[512] = {0};
	float angle = 0.0f;
	int scanned = sscanf(p, "%511s %f", path, &angle);
	if (scanned < 1) return false;

	while (angle < 0.0f) angle += 360.0f;
	while (angle >= 360.0f) angle -= 360.0f;

	vec3_t floor = {0.0f,0.0f,0.0f};
	if (cl && cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);

	int ent = NXRP_PlaceModel(path, floor, angle);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", path, ent));
		NXRP_RegisterSpawn(ent);
		vec3_t zeroMins = {0.0f, 0.0f, 0.0f};
		vec3_t zeroMaxs = {0.0f, 0.0f, 0.0f};
		vec3_t zeroOffset = {0.0f, 0.0f, 0.0f};
		NXRP_SaveSpawnToMap(ent, path, zeroMins, zeroMaxs, zeroOffset, angle);
		return true;
	}
	NXRP_PrintConsoleToPlayer(cl, "Failed to spawn model.");
	return false;
}

// Alias: spawn prop at client using explicit extents. Keep same behavior as NXRP_SpawnPropAtClient.
int NXRP_SpawnPropAtClientWithExtents(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw) {
	return NXRP_SpawnPropAtClient(cl, modelPath, propMins, propMaxs, yaw);
}

// Remove a saved spawn entry by its zero-based index. This rewrites the per-map file.
bool NXRP_RemoveSavedSpawnByIndex(int index) {
	std::string filename;
	std::vector<std::string> lines;
	if (!NXRP_ReadPerMapLines("nxrp_props", lines, filename)) return false;

	if (index < 0 || index >= (int)lines.size()) return false;

	// attempt to free the entity for the removed entry if available.
	std::string targetLine = lines[index];
	int entToFree = -1;
	size_t entPos = targetLine.find("\"ent\"");
	if (entPos != std::string::npos) {
		size_t colon = targetLine.find(':', entPos);
		if (colon != std::string::npos) {
			size_t valStart = colon + 1;
			while (valStart < targetLine.size() && isspace((unsigned char)targetLine[valStart])) ++valStart;
			size_t valEnd = valStart;
			if (valEnd < targetLine.size() && (targetLine[valEnd] == '-' || isdigit((unsigned char)targetLine[valEnd]))) ++valEnd;
			while (valEnd < targetLine.size() && isdigit((unsigned char)targetLine[valEnd])) ++valEnd;
			if (valEnd > valStart) {
				try { entToFree = std::stoi(targetLine.substr(valStart, valEnd - valStart)); } catch (...) { entToFree = -1; }
			}
		}
	}

	if (entToFree >= 0) {
		if (entToFree >= 0 && entToFree < sv.num_entities) {
			sharedEntity_t* e = SV_GentityNum(entToFree);
			if (e) {
				void* dll = GVM_GetDllHandle();
				if (dll) {
					void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
					if (G_FreeEntity_f) {
						void* old = GVM_BeginNative();
						G_FreeEntity_f(e);
						GVM_EndNative(old);
						NXRP_RegisterSpawn(-1);
					}
				}
			}
		}
	}

	// Remove the selected entry and rewrite file
	lines.erase(lines.begin() + index);
	if (!NXRP_WriteLinesToFile(filename, lines)) return false;
	return true;
}

// Persist a spawn to a per-map JSON file in the fs_homepath/nxrp_props directory.
// Accept explicit collider extents, collider offset and yaw so loading can
// recreate the same collision box as when spawned via !prop s.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, const vec3_t colliderOffset, float yaw) {
	if (entnum < 0 || !modelPath) return;
	(void)propMins;
	(void)propMaxs;
	(void)colliderOffset;
	(void)yaw;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	char buf[1024];
	std::string filename;
	NXRP_GetPerMapFilePath("nxrp_props", filename);

	int nextIndex = 0;
	std::vector<std::string> lines;
	if (NXRP_ReadNonEmptyLinesFromFile(filename, lines)) nextIndex = (int)lines.size();

	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	snprintf(buf, sizeof(buf), "{ \"id\": %d, \"index\": %d, \"model\": \"%s\", \"position\": [%.0f, %.0f, %.0f], \"angles\": [%.0f, %.0f, %.0f] }\n",
		entnum, nextIndex, modelPath,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
		e->s.apos.trBase[0], e->s.apos.trBase[1], e->s.apos.trBase[2]);
	fputs(buf, f);
	fclose(f);
}

// Load saved spawns for the current map and attempt to spawn them into the world.
// Returns the number of spawns successfully created.
int NXRP_LoadSavedSpawnsForMap(client_t* cl) {
	// Ensure per-map directory exists (handled by NXRP_GetPerMapFilePath)
	std::string filename;
	NXRP_GetPerMapFilePath("nxrp_props", filename);

	std::ifstream ifs(filename);
	if (!ifs.is_open()) {
		return 0;
	}

	std::string line;
	int spawned = 0;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		const char* s = line.c_str();
		std::string modelPath;
		float ox=0, oy=0, oz=0;
		float ax=0, ay=0, az=0;
		const char* modelKey = "\"model\"";
		const char* originKey = "\"position\"";
		const char* anglesKey = "\"angles\"";
		const char* m = strstr(s, modelKey);
		if (m) { m = strchr(m, ':'); if (m) m++; while (m && *m && isspace((unsigned char)*m)) m++; if (m && *m == '"') { m++; const char* me = strchr(m, '"'); if (me && me > m) modelPath.assign(m, me - m); } }
		const char* o = strstr(s, originKey);
		if (o) { o = strchr(o, '['); if (o) { o++; sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz); } }
		const char* a = strstr(s, anglesKey);
		if (a) { a = strchr(a, '['); if (a) { a++; sscanf(a, "%f %*[, ] %f %*[, ] %f", &ax, &ay, &az); } }

		if (modelPath.empty()) continue;

		vec3_t savedPosition = { ox, oy, oz };
		float yaw = ay;
		if (cl) {
			NXRP_PrintConsoleToPlayer(cl, va("NXRP: loading prop %s; savedPosition=[%.2f %.2f %.2f] yaw=%.2f",
				modelPath.c_str(), ox, oy, oz,
				yaw));
		}
		int ent = NXRP_PlaceModel(modelPath.c_str(), savedPosition, yaw);
		if (ent >= 0) {
			NXRP_RegisterSpawn(ent);
			spawned++;
			if (cl) NXRP_PrintConsoleToPlayer(cl, va("NXRP: spawned entity %d for model %s", ent, modelPath.c_str()));
		} else {
			if (cl) NXRP_PrintConsoleToPlayer(cl, va("NXRP: failed to spawn model %s", modelPath.c_str()));
		}
	}
	ifs.close();
	return spawned;
}

// Lookup a prop entry by name in a simple catalog. If the name appears to be
// a direct model path (contains '/' or ends with common model extensions),
// return that as the model and provide default extents. Otherwise attempt to
// read a catalog file under nxrp_props/catalog.txt with simple CSV-like lines
// of the form: key|modelPath|minx,miny,minz|maxx,maxy,maxz|originx,originy,originz|yaw
// New: support JSON-based catalog file nxrp_props_catalog.json in the nxrp_props directory.
// If present, the JSON maps keys to objects with fields: model, extents (array of 3), origin (array of 3), yaw
bool NXRP_GetPropFromCatalog(const char* name, std::string& outModel, vec3_t& outMins, vec3_t& outMaxs, vec3_t& outOrigin, float& outYaw) {
	if (!name || !name[0]) return false;

	// Initialize outputs to safe defaults to avoid returning uninitialized data
	outMins[0] = outMins[1] = outMins[2] = 0.0f;
	outMaxs[0] = outMaxs[1] = outMaxs[2] = 0.0f;
	outOrigin[0] = outOrigin[1] = outOrigin[2] = 0.0f;
	outYaw = 0.0f;
	std::string sname = name;
	// If it looks like a path, attempt to match it against the JSON catalog's model fields first.
	bool looksLikePath = (sname.find('/') != std::string::npos || sname.find('\\') != std::string::npos || sname.find('.') != std::string::npos);

	// Read optional JSON catalog from the nxrp_props folder. Use NXRP helpers
	// to determine the per-map directory so we avoid duplicating fs_homepath logic.
	std::string perMapPath;
	NXRP_GetPerMapFilePath("nxrp_props", perMapPath);
	// Derive directory from the per-map path (strip trailing /<map>.json)
	std::string dir = perMapPath;
	{
		size_t p1 = dir.find_last_of('/');
		size_t p2 = dir.find_last_of('\\');
		size_t p = (p1 == std::string::npos) ? p2 : ((p2 == std::string::npos) ? p1 : std::max(p1, p2));
		if (p != std::string::npos) dir = dir.substr(0, p);
	}
	std::string jsonPath = dir + "/nxrp_props_catalog.json";
	std::string content;
	qboolean loadedCatalog = qfalse;
	if (NXRP_ReadFileToString(jsonPath.c_str(), content)) {
		loadedCatalog = qtrue;
		//NXRP_PrintConsoleToPlayer(NULL, va("NXRP: props catalog found: %s", jsonPath.c_str()));
	} else {
		// Fallback: allow catalog under fs_homepath/nxrp as well
		std::string nxrpDir;
		if (NXRP_GetFolderPath("nxrp", nxrpDir)) {
			std::string altJsonPath = nxrpDir + "/nxrp_props_catalog.json";
			if (NXRP_ReadFileToString(altJsonPath.c_str(), content)) {
				loadedCatalog = qtrue;
				//NXRP_PrintConsoleToPlayer(NULL, va("NXRP: props catalog found: %s", altJsonPath.c_str()));
			}
		}
	}
	if (!loadedCatalog) {
		NXRP_PrintConsoleToPlayer(NULL, "NXRP: props catalog not found");
	}
	if (loadedCatalog) {
		// crude JSON parsing: if name is a key, use that entry. If name looks like a path, search for an entry whose model matches the path.
		std::string key = '"' + sname + '"';
		size_t kpos = content.find(key);
		if (kpos == std::string::npos && looksLikePath) {
			// search for model: "<sname>"
			std::string modelKey = "\"model\"";
			size_t pos = 0;
			while (true) {
				size_t mpos = content.find(modelKey, pos);
				if (mpos == std::string::npos) break;
				size_t colon = content.find(':', mpos);
				if (colon == std::string::npos) break;
				size_t q = content.find('"', colon);
				if (q == std::string::npos) break;
				size_t qe = content.find('"', q+1);
				if (qe == std::string::npos) break;
				std::string modelVal = content.substr(q+1, qe-q-1);
				if (modelVal == sname) { kpos = content.rfind('{', mpos); break; }
				pos = qe + 1;
			}
		}
		if (kpos != std::string::npos) {
			size_t brace = content.find('{', kpos);
			if (brace != std::string::npos) {
				size_t end = content.find('}', brace);
				std::string obj = (end != std::string::npos) ? content.substr(brace, end - brace + 1) : content.substr(brace);
				// model
				size_t mpos = obj.find("\"model\"");
				if (mpos != std::string::npos) {
					size_t c = obj.find(':', mpos);
					size_t q = obj.find('"', c);
					if (q != std::string::npos) {
						size_t qe = obj.find('"', q+1);
						if (qe != std::string::npos) outModel = obj.substr(q+1, qe-q-1);
					}
				}
				// collidersize / extents -> use as half-extents (prefer collidersize)
				size_t csizePos = obj.find("\"collidersize\"");
				if (csizePos != std::string::npos) {
					size_t lb = obj.find('[', csizePos);
					if (lb != std::string::npos) {
						float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outMins[0] = -a; outMins[1] = -b; outMins[2] = -c; outMaxs[0] = a; outMaxs[1] = b; outMaxs[2] = c; }
					}
				} else {
					size_t epos = obj.find("\"extents\"");
					if (epos != std::string::npos) {
						size_t lb = obj.find('[', epos);
						if (lb != std::string::npos) {
							float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outMins[0] = -a; outMins[1] = -b; outMins[2] = -c; outMaxs[0] = a; outMaxs[1] = b; outMaxs[2] = c; }
						}
					}
				}

				// collideroffset / origin
				size_t coffsetPos = obj.find("\"collideroffset\"");
				if (coffsetPos != std::string::npos) {
					size_t lb = obj.find('[', coffsetPos);
					if (lb != std::string::npos) { float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outOrigin[0]=a; outOrigin[1]=b; outOrigin[2]=c; } }
				} else {
					size_t opos = obj.find("\"origin\"");
					if (opos != std::string::npos) {
						size_t lb = obj.find('[', opos);
						if (lb != std::string::npos) { float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outOrigin[0]=a; outOrigin[1]=b; outOrigin[2]=c; } }
					}
				}

				// colliderangle / yaw
				size_t canglePos = obj.find("\"colliderangle\"");
				if (canglePos != std::string::npos) {
					size_t c = obj.find(':', canglePos); if (c != std::string::npos) { float y; if (sscanf(obj.c_str()+c, ":%f", &y) == 1) outYaw = y; }
				} else {
					size_t ypos = obj.find("\"yaw\"");
					if (ypos != std::string::npos) { size_t c = obj.find(':', ypos); if (c != std::string::npos) { float y; if (sscanf(obj.c_str()+c, ":%f", &y) == 1) outYaw = y; } }
				}

				if (outModel.empty()) outModel = sname;
				NXRP_PrintConsoleToPlayer(NULL, va("NXRP: model found in catalog: %s", sname.c_str()));
				//NXRP_PrintConsoleToPlayer(NULL, va("NXRP: catalog collider size: [%.2f %.2f %.2f]", outMaxs[0] - outMins[0], outMaxs[1] - outMins[1], outMaxs[2] - outMins[2]));
				return true;
			}
		}
	}

	// Fallback: legacy catalog.txt / catalog.csv parsing
	std::string cat1 = dir + "/catalog.txt";
	std::string cat2 = dir + "/catalog.csv";
	std::ifstream ifs;
	ifs.open(cat1);
	if (!ifs.is_open()) ifs.open(cat2);
	if (!ifs.is_open()) {
		NXRP_PrintConsoleToPlayer(NULL, va("NXRP: model not found in catalog: %s", sname.c_str()));
		return false;
	}

	std::string line;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		// split by '|'
		std::vector<std::string> parts;
		std::istringstream iss(line);
		std::string tok;
		while (std::getline(iss, tok, '|')) parts.push_back(tok);
		if (parts.empty()) continue;
		// trim whitespace
		auto trim = [](std::string &str){ size_t a = str.find_first_not_of(" \t\r\n"); size_t b = str.find_last_not_of(" \t\r\n"); if (a==std::string::npos) { str.clear(); } else { str = str.substr(a, b-a+1); } };
		trim(parts[0]);
		if (!parts[0].empty() && parts[0] == sname) {
			if (parts.size() >= 2) { trim(parts[1]); outModel = parts[1]; }
			// parse mins
			if (parts.size() >= 3) {
				float a,b,c; if (sscanf(parts[2].c_str(), "%f,%f,%f", &a,&b,&c) == 3) { outMins[0]=a; outMins[1]=b; outMins[2]=c; }
			}
			if (parts.size() >= 4) {
				float a,b,c; if (sscanf(parts[3].c_str(), "%f,%f,%f", &a,&b,&c) == 3) { outMaxs[0]=a; outMaxs[1]=b; outMaxs[2]=c; }
			}
			if (parts.size() >= 5) {
				float a,b,c; if (sscanf(parts[4].c_str(), "%f,%f,%f", &a,&b,&c) == 3) { outOrigin[0]=a; outOrigin[1]=b; outOrigin[2]=c; }
			}
			if (parts.size() >= 6) {
				float y; if (sscanf(parts[5].c_str(), "%f", &y) == 1) outYaw = y;
			}
			if (outModel.empty()) outModel = sname;
			if (!outMins[0] && !outMins[1] && !outMins[2]) { outMins[0]=-24; outMins[1]=-24; outMins[2]=-8; }
			if (!outMaxs[0] && !outMaxs[1] && !outMaxs[2]) { outMaxs[0]=24; outMaxs[1]=24; outMaxs[2]=24; }
			if (!outYaw) outYaw = 0.0f;
			NXRP_PrintConsoleToPlayer(NULL, va("NXRP: model found in catalog: %s", sname.c_str()));
			//NXRP_PrintConsoleToPlayer(NULL, va("NXRP: catalog collider size: [%.2f %.2f %.2f]", outMaxs[0] - outMins[0], outMaxs[1] - outMins[1], outMaxs[2] - outMins[2]));
			ifs.close();
			return true;
		}
	}
	ifs.close();
	NXRP_PrintConsoleToPlayer(NULL, va("NXRP: model not found in catalog: %s", sname.c_str()));
	return false;
}

