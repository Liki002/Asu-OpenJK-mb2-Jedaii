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

typedef struct {
    int level;
    int xp;
    int playerKills;
    int npcKills;
    int duelWins;
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

void CL_XP_AddXP(int amount, const char *reason);
void CL_XP_OnPlayerKill(void);
void CL_XP_OnNPCKill(void);
void CL_XP_OnDuelWin(void);

int  CL_XP_GetLevel(void);
int  CL_XP_GetXP(void);
int  CL_XP_GetRequiredXP(int level);
void CL_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent);

const char *CL_XP_GetProfileName(void);
void CL_XP_SetProfileName(const char *name);
