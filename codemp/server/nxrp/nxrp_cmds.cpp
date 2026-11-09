/* nxrp_cmds.cpp - additional NXRP chat commands
 * Implements: !info (prints NXRP version)
 */

#include "server/nxrp/nxrp_main.h"
#include "server/nxrp/nxrp_utils/nxrp_utils.h"

#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <sys/stat.h>
#if defined(_WIN32)
	#include <direct.h>
#endif
#include <algorithm>
#include "qcommon/md5.h"

static void NXRP_CheckGentity(client_t* cl, sharedEntity_t* ent) {
	NXRP_PrintConsole(cl, "gentity.s.number: %d", ent->s.number);
	NXRP_PrintConsole(cl, "gentity.s.eType: %d", ent->s.eType);
	NXRP_PrintConsole(cl, "gentity.s.eFlags: %d", ent->s.eFlags);
	NXRP_PrintConsole(cl, "gentity.s.eFlags2: %d", ent->s.eFlags2);
	NXRP_PrintConsole(cl, "gentity.s.origin2: %.2f %.2f %.2f", ent->s.origin2[0], ent->s.origin2[1], ent->s.origin2[2]);
	NXRP_PrintConsole(cl, "gentity.s.speed: %.2f", ent->s.speed);
	NXRP_PrintConsole(cl, "gentity.s.activeForcePass: %d", ent->s.activeForcePass);
	NXRP_PrintConsole(cl, "gentity.s.emplacedOwner: %d", ent->s.emplacedOwner);
	NXRP_PrintConsole(cl, "gentity.s.fireflag: %d", ent->s.fireflag);
	NXRP_PrintConsole(cl, "gentity.s.modelindex: %d", ent->s.modelindex);
	NXRP_PrintConsole(cl, "gentity.s.modelindex2: %d", ent->s.modelindex2);
	NXRP_PrintConsole(cl, "gentity.s.frame: %d", ent->s.frame);
	NXRP_PrintConsole(cl, "gentity.s.weapon: %d", ent->s.weapon);
	NXRP_PrintConsole(cl, "gentity.s.clientNum: %d", ent->s.clientNum);
	NXRP_PrintConsole(cl, "gentity.s.otherEntityNum: %d", ent->s.otherEntityNum);
	NXRP_PrintConsole(cl, "gentity.s.otherEntityNum2: %d", ent->s.otherEntityNum2);
	NXRP_PrintConsole(cl, "gentity.s.groundEntityNum: %d", ent->s.groundEntityNum);
	NXRP_PrintConsole(cl, "gentity.s.owner: %d", ent->s.owner);
	NXRP_PrintConsole(cl, "gentity.s.teamowner: %d", ent->s.teamowner);
	NXRP_PrintConsole(cl, "gentity.s.shouldtarget: %d", ent->s.shouldtarget);
	NXRP_PrintConsole(cl, "gentity.s.powerups: %d", ent->s.powerups);
	NXRP_PrintConsole(cl, "gentity.s.loopSound: %d", ent->s.loopSound);
	NXRP_PrintConsole(cl, "gentity.s.loopIsSoundset: %d", ent->s.loopIsSoundset);
	NXRP_PrintConsole(cl, "gentity.s.soundSetIndex: %d", ent->s.soundSetIndex);
	NXRP_PrintConsole(cl, "gentity.s.saberInFlight: %d", ent->s.saberInFlight);
	NXRP_PrintConsole(cl, "gentity.s.saberEntityNum: %d", ent->s.saberEntityNum);
	NXRP_PrintConsole(cl, "gentity.s.saberMove: %d", ent->s.saberMove);
	NXRP_PrintConsole(cl, "gentity.s.forcePowersActive: %d", ent->s.forcePowersActive);
	NXRP_PrintConsole(cl, "gentity.s.saberHolstered: %d", ent->s.saberHolstered);
	NXRP_PrintConsole(cl, "gentity.s.isJediMaster: %d", ent->s.isJediMaster);
	NXRP_PrintConsole(cl, "gentity.s.isPortalEnt: %d", ent->s.isPortalEnt);
	NXRP_PrintConsole(cl, "gentity.s.solid: %d", ent->s.solid);
	NXRP_PrintConsole(cl, "gentity.s.event: %d", ent->s.event);
	NXRP_PrintConsole(cl, "gentity.s.eventParm: %d", ent->s.eventParm);
	NXRP_PrintConsole(cl, "gentity.s.iModelScale: %d", ent->s.iModelScale);
	NXRP_PrintConsole(cl, "gentity.s.brokenLimbs: %d", ent->s.brokenLimbs);
	NXRP_PrintConsole(cl, "gentity.s.hasLookTarget: %d", ent->s.hasLookTarget);
	NXRP_PrintConsole(cl, "gentity.s.lookTarget: %d", ent->s.lookTarget);
	NXRP_PrintConsole(cl, "gentity.s.customRGBA: %d %d %d %d", ent->s.customRGBA[0], ent->s.customRGBA[1], ent->s.customRGBA[2], ent->s.customRGBA[3]);
	NXRP_PrintConsole(cl, "gentity.s.NPC_class: %d", ent->s.NPC_class);
	NXRP_PrintConsole(cl, "gentity.s.m_iVehicleNum: %d", ent->s.m_iVehicleNum);
	NXRP_PrintConsole(cl, "gentity.s.health: %d", ent->s.health);
	NXRP_PrintConsole(cl, "gentity.s.maxhealth: %d", ent->s.maxhealth);
	NXRP_PrintConsole(cl, "gentity.s.userInt1: %d", ent->s.userInt1);
	NXRP_PrintConsole(cl, "gentity.s.userInt2: %d", ent->s.userInt2);
	NXRP_PrintConsole(cl, "gentity.s.userInt3: %d", ent->s.userInt3);
	NXRP_PrintConsole(cl, "gentity.s.userFloat1: %.2f", ent->s.userFloat1);
	NXRP_PrintConsole(cl, "gentity.s.userFloat2: %.2f", ent->s.userFloat2);
	NXRP_PrintConsole(cl, "gentity.s.userFloat3: %.2f", ent->s.userFloat3);
	NXRP_PrintConsole(cl, "gentity.s.userVec1: %.2f %.2f %.2f", ent->s.userVec1[0], ent->s.userVec1[1], ent->s.userVec1[2]);
	NXRP_PrintConsole(cl, "gentity.s.userVec2: %.2f %.2f %.2f", ent->s.userVec2[0], ent->s.userVec2[1], ent->s.userVec2[2]);
	NXRP_PrintConsole(cl, "gentity.s.origin: %.2f %.2f %.2f", ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
	NXRP_PrintConsole(cl, "gentity.s.angles: %.2f %.2f %.2f", ent->s.angles[0], ent->s.angles[1], ent->s.angles[2]);
	NXRP_PrintConsole(cl, "gentity.r.currentOrigin: %.2f %.2f %.2f", ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
	NXRP_PrintConsole(cl, "gentity.r.currentAngles: %.2f %.2f %.2f", ent->r.currentAngles[0], ent->r.currentAngles[1], ent->r.currentAngles[2]);
	NXRP_PrintConsole(cl, "gentity.r.mins: %.2f %.2f %.2f", ent->r.mins[0], ent->r.mins[1], ent->r.mins[2]);
	NXRP_PrintConsole(cl, "gentity.r.maxs: %.2f %.2f %.2f", ent->r.maxs[0], ent->r.maxs[1], ent->r.maxs[2]);
	NXRP_PrintConsole(cl, "gentity.r.contents: %d", ent->r.contents);
	NXRP_PrintConsole(cl, "gentity.r.svFlags: %d", ent->r.svFlags);
	NXRP_PrintConsole(cl, "gentity.health: %d", ent->health);
	NXRP_PrintConsole(cl, "gentity.client: %s", ent->client ? "yes" : "no");
}

static void NXRP_CheckPlayerState(client_t* cl, playerState_t* ps) {
	NXRP_PrintConsole(cl, "playerState.clientNum: %d", ps->clientNum);
	NXRP_PrintConsole(cl, "playerState.origin: %.2f %.2f %.2f", ps->origin[0], ps->origin[1], ps->origin[2]);
	NXRP_PrintConsole(cl, "playerState.gravity: %d", ps->gravity);
	NXRP_PrintConsole(cl, "playerState.speed: %.2f", ps->speed);
	NXRP_PrintConsole(cl, "playerState.basespeed: %d", ps->basespeed);
	NXRP_PrintConsole(cl, "playerState.groundEntityNum: %d", ps->groundEntityNum);
	NXRP_PrintConsole(cl, "playerState.eFlags: %d", ps->eFlags);
	NXRP_PrintConsole(cl, "playerState.eFlags2: %d", ps->eFlags2);
	NXRP_PrintConsole(cl, "playerState.weapon: %d", ps->weapon);
	//NXRP_PrintConsole(cl, "playerState.viewheight: %d", ps->viewheight);
	for (int i = 0; i < MAX_STATS; ++i) NXRP_PrintConsole(cl, "playerState.stats[%d]: %d", i, ps->stats[i]);
	NXRP_PrintConsole(cl, "playerState.stats.armor: %d", ps->stats[STAT_ARMOR]);
	NXRP_PrintConsole(cl, "playerState.stats.maxHealth: %d", ps->stats[STAT_MAX_HEALTH]);
	NXRP_PrintConsole(cl, "playerState.stats.weapons: %d", ps->stats[STAT_WEAPONS]);
	NXRP_PrintConsole(cl, "playerState.persistant.score: %d", ps->persistant[PERS_SCORE]);
	NXRP_PrintConsole(cl, "playerState.persistant.team: %d", ps->persistant[PERS_TEAM]);
	NXRP_PrintConsole(cl, "playerState.persistant.rank: %d", ps->persistant[PERS_RANK]);
	NXRP_PrintConsole(cl, "playerState.persistant.killed: %d", ps->persistant[PERS_KILLED]);
	//for (int i = 0; i < MAX_POWERUPS; ++i) NXRP_PrintConsole(cl, "playerState.powerups[%d]: %d", i, ps->powerups[i]);
	//for (int i = 0; i < MAX_AMMO; ++i) NXRP_PrintConsole(cl, "playerState.ammo[%d]: %d", i, ps->ammo[i]);
	//for (int i = 0; i < MAX_PERSISTANT; ++i) NXRP_PrintConsole(cl, "playerState.persistant[%d]: %d", i, ps->persistant[i]);
	//NXRP_PrintConsole(cl, "playerState.ping: %d", ps->ping);
	NXRP_PrintConsole(cl, "playerState.saberMove: %d", ps->saberMove);
	NXRP_PrintConsole(cl, "playerState.saberEntityNum: %d", ps->saberEntityNum);
	NXRP_PrintConsole(cl, "playerState.activeForcePass: %d", ps->activeForcePass);
	NXRP_PrintConsole(cl, "playerState.jetpackFuel: %d", ps->jetpackFuel);
	NXRP_PrintConsole(cl, "playerState.cloakFuel: %d", ps->cloakFuel);
	NXRP_PrintConsole(cl, "playerState.heldByClient: %d", ps->heldByClient);
	NXRP_PrintConsole(cl, "playerState.iModelScale: %d", ps->iModelScale);
	NXRP_PrintConsole(cl, "playerState.customRGBA: %d %d %d %d", ps->customRGBA[0], ps->customRGBA[1], ps->customRGBA[2], ps->customRGBA[3]);
	for (int i = 0; i < MAX_PERSISTANT; ++i) NXRP_PrintConsole(cl, "playerState.persistant[%d]: %d", i, ps->persistant[i]);
	NXRP_PrintConsole(cl, "playerState.fd.forcePowersKnown: %d", ps->fd.forcePowersKnown);
	NXRP_PrintConsole(cl, "playerState.fd.forcePower: %d", ps->fd.forcePower);
	NXRP_PrintConsole(cl, "playerState.fd.forcePowerMax: %d", ps->fd.forcePowerMax);
	NXRP_PrintConsole(cl, "playerState.fd.saberAnimLevel: %d", ps->fd.saberAnimLevel);
	NXRP_PrintConsole(cl, "playerState.fd.saberAnimLevelBase: %d", ps->fd.saberAnimLevelBase);
	NXRP_PrintConsole(cl, "playerState.fd.forceSide: %d", ps->fd.forceSide);
	NXRP_PrintConsole(cl, "playerState.fd.forceRank: %d", ps->fd.forceRank);
}

static void NXRP_CheckMB2(client_t* cl, playerState_t* ps) {
	NXRP_PrintConsole(cl, "MB2 diagnostics for client %d", ps->clientNum);
	static const char* classNames[] = {
		"NONE", "STORMTROOPER", "SOLDER", "COMMANDER", "ELITE_SOLDER",
		"SITH", "JEDI", "BOUNTY_HUNTER", "HERO", "SBD", "WOOKIE", "DEKA",
		"CLONE", "MANDO", "ARC"
	};
	const int mbClass = cl->gentity->client ? (int)cl->gentity->client->MBClass : 0;
	const char* className = (mbClass >= 0 && mbClass < (int)(sizeof(classNames) / sizeof(classNames[0]))) ? classNames[mbClass] : "UNKNOWN";
	const char* model = Info_ValueForKey(cl->userinfo, "model");
	NXRP_PrintConsole(cl, "MB2 class: %d (%s)", mbClass, className);
	NXRP_PrintConsole(cl, "MB2 model: %s", model && *model ? model : "<none>");
	NXRP_PrintConsole(cl, "MB2 player configstring index: %d", MB2_CS_PLAYERS + ps->clientNum);
	{
		char playerConfig[MAX_STRING_CHARS] = {0};
		char name[128] = {0};
		char modelValue[128] = {0};
		char classValue[128] = {0};
		char mbClassValue[128] = {0};


		SV_GetConfigstring(MB2_CS_PLAYERS + ps->clientNum, playerConfig, sizeof(playerConfig));
		Q_strncpyz(name, Info_ValueForKey(playerConfig, "n"), sizeof(name));
		Q_strncpyz(modelValue, Info_ValueForKey(playerConfig, "m"), sizeof(modelValue));
		Q_strncpyz(classValue, Info_ValueForKey(playerConfig, "sc"), sizeof(classValue));
		Q_strncpyz(mbClassValue, Info_ValueForKey(playerConfig, "mbc"), sizeof(mbClassValue));
		NXRP_PrintConsole(cl, "MB2 player configstring: %s", playerConfig[0] ? playerConfig : "<empty>");
		NXRP_PrintConsole(cl, "MB2 config name: %s", name[0] ? name : "<none>");
		NXRP_PrintConsole(cl, "MB2 config model: %s", modelValue[0] ? modelValue : "<none>");
		NXRP_PrintConsole(cl, "MB2 config siege class: %s", classValue[0] ? classValue : "<none>");
		NXRP_PrintConsole(cl, "MB2 config class id: %s", mbClassValue[0] ? mbClassValue : "<none>");
	}

	/*NXRP_PrintConsole(cl, "MB2 searching live reliable commands for class/attribute data...");

	int foundClassCommand = 0;
	for (int i = 0; i < MAX_RELIABLE_COMMANDS; ++i) {
		const char* command = cl->reliableCommands[i];
		if (!command || !*command) {
			continue;
		}
		if (strstr(command, "mbc") || strstr(command, "MB_ATT_") || strstr(command, "attributes") ||
			!Q_stricmpn(command, "cs ", 3) || !Q_stricmpn(command, "print ", 6)) {
			NXRP_PrintConsole(cl, "MB2 reliable[%d] full: %s", i, command);
			foundClassCommand = 1;
		}
	}
	if (!foundClassCommand) {
		NXRP_PrintConsole(cl, "MB2 no class or attribute data found in reliable commands");
	}*/

	NXRP_PrintConsole(cl, "MB2 forcePowersKnown: %d", ps->fd.forcePowersKnown);
	NXRP_PrintConsole(cl, "MB2 forcePowersActive: %d", ps->fd.forcePowersActive);
	NXRP_PrintConsole(cl, "MB2 forceSide: %d", ps->fd.forceSide);
	NXRP_PrintConsole(cl, "MB2 forceRank: %d", ps->fd.forceRank);
	for (int i = 0; i < NUM_FORCE_POWERS; ++i) {
		if (ps->fd.forcePowersKnown & (1 << i)) {
			NXRP_PrintConsole(cl, "MB2 force[%d]: level %d", i, ps->fd.forcePowerLevel[i]);
		}
	}
	NXRP_PrintConsole(cl, "MB2 saberAnimLevel: %d", ps->fd.saberAnimLevel);
	NXRP_PrintConsole(cl, "MB2 saberAnimLevelBase: %d", ps->fd.saberAnimLevelBase);
	NXRP_PrintConsole(cl, "MB2 weapon: %d", ps->weapon);
	NXRP_PrintConsole(cl, "MB2 weaponstate: %d", ps->weaponstate);
	NXRP_PrintConsole(cl, "MB2 health: %d", ps->stats[STAT_HEALTH]);
	NXRP_PrintConsole(cl, "MB2 maxHealth: %d", ps->stats[STAT_MAX_HEALTH]);
	NXRP_PrintConsole(cl, "MB2 armor: %d", ps->stats[STAT_ARMOR]);
	NXRP_PrintConsole(cl, "MB2 vehicleNum: %d", ps->m_iVehicleNum);
	NXRP_PrintConsole(cl, "MB2 jetpackFuel: %d", ps->jetpackFuel);
	NXRP_PrintConsole(cl, "MB2 cloakFuel: %d", ps->cloakFuel);
	NXRP_PrintConsole(cl, "MB2 custom userInt: %d %d %d", ps->userInt1, ps->userInt2, ps->userInt3);
	NXRP_PrintConsole(cl, "MB2 custom userFloat: %.2f %.2f %.2f", ps->userFloat1, ps->userFloat2, ps->userFloat3);
}

static std::string NXRP_JsonEscape(const char* value) {
	std::string escaped;
	if (!value) return escaped;
	for (const unsigned char* p = (const unsigned char*)value; *p; ++p) {
		switch (*p) {
		case '\\': escaped += "\\\\"; break;
		case '"': escaped += "\\\""; break;
		case '\n': escaped += "\\n"; break;
		case '\r': escaped += "\\r"; break;
		case '\t': escaped += "\\t"; break;
		default:
			if (*p < 0x20) escaped += '?';
			else escaped += (char)*p;
			break;
		}
	}
	return escaped;
}

static std::vector<std::pair<std::string, std::string> > NXRP_ParseInfoString(const char* info) {
	std::vector<std::pair<std::string, std::string> > fields;
	if (!info) return fields;

	const char* cursor = info;
	while (*cursor) {
		if (*cursor == '\\') ++cursor;
		const char* keyStart = cursor;
		while (*cursor && *cursor != '\\') ++cursor;
		if (!*cursor) break;
		std::string key(keyStart, cursor - keyStart);
		++cursor;
		const char* valueStart = cursor;
		while (*cursor && *cursor != '\\') ++cursor;
		fields.push_back(std::make_pair(key, std::string(valueStart, cursor - valueStart)));
	}
	return fields;
}

static std::string NXRP_JsonEscapeString(const std::string& value) {
	return NXRP_JsonEscape(value.c_str());
}

static qboolean NXRP_DumpMB2Client(client_t* cl, playerState_t* ps) {
	std::string directory;
	if (!NXRP_GetFolderPath("nxrp", directory)) {
		NXRP_PrintConsole(cl, "Could not create the NXRP homepath directory");
		return qfalse;
	}

	char filename[64];
	Com_sprintf(filename, sizeof(filename), "mb2_client_%d.json", ps->clientNum);
	const std::string path = directory + "/" + filename;

	char playerConfig[MAX_STRING_CHARS] = {0};
	SV_GetConfigstring(MB2_CS_PLAYERS + ps->clientNum, playerConfig, sizeof(playerConfig));
	const std::vector<std::pair<std::string, std::string> > configFields = NXRP_ParseInfoString(playerConfig);
	const int gameClientSize = sv.gameClientSize;
	const size_t sharedSize = sizeof(playerState_t);
	const size_t maxRawTail = 65536;
	size_t rawTailSize = 0;
	if (gameClientSize > (int)sharedSize && gameClientSize - (int)sharedSize <= (int)maxRawTail) {
		rawTailSize = (size_t)gameClientSize - sharedSize;
	}

	std::ostringstream output;
	output << "{\n";
	output << "  \"clientNum\": " << ps->clientNum << ",\n";
	output << "  \"serverGameClientSize\": " << gameClientSize << ",\n";
	output << "  \"knownPlayerStateSize\": " << sharedSize << ",\n";
	output << "  \"unknownGameClientTailSize\": " << rawTailSize << ",\n";
	output << "  \"userinfo\": \"" << NXRP_JsonEscape(cl->userinfo) << "\",\n";
	output << "  \"playerConfigstring\": \"" << NXRP_JsonEscape(playerConfig) << "\",\n";
	output << "  \"playerConfigFields\": {\n";
	for (size_t i = 0; i < configFields.size(); ++i) {
		output << "    \"" << NXRP_JsonEscapeString(configFields[i].first) << "\": \""
			<< NXRP_JsonEscapeString(configFields[i].second) << "\""
			<< (i + 1 < configFields.size() ? "," : "") << "\n";
	}
	output << "  },\n";
	output << "  \"playerState\": {\n";
	output << "    \"weapon\": " << ps->weapon << ",\n";
	output << "    \"weaponstate\": " << ps->weaponstate << ",\n";
	output << "    \"health\": " << ps->stats[STAT_HEALTH] << ",\n";
	output << "    \"maxHealth\": " << ps->stats[STAT_MAX_HEALTH] << ",\n";
	output << "    \"armor\": " << ps->stats[STAT_ARMOR] << ",\n";
	output << "    \"forcePowersKnown\": " << ps->fd.forcePowersKnown << ",\n";
	output << "    \"forcePowersActive\": " << ps->fd.forcePowersActive << ",\n";
	output << "    \"forcePower\": " << ps->fd.forcePower << ",\n";
	output << "    \"forcePowerMax\": " << ps->fd.forcePowerMax << ",\n";
	output << "    \"saberAnimLevel\": " << ps->fd.saberAnimLevel << ",\n";
	output << "    \"saberAnimLevelBase\": " << ps->fd.saberAnimLevelBase << ",\n";
	output << "    \"jetpackFuel\": " << ps->jetpackFuel << ",\n";
	output << "    \"cloakFuel\": " << ps->cloakFuel << ",\n";
	output << "    \"userInt1\": " << ps->userInt1 << ",\n";
	output << "    \"userInt2\": " << ps->userInt2 << ",\n";
	output << "    \"userInt3\": " << ps->userInt3 << "\n";
	output << "  },\n";
	output << "  \"unknownGameClientTail\": [\n";

	const unsigned char* clientBytes = (const unsigned char*)cl->gentity->client;
	for (size_t offset = 0; offset < rawTailSize; offset += sizeof(uint32_t)) {
		uint32_t word = 0;
		const size_t bytes = (rawTailSize - offset < sizeof(word)) ? rawTailSize - offset : sizeof(word);
		memcpy(&word, clientBytes + sharedSize + offset, bytes);
		char line[96];
		Com_sprintf(line, sizeof(line), "    { \"offset\": %u, \"hex\": \"0x%08x\", \"signed\": %d, \"unsigned\": %u }%s\n",
			(unsigned)(sharedSize + offset), (unsigned)word, (int32_t)word, (unsigned)word,
			offset + sizeof(word) < rawTailSize ? "," : "");
		output << line;
	}
	output << "  ]\n}\n";
	const std::string content = output.str();
	if (!NXRP_WriteStringToFile(path.c_str(), content)) {
		NXRP_PrintConsole(cl, "Could not write %s", path.c_str());
		return qfalse;
	}
	NXRP_PrintConsole(cl, "Wrote MB2 client diagnostics to %s", path.c_str());
	return qtrue;
}

// Minimal skills listing handler
qboolean NXRP_cmd_skills( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	std::string dir;
	NXRP_GetFolderPath("nxrp", dir); // ensure nxrp folder exists; Skills impl will resolve files inside
	NXRP_Skills_ListImpl(cl, dir.c_str());
	return qtrue;
}

qboolean NXRP_cmd_check( client_t *cl, const char *chatCursor ) {
	if (!cl || cl->state != CS_ACTIVE || !cl->gentity || !cl->gentity->playerState) {
		NXRP_PrintConsole(cl, "You must be an active player to use this command");
		return qtrue;
	}

	playerState_t* ps = cl->gentity->playerState;
	if (chatCursor && *chatCursor) {
		char firstToken[32] = {0};
		char subcommand[32] = {0};
		if (sscanf(chatCursor, "%31s %31s", firstToken, subcommand) >= 1) {
			if (!Q_stricmp(firstToken, "check") && subcommand[0]) {
				Q_strncpyz(firstToken, subcommand, sizeof(firstToken));
			}
			if (!Q_stricmp(firstToken, "gentity")) {
				NXRP_CheckGentity(cl, cl->gentity);
				return qtrue;
			}
			if (!Q_stricmp(firstToken, "playerstate")) {
				NXRP_CheckPlayerState(cl, ps);
				return qtrue;
			}
			if (!Q_stricmp(firstToken, "mb2")) {
				if (!Q_stricmp(subcommand, "dump")) {
					NXRP_DumpMB2Client(cl, ps);
					return qtrue;
				}
				NXRP_CheckMB2(cl, ps);
				return qtrue;
			}
			NXRP_PrintConsole(cl, "Usage: !check | !check gentity | !check playerstate | !check mb2 [dump]");
			return qtrue;
		}
	}
	static const char* kForceNames[NUM_FORCE_POWERS] = {
		"heal",
		"levitation",
		"speed",
		"push",
		"pull",
		"telepathy",
		"grip",
		"lightning",
		"rage",
		"protect",
		"absorb",
		"team_heal",
		"team_force",
		"drain",
		"see",
		"saber_offense",
		"saber_defense",
		"saber_throw",
	};

	NXRP_PrintConsole(cl, "Known force powers:");
	for (int i = 0; i < NUM_FORCE_POWERS; ++i) {
		if (ps->fd.forcePowersKnown & (1 << i)) {
			NXRP_PrintConsole(cl, "  %s: level %d", kForceNames[i], ps->fd.forcePowerLevel[i]);
		}
	}

	return qtrue;
}

// Handler for: !skill <skillname>
qboolean NXRP_cmd_skill( client_t *cl, const char *chatCursor ) {
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	if (!chatCursor || !*chatCursor) {
		NXRP_PrintConsole(cl, "Usage: !skill <skillid>");
		return qtrue;
	}

	// parse first token as skill id/token
	char tok[MAX_TOKEN_CHARS] = {0};
	if (sscanf(chatCursor, "%31s", tok) < 1) { NXRP_PrintConsole(cl, "Usage: !skill <skillid>"); return qtrue; }

	std::string dir;
	NXRP_GetFolderPath("nxrp", dir);
	NXRP_Skill_PurchaseImpl(cl, dir.c_str(), tok);
	return qtrue;
}


// hex decode moved to nxrp_utils.cpp

// Version variable for NXRP. Update as needed.
const char *NXRP_VERSION = "0.2.0";

// Parse username and password from chatCursor. Accept either "user pass" or "subcmd user pass".
static bool parse_user_pass(const char *chatCursor, std::string &outUser, std::string &outPass) {
	if (!chatCursor) return false;
	std::istringstream iss(chatCursor);
	std::vector<std::string> toks;
	std::string w;
	while (iss >> w) toks.push_back(w);
	if (toks.size() == 2) {
		outUser = toks[0]; outPass = toks[1]; return true;
	}
	if (toks.size() >= 3) {
		// e.g.: "register user pass" or "login user pass"
		outUser = toks[1]; outPass = toks[2]; return true;
	}
	return false;
}

// Handler for: !info
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor ) {
	// Support subcommands: cmds, acmds, help
	if (!NXRP_EnsureLoggedIn(cl)) return qtrue;
	const char* s = chatCursor ? chatCursor : "";
	if (s && *s) {
		// Trim leading spaces
		while (*s == ' ' || *s == '\t') ++s;
		if (!Q_stricmp(s, "cmds") || !Q_stricmp(s, "commands")) {
			NXRP_PrintConsole(cl, "General commands:");
			NXRP_PrintConsole(cl, "  !info | !nxrp | /info - show info/help");
			NXRP_PrintConsole(cl, "  !register <user> <pass> - create account");
			NXRP_PrintConsole(cl, "  !login <user> <pass> - login");
			NXRP_PrintConsole(cl, "  !pos - show position");
			NXRP_PrintConsole(cl, "  !skills - list skills");
			return qtrue;
		}
		if (!Q_stricmp(s, "acmds") || !Q_stricmp(s, "admin")) {
			NXRP_PrintConsole(cl, "Admin commands:");
			NXRP_PrintConsole(cl, "  !prop s/t/m - spawn/manage props");
			NXRP_PrintConsole(cl, "  !tp - teleport");
			NXRP_PrintConsole(cl, "  !list - list entities");
			NXRP_PrintConsole(cl, "  (admin only)");
			return qtrue;
		}
	}

	// Default info block
	NXRP_PrintConsole(cl, "------ NXRP ------");
	const char* username = (cl && cl->nxrp_username[0]) ? cl->nxrp_username : "<not logged in>";
	NXRP_PrintConsole(cl, va("%s %d/50", username, 1));
	NXRP_PrintConsole(cl, va("%d/100 Credits: %d", 0, 0));
	NXRP_PrintConsole(cl, "------------------");
	NXRP_PrintConsole(cl, "1. /info cmds\t- General Commands");
	NXRP_PrintConsole(cl, "2. /info acmds\t- Admin Commands");
	NXRP_PrintConsole(cl, "3. /skills\t\t- List Skills");
	return qtrue;
}

// Handler for: !register <user> <pass>
qboolean NXRP_cmd_register(client_t* cl, const char* chatCursor)
{
	std::string user, pass;

	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsole(cl, "Usage: !register <user> <pass>");
		return qtrue;
	}

	std::string safe;
	safe.reserve(user.size());

	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_')
			safe.push_back((char)std::tolower((unsigned char)c));
	}

	if (safe.empty()) {
		NXRP_PrintConsole(cl, "Invalid username");
		return qtrue;
	}

	std::string dir;
	NXRP_GetFolderPath("nxrp_accounts", dir);

	std::string filepath = dir + "/" + safe + ".json";

	std::ifstream ifs(filepath.c_str());

	if (ifs.good()) {
		NXRP_PrintConsole(cl, "Username already exists");
		return qtrue;
	}

	// Generate a random salt
	unsigned char salt[8];

	if (!Sys_RandomBytes(salt, sizeof(salt))) {
		for (int i = 0; i < (int)sizeof(salt); ++i)
			salt[i] = (unsigned char)(rand() & 0xFF);
	}

	// Hash the password with the salt
	unsigned char digest[MD5_DIGEST_SIZE];

	hmacMD5Context_t ctx;

	HMAC_MD5_Init(&ctx, salt, sizeof(salt));
	HMAC_MD5_Update(
		&ctx,
		(const unsigned char*)pass.c_str(),
		(unsigned int)pass.size()
	);
	HMAC_MD5_Final(&ctx, digest);

	std::string saltHex = NXRP_HexEncode(salt, sizeof(salt));
	std::string digestHex = NXRP_HexEncode(digest, MD5_DIGEST_SIZE);

	std::string passwordHash = saltHex + ":" + digestHex;

	// Create the account file
	if (!NXRP_uJson_CreateAccountFile(filepath, user, passwordHash)) {
		NXRP_PrintConsole(
			cl,
			"Registration failed: cannot create account file"
		);
		return qtrue;
	}

	NXRP_PrintConsole(cl, "Registration successful");
	return qtrue;
}

