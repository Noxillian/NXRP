#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils.h"

#include <fstream>

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
