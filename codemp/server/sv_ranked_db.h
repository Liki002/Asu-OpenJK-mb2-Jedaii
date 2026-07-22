#ifndef SV_RANKED_DB_H
#define SV_RANKED_DB_H

#include "qcommon/q_shared.h"
#include "server.h"

// Define a maximum string length for user names and IPs, passwords etc.
#define MAX_AUTH_STRING 64

qboolean Ranked_IsValidAuthString(const char *s);
void Ranked_SafeUserKey(const char *username, char *out, int outSize);

// Struct for keeping track of what a player has done in the current
// round/match. This matches the volatile data in gameEventHandler.ts
typedef struct {
  int roundKills;
  int roundDeaths;
  int roundObjective;
  int latestTeamId; // To identify if they are Red or Blue for match win
  int killStreak;
  int bountyValue;
  int multiKillCount;
  int bombStreak;
  int lastKillTime;   // milliseconds
  int lastActiveTime; // milliseconds

  // Track kills against specific players for domination (1 player -> count)
  // To keep it simple in C++, we just use an array matching MAX_CLIENTS
  int killsOnPlayers[64];

  qboolean loggedIn;
  qboolean isTemp;   // If qtrue, player has not authenticated — stats are in-memory only, not persisted
  qboolean inDuel;
  qboolean isFrozen; // Admin Force Freeze active
  vec3_t frozenOrigin; // Position where player was frozen
  int burnExpireTime; // Timestamp when burn effect ends
  int burnNextDamageTime; // Timestamp for next periodic 5 HP burn damage
  float speedMultiplier; // Movement speed multiplier (e.g. 2.0 = 2x speed)
  int grantedWeaponsMask; // Bitmask of weapons granted during session/round
  int lastGrantedWeapon;  // ID of last granted weapon to enforce active weapon holding
  qboolean godForce;      // Infinite force energy
  int grantedForcePowersMask; // Bitmask of force powers granted
  int grantedForceLevels[18]; // Level per force power (1-3)
  int remainingLives;     // Custom lives count (-1 = off, >=0 = limited lives)
  qboolean livesActive;   // Custom lives system active for player
  qboolean wasDeadLastFrame; // Helper for death detection
  int duelOpponent;
  int duelStartTime; // ms
  
  // Economy / betting state
  int currentBetTarget;
  int currentBetAmount;

  int lastRollTime;   // Last time !roll was used
  int jailExpireTime; // Engine time when probation/jail expires

  int activeXpBoost;    // +50% XP this round
  int activeCrBoost;    // +50% credits this round
  int activeLuckyCharm; // +10% roll luck this round
  int activeEloBoost;   // +15% FR gain for next duel win

  // Adventure system (per-player)
  int adventureNodeIdx;     // -1 = not on an adventure, else index into sv_rankedAdventureNodes[]
  int adventureCooldownEnd; // svs.time when player can start another adventure

  int tempElo;                    // In-memory session ELO (starts at 1000 for guests, synced on login for registered)
  char username[MAX_AUTH_STRING]; // Used for saving back to DB
} rankedMatchState_t;

// Externally accessible match state array (parallel to svs.clients)
extern rankedMatchState_t sv_rankedPlayers[64];

// Core Database Methods
struct cJSON;
struct cJSON *SV_Ranked_GetAccount(const char *username);
struct cJSON *SV_Ranked_GetAccountByGUID(const char *guid);
void SV_Ranked_GenerateRandomUsername(char *out, int size);
void SV_Ranked_GenerateRandomPassword(char *out, int size);
const char *SV_Ranked_GetActiveMode(void);
void SV_Ranked_Init(void);
void SV_Ranked_Shutdown(void);
void SV_Ranked_LoadAccounts(void);
void SV_Ranked_SaveAccounts(void);
void SV_Ranked_Log(const char *fmt, ...);

// Configuration System
void SV_Ranked_LoadConfig(void);
void SV_Ranked_SaveConfig(void);
struct cJSON* SV_Ranked_GetSetting(const char *key);

// User Authentication
void SV_Ranked_LoginOrRegister(client_t *cl, const char *username,
                               const char *password);
void SV_Ranked_Logout(client_t *cl);