// Handler for: !login <user> <pass>
qboolean NXRP_cmd_login( client_t *cl, const char *chatCursor ) {
	std::string user, pass;
	if (!parse_user_pass(chatCursor, user, pass)) {
		NXRP_PrintConsole(cl, "Usage: !login <user> <pass>");
		return qtrue;
	}

	std::string safe;
	safe.reserve(user.size());
	for (char c : user) {
		if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	}
	if (safe.empty()) {
		NXRP_PrintConsole(cl, "Invalid username");
		return qtrue;
	}

	std::string dir;
	NXRP_GetFolderPath("nxrp_accounts", dir);
	std::string filepath = dir + "/" + safe + ".json";
	std::ifstream ifs(filepath);
	if (!ifs.is_open()) {
		NXRP_PrintConsole(cl, "Login failed: unknown user");
		return qtrue;
	}

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	// simple parse for password field: "password": "..."
	std::string key = "\"password\"";
	size_t k = content.find(key);
	if (k == std::string::npos) {
		NXRP_PrintConsole(cl, "Login failed");
		return qtrue;
	}
	size_t colon = content.find(':', k + key.size());
	if (colon == std::string::npos) { NXRP_PrintConsole(cl, "Login failed"); return qtrue; }
	size_t quote = content.find('"', colon);
	if (quote == std::string::npos) { NXRP_PrintConsole(cl, "Login failed"); return qtrue; }
	size_t qend = content.find('"', quote + 1);
	if (qend == std::string::npos) { NXRP_PrintConsole(cl, "Login failed"); return qtrue; }
	std::string stored = content.substr(quote + 1, qend - (quote + 1));

	// stored format: "<saltHex>:<digestHex>", digest = HMAC-MD5(salt, password)
	size_t colonIdx = stored.find(':');
	if (colonIdx != std::string::npos) {
		std::string saltHex = stored.substr(0, colonIdx);
		std::string digHex = stored.substr(colonIdx + 1);
		std::vector<unsigned char> saltBin, digBin;
		if (NXRP_HexDecode(saltHex, saltBin) && NXRP_HexDecode(digHex, digBin) && digBin.size() == MD5_DIGEST_SIZE) {
			unsigned char outdig[MD5_DIGEST_SIZE];
			hmacMD5Context_t ctx2;
			HMAC_MD5_Init(&ctx2, saltBin.data(), (unsigned int)saltBin.size());
			HMAC_MD5_Update(&ctx2, (const unsigned char*)pass.c_str(), (unsigned int)pass.size());
			HMAC_MD5_Final(&ctx2, outdig);
			if (memcmp(outdig, digBin.data(), MD5_DIGEST_SIZE) == 0) {
				NXRP_PrintConsole(cl, "Login successful");
				// mark client as logged in by storing username in their session info
				if (cl && cl->state == CS_ACTIVE) {
					Q_strncpyz(cl->nxrp_username, user.c_str(), sizeof(cl->nxrp_username));
				}
				return qtrue;
			}
		}
	}
	NXRP_PrintConsole(cl, "Login failed: incorrect password");
	return qtrue;
}

