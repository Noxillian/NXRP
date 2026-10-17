#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"

#include <fstream>
#include <cctype>
#include <sstream>

// Minimal JSON parsing for the simple skills schema used by NXRP.
// This is intentionally lightweight and tolerant; replace with a proper
// JSON library if schema complexity grows.

bool NXRP_ParseSkillsJson(const std::string &content, std::vector<NXRP_SimpleSkill> &out) {
	out.clear();
	size_t skillsPos = content.find("\"skills\"");
	if (skillsPos == std::string::npos) return false;
	size_t arrStart = content.find('[', skillsPos);
	if (arrStart == std::string::npos) return false;
	size_t pos = arrStart + 1;

	while (true) {
		size_t idpos = content.find("\"id\"", pos);
		if (idpos == std::string::npos) break;
		NXRP_SimpleSkill s;
		size_t q1 = content.find('"', idpos + 4);
		if (q1 == std::string::npos) break;
		size_t q2 = content.find('"', q1 + 1);
		if (q2 == std::string::npos) break;
		s.id = content.substr(q1+1, q2-q1-1);

		size_t namepos = content.find("\"name\"", idpos);
		if (namepos != std::string::npos) {
			q1 = content.find('"', namepos + 6);
			q2 = content.find('"', q1 + 1);
			if (q1 != std::string::npos && q2 != std::string::npos) s.name = content.substr(q1+1, q2-q1-1);
		}

		size_t descpos = content.find("\"description\"", idpos);
		if (descpos != std::string::npos) {
			q1 = content.find('"', descpos + 13);
			q2 = content.find('"', q1 + 1);
			if (q1 != std::string::npos && q2 != std::string::npos) s.description = content.substr(q1+1, q2-q1-1);
		}

		size_t maxpos = content.find("\"maxLevel\"", idpos);
		if (maxpos != std::string::npos) {
			size_t colon = content.find(':', maxpos);
			if (colon != std::string::npos) s.maxLevel = atoi(content.c_str() + colon + 1);
		}

		size_t levelspos = content.find("\"levels\"", idpos);
		if (levelspos != std::string::npos) {
			size_t lb = content.find('[', levelspos);
			size_t rb = (lb == std::string::npos) ? std::string::npos : content.find(']', lb);
			if (lb != std::string::npos && rb != std::string::npos) {
				size_t p = lb + 1;
				while (p < rb) {
					size_t levelKey = content.find("\"level\"", p);
					if (levelKey == std::string::npos || levelKey > rb) break;
					int lvl = 0; int cost = 0;
					size_t colon = content.find(':', levelKey);
					if (colon != std::string::npos) lvl = atoi(content.c_str() + colon + 1);
					size_t costKey = content.find("\"cost\"", levelKey);
					if (costKey != std::string::npos && costKey < rb) {
						colon = content.find(':', costKey);
						if (colon != std::string::npos) cost = atoi(content.c_str() + colon + 1);
					}
					s.costs.push_back(cost);
					p = levelKey + 6;
				}
			}
		}

		out.push_back(s);
		pos = idpos + 1;
	}

	return !out.empty();
}

bool NXRP_ReadFileToString(const char *path, std::string &out) {
	if (!path) return false;
	std::ifstream ifs(path);
	if (!ifs.is_open()) return false;
	out.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return true;
}

bool NXRP_WriteStringToFile(const char *path, const std::string &content) {
	if (!path) return false;
	std::ofstream ofs(path, std::ios::trunc);
	if (!ofs.is_open()) return false;
	ofs << content;
	ofs.close();
	return true;
}
