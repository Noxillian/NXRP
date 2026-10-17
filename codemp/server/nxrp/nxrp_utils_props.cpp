#include "server/nxrp/nxrp_utils.h"
#include "server/nxrp/nxrp_main.h"
#include <fstream>
#include <cctype>
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <cstdio>
#include <string>

// Load saved entries for the current map and spawn them. Returns how many were spawned.
int NXRP_LoadSavedSpawnsForMap(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) return 0;

	std::string line;
	int spawned = 0;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		const char* s = line.c_str();
		const char* modelKey = "\"model\"";
		const char* originKey = "\"origin\"";
		const char* anglesKey = "\"angles\"";
		const char* m = strstr(s, modelKey);
		if (!m) continue;
		m = strchr(m, ':'); if (!m) continue; m++;
		while (*m && isspace((unsigned char)*m)) m++;
		if (*m != '"') { continue; }
		m++;
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

// Simple catalog lookup: read nxrp_props_catalog.json in module dir and find entry by name.
bool NXRP_GetPropFromCatalog(const char* name, std::string& outModel, vec3_t& outMins, vec3_t& outMaxs, vec3_t& outOrigin, float& outYaw) {
	if (!name || !name[0]) return false;
	std::string catalogPath = std::string("codemp/server/nxrp/nxrp_props_catalog.json");
	std::ifstream ifs(catalogPath);
	if (!ifs.is_open()) return false;

	// Very small JSON parse: look for "name": { ... }
	std::string line;
	std::string block;
	bool inBlock = false;
	std::string key = std::string("\"") + name + std::string("\"");
	while (std::getline(ifs, line)) {
		if (!inBlock) {
			if (line.find(key) != std::string::npos && line.find('{') != std::string::npos) {
				inBlock = true;
				block = line;
				if (line.find('}') != std::string::npos) break;
			}
		} else {
			block += line;
			if (line.find('}') != std::string::npos) break;
		}
	}
	ifs.close();
	if (!inBlock) return false;

	// Extract model
	const char* s = block.c_str();
	const char* modelKey = "\"model\"";
	const char* m = strstr(s, modelKey);
	if (m) { m = strchr(m, ':'); if (m) m++; while (m && *m && isspace((unsigned char)*m)) m++; if (m && *m == '"') { m++; const char* me = strchr(m, '"'); if (me && me > m) outModel.assign(m, me - m); } }

	// extents: [sx, sy, sz]
	const char* extKey = "\"extents\"";
	const char* e = strstr(s, extKey);
	if (e) { e = strchr(e, '['); if (e) { e++; float sx=0, sy=0, sz=0; sscanf(e, "%f %*[, ] %f %*[, ] %f", &sx, &sy, &sz); outMins[0] = -sx; outMins[1] = -sy; outMins[2] = -sz; outMaxs[0] = sx; outMaxs[1] = sy; outMaxs[2] = sz; } }

	// origin
	const char* oKey = "\"origin\"";
	const char* o = strstr(s, oKey);
	if (o) { o = strchr(o, '['); if (o) { o++; float ox=0, oy=0, oz=0; sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz); outOrigin[0] = ox; outOrigin[1] = oy; outOrigin[2] = oz; } }

	// yaw
	const char* yawKey = "\"yaw\"";
	const char* y = strstr(s, yawKey);
	outYaw = 0.0f;
	if (y) { y = strchr(y, ':'); if (y) { y++; float yy=0; sscanf(y, "%f", &yy); outYaw = yy; } }

	return true;
}

// Persist an NPC spawn to a per-map JSON file in the fs_homepath/nxrp_npcs directory.
void NXRP_SaveNPCToMap(int entnum, const char* npcType) {
	if (entnum < 0 || !npcType) return;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	char buf[1024];
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), NULL);
#else
	mkdir(dir.c_str(), 0755);
#endif

	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	int nextIndex = 0;
	{
		std::ifstream existing(filename);
		if (existing.is_open()) {
			std::string l;
			while (std::getline(existing, l)) { if (!l.empty()) nextIndex++; }
			existing.close();
		}
	}

	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"id\": %d, \"index\": %d, \"npc\": \"%s\", \"origin\": [%.0f, %.0f, %.0f] }\n",
		entnum, entnum, nextIndex, npcType,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2]);
	fputs(buf, f);
	fclose(f);
}