// Handler for: !account
qboolean NXRP_cmd_account( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!cl || cl->state != CS_ACTIVE) {
		NXRP_PrintConsole(cl, "You must be an active player to use this command");
		return qtrue;
	}
	const char *username = (cl && cl->nxrp_username[0]) ? cl->nxrp_username : nullptr;
	if (!username) {
		NXRP_PrintConsole(cl, "You are not logged in");
		return qtrue;
	}

	std::string user = username;
	std::string safe;
	for (char c : user) if (std::isalnum((unsigned char)c) || c == '_') safe.push_back((char)std::tolower((unsigned char)c));
	if (safe.empty()) { NXRP_PrintConsole(cl, "Invalid stored username"); return qtrue; }

	std::string dir;
	NXRP_GetFolderPath("nxrp_accounts", dir);
	std::string filepath = dir + "/" + safe + ".json";

	std::string content;
	if (!NXRP_ReadFileToString(filepath.c_str(), content)) { NXRP_PrintConsole(cl, "Account file not found"); return qtrue; }

	std::string level, exp, credits, skillpoints;
	std::string tmp;
	if (!NXRP_GetAccountFieldFromFile(filepath.c_str(), "level", tmp)) level = "0"; else level = tmp;
	if (!NXRP_GetAccountFieldFromFile(filepath.c_str(), "exp", tmp)) exp = "0"; else exp = tmp;
	if (!NXRP_GetAccountFieldFromFile(filepath.c_str(), "credits", tmp)) credits = "0"; else credits = tmp;
	if (!NXRP_GetAccountFieldFromFile(filepath.c_str(), "skillpoints", tmp)) skillpoints = "0"; else skillpoints = tmp;

	NXRP_PrintConsole(cl, "Account info for %s:", username);
	NXRP_PrintConsole(cl, "  Username: %s", username);
	NXRP_PrintConsole(cl, "  Level: %s", level.c_str());
	NXRP_PrintConsole(cl, "  Exp: %s", exp.c_str());
	NXRP_PrintConsole(cl, "  Credits: %s", credits.c_str());
	NXRP_PrintConsole(cl, "  Skill Points: %s", skillpoints.c_str());
	return qtrue;
}

// Non-admin commands remain here (info, register, login, account)

// Handler for: !pos
qboolean NXRP_cmd_pos( client_t *cl, const char *chatCursor ) {
	(void)chatCursor;
	if (!cl || cl->state != CS_ACTIVE) {
		NXRP_PrintConsole(cl, "You must be an active player to use this command");
		return qtrue;
	}
	if (!cl->gentity) {
		NXRP_PrintConsole(cl, "No game entity available for your client");
		return qtrue;
	}

	// Use the entity's current origin
	vec3_t org;
	VectorCopy(cl->gentity->r.currentOrigin, org);

	NXRP_PrintConsole(cl, "Position: %.2f, %.2f, %.2f", org[0], org[1], org[2]);
	return qtrue;
}
