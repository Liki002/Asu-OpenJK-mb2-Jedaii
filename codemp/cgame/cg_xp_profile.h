/*
===========================================================================
Copyright (C) 2026 AsuTech / OpenJK Contributors
Standalone Client-Side XP & Leveling System with Anti-Cheat Tamper Protection
===========================================================================
*/

#pragma once

#include "cg_local.h"

#define MAX_XP_LEVEL 100

// Fixed XP Grants (Non-cheatable, hardcoded)
#define XP_GRANT_PLAYER_KILL 50
#define XP_GRANT_NPC_KILL    20
#define XP_GRANT_DUEL_WIN    100

typedef struct {
    int level;
    int xp;
    int playerKills;
    int npcKills;
    int duelWins;
    char profileName[64];
    unsigned int checksum;
} xpProfile_t;

// API functions
void CG_XP_Init(void);
void CG_XP_SaveProfile(void);
void CG_XP_LoadProfile(void);

void CG_XP_CheckGameEvents(void);
void CG_XP_OnPrintMessage(const char *msg);

void CG_XP_AddXP(int amount, const char *reason);
void CG_XP_OnPlayerKill(void);
void CG_XP_OnNPCKill(void);
void CG_XP_OnDuelWin(void);

int  CG_XP_GetLevel(void);
int  CG_XP_GetXP(void);
int  CG_XP_GetRequiredXP(int level);
void CG_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent);

const char *CG_XP_GetProfileName(void);
void CG_XP_SetProfileName(const char *name);

// HUD Rendering
void CG_XP_DrawHUD(void);
