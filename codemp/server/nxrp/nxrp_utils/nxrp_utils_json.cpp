// Renamed from nxrp_util_json.cpp to nxrp_utils_json.cpp
#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"

#include <fstream>
#include <cctype>
#include <sstream>

// Minimal JSON parsing for the simple skills schema used by NXRP. Lightweight
// and tolerant parser; replace with a proper JSON library if needed.

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
					int cost = 0;
					size_t colon = content.find(':', levelKey);
					// level value is ignored by the simplified schema; only cost is stored
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

// Read the numeric "level" field from a simple account JSON file at filepath.
// Returns true on success and sets outLevel. This uses a tolerant text search
// similar to other NXRP helpers and expects a top-level "level": <number> entry.
bool NXRP_GetAccountLevelFromFile(const char *filepath, int &outLevel) {
	std::string sval;
	if (!NXRP_GetAccountFieldFromFile(filepath, "level", sval)) return false;
	outLevel = atoi(sval.c_str());
	return true;
}

// Set the numeric "level" field in the account JSON file at filepath.
// Returns true on success. This performs a textual replacement of the numeric
// token following the "level" key and writes the updated content back.
bool NXRP_SetAccountLevelInFile(const char *filepath, int newLevel) {
	return NXRP_SetAccountFieldInFile(filepath, "level", std::to_string(newLevel), false);
}

// Generic account field accessors
bool NXRP_GetAccountFieldFromFile(const char *filepath, const char *field, std::string &outValue) {
	if (!filepath || !field) return false;
	std::string content;
	if (!NXRP_ReadFileToString(filepath, content)) return false;
	std::string key = std::string("\"") + field + "\"";
	size_t k = content.find(key);
	if (k == std::string::npos) return false;
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) return false;
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	if (pos >= content.size()) return false;
	// If value is a quoted string
	if (content[pos] == '"') {
		size_t q = content.find('"', pos + 1);
		if (q == std::string::npos) return false;
		outValue = content.substr(pos + 1, q - (pos + 1));
		return true;
	}
	// Otherwise read until comma or closing brace
	size_t end = pos;
	while (end < content.size() && content[end] != ',' && content[end] != '}' && content[end] != '\n' && content[end] != '\r') ++end;
	// trim trailing whitespace
	while (end > pos && isspace((unsigned char)content[end-1])) --end;
	outValue = content.substr(pos, end - pos);
	return true;
}

bool NXRP_SetAccountFieldInFile(const char *filepath, const char *field, const std::string &newValue, bool quoteValue) {
	if (!filepath || !field) return false;
	std::string content;
	if (!NXRP_ReadFileToString(filepath, content)) return false;
	std::string key = std::string("\"") + field + "\"";
	size_t k = content.find(key);
	std::string toInsert;
	if (quoteValue) toInsert = std::string("\"") + newValue + "\"";
	else toInsert = newValue;
	if (k == std::string::npos) {
		// insert before final '}'
		size_t rb = content.rfind('}');
		if (rb == std::string::npos) return false;
		// determine if we need a comma
		size_t insertPos = rb;
		// trim trailing whitespace before rb
		size_t t = rb;
		while (t > 0 && isspace((unsigned char)content[t-1])) --t;
		bool needsComma = false;
		if (t > 1) {
			// If file not empty object
			needsComma = true;
		}
		std::string addition = (needsComma ? std::string(",\n  ") : std::string("\n  ")) + key + ": " + toInsert + "\n";
		content.insert(insertPos, addition);
		return NXRP_WriteStringToFile(filepath, content);
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) return false;
	size_t pos = colon + 1;
	while (pos < content.size() && isspace((unsigned char)content[pos])) pos++;
	size_t end = pos;
	if (end < content.size() && content[end] == '"') {
		++end;
		while (end < content.size() && content[end] != '"') ++end;
		if (end < content.size()) ++end; // include closing quote
	} else {
		while (end < content.size() && content[end] != ',' && content[end] != '}') ++end;
	}
	std::string newContent = content.substr(0, pos);
	newContent += toInsert;
	newContent += content.substr(end);
	return NXRP_WriteStringToFile(filepath, newContent);
}
