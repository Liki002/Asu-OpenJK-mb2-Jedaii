/*
===========================================================================
Copyright (C) 2026 AsuTech / OpenJK Contributors
Standalone Engine-Level Client XP & Leveling System for MBII / OpenJK Executable
===========================================================================
*/

#include "cl_xp_profile.h"
#include "client.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static clXpProfile_t g_xpProfile;
static qboolean g_xpInitialized = qfalse;

// Popup notification state
static int g_xpPopupAmount = 0;
static char g_xpPopupReason[64] = {0};
static int g_xpPopupTime = 0;

// Tracking state for automated detection
static int g_lastKills = -1;
static qboolean g_lastDuelInProgress = qfalse;

// Secret salt for anti-cheat hash signature
static const char XP_SECRET_SALT[] = "JEDAII_XP_STANDALONE_SECURE_SALT_2026_x89!";

// -------------------------------------------------------------------------
// Anti-Cheat Signature Calculation
// -------------------------------------------------------------------------
static unsigned int CL_XP_CalculateChecksum(const clXpProfile_t *prof) {
	unsigned int hash = 5381;
	const char *p;

	for (p = XP_SECRET_SALT; *p; p++) {
		hash = ((hash << 5) + hash) + (unsigned char)(*p);
	}

	hash = ((hash << 5) + hash) + (unsigned int)prof->level;
	hash = ((hash << 5) + hash) + (unsigned int)prof->xp;
	hash = ((hash << 5) + hash) + (unsigned int)prof->playerKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->npcKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->duelWins;

	for (p = prof->profileName; *p; p++) {
		hash = ((hash << 5) + hash) + (unsigned char)(*p);
	}

	return hash;
}

// -------------------------------------------------------------------------
// Required XP Calculation
// -------------------------------------------------------------------------
int CL_XP_GetRequiredXP(int level) {
	int l;
	int total = 0;

	if (level <= 1) {
		return 0;
	}
	if (level > MAX_XP_LEVEL) {
		level = MAX_XP_LEVEL;
	}

	for (l = 1; l < level; l++) {
		total += 100 + (l - 1) * 50;
	}
	return total;
}

static int CL_XP_CalculateLevelFromXP(int totalXP) {
	int lvl;

	if (totalXP <= 0) {
		return 1;
	}

	for (lvl = 1; lvl < MAX_XP_LEVEL; lvl++) {
		if (totalXP < CL_XP_GetRequiredXP(lvl + 1)) {
			return lvl;
		}
	}
	return MAX_XP_LEVEL;
}

void CL_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent) {
	int curLvl = CL_XP_GetLevel();
	int baseXP = CL_XP_GetRequiredXP(curLvl);
	int targetXP = CL_XP_GetRequiredXP(curLvl + 1);

	if (curLvl >= MAX_XP_LEVEL) {
		if (currentLevelXP) *currentLevelXP = 1;
		if (nextLevelXP) *nextLevelXP = 1;
		if (percent) *percent = 1.0f;
		return;
	}

	int curProgress = g_xpProfile.xp - baseXP;
	int reqProgress = targetXP - baseXP;

	if (curProgress < 0) curProgress = 0;
	if (reqProgress <= 0) reqProgress = 1;

	if (currentLevelXP) *currentLevelXP = curProgress;
	if (nextLevelXP) *nextLevelXP = reqProgress;
	if (percent) {
		*percent = (float)curProgress / (float)reqProgress;
		if (*percent > 1.0f) *percent = 1.0f;
		if (*percent < 0.0f) *percent = 0.0f;
	}
}

static void CL_XP_UpdateEngineCVars(void) {
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	Cvar_Set("cg_rpg_xp", va("%d", curXP));
	Cvar_Set("cg_rpg_xp_max", va("%d", reqXP));
	Cvar_Set("cg_rpg_level", va("%d", g_xpProfile.level));
	Cvar_Set("cg_rpg_name", g_xpProfile.profileName);

	// Rank title based on level
	const char *rankTitle = "Padawan";
	if (g_xpProfile.level >= 75) rankTitle = "Grandmaster";
	else if (g_xpProfile.level >= 50) rankTitle = "Jedi Master";
	else if (g_xpProfile.level >= 25) rankTitle = "Jedi Knight";
	else if (g_xpProfile.level >= 10) rankTitle = "Apprentice";
	Cvar_Set("cg_rpg_rank", rankTitle);
}

