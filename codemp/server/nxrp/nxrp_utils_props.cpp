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

// Forward declaration for delayed client command helper (defined elsewhere)
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Simple helper: lookup a prop catalog entry by name. Returns true if found
// and fills outModel/outMins/outMaxs/outOrigin/outYaw.
bool NXRP_GetPropFromCatalog(const char* name, std::string& outModel, vec3_t& outMins, vec3_t& outMaxs, vec3_t& outOrigin, float& outYaw) {
	if (!name || !name[0]) return false;
	std::string catalogPath = std::string("codemp/server/nxrp/nxrp_props_catalog.json");
	std::ifstream ifs(catalogPath);
	if (!ifs.is_open()) return false;

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

// Load saved prop entries for the current map and spawn them. Returns how many were spawned.
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

// Persist a spawn to a per-map JSON file in the fs_homepath/nxrp_props directory.
void NXRP_SaveSpawnToMap(int entnum, const char* modelPath) {
	if (entnum < 0 || !modelPath) return;

	sharedEntity_t* e = SV_GentityNum(entnum);
	if (!e) return;

	// Build JSON entry: { "ent": N, "model": "path", "origin": [x,y,z], "angles": [x,y,z] }
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

	// Append the entry to the map file.
	FILE* f = fopen(filename.c_str(), "a");
	if (!f) return;

	snprintf(buf, sizeof(buf), "{ \"ent\": %d, \"model\": \"%s\", \"origin\": [%.1f, %.1f, %.1f], \"angles\": [%.1f, %.1f, %.1f] }\n",
		entnum, modelPath,
		e->s.pos.trBase[0], e->s.pos.trBase[1], e->s.pos.trBase[2],
		e->s.apos.trBase[0], e->s.apos.trBase[1], e->s.apos.trBase[2]);
	fputs(buf, f);
	fclose(f);
}
