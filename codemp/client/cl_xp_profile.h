/*
===========================================================================
Copyright (C) 2026 AsuTech / OpenJK Contributors
Standalone Engine-Level Client XP & Leveling System for MBII / OpenJK Executable
===========================================================================
*/

#pragma once

#include "client.h"

#define MAX_XP_LEVEL 1000

// Fixed XP Grants (Non-cheatable, hardcoded)
#define XP_GRANT_PLAYER_KILL 50
#define XP_GRANT_NPC_KILL    20
#define XP_GRANT_DUEL_WIN    100
#define XP_GRANT_ROUND_WIN   150
#define XP_GRANT_FLAWLESS    50
#define XP_GRANT_ASSIST      25

// Faction types
typedef enum {
    FACTION_JEDI = 0,
    FACTION_SITH = 1
} rpgFaction_t;

typedef struct {
    int level;
    int xp;
    int kills;       // Player Kills (+50 XP)
    int deaths;      // Player Deaths
    int npcKills;    // NPC Kills (+20 XP)
    int duelWins;    // Private Duel Wins (+100 XP)
    int duelLosses;  // Private Duel Losses
    int faction;     // 0 = Jedi, 1 = Sith

    // MB2 Mode Stats
    int openWins;      // Open Mode Wins (+150 XP)
    int openLosses;    // Open Mode Losses
    int legendsWins;   // Legends Mode Wins (+150 XP)
    int legendsLosses; // Legends Mode Losses
    int faWins;        // Full Authentic Wins (+150 XP)
    int faLosses;      // Full Authentic Losses

    // Weapon/Style Breakdown
    int saberKills;    // Saber Kills
    int gunnerKills;   // Blaster / Gun Kills
    int flawlessWins;  // Flawless Duel Wins (no damage taken)

    char profileName[64];
    unsigned int checksum;
} clXpProfile_t;

// API functions for engine client
void CL_XP_Init(void);
void CL_XP_SaveProfile(void);
void CL_XP_LoadProfile(void);

void CL_XP_CheckGameEvents(void);
void CL_XP_OnPrintMessage(const char *msg);
void CL_XP_PrintStatus_f(void);
void CL_XP_PrintRanks_f(void);
void CL_XP_SetFaction_f(void);
void CL_XP_ToggleCard_f(void);

void CL_XP_AddXP(int amount, const char *reason);
void CL_XP_OnPlayerKill(int weapon);
void CL_XP_OnPlayerDeath(void);
void CL_XP_OnNPCKill(void);
void CL_XP_OnDuelWin(qboolean flawless);
void CL_XP_OnDuelLoss(void);
void CL_XP_OnRoundWin(int mbmode);
void CL_XP_OnRoundLoss(int mbmode);

int  CL_XP_GetLevel(void);
int  CL_XP_GetXP(void);
int  CL_XP_GetRequiredXP(int level);
void CL_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent);

const char *CL_XP_GetProfileName(void);
void CL_XP_SetProfileName(const char *name);
const char *CL_XP_GetRankTitle(int level, int faction);

extern clXpProfile_t g_xpProfile;
extern qboolean g_xpDrawCard;
extern int g_roundWon;