// -------------------------------------------------------------------------
// Profile Save & Load with Anti-Cheat Verification
// -------------------------------------------------------------------------
void CL_XP_SaveProfile(void) {
	g_xpProfile.checksum = CL_XP_CalculateChecksum(&g_xpProfile);

	FILE *f = fopen("xp_profile.dat", "wb");
	if (!f) {
		Com_Printf("^1[RPG MOD] Error: Failed to open xp_profile.dat for writing!\n");
		return;
	}

	fwrite(&g_xpProfile, sizeof(clXpProfile_t), 1, f);
	fclose(f);

	CL_XP_UpdateEngineCVars();
}

void CL_XP_LoadProfile(void) {
	clXpProfile_t loaded;

	memset(&g_xpProfile, 0, sizeof(clXpProfile_t));
	g_xpProfile.level = 1;
	g_xpProfile.xp = 0;
	Q_strncpyz(g_xpProfile.profileName, "Jedi Warrior", sizeof(g_xpProfile.profileName));

	FILE *f = fopen("xp_profile.dat", "rb");
	if (!f) {
		Com_Printf("^5[RPG MOD] Creating new client XP profile...\n");
		CL_XP_SaveProfile();
		return;
	}

	size_t readCount = fread(&loaded, sizeof(clXpProfile_t), 1, f);
	fclose(f);

	if (readCount < 1) {
		Com_Printf("^3[RPG MOD] Warning: Corrupted profile file. Resetting profile.\n");
		CL_XP_SaveProfile();
		return;
	}

	// Verify anti-cheat signature
	unsigned int expectedChecksum = CL_XP_CalculateChecksum(&loaded);
	if (loaded.checksum != expectedChecksum) {
		Com_Printf("^1[RPG MOD] ANTI-CHEAT WARNING: Tampered XP profile detected! Resetting progress to Level 1.\n");
		CL_XP_SaveProfile();
		return;
	}

	// Verify hardcoded level integrity matches XP
	int expectedLevel = CL_XP_CalculateLevelFromXP(loaded.xp);
	if (loaded.level != expectedLevel) {
		Com_Printf("^1[RPG MOD] ANTI-CHEAT WARNING: Level mismatch detected! Correcting level to %i.\n", expectedLevel);
		loaded.level = expectedLevel;
	}

	g_xpProfile = loaded;

	CL_XP_UpdateEngineCVars();

	// Print prominent console banner
	Com_Printf("\n");
	Com_Printf("^5=====================================================\n");
	Com_Printf("^2  [RPG MOD] Standalone Client XP System LOADED!\n");
	Com_Printf("^7  Profile Name : ^3%s\n", g_xpProfile.profileName);
	Com_Printf("^7  Level        : ^3Level %i ^7(Total XP: ^3%i^7)\n", g_xpProfile.level, g_xpProfile.xp);
	Com_Printf("^7  Duels Won    : ^3%i ^7| Player Kills: ^3%i ^7| NPC Kills: ^3%i\n", g_xpProfile.duelWins, g_xpProfile.playerKills, g_xpProfile.npcKills);
	Com_Printf("^7  Type ^3/rpg_status^7 in console for full stats.\n");
	Com_Printf("^5=====================================================\n\n");
}

void CL_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	g_lastKills = -1;
	g_lastDuelInProgress = qfalse;

	Cmd_AddCommand("rpg_status", CL_XP_PrintStatus_f, "Print RPG client profile status");

	CL_XP_LoadProfile();
}

void CL_XP_PrintStatus_f(void) {
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	Com_Printf("\n^5=====================================================\n");
	Com_Printf("^2  [RPG MOD] Standalone Client XP & Leveling System\n");
	Com_Printf("^7  Profile Name  : ^3%s\n", CL_XP_GetProfileName());
	Com_Printf("^7  Level         : ^3%i ^7(Max %i)\n", CL_XP_GetLevel(), MAX_XP_LEVEL);
	Com_Printf("^7  Total XP      : ^3%i\n", CL_XP_GetXP());
	Com_Printf("^7  Level Progress: ^3%i / %i XP ^7(%.1f%%)\n", curXP, reqXP, percent * 100.0f);
	Com_Printf("^7  Player Kills  : ^3%i\n", g_xpProfile.playerKills);
	Com_Printf("^7  NPC Kills     : ^3%i\n", g_xpProfile.npcKills);
	Com_Printf("^7  Duels Won     : ^3%i\n", g_xpProfile.duelWins);
	Com_Printf("^2  Anti-Cheat Protection: ENABLED & ACTIVE\n");
	Com_Printf("^5=====================================================\n\n");
}

