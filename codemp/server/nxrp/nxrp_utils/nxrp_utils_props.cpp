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
	const size_t maxLen = 900; // keep within safe console buffer
	bool any = false;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		any = true;
// Parse the JSON-ish line and print a concise one-line summary per saved spawn.
// Expected format: { "ent": <n>, "index": <n>, "model": "<path>", "origin": [x, y, z], "angles": [ax, ay, az] }
	std::string s = line;
	if (s.size() > maxLen) s = s.substr(0, maxLen - 3) + "...";

	// helper to extract integer fields
	auto extract_int = [&](const char* key, int def)->int {
		size_t p = s.find(key);
		if (p == std::string::npos) return def;
		size_t colon = s.find(':', p + strlen(key)); if (colon == std::string::npos) return def;
		size_t v = colon + 1; while (v < s.size() && isspace((unsigned char)s[v])) ++v;
		size_t e = v; if (e < s.size() && (s[e] == '-' || isdigit((unsigned char)s[e]))) ++e; while (e < s.size() && isdigit((unsigned char)s[e])) ++e;
		if (e > v) {
			try { return std::stoi(s.substr(v, e-v)); } catch(...) { return def; }
		}
		return def;
	};

	int entv = extract_int("\"ent\"", -1);
	int idxv = extract_int("\"index\"", idx);

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

	// print a single consolidated line per spawn
	if (model.empty()) model = "<unknown>";
	NXRP_PrintConsoleToPlayerFmt(cl, "%d: ent=%d model=%s origin=%.0f,%.0f,%.0f angles=%.0f,%.0f,%.0f", idx, entv, model.c_str(), ox, oy, oz, ax, ay, az);
	idx++;
	}
	if (!any) NXRP_PrintConsoleToPlayer(cl, "No saved spawns for this map");
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
		float savedYaw = ay;
		bool hasCatalog = NXRP_GetPropFromCatalog(modelPath.c_str(), modelPath, mins, maxs, origin, yaw);

		// Preserve saved origin/yaw when catalog does not provide them.
		const float eps = 0.001f;
		if (hasCatalog) {
			// If catalog returned an all-zero origin, prefer the saved origin from file
			if (fabs(origin[0]) < eps && fabs(origin[1]) < eps && fabs(origin[2]) < eps) {
				origin[0] = ox; origin[1] = oy; origin[2] = oz;
			}
			// If catalog left yaw at zero but our saved yaw was non-zero, prefer saved yaw
			if (fabs(yaw) < eps && fabs(savedYaw) >= eps) {
				yaw = savedYaw;
			}
		}

		if (!hasCatalog) {
			VectorCopy(defaultMins, mins);
			VectorCopy(defaultMaxs, maxs);
			origin[0] = ox; origin[1] = oy; origin[2] = oz;
		}

		int ent = NXRP_PlaceModel(modelPath.c_str(), mins, maxs, yaw, origin);
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
	std::string sname = name;
	// If it looks like a path, attempt to match it against the JSON catalog's model fields first.
	bool looksLikePath = (sname.find('/') != std::string::npos || sname.find('\\') != std::string::npos || sname.find('.') != std::string::npos);

	const char* home = Cvar_VariableString("fs_homepath");
	std::string dir = (home && home[0]) ? std::string(home) + "/nxrp_props" : std::string("nxrp_props");
	// Try JSON first: nxrp_props_catalog.json
	std::string jsonPath = dir + "/nxrp_props_catalog.json";
	std::ifstream jifs(jsonPath);
	if (jifs.is_open()) {
		std::string content((std::istreambuf_iterator<char>(jifs)), std::istreambuf_iterator<char>());
		jifs.close();
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
				// extents -> use as half-extents
				size_t epos = obj.find("\"extents\"");
				if (epos != std::string::npos) {
					size_t lb = obj.find('[', epos);
					if (lb != std::string::npos) {
						float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outMins[0] = -a; outMins[1] = -b; outMins[2] = -c; outMaxs[0] = a; outMaxs[1] = b; outMaxs[2] = c; }
					}
				}
				// origin
				size_t opos = obj.find("\"origin\"");
				if (opos != std::string::npos) {
					size_t lb = obj.find('[', opos);
					if (lb != std::string::npos) { float a,b,c; if (sscanf(obj.c_str()+lb, "[%f %*[, ] %f %*[, ] %f", &a,&b,&c) == 3) { outOrigin[0]=a; outOrigin[1]=b; outOrigin[2]=c; } }
				}
				// yaw
				size_t ypos = obj.find("\"yaw\"");
				if (ypos != std::string::npos) { size_t c = obj.find(':', ypos); if (c != std::string::npos) { float y; if (sscanf(obj.c_str()+c, ":%f", &y) == 1) outYaw = y; } }

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