// Economy and Progression Additions
void UpdateAccountCredits(const char *username, int credDelta);
int SV_Ranked_CalculateLevel(int xp);
const char* SV_Ranked_GetTitle(int fr, struct cJSON *acc = 0);

// Match Updates
void SV_Ranked_ClientConnect(client_t *cl);
void SV_Ranked_ClientDisconnect(client_t *cl);
void SV_Ranked_AutoRegisterByGUID(client_t *cl);

// Trivia
void SV_Ranked_Trivia_HandleAnswer(client_t *cl, const char *message);

// Adventure
void SV_Ranked_Adventure_Start(client_t *cl);
void SV_Ranked_Adventure_Choose(client_t *cl, int choiceIndex);

// Simple stat display
void SV_Ranked_ShowStats(client_t *cl);
void SV_Ranked_ShowStatsTarget(client_t *cl, const char *targetName);
void SV_Ranked_ShowTop(client_t *cl);
void SV_Ranked_ShowRank(client_t *cl);
void SV_Ranked_ShowRankThresholds(client_t *cl);
void SV_Ranked_ShowTopCredits(client_t *cl);
void SV_Ranked_ShowTopPotato(client_t *cl);
void SV_Ranked_ShowQuests(client_t *cl);
void SV_Ranked_ShowAchievements(client_t *cl);
void SV_Ranked_SetBounty(client_t *cl, const char *targetName, int amount);
void SV_Ranked_ShowBountyList(client_t *cl);
void SV_Ranked_ShowCredits(client_t *cl);
void SV_Ranked_ShowShop(client_t *cl);
void SV_Ranked_ShopBuy(client_t *cl, const char *itemName);
void SV_Ranked_ShopSell(client_t *cl, const char *itemName);
void SV_Ranked_ShopUse(client_t *cl, const char *itemName);
void SV_Ranked_Cmd_SetWinMsg(client_t *cl, const char *msg);
void SV_Ranked_Cmd_SetWinSnd(client_t *cl, const char *snd);
void SV_Ranked_Cmd_AdminGiveItem(client_t *cl, const char *target, const char *itemKey, int amount);
qboolean SV_Ranked_IsAdmin(client_t *cl);
qboolean SV_Ranked_IsHighAdmin(client_t *cl);
void SV_Ranked_Cmd_AdminGiveGun(client_t *cl, const char *target, const char *gunName);
void SV_Ranked_Cmd_AdminGiveAll(client_t *cl, const char *targetName);
void SV_Ranked_Cmd_AdminFreeze(client_t *cl, const char *target);
void SV_Ranked_Cmd_AdminUnfreeze(client_t *cl, const char *target);
void SV_Ranked_Cmd_Bring(client_t *cl, const char *target);
void SV_Ranked_Cmd_Goto(client_t *cl, const char *target);
void SV_Ranked_Cmd_Burn(client_t *cl, const char *target);
void SV_Ranked_Cmd_Speed(client_t *cl, const char *target, float multiplier);

// Daily Quest System
void SV_Ranked_CheckAndRefreshDailyQuests(const char *username, client_t *cl);
void SV_Ranked_ProgressQuest(const char *username, const char *statKey, int amount, client_t *cl);
void SV_Ranked_ShowQuests(client_t *cl);

// Weapon Name Mapping
const char *SV_Ranked_GetWeaponName(const char *raw);

// Achievement System
void SV_Ranked_GrantAchievement(const char *username, const char *achId, client_t *cl);
void SV_Ranked_CheckKillAchievements(const char *username, int totalKills, int streak, int meleeKills, int bombKills, int dominations, client_t *cl);
void SV_Ranked_CheckDuelAchievements(const char *username, int duelWins, int elo, client_t *cl);
void SV_Ranked_CheckEconomyAchievements(const char *username, client_t *cl);
void SV_Ranked_CheckLevelAchievements(const char *username, int newLevel, client_t *cl);

// Rival System
void SV_Ranked_TrackRival(const char *username, const char *opponentGuid, const char *opponentName);
void SV_Ranked_RecordRecentDuel(const char *username, const char *opponentName, qboolean isWin, int eloChange);

// Shop Pricing overrides
int SV_Ranked_GetShopPrice(const char *itemKey, int defaultPrice);
int SV_Ranked_GetShopSellBack(const char *itemKey, int defaultSellBack);

#endif // SV_RANKED_DB_H