// -------------------------------------------------------------------------
// Automated Event Detection & Triggers
// -------------------------------------------------------------------------
void CL_XP_CheckGameEvents(void) {
	if (cl.snap.ping == 0 && cls.state != CA_ACTIVE) {
		g_lastKills = -1;
		g_lastDuelInProgress = qfalse;
		return;
	}

	// 1. Automatic Player Kill tracking via PERS_SCORE
	int currentKills = cl.snap.ps.persistant[PERS_SCORE];
	if (g_lastKills != -1 && currentKills > g_lastKills) {
		int diff = currentKills - g_lastKills;
		for (int i = 0; i < diff; i++) {
			CL_XP_OnPlayerKill();
		}
	}
	g_lastKills = currentKills;

	// 2. Automatic Duel Victory tracking via duelInProgress
	qboolean currentDuel = (cl.snap.ps.duelInProgress != 0) ? qtrue : qfalse;
	if (g_lastDuelInProgress && !currentDuel) {
		if (cl.snap.ps.stats[STAT_HEALTH] > 0) {
			CL_XP_OnDuelWin();
		}
	}
	g_lastDuelInProgress = currentDuel;
}

void CL_XP_OnPrintMessage(const char *msg) {
	if (!msg || !msg[0]) {
		return;
	}

	cvar_t *clName = Cvar_Get("name", "Player", 0);
	const char *myName = (clName && clName->string[0]) ? clName->string : NULL;

	if (Q_stristr((char*)msg, "won the duel") || Q_stristr((char*)msg, "wins the duel") || Q_stristr((char*)msg, "won duel")) {
		if (!myName || Q_stristr((char*)msg, myName)) {
			static int lastDuelMsgTime = 0;
			if (cls.realtime - lastDuelMsgTime > 2000) {
				lastDuelMsgTime = cls.realtime;
				CL_XP_OnDuelWin();
			}
		}
	}

	if (Q_stristr((char*)msg, "was slain by") || Q_stristr((char*)msg, "was killed by") || Q_stristr((char*)msg, "slain by")) {
		if (myName && Q_stristr((char*)msg, myName)) {
			static int lastNPCKillTime = 0;
			if (cls.realtime - lastNPCKillTime > 500) {
				lastNPCKillTime = cls.realtime;
				CL_XP_OnNPCKill();
			}
		}
	}
}

// -------------------------------------------------------------------------
// XP Actions & Level Up Handling
// -------------------------------------------------------------------------
void CL_XP_AddXP(int amount, const char *reason) {
	if (amount <= 0) {
		return;
	}

	int oldLevel = g_xpProfile.level;
	g_xpProfile.xp += amount;

	int newLevel = CL_XP_CalculateLevelFromXP(g_xpProfile.xp);
	g_xpProfile.level = newLevel;

	CL_XP_SaveProfile();

	if (newLevel > oldLevel) {
		Com_Printf("^2*** LEVEL UP! You reached Level %i! ***\n", newLevel);
	}
}

void CL_XP_OnPlayerKill(void) {
	g_xpProfile.playerKills++;
	CL_XP_AddXP(XP_GRANT_PLAYER_KILL, "Player Kill");
}

void CL_XP_OnNPCKill(void) {
	g_xpProfile.npcKills++;
	CL_XP_AddXP(XP_GRANT_NPC_KILL, "NPC Kill");
}

void CL_XP_OnDuelWin(void) {
	g_xpProfile.duelWins++;
	CL_XP_AddXP(XP_GRANT_DUEL_WIN, "Duel Victory");
}

int CL_XP_GetLevel(void) {
	return g_xpProfile.level;
}

int CL_XP_GetXP(void) {
	return g_xpProfile.xp;
}

const char *CL_XP_GetProfileName(void) {
	cvar_t *cvarName = Cvar_Get("cg_rpg_name", "", 0);
	if (cvarName && cvarName->string && cvarName->string[0]) {
		return cvarName->string;
	}
	return g_xpProfile.profileName;
}

void CL_XP_SetProfileName(const char *name) {
	if (name && name[0]) {
		Q_strncpyz(g_xpProfile.profileName, name, sizeof(g_xpProfile.profileName));
		CL_XP_SaveProfile();
	}
}
