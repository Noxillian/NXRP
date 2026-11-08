#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"
#include "server/sv_gameapi.h"
#include "sys/sys_loadlib.h"
#include <string>
#include <set>
#include <fstream>
#include <cctype>
#include <cstring>
#if defined(_WIN32)
	#include <windows.h>
#else
	#include <dirent.h>
	#include <sys/stat.h>
#endif

#include "game/bg_weapons.h"
#include "game/bg_weapons.h"

// helper to execute a client command after a delay
void SV_ExecuteClientCommandDelayed_h(client_t* cl, std::string cmd, int delay);

// Forward declarations for command handlers used across the module
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor );

enum NXRP_SubCmd {
	NXRP_SUB_UNKNOWN = 0,
	NXRP_SUB_SPAWN,
	NXRP_SUB_SKILLS,
	NXRP_SUB_SKILL,
	NXRP_SUB_CHECK,
	NXRP_SUB_MMOVE,
	NXRP_SUB_TP,
	NXRP_SUB_VEHICLE,
	NXRP_SUB_QUEST,
	NXRP_SUB_INFO,

	NXRP_SUB_ACCOUNT,
	NXRP_SUB_GIVEALL,
	NXRP_SUB_BOSS,
	NXRP_SUB_NOCLIP,
	NXRP_SUB_SPEED,
	NXRP_SUB_ARMOR,
	NXRP_SUB_SABERSIZE,
	NXRP_SUB_SETSIZE,
	NXRP_SUB_REGISTER,
	NXRP_SUB_LOGIN,
	NXRP_SUB_POS,
	NXRP_SUB_MODEL,

	NXRP_SUB_PMODEL,
	NXRP_SUB_LIST
};

static NXRP_SubCmd NXRP_ParseSubcmd( const char *s ) {
	if ( !s || !s[0] )					return NXRP_SUB_UNKNOWN;
	if ( !Q_stricmp( s, "spawn" ) )		return NXRP_SUB_SPAWN;
	if ( !Q_stricmp( s, "info" ) )		return NXRP_SUB_INFO;
	if ( !Q_stricmp( s, "skills" ) )	return NXRP_SUB_SKILLS;
	if ( !Q_stricmp( s, "account" ) )	return NXRP_SUB_ACCOUNT;
	if ( !Q_stricmp( s, "giveall" ) )	return NXRP_SUB_GIVEALL;
	if ( !Q_stricmp( s, "noclip" ) )	return NXRP_SUB_NOCLIP;
	if ( !Q_stricmp( s, "speed" ) )		return NXRP_SUB_SPEED;
	if ( !Q_stricmp( s, "armor" ) )		return NXRP_SUB_ARMOR;
	if ( !Q_stricmp( s, "sabersize" ) )	return NXRP_SUB_SABERSIZE;
	if ( !Q_stricmp( s, "setsize" ) )	return NXRP_SUB_SETSIZE;
	if ( !Q_stricmp( s, "register" ) )	return NXRP_SUB_REGISTER;
	if ( !Q_stricmp( s, "login" ) )		return NXRP_SUB_LOGIN;
	if ( !Q_stricmp( s, "skill" ) )		return NXRP_SUB_SKILL;
	if ( !Q_stricmp( s, "check" ) )		return NXRP_SUB_CHECK;
	if ( !Q_stricmp( s, "pos" ) )		return NXRP_SUB_POS;
	if ( !Q_stricmp( s, "tp" ) )		return NXRP_SUB_TP;
	if ( !Q_stricmp( s, "prop" ) )		return NXRP_SUB_MODEL;
	if ( !Q_stricmp( s, "boss" ) )		return NXRP_SUB_BOSS;
	if ( !Q_stricmp( s, "vehicle" ) )	return NXRP_SUB_VEHICLE;
	if ( !Q_stricmp( s, "quest" ) )		return NXRP_SUB_QUEST;
	if ( !Q_stricmp( s, "list" ) )		return NXRP_SUB_LIST;
	return NXRP_SUB_UNKNOWN;
}

// Initialize FFA helpers (implemented in nxrp_ffa.cpp)

