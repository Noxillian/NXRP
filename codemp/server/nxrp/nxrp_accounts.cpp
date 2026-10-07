/* nxrp_accounts.cpp - account handlers for nxrp extension
 * Implements simple JSON-backed account creation and login.
 */

#include "server/nxrp/nxrp_accounts.h"
#include "server/nxrp/nxrp_main.h"

#include <string>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// Reuse helpers from nxrp_main.cpp by re-declaring small utilities here.
// These are intentionally simple and duplicated to keep the account module
// self-contained for now.
static void SV_nxrp_EnsureDirExists(const char* path) {
#if defined(_WIN32)
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

static void SV_nxrp_SanitizeUsername(const char* in, char* out, size_t outlen) {
	size_t j = 0;
	for ( size_t i = 0; in[i] && j + 1 < outlen; ++i ) {
		char c = in[i];
		if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_') ) {
			out[j++] = c;
		} else if ( c == ' ' || c == '-' ) {
			out[j++] = '_';
		}
	}
	out[j] = '\0';
}

static bool SV_nxrp_WriteAccountFile(const char* dirpath, const char* username, const char* password) {
	char filename[MAX_OSPATH];
	char safe[MAX_TOKEN_CHARS];
	SV_nxrp_SanitizeUsername(username, safe, sizeof(safe));
	Com_sprintf(filename, sizeof(filename), "%s/%s.json", dirpath, safe);

	std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
	if ( !ofs.is_open() ) return false;
	ofs << "{\n";
	ofs << "  \"username\": \"" << username << "\",\n";
	ofs << "  \"password\": \"" << password << "\",\n";
	ofs << "  \"acc-group\": \"player\",\n";
	ofs << "  \"exp\": 0,\n";
	ofs << "  \"level\": 1\n";
	ofs << "}\n";
	ofs.close();
	return true;
}

static bool SV_nxrp_ReadAccountPassword(const char* dirpath, const char* username, std::string &outPassword) {
	char filename[MAX_OSPATH];
	char safe[MAX_TOKEN_CHARS];
	SV_nxrp_SanitizeUsername(username, safe, sizeof(safe));
	Com_sprintf(filename, sizeof(filename), "%s/%s.json", dirpath, safe);

	std::ifstream ifs(filename);
	if ( !ifs.is_open() ) return false;
	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string content = ss.str();
	ifs.close();

	size_t p = content.find("\"password\"");
	if ( p == std::string::npos ) return false;
	size_t colon = content.find(':', p);
	if ( colon == std::string::npos ) return false;
	size_t firstQuote = content.find('"', colon);
	if ( firstQuote == std::string::npos ) return false;
	size_t secondQuote = content.find('"', firstQuote + 1);
	if ( secondQuote == std::string::npos ) return false;
	outPassword = content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
	return true;
}

qboolean SV_nxrp_HandleNxRegister( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char username[MAX_TOKEN_CHARS] = {0};
	char password[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s %31s", subcmd, username, password ) < 3 ) {
		SV_SendServerCommand( cl, "print \"Usage: !nx register <username> <password>\\n\"\n" );
		return qtrue;
	}

	const char* home = Cvar_VariableString("fs_homepath");
	const char* game = Cvar_VariableString("fs_game");
	char dirpath[MAX_OSPATH];
	if ( home && home[0] ) {
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", home, game);
	} else {
		const char* base = Cvar_VariableString("fs_basepath");
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", base, game);
	}

	SV_nxrp_EnsureDirExists(dirpath);

	char safe[MAX_TOKEN_CHARS];
	SV_nxrp_SanitizeUsername(username, safe, sizeof(safe));
	char filepath[MAX_OSPATH];
	Com_sprintf(filepath, sizeof(filepath), "%s/%s.json", dirpath, safe);
	std::ifstream check(filepath);
	if ( check.is_open() ) {
		check.close();
		SV_SendServerCommand( cl, "print \"Account already exists\\n\"\n" );
		return qtrue;
	}

	if ( SV_nxrp_WriteAccountFile(dirpath, username, password) ) {
		SV_SendServerCommand( cl, "print \"Account created successfully\\n\"\n" );
	} else {
		SV_SendServerCommand( cl, "print \"Failed to create account (filesystem error)\\n\"\n" );
	}
	return qtrue;
}

qboolean SV_nxrp_HandleNxLogin( client_t *cl, const char *chatCursor ) {
	char subcmd[MAX_TOKEN_CHARS] = {0};
	char username[MAX_TOKEN_CHARS] = {0};
	char password[MAX_TOKEN_CHARS] = {0};
	if ( sscanf( chatCursor, "%31s %31s %31s", subcmd, username, password ) < 3 ) {
		SV_SendServerCommand( cl, "print \"Usage: !nx login <username> <password>\\n\"\n" );
		return qtrue;
	}

	const char* home = Cvar_VariableString("fs_homepath");
	const char* game = Cvar_VariableString("fs_game");
	char dirpath[MAX_OSPATH];
	if ( home && home[0] ) {
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", home, game);
	} else {
		const char* base = Cvar_VariableString("fs_basepath");
		Com_sprintf(dirpath, sizeof(dirpath), "%s/%s/nxrp/nx_accounts", base, game);
	}

	std::string storedPassword;
	if ( !SV_nxrp_ReadAccountPassword(dirpath, username, storedPassword) ) {
		SV_SendServerCommand( cl, "print \"Login failed: account not found\\n\"\n" );
		return qtrue;
	}

	if ( storedPassword == password ) {
		SV_SendServerCommand( cl, "print \"Login successful\\n\"\n" );
	} else {
		SV_SendServerCommand( cl, "print \"Login failed: incorrect password\\n\"\n" );
	}
	return qtrue;
}
