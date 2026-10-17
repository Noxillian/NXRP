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