// Load saved NPC entries for the current map and spawn them. Returns how many were spawned.
int NXRP_LoadSavedNPCsForMap(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_npcs" : std::string("nxrp_npcs");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	std::ifstream ifs(filename);
	if (!ifs.is_open()) return 0;

	std::string line;
	int spawned = 0;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		const char* s = line.c_str();
		const char* npcKey = "\"npc\"";
		const char* originKey = "\"origin\"";
		const char* n = strstr(s, npcKey);
		if (!n) continue;
		n = strchr(n, ':'); if (!n) continue; n++;
		while (*n && isspace((unsigned char)*n)) n++;
		if (*n != '"') continue;
		n++;
		const char* nend = strchr(n, '"'); if (!nend) continue;
		std::string npcType(n, nend - n);

		float ox=0, oy=0, oz=0;
		const char* o = strstr(s, originKey);
		if (o) {
			o = strchr(o, '[');
			if (o) {
				o++;
				sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz);
			}
		}

		// Construct npc spawn command and execute via delayed client command helper
		char cmdBuf[128];
		Com_sprintf(cmdBuf, sizeof(cmdBuf), "npc spawn %s", npcType.c_str());
		NXRP_PrintConsoleToPlayer(cl, "^3Spawning NPC from saved file");
		SV_ExecuteClientCommandDelayed_h(cl, std::string(cmdBuf), 1);
		spawned++;
	}
	ifs.close();
	return spawned;
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
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
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

	// Attempt to free the runtime entity for the removed entry if available.
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
					// Use NXRP_FreeEntity wrapper to observe and forward frees
					NXRP_FreeEntity((void*)e);
					NXRP_RegisterSpawn(-1);
				}
			}
		}
	}

	// Remove the selected entry and rewrite file
	lines.erase(lines.begin() + index);
	FILE* f = fopen(filename.c_str(), "w");
	if (!f) return false;
	for (const auto& l : lines) {
		fputs(l.c_str(), f);
		fputc('\n', f);
	}
	fclose(f);
	return true;
}

// Persist a spawn to a per-map JSON file in the fs_homepath/nxrp_props directory.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath) {
	if (entnum < 0 || !modelPath) return;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	char buf[1024];
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), NULL);
#else
	mkdir(dir.c_str(), 0755);
#endif

	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

	int nextIndex = 0;
	{
		std::ifstream existing(filename);
		if (existing.is_open()) {
			std::string l;
			while (std::getline(existing, l)) { if (!l.empty()) nextIndex++; }
			existing.close();
		}
	}

	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"id\": %d, \"index\": %d, \"model\": \"%s\", \"origin\": [%.0f, %.0f, %.0f], \"angles\": [%.0f, %.0f, %.0f] }\n",
		entnum, entnum, nextIndex, modelPath,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
		e->s.apos.trBase[0], e->s.apos.trBase[1], e->s.apos.trBase[2]);
	fputs(buf, f);
	fclose(f);
}

// Read the per-map JSON file and send entries to the player as a simple indexed list.
void NXRP_ListSavedSpawns(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
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
		// Parse model path and origin from the saved JSON-like line
		const char* s = line.c_str();
		std::string modelPath;
		float ox=0, oy=0, oz=0;
		const char* modelKey = "\"model\"";
		const char* originKey = "\"origin\"";
		const char* m = strstr(s, modelKey);
		if (m) {
			m = strchr(m, ':'); if (m) m++;
			while (m && *m && isspace((unsigned char)*m)) m++;
			if (m && *m == '"') {
				m++;
				const char* me = strchr(m, '"');
				if (me && me > m) modelPath.assign(m, me - m);
			}
		}
		const char* o = strstr(s, originKey);
		if (o) {
			o = strchr(o, '[');
			if (o) {
				o++;
				sscanf(o, "%f %*[, ] %f %*[, ] %f", &ox, &oy, &oz);
			}
		}

		char buf[1024];
		snprintf(buf, sizeof(buf), "%d.\t%s\t%.0f, %.0f, %.0f", idx, modelPath.c_str(), ox, oy, oz);
		NXRP_PrintConsoleToPlayer(cl, buf);
		idx++;
	}
	ifs.close();
	if (idx == 0) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
}

// Alias: spawn prop at client using explicit extents. Keep same behavior as NXRP_SpawnPropAtClient.
int NXRP_SpawnPropAtClientWithExtents(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw) {
	return NXRP_SpawnPropAtClient(cl, modelPath, propMins, propMaxs, yaw);
}