qboolean NXRP_HandleChatCommands( client_t *cl, const char *commandName, const char *chatCursor ) {
	if ( !commandName ) return qfalse;

	char subcmd[MAX_TOKEN_CHARS] = {0};
	char normalizedCursor[1024] = {0};
	const char *dispatchCursor = chatCursor ? chatCursor : "";

	Q_strncpyz( subcmd, commandName, sizeof(subcmd) );
	Com_sprintf(
		normalizedCursor,
		sizeof(normalizedCursor),
		"%s%s%s",
		subcmd,
		(dispatchCursor[0] ? " " : ""),
		dispatchCursor
	);
	dispatchCursor = normalizedCursor;

	NXRP_SubCmd sc = NXRP_ParseSubcmd( subcmd );

	// Special handling for prop subcommands: support syntax
	//   !prop s <modelpath>
	//   !prop r <index>
	if (!Q_stricmp(subcmd, "prop")) {
		// Parse "prop" subcommands manually to avoid fragile sscanf patterns.
		const char* p = dispatchCursor;
		while (*p == ' ' || *p == '\t') {
			++p;
		}

		char first[MAX_TOKEN_CHARS] = {0};
		int firstLen = 0;
		while (*p && *p != ' ' && *p != '\t') {
			if (firstLen < (MAX_TOKEN_CHARS - 1)) {
				first[firstLen++] = *p;
			}
			++p;
		}
		first[firstLen] = '\0';

		while (*p == ' ' || *p == '\t') {
			++p;
		}

		char action[MAX_TOKEN_CHARS] = {0};
		if (!Q_stricmp(first, "prop")) {
			int actionLen = 0;
			while (*p && *p != ' ' && *p != '\t') {
				if (actionLen < (MAX_TOKEN_CHARS - 1)) {
					action[actionLen++] = *p;
				}
				++p;
			}
			action[actionLen] = '\0';
		} else {
			Q_strncpyz(action, first, sizeof(action));
		}

		while (*p == ' ' || *p == '\t') {
			++p;
		}

		// If action is numeric, treat as index for directional prop move: "!nx prop <index> <dir> <amount>"
		bool actionIsNumber = false;
		if (action[0] == '-' || (action[0] >= '0' && action[0] <= '9')) {
			actionIsNumber = true;
			for (int ai = 1; action[ai]; ++ai) {
				if (!(action[ai] >= '0' && action[ai] <= '9')) { actionIsNumber = false; break; }
			}
		}
		if (actionIsNumber) {
			int idx = atoi(action);
			return NXRP_cmdAdmin_prop_dir(cl, idx, p);
		}
		if (!Q_stricmp(action, "s")) {
			return NXRP_cmdAdmin_prop_s(cl, p);
		} else if (!Q_stricmp(action, "a")) {
			return NXRP_cmdAdmin_prop_a(cl, p);
		} else if (!Q_stricmp(action, "r")) {
			return NXRP_cmdAdmin_prop_r(cl, p);
		} else if (!Q_stricmp(action, "load")) {
			return NXRP_cmdAdmin_prop_load(cl, dispatchCursor);
		} else if (!Q_stricmp(action, "list")) {
			return NXRP_cmdAdmin_prop_list(cl, dispatchCursor);
		} else if (!Q_stricmp(action, "scale")) {
			return NXRP_cmdAdmin_prop_scale(cl, p);
		}
	}

	switch ( sc ) {
	case NXRP_SUB_TP:
		return NXRP_cmdAdmin_tp(cl, dispatchCursor);

	case NXRP_SUB_SPAWN:
		return NXRP_cmdAdmin_spawn(cl, dispatchCursor);

	case NXRP_SUB_QUEST:
		return NXRP_cmdAdmin_quest(cl, dispatchCursor);

	case NXRP_SUB_MODEL:
		return NXRP_cmdAdmin_prop_s(cl, dispatchCursor);

	case NXRP_SUB_UNKNOWN:
	{
		char firstTok[MAX_TOKEN_CHARS] = {0};
		if (sscanf(dispatchCursor, "%31s", firstTok) == 1) {
			if (!Q_stricmp(firstTok, "t")) {
				// dispatch to prop_t handler; skip the "t" token
				const char* p = dispatchCursor;
				while (*p == ' ' || *p == '\t') ++p;
				// skip token
				while (*p && *p != ' ' && *p != '\t') ++p;
				while (*p == ' ' || *p == '\t') ++p;
				// removed prop-t shorthand
				return qtrue;
			}
			if (!Q_stricmp(firstTok, "test2")) {
				// test2 command removed
				return qtrue;
			}
		}
		break;
	}

	case NXRP_SUB_BOSS:
		return NXRP_cmdAdmin_boss(cl, dispatchCursor);
	case NXRP_SUB_LIST:
		return NXRP_cmdAdmin_list(cl, dispatchCursor);
	case NXRP_SUB_VEHICLE:
		return NXRP_cmdAdmin_vehicle(cl, dispatchCursor);
	case NXRP_SUB_POS:
		return NXRP_cmd_pos(cl, dispatchCursor);
	case NXRP_SUB_SKILLS:
		return NXRP_cmd_skills(cl, dispatchCursor);
	case NXRP_SUB_SKILL:
		return NXRP_cmd_skill(cl, dispatchCursor);
	case NXRP_SUB_CHECK:
		return NXRP_cmd_check(cl, dispatchCursor);
	case NXRP_SUB_INFO:
		return NXRP_cmd_info( cl, dispatchCursor );
	case NXRP_SUB_ACCOUNT:
		return NXRP_cmd_account( cl, dispatchCursor );
	case NXRP_SUB_GIVEALL:
		return NXRP_cmdAdmin_giveall( cl, dispatchCursor );
	case NXRP_SUB_NOCLIP:
		return NXRP_cmdAdmin_noclip( cl, dispatchCursor );
	case NXRP_SUB_SPEED:
		return NXRP_cmdAdmin_speed( cl, dispatchCursor );
	case NXRP_SUB_ARMOR:
		return NXRP_cmdAdmin_armor( cl, dispatchCursor );
	case NXRP_SUB_SABERSIZE:
		return NXRP_cmdAdmin_sabersize( cl, dispatchCursor );
	case NXRP_SUB_SETSIZE:
		return NXRP_cmdAdmin_setsize( cl, dispatchCursor );
	case NXRP_SUB_REGISTER:
		return NXRP_cmd_register( cl, dispatchCursor );
	case NXRP_SUB_LOGIN:
		return NXRP_cmd_login( cl, dispatchCursor );
	default:
		break;
	}

	return qtrue;
}

extern "C" qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );

extern "C" qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor ) {
	return NXRP_HandleChatCommands(cl, commandName, chatCursor);
}

