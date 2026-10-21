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
		// Expected format: { "ent": <n>, "index": <n>, "model": "<path>", "origin": [x, y, z], "angles": [ax, ay, az] }
		std::string s = line;
		if (s.size() > maxLen) s = s.substr(0, maxLen - 3) + "...";

		int entv = NXRP_ExtractIntField(s, "\"ent\"", -1);
		int idxv = NXRP_ExtractIntField(s, "\"index\"", idx);

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

		// extract origin and angles
		float ox=0, oy=0, oz=0, ax=0, ay=0, az=0;
		size_t opos = s.find("\"origin\"");
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
		NXRP_PrintConsoleToPlayerFmt(cl, "%d: ent=%d model=%s origin=%.0f,%.0f,%.0f angles=%.0f,%.0f,%.0f", idx, entv, model.c_str(), ox, oy, oz, ax, ay, az);
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
	const char* originKey = "\"origin\"";
	const char* m = strstr(s, modelKey);
	if (m) { m = strchr(m, ':'); if (m) m++; while (m && *m && isspace((unsigned char)*m)) m++; if (m && *m == '"') { m++; const char* me = strchr(m, '"'); if (me && me > m) modelPath.assign(m, me - m); } }
	const char* o = strstr(s, originKey);
	if (o) { o = strchr(o, '['); if (o) { o++; sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz); } }

	// Free any runtime entity referenced in this saved entry to avoid duplicates
	int entToFree = -1;
	size_t entPos = target.find("\"ent\"");
	if (entPos != std::string::npos) {
		size_t colon = target.find(':', entPos);
		if (colon != std::string::npos) {
			size_t valStart = colon + 1;
			while (valStart < target.size() && isspace((unsigned char)target[valStart])) ++valStart;
			size_t valEnd = valStart;
			if (valEnd < target.size() && (target[valEnd] == '-' || isdigit((unsigned char)target[valEnd]))) ++valEnd;
			while (valEnd < target.size() && isdigit((unsigned char)target[valEnd])) ++valEnd;
			if (valEnd > valStart) {
				try { entToFree = std::stoi(target.substr(valStart, valEnd - valStart)); } catch(...) { entToFree = -1; }
			}
		}
	}
	if (entToFree >= 0 && entToFree < sv.num_entities) {
		sharedEntity_t* oldEnt = SV_GentityNum(entToFree);
		if (oldEnt) {
			NXRP_FreeEntity((void*)oldEnt);
			NXRP_RegisterSpawn(-1);
			NXRP_PrintConsoleToPlayer(cl, va("Freed old saved runtime entity %d", entToFree));
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

	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = { 24.0f, 24.0f, 24.0f };
	float yaw = 0.0f;
	vec3_t zeroOffset2 = {0.0f, 0.0f, 0.0f};
	int newEnt = NXRP_PlaceModel(modelPath.c_str(), mins, maxs, yaw, zeroOffset2, newFloor);
	if (newEnt >= 0) {
		NXRP_RegisterSpawn(newEnt);
		// update saved entry with new origin and preserve collider metadata when present
		char buf[1024];
		snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"id\": %d, \"index\": %d, \"model\": \"%s\", \"origin\": [%.0f, %.0f, %.0f], \"angles\": [%.0f, %.0f, %.0f], \"propMins\": [%.2f, %.2f, %.2f], \"propMaxs\": [%.2f, %.2f, %.2f], \"colliderOffset\": [%.2f, %.2f, %.2f], \"yaw\": %.2f }",
			newEnt, newEnt, index, modelPath.c_str(), newFloor[0], newFloor[1], newFloor[2], 0.0f, yaw, 0.0f,
			mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], 0.0f, 0.0f, 0.0f, yaw);
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

// Move the last spawned prop to absolute coordinates x,y,z (chatCursor contains three floats)
bool NXRP_MoveLastSpawnTo(client_t* cl, const char* chatCursor) {
	if (!chatCursor) return false;
	float x=0,y=0,z=0;
	int cnt = sscanf(chatCursor, "%f %f %f", &x, &y, &z);
	if (cnt != 3) return false;

	int last = NXRP_GetLastSpawn();
	if (last < 0 || last >= sv.num_entities) { NXRP_PrintConsoleToPlayer(cl, "No last spawned model to move"); return false; }

	sharedEntity_t* e = SV_GentityNum(last);
	if (!e) { NXRP_PrintConsoleToPlayer(cl, "Last spawned entity not found"); return false; }

	std::string filename;
	std::vector<std::string> lines;
	NXRP_GetPerMapFilePath("nxrp_props", filename);
	NXRP_ReadNonEmptyLinesFromFile(filename, lines);

	bool removed = false;
	std::string removedLine;
	if (!lines.empty()) { removedLine = lines.back(); lines.pop_back(); removed = true; FILE* f = fopen(filename.c_str(), "w"); if (f) { for (const auto &ll : lines) { fputs(ll.c_str(), f); fputc('\n', f); } fclose(f); } }

	// free old entity
	void* dll = GVM_GetDllHandle();
	if (!dll) { NXRP_PrintConsoleToPlayer(cl, "Game VM not available"); return false; }
	void (*G_FreeEntity_f)(void*) = (void (*)(void*))Sys_LoadFunction(dll, "G_FreeEntity");
	if (!G_FreeEntity_f) { 
		NXRP_PrintConsoleToPlayer(cl, "G_FreeEntity not available; aborting move");
		return false;
	}
	{
		void* old = GVM_BeginNative();
		G_FreeEntity_f(e);
		GVM_EndNative(old);
	}
	// mark that last spawn is gone
	NXRP_RegisterSpawn(-1);
	NXRP_PrintConsoleToPlayer(cl, "Old entity freed");

	// spawn new at x y z using the model path parsed from the removed line
	if (!removed || removedLine.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Could not find saved model entry for the last spawn; aborting move.");
		return false;
	}

	// parse model path and angles from removedLine
	std::string modelPathStr;
	float savedAx=0.0f, savedAy=0.0f, savedAz=0.0f;
	const char* s = removedLine.c_str();
	const char* modelKey = "\"model\"";
	const char* anglesKey = "\"angles\"";
	const char* m = strstr(s, modelKey);
	if (m) {
		m = strchr(m, ':'); if (m) m++;
		while (m && *m && isspace((unsigned char)*m)) m++;
		if (m && *m == '"') {
			m++;
			// find closing quote - handle simple escaped quotes by searching for '"' directly
			const char* me = strchr(m, '"');
			if (me && me > m) modelPathStr.assign(m, me - m);
		}
	}
	const char* a = strstr(s, anglesKey);
	if (a) {
		a = strchr(a, '[');
		if (a) {
			a++;
			// sscanf may encounter commas or spaces; accept both
			sscanf(a, "%f %*[, ] %f %*[, ] %f", &savedAx, &savedAy, &savedAz);
		}
	}

	// Basic validation and debug output
	if (modelPathStr.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Saved entry missing model path; aborting move.");
		return false;
	}
	// Trim whitespace from modelPathStr
	{
		size_t a = modelPathStr.find_first_not_of(" \t\r\n");
		size_t b = modelPathStr.find_last_not_of(" \t\r\n");
		if (a == std::string::npos) modelPathStr.clear(); else modelPathStr = modelPathStr.substr(a, b - a + 1);
	}
	// Debug: show parsed values and the raw removed line
	NXRP_PrintConsoleToPlayer(cl, va("Moving model '%s' to (%.0f %.0f %.0f) with saved yaw %.1f", modelPathStr.c_str(), x, y, z, savedAy));
	NXRP_PrintConsoleToPlayer(cl, va("Saved line: %s", removedLine.c_str()));

	// Quick check: query the game VM for the model index to ensure the model exists
	void* dll_check = GVM_GetDllHandle();
	if (dll_check) {
		int (*gModelIndex_f)(const char*) = (int (*)(const char*))Sys_LoadFunction(dll_check, "G_ModelIndex");
		if (gModelIndex_f) {
			void* old = GVM_BeginNative();
			int idx = gModelIndex_f(modelPathStr.c_str());
			GVM_EndNative(old);
			NXRP_PrintConsoleToPlayer(cl, va("G_ModelIndex('%s') = %d", modelPathStr.c_str(), idx));
		}
	}

	if (modelPathStr.empty()) {
		NXRP_PrintConsoleToPlayer(cl, "Saved entry missing model path; aborting move.");
		return false;
	}

	// Parse saved origin from the removed line and add the provided offsets (x,y,z)
	float savedOx = 0.0f, savedOy = 0.0f, savedOz = 0.0f;
	const char* originKey = "\"origin\"";
	const char* o = strstr(removedLine.c_str(), originKey);
	if (o) {
		o = strchr(o, '[');
		if (o) {
			o++;
			sscanf(o, "%f %*[, ] %f %*[, ] %f", &savedOx, &savedOy, &savedOz);
		}
	}

	// Compute the new floor as saved origin + provided offsets
	vec3_t newFloor = { savedOx + x, savedOy + y, savedOz + z };
	// snap to integers like spawn/save behavior
	newFloor[0] = roundf(newFloor[0]); newFloor[1] = roundf(newFloor[1]); newFloor[2] = roundf(newFloor[2]);

	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = {  24.0f,  24.0f, 24.0f };
	float yaw = savedAy; // preserve saved yaw

	vec3_t zeroOffset2 = {0.0f, 0.0f, 0.0f};
	int newEnt = NXRP_PlaceModel(modelPathStr.c_str(), mins, maxs, yaw, zeroOffset2, newFloor);
	if (newEnt >= 0) {
		NXRP_RegisterSpawn(newEnt);
		// Save spawn with collider info so future loads reproduce the same collider.
		NXRP_SaveSpawnToMap(newEnt, modelPathStr.c_str(), mins, maxs, zeroOffset2, yaw);
		NXRP_PrintConsoleToPlayer(cl, va("Moved last spawn to new entity %d", newEnt));
		return true;
	}
	NXRP_PrintConsoleToPlayer(cl, "Failed to place model at requested coordinates");
	return false;
}

// Admin-prop spawn handler moved from nxrp_admincmds: spawn by path or catalog name
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

	std::string modelFromCatalog;
	vec3_t mins = { -24.0f, -24.0f, -8.0f };
	vec3_t maxs = { 24.0f, 24.0f, 24.0f };
	vec3_t colliderOrigin = {0.0f,0.0f,0.0f};
	vec3_t floor = {0.0f,0.0f,0.0f};
	float catalogYaw = angle;

	bool hasCatalog = NXRP_GetPropFromCatalog(path, modelFromCatalog, mins, maxs, colliderOrigin, catalogYaw);
	const float eps = 0.001f;
	if (hasCatalog) {
		Q_strncpyz(path, modelFromCatalog.c_str(), sizeof(path));
		NXRP_PrintConsoleToPlayer(cl, "found prop in list, using custom collider box");
		if (scanned < 2) { if (fabsf(catalogYaw) >= eps) angle = catalogYaw; }
		if (cl && cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);
	} else {
		if (cl && cl->gentity) VectorCopy(cl->gentity->r.currentOrigin, floor);
		NXRP_PrintConsoleToPlayer(cl, va("No catalog entry found for %s", path));
	}

	int ent = NXRP_PlaceModel(path, mins, maxs, angle, colliderOrigin, floor);
	if (ent >= 0) {
		NXRP_PrintConsoleToPlayer(cl, va("Spawned model %s as entity %d", path, ent));
		NXRP_RegisterSpawn(ent);
	// Save spawn including collider extents and origin offset so loading will
	// recreate the same collider as when spawned via !prop s
	NXRP_SaveSpawnToMap(ent, path, mins, maxs, colliderOrigin, angle);
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

	// Store collider extents and collider offset (origin) along with saved origin
	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"id\": %d, \"index\": %d, \"model\": \"%s\", \"origin\": [%.0f, %.0f, %.0f], \"angles\": [%.0f, %.0f, %.0f], \"propMins\": [%.2f, %.2f, %.2f], \"propMaxs\": [%.2f, %.2f, %.2f], \"colliderOffset\": [%.2f, %.2f, %.2f], \"yaw\": %.2f }\n",
		entnum, entnum, nextIndex, modelPath,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
		e->s.apos.trBase[0], e->s.apos.trBase[1], e->s.apos.trBase[2],
		propMins[0], propMins[1], propMins[2],
		propMaxs[0], propMaxs[1], propMaxs[2],
		colliderOffset[0], colliderOffset[1], colliderOffset[2],
		yaw);
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
	// default extents used when catalog entry not available
	vec3_t defaultMins = { -24.0f, -24.0f, -8.0f };
	vec3_t defaultMaxs = { 24.0f, 24.0f, 24.0f };

	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		const char* s = line.c_str();
		std::string modelPath;
		float ox=0, oy=0, oz=0;
		float ax=0, ay=0, az=0;
		const char* modelKey = "\"model\"";
		const char* originKey = "\"origin\"";
		const char* anglesKey = "\"angles\"";
		const char* m = strstr(s, modelKey);
		if (m) { m = strchr(m, ':'); if (m) m++; while (m && *m && isspace((unsigned char)*m)) m++; if (m && *m == '"') { m++; const char* me = strchr(m, '"'); if (me && me > m) modelPath.assign(m, me - m); } }
		const char* o = strstr(s, originKey);
		if (o) { o = strchr(o, '['); if (o) { o++; sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz); } }
		const char* a = strstr(s, anglesKey);
		if (a) { a = strchr(a, '['); if (a) { a++; sscanf(a, "%f %*[, ] %f %*[, ] %f", &ax, &ay, &az); } }

		// Parse optional saved collider metadata (propMins, propMaxs, colliderOffset, yaw)
		vec3_t savedPropMins = {0,0,0}, savedPropMaxs = {0,0,0}, savedColliderOffset = {0,0,0};
		float savedYaw = ay;
		const char* pmKey = "\"propMins\"";
		const char* pM = strstr(s, pmKey);
		if (pM) { pM = strchr(pM, '['); if (pM) { pM++; sscanf(pM, "%f %*[, ] %f %*[, ] %f", &savedPropMins[0], &savedPropMins[1], &savedPropMins[2]); } }
		const char* pM2Key = "\"propMaxs\"";
		const char* pM2 = strstr(s, pM2Key);
		if (pM2) { pM2 = strchr(pM2, '['); if (pM2) { pM2++; sscanf(pM2, "%f %*[, ] %f %*[, ] %f", &savedPropMaxs[0], &savedPropMaxs[1], &savedPropMaxs[2]); } }
		const char* coffKey = "\"colliderOffset\"";
		const char* cOff = strstr(s, coffKey);
		if (cOff) { cOff = strchr(cOff, '['); if (cOff) { cOff++; sscanf(cOff, "%f %*[, ] %f %*[, ] %f", &savedColliderOffset[0], &savedColliderOffset[1], &savedColliderOffset[2]); } }
		const char* yawKey = "\"yaw\"";
		const char* yk = strstr(s, yawKey);
		if (yk) { yk = strchr(yk, ':'); if (yk) { float ytmp; if (sscanf(yk, ":%f", &ytmp) == 1) savedYaw = ytmp; } }

		if (modelPath.empty()) continue;

		// attempt to find prop extents in catalog or use saved collider metadata
		vec3_t mins, maxs, origin; float yaw = ay; // use yaw from angles' second component if present
		bool hasCatalog = NXRP_GetPropFromCatalog(modelPath.c_str(), modelPath, mins, maxs, origin, yaw);
		bool hasSavedCollider = (savedPropMins[0] != 0.0f || savedPropMins[1] != 0.0f || savedPropMins[2] != 0.0f
			|| savedPropMaxs[0] != 0.0f || savedPropMaxs[1] != 0.0f || savedPropMaxs[2] != 0.0f
			|| savedColliderOffset[0] != 0.0f || savedColliderOffset[1] != 0.0f || savedColliderOffset[2] != 0.0f);

		// If saved spawn contains explicit collider metadata, prefer that over catalog
		if (hasSavedCollider) {
			VectorCopy(savedPropMins, mins);
			VectorCopy(savedPropMaxs, maxs);
			VectorCopy(savedColliderOffset, origin);
			yaw = savedYaw;
		} else {
			// Preserve saved origin/yaw when catalog does not provide them.
			const float eps = 0.001f;
			if (hasCatalog) {
				// If catalog returned an all-zero origin, prefer the saved origin from file
				// If catalog returned an all-zero origin, do NOT substitute the saved
				// world origin as a colliderOffset. The colliderOffset should remain
				// model-local. Use zero offset instead when catalog provides no local
				// origin.
				if (fabs(origin[0]) < eps && fabs(origin[1]) < eps && fabs(origin[2]) < eps) {
					origin[0] = 0.0f; origin[1] = 0.0f; origin[2] = 0.0f;
				}
				// If catalog left yaw at zero but our saved yaw was non-zero, prefer saved yaw
				if (fabs(yaw) < eps && fabs(savedYaw) >= eps) {
					yaw = savedYaw;
				}
			} else {
				const float eps = 0.001f;
				if (!hasCatalog) {
					VectorCopy(defaultMins, mins);
					VectorCopy(defaultMaxs, maxs);
					// No catalog: do not use saved world origin as colliderOffset. Keep
					// colliderOffset model-local (zero) and preserve saved origin only for
					// entity placement below.
					origin[0] = 0.0f; origin[1] = 0.0f; origin[2] = 0.0f;
				}
			}
		}

		// For saved-spawn loading, origin previously came from catalog or saved file
		// Treat 'origin' here as the colliderOffset (position of collider relative to entity origin)
		// and place the entity so its stored origin (ox,oy,oz) is preserved.
		// NXRP_PlaceModel expects a 'floor' reference where it computes
		// entityOrigin.z = floor.z - propMins[2]. To restore a previously saved
		// entity origin (ox,oy,oz) we must pass floor.z = ox.z + propMins[2].
		vec3_t savedFloor = { ox, oy, oz + mins[2] };
		// Debug: report computed placement parameters when a client requested load
		if (cl) {
			NXRP_PrintConsoleToPlayer(cl, va("NXRP: loading prop %s; savedOrigin=[%.2f %.2f %.2f] propMins=[%.2f %.2f %.2f] propMaxs=[%.2f %.2f %.2f] colliderOffset=[%.2f %.2f %.2f] yaw=%.2f savedFloor=[%.2f %.2f %.2f]",
				modelPath.c_str(), ox, oy, oz,
				mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
				origin[0], origin[1], origin[2], yaw,
				savedFloor[0], savedFloor[1], savedFloor[2]));
		}
		int ent = NXRP_PlaceModel(modelPath.c_str(), mins, maxs, yaw, origin, savedFloor);
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
	if (NXRP_ReadFileToString(jsonPath.c_str(), content)) {
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
	if (!ifs.is_open()) return false;

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
			ifs.close();
			return true;
		}
	}
	ifs.close();
	return false;
}

