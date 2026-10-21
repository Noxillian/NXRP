/* nxrp_main.h - simple NXRP chat command handler
 * This file declares a handler that processes economy-style chat
  * commands beginning with '!'. (header touch)
 */
#ifndef NXRP_MAIN_H
#define NXRP_MAIN_H

#include "../server.h"
#include <string>
#include <vector>

// Primary chat handler called from server code.
// Use the NXRP_ prefix consistently across the project.
qboolean NXRP_HandleChatCommands( client_t *cl, const char *commandName, const char *chatCursor );

#ifdef __cplusplus
extern "C" {
#endif
// C-linked server entrypoint (called from C translation units such as sv_client.c)
qboolean NXRP_HandleChat( client_t *cl, const char *commandName, const char *chatCursor );
#ifdef __cplusplus
}
#endif

qboolean NXRP_cmd_register( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_login( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_giveall( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_boss( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_account( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_noclip( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_pos( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_s( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_r( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_m( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_load( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_list( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_prop_dir( client_t *cl, int index, const char *args );
qboolean NXRP_cmdAdmin_prop_dir_chat( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_spawn( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_npc( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_tp( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_vehicle( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_ffa( client_t *cl, const char *chatCursor );
qboolean NXRP_cmdAdmin_list( client_t *cl, const char *chatCursor );
void NXRP_ListSavedSpawnsForPlayer(client_t* cl);

qboolean NXRP_cmd_skills( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_skill( client_t *cl, const char *chatCursor );


// Public info/help/skills commands (usable by logged-in players)
qboolean NXRP_cmd_info( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_skills( client_t *cl, const char *chatCursor );
qboolean NXRP_cmd_skill( client_t *cl, const char *chatCursor );

// Internal implementations for skills handling (implemented in nxrp_skills.cpp)
qboolean NXRP_Skills_ListImpl( client_t *cl, const char *homepath );
qboolean NXRP_Skill_PurchaseImpl( client_t *cl, const char *homepath, const char *skillToken );
void NXRP_Skills_TestImpl(client_t* cl);
// Ensure runtime skills.json exists under fs_homepath/nxrp by copying from
// installed or repository locations when missing. Called at server startup.
void NXRP_EnsureRuntimeSkills(void);

// Repo skill accessors (hardcoded skill data provided by nxrp_skills.cpp)
void NXRP_LoadRepoSkills(void);
int NXRP_GetRepoSkillCount(void);
int NXRP_FindRepoSkillIndexById(const char *id);
const char* NXRP_GetRepoSkillIdByIndex(int idx);
const char* NXRP_GetRepoSkillNameByIndex(int idx);
const char* NXRP_GetRepoSkillDescriptionByIndex(int idx);
int NXRP_GetRepoSkillMaxLevelByIndex(int idx);
int NXRP_GetRepoSkillLevelCount(int skillIdx);
int NXRP_GetRepoSkillLevelCost(int skillIdx, int levelIdx);
int NXRP_GetRepoSkillLevelEffectsCount(int skillIdx, int levelIdx);
const char* NXRP_GetRepoSkillLevelEffectKey(int skillIdx, int levelIdx, int effectIdx);
const char* NXRP_GetRepoSkillLevelEffectValue(int skillIdx, int levelIdx, int effectIdx);

// Simple file I/O helpers for JSON reading/writing
bool NXRP_ReadFileToString(const char *path, std::string &out);
bool NXRP_WriteStringToFile(const char *path, const std::string &content);

// Generic account field accessors (read/write string values; quoting optional)
bool NXRP_GetAccountFieldFromFile(const char *filepath, const char *field, std::string &outValue);
bool NXRP_SetAccountFieldInFile(const char *filepath, const char *field, const std::string &newValue, bool quoteValue);
// Account-level helpers: read/modify numeric "level" field in an account JSON file
bool NXRP_GetAccountLevelFromFile(const char *filepath, int &outLevel);
bool NXRP_SetAccountLevelInFile(const char *filepath, int newLevel);

// Simple skill struct used by the JSON reader and skills module
struct NXRP_SimpleSkill {
	std::string id;
	std::string name;
	std::string description;
	int maxLevel = 0;
	std::vector<int> costs;
};

// Parse skills.json content into a vector of NXRP_SimpleSkill. Returns true on success.
bool NXRP_ParseSkillsJson(const std::string &content, std::vector<NXRP_SimpleSkill> &out);

// Event helpers
void NXRP_OnNPCKilled( client_t *killer, const char *npcName );
void NXRP_OnPlayerSpawned( client_t *cl );
// Forward declaration for player-killed events delivered from the game VM via trap
void NXRP_OnPlayerKilled(client_t *attacker, client_t *victim, int meansOfDeath);
// Additional NXRP event hooks
void NXRP_OnPlayerConnect(int clientNum, qboolean firstTime, qboolean isBot);
void NXRP_OnPlayerUserinfoChanged(int clientNum);
void NXRP_OnPlayerBegin(int clientNum);
void NXRP_OnPlayerDisconnect(int clientNum);
void NXRP_OnPlayerKilled(client_t *attacker, client_t *victim, int meansOfDeath);
void NXRP_OnScoreChanged(client_t *cl, int oldScore, int newScore);
void NXRP_OnNPCSpawned(const char *npcName);
void NXRP_OnMapStart(void);
void NXRP_OnRoundStart(void);
void NXRP_OnRoundEnd(int winningTeam);
void NXRP_OnRoundRestart(void);
void NXRP_OnMatchStart(void);
void NXRP_OnMatchEnd(void);
qboolean NXRP_TryMarkNPCKillSeen(int entNum);

// Round index helper (1-based within the current map). Returns 0 before
// the first round has started.
int NXRP_GetRoundNumber(void);

// Server-side NXRP helpers
void SV_NXRP_ScoreInit(void);
void SV_NXRP_ScoreFrame(void);
void SV_NXRP_NPCDeathPollFrame(void);

// FFA respawn helpers
void NXRP_FFA_Init(void);
void NXRP_FFA_SetEnabled(qboolean enable);
qboolean NXRP_FFA_IsEnabled(void);
// Frame hook to reapply timers/mode each server frame while FFA is enabled
void NXRP_FFA_Frame(void);
// FFA helpers are optional; commented out while disabled
//void NXRP_FFA_SetEnabled(qboolean enable);
//qboolean NXRP_FFA_IsEnabled(void);

// helper to spawn a misc model near a client (implemented in social.cpp)
int SV_SpawnModelAtClient(client_t* cl, const char* modelPath);

#endif // NXRP_MAIN_H
