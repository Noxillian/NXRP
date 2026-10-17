// Restored prop persistence and catalog implementations moved into nxrp_utils
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
		char buf[1024];
		snprintf(buf, sizeof(buf), "%d:", idx);
		NXRP_PrintConsoleToPlayer(cl, buf);
		const size_t chunk = 200;
		for (size_t off = 0; off < line.size(); off += chunk) {
			std::string part = line.substr(off, std::min(chunk, line.size() - off));
			NXRP_PrintConsoleToPlayer(cl, part.c_str());
		}
		idx++;
	}
	ifs.close();
	if (idx == 0) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
}

// Alias: spawn prop at client using explicit extents. Keep same behavior as NXRP_SpawnPropAtClient.
int NXRP_SpawnPropAtClientWithExtents(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw) {
	return NXRP_SpawnPropAtClient(cl, modelPath, propMins, propMaxs, yaw);
}

// Remove a saved spawn entry by its zero-based index. This rewrites the per-map file.
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

// Load saved spawns for the current map and attempt to spawn them into the world.
// Returns the number of spawns successfully created.
int NXRP_LoadSavedSpawnsForMap(client_t* cl) {
	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
	const char* map = Cvar_VariableString("mapname");
	if (!map || !map[0]) map = "nomap";
	std::string filename = dir + "/" + std::string(map) + ".json";

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

		if (modelPath.empty()) continue;

		// attempt to find prop extents in catalog
		vec3_t mins, maxs, origin; float yaw = ay; // use yaw from angles' second component if present
		bool hasCatalog = NXRP_GetPropFromCatalog(modelPath.c_str(), modelPath, mins, maxs, origin, yaw);
		if (!hasCatalog) {
			VectorCopy(defaultMins, mins);
			VectorCopy(defaultMaxs, maxs);
			origin[0] = ox; origin[1] = oy; origin[2] = oz;
		}

		int ent = NXRP_PlaceModel(modelPath.c_str(), mins, maxs, yaw, origin);
		if (ent >= 0) {
			NXRP_RegisterSpawn(ent);
			spawned++;
		}
	}
	ifs.close();
	if (cl) NXRP_PrintConsoleToPlayer(cl, va("Loaded %d saved spawns for this map", spawned));
	return spawned;
}

// Lookup a prop entry by name in a simple catalog. If the name appears to be
// a direct model path (contains '/' or ends with common model extensions),
// return that as the model and provide default extents. Otherwise attempt to
// read a catalog file under nxrp_props/catalog.txt with simple CSV-like lines
// of the form: key|modelPath|minx,miny,minz|maxx,maxy,maxz|originx,originy,originz|yaw
bool NXRP_GetPropFromCatalog(const char* name, std::string& outModel, vec3_t& outMins, vec3_t& outMaxs, vec3_t& outOrigin, float& outYaw) {
	if (!name || !name[0]) return false;
	std::string sname = name;
	// heuristic: if it looks like a path, return it with default extents
	if (sname.find('/') != std::string::npos || sname.find('\\') != std::string::npos || sname.find('.') != std::string::npos) {
		outModel = sname;
		outMins[0] = -24.0f; outMins[1] = -24.0f; outMins[2] = -8.0f;
		outMaxs[0] = 24.0f; outMaxs[1] = 24.0f; outMaxs[2] = 24.0f;
		outOrigin[0] = outOrigin[1] = outOrigin[2] = 0.0f;
		outYaw = 0.0f;
		return true;
	}

	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
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
int NXRP_SpawnPropAtClientWithExtents(client_t* cl, const char* modelPath, const vec3_t propMins, const vec3_t propMaxs, float yaw) {
	return NXRP_SpawnPropAtClient(cl, modelPath, propMins, propMaxs, yaw);
}

// Remove a saved spawn entry by its zero-based index. This rewrites the per-map file.
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
