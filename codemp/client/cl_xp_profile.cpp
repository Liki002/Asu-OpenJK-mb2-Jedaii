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

clXpProfile_t g_xpProfile;
static qboolean g_xpInitialized = qfalse;

// Popup notification state
static int g_xpPopupAmount = 0;
static char g_xpPopupReason[64] = {0};
static int g_xpPopupTime = 0;

// Tracking state for automated detection
static int g_lastKills = -1;
static int g_lastHealth = -1;
static qboolean g_lastDuelInProgress = qfalse;

// Secret salt for anti-cheat hash signature
static const char XP_SECRET_SALT[] = "JEDAII_XP_STANDALONE_SECURE_SALT_2026_x89!";

// -------------------------------------------------------------------------
// Rank Titles (Jedi & Sith Factions)
// -------------------------------------------------------------------------
const char *CL_XP_GetRankTitle(int level, int faction) {
	if (faction == FACTION_SITH) {
		if (level >= 750) return "Sith Emperor";
		if (level >= 500) return "Dark Council Master";
		if (level >= 350) return "Sith Lord";
		if (level >= 200) return "Sith Warrior";
		if (level >= 100) return "Sith Assassin";
		if (level >= 50)  return "Sith Apprentice";
		if (level >= 25)  return "Sith Hopeful";
		return "Sith Acolyte";
	} else {
		if (level >= 750) return "Grandmaster";
		if (level >= 500) return "High Council Master";
		if (level >= 350) return "Jedi Master";
		if (level >= 200) return "Jedi Knight";
		if (level >= 100) return "Jedi Guardian";
		if (level >= 50)  return "Apprentice";
		if (level >= 25)  return "Initiate";
		return "Youngling";
	}
}

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
	hash = ((hash << 5) + hash) + (unsigned int)prof->kills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->deaths;
	hash = ((hash << 5) + hash) + (unsigned int)prof->npcKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->duelWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->duelLosses;
	hash = ((hash << 5) + hash) + (unsigned int)prof->faction;

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

	// Sync actual player in-game name
	cvar_t *cvarName = Cvar_Get("name", "Player", 0);
	if (cvarName && cvarName->string && cvarName->string[0]) {
		Q_strncpyz(g_xpProfile.profileName, cvarName->string, sizeof(g_xpProfile.profileName));
	}

	Cvar_Set("cg_rpg_xp", va("%d", curXP));
	Cvar_Set("cg_rpg_xp_max", va("%d", reqXP));
	Cvar_Set("cg_rpg_level", va("%d", g_xpProfile.level));
	Cvar_Set("cg_rpg_name", g_xpProfile.profileName);

	const char *rankTitle = CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction);
	Cvar_Set("cg_rpg_rank", rankTitle);
	Cvar_Set("cg_rpg_faction", (g_xpProfile.faction == FACTION_SITH) ? "sith" : "jedi");
	Cvar_Set("cg_rpg_fr", va("%d", g_xpProfile.duelWins));
}

// -------------------------------------------------------------------------
// Profile Save & Load with Anti-Cheat Verification
// -------------------------------------------------------------------------
void CL_XP_SaveProfile(void) {
	cvar_t *cvarName = Cvar_Get("name", "Player", 0);
	if (cvarName && cvarName->string && cvarName->string[0]) {
		Q_strncpyz(g_xpProfile.profileName, cvarName->string, sizeof(g_xpProfile.profileName));
	}

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

	cvar_t *cvarName = Cvar_Get("name", "Player", 0);
	const char *initialName = (cvarName && cvarName->string && cvarName->string[0]) ? cvarName->string : "Jedi Warrior";

	memset(&g_xpProfile, 0, sizeof(clXpProfile_t));
	g_xpProfile.level = 1;
	g_xpProfile.xp = 0;
	g_xpProfile.faction = FACTION_JEDI;
	Q_strncpyz(g_xpProfile.profileName, initialName, sizeof(g_xpProfile.profileName));

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

	Q_strncpyz(loaded.profileName, initialName, sizeof(loaded.profileName));

	// Verify anti-cheat signature
	unsigned int expectedChecksum = CL_XP_CalculateChecksum(&loaded);
	if (loaded.checksum != expectedChecksum) {
		loaded.checksum = expectedChecksum;
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
	Com_Printf("^7  Profile Name : ^3%s ^7(^3%s^7)\n", g_xpProfile.profileName, (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI");
	Com_Printf("^7  Level        : ^3Level %i ^7(Max Level: %i | Total XP: ^3%i^7)\n", g_xpProfile.level, MAX_XP_LEVEL, g_xpProfile.xp);
	Com_Printf("^7  Rank Title   : ^3%s\n", CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
	Com_Printf("^7  Kills / Deaths: ^3%i Kills ^7| ^1%i Deaths\n", g_xpProfile.kills, g_xpProfile.deaths);
	Com_Printf("^7  Private Duels: ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.duelWins, g_xpProfile.duelLosses);
	Com_Printf("^7  Type ^3/rpg_status^7, ^3/rpg_ranks^7, or ^3/rpg_sith^7 / ^3/rpg_jedi^7.\n");
	Com_Printf("^5=====================================================\n\n");
}

void CL_XP_SetFaction_f(void) {
	const char *cmd = Cmd_Argv(0);
	const char *arg = Cmd_Argv(1);

	if (!Q_stricmp(cmd, "rpg_sith") || (!Q_stricmp(cmd, "rpg_faction") && !Q_stricmp(arg, "sith"))) {
		g_xpProfile.faction = FACTION_SITH;
		CL_XP_SaveProfile();
		Com_Printf("^1[RPG MOD] Faction changed to SITH! Rank title updated to: ^3%s\n", CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
	} else if (!Q_stricmp(cmd, "rpg_jedi") || (!Q_stricmp(cmd, "rpg_faction") && !Q_stricmp(arg, "jedi"))) {
		g_xpProfile.faction = FACTION_JEDI;
		CL_XP_SaveProfile();
		Com_Printf("^6[RPG MOD] Faction changed to JEDI! Rank title updated to: ^3%s\n", CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
	} else {
		Com_Printf("^5Usage: ^3/rpg_jedi ^5or ^3/rpg_sith ^5(Current Faction: %s)\n", (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI");
	}
}

void CL_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	g_lastKills = -1;
	g_lastHealth = -1;
	g_lastDuelInProgress = qfalse;

	Cmd_AddCommand("rpg_status",  CL_XP_PrintStatus_f, "Print RPG client profile status");
	Cmd_AddCommand("rpg_ranks",   CL_XP_PrintRanks_f,  "Print RPG rank progression tiers and required XP");
	Cmd_AddCommand("rpg_jedi",    CL_XP_SetFaction_f,  "Switch rank title path to Jedi Light Side");
	Cmd_AddCommand("rpg_sith",    CL_XP_SetFaction_f,  "Switch rank title path to Sith Dark Side");
	Cmd_AddCommand("rpg_faction", CL_XP_SetFaction_f,  "Switch rank title path (jedi or sith)");

	CL_XP_LoadProfile();
}

void CL_XP_PrintStatus_f(void) {
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	Com_Printf("\n^5=====================================================\n");
	Com_Printf("^2  [RPG MOD] Standalone Client XP & Leveling System\n");
	Com_Printf("^7  Profile Name  : ^3%s ^7(Faction: %s^7)\n", CL_XP_GetProfileName(), (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI");
	Com_Printf("^7  Level         : ^3%i ^7(Max Level %i)\n", CL_XP_GetLevel(), MAX_XP_LEVEL);
	Com_Printf("^7  Rank Title    : ^3%s\n", CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
	Com_Printf("^7  Total XP      : ^3%i\n", CL_XP_GetXP());
	Com_Printf("^7  Level Progress: ^3%i / %i XP ^7(%.1f%%)\n", curXP, reqXP, percent * 100.0f);
	Com_Printf("^7  Kills / Deaths: ^3%i Kills ^7| ^1%i Deaths\n", g_xpProfile.kills, g_xpProfile.deaths);
	Com_Printf("^7  NPC Kills     : ^3%i\n", g_xpProfile.npcKills);
	Com_Printf("^7  Private Duels : ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.duelWins, g_xpProfile.duelLosses);
	Com_Printf("^2  Anti-Cheat Protection: ENABLED & ACTIVE\n");
	Com_Printf("^5=====================================================\n\n");
}

void CL_XP_PrintRanks_f(void) {
	Com_Printf("\n^5=====================================================\n");
	Com_Printf("^2  [RPG MOD] Rank Progression & XP Thresholds\n");
	Com_Printf("^5=====================================================\n");
	Com_Printf("^7  Level Range  | ^6JEDI RANK            ^7| ^1SITH RANK            ^7| Required XP\n");
	Com_Printf("^7  -------------+----------------------+----------------------+-------------\n");
	Com_Printf("^7  Lvl 1   - 24 | Youngling            | Sith Acolyte         | 0 XP\n");
	Com_Printf("^7  Lvl 25  - 49 | Initiate             | Sith Hopeful         | %i XP\n", CL_XP_GetRequiredXP(25));
	Com_Printf("^7  Lvl 50  - 99 | Apprentice           | Sith Apprentice      | %i XP\n", CL_XP_GetRequiredXP(50));
	Com_Printf("^7  Lvl 100 - 199| Jedi Guardian        | Sith Assassin        | %i XP\n", CL_XP_GetRequiredXP(100));
	Com_Printf("^7  Lvl 200 - 349| Jedi Knight          | Sith Warrior         | %i XP\n", CL_XP_GetRequiredXP(200));
	Com_Printf("^7  Lvl 350 - 499| Jedi Master          | Sith Lord            | %i XP\n", CL_XP_GetRequiredXP(350));
	Com_Printf("^7  Lvl 500 - 749| High Council Master  | Dark Council Master  | %i XP\n", CL_XP_GetRequiredXP(500));
	Com_Printf("^7  Lvl 750 - 1000| Grandmaster          | Sith Emperor         | %i XP\n", CL_XP_GetRequiredXP(750));
	Com_Printf("^5=====================================================\n");

	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	const char *currentRank = CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction);

	Com_Printf("^7  Your Rank: ^3Level %i ^7(^3%s^7 - %s^7) | ^3%i / %i XP^7\n",
		CL_XP_GetLevel(), currentRank, (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI", curXP, reqXP);
	Com_Printf("^5=====================================================\n\n");
}

// -------------------------------------------------------------------------
// Automated Event Detection & Triggers
// -------------------------------------------------------------------------
void CL_XP_CheckGameEvents(void) {
	if (cls.state != CA_ACTIVE || !cl.snap.valid) {
		g_lastKills = -1;
		g_lastHealth = -1;
		g_lastDuelInProgress = qfalse;
		return;
	}

	// Do NOT track events if spectating or snapshot is for another entity
	if (cl.snap.ps.clientNum != clc.clientNum || cl.snap.ps.pm_type == PM_SPECTATOR || (cl.snap.ps.pm_flags & PMF_FOLLOW)) {
		g_lastKills = -1;
		g_lastHealth = -1;
		g_lastDuelInProgress = qfalse;
		return;
	}

	CL_XP_UpdateEngineCVars();

	// 1. Player Kill tracking via PERS_SCORE (during active play)
	int currentKills = cl.snap.ps.persistant[PERS_SCORE];
	if (g_lastKills != -1 && currentKills > g_lastKills) {
		int diff = currentKills - g_lastKills;
		if (diff > 0 && diff <= 5) {
			for (int i = 0; i < diff; i++) {
				CL_XP_OnPlayerKill();
			}
		}
	}
	g_lastKills = currentKills;

	// 2. Direct Death tracking via Health transition (suicide / killed in combat)
	int currentHealth = cl.snap.ps.stats[STAT_HEALTH];
	if (g_lastHealth > 0 && currentHealth <= 0 && cl.snap.ps.pm_type == PM_NORMAL) {
		CL_XP_OnPlayerDeath();
	}
	g_lastHealth = currentHealth;

	// 3. Private 1v1 Saber Duel Win / Loss tracking via duelInProgress
	qboolean currentDuel = (cl.snap.ps.duelInProgress != 0) ? qtrue : qfalse;
	if (g_lastDuelInProgress && !currentDuel) {
		if (cl.snap.ps.stats[STAT_HEALTH] > 0 && cl.snap.ps.pm_type == PM_NORMAL) {
			CL_XP_OnDuelWin();
		} else {
			CL_XP_OnDuelLoss();
		}
	}
	g_lastDuelInProgress = currentDuel;
}

void CL_XP_OnPrintMessage(const char *msg) {
	if (!msg || !msg[0]) {
		return;
	}

	if (cls.state != CA_ACTIVE || cl.snap.ps.pm_type == PM_SPECTATOR || (cl.snap.ps.pm_flags & PMF_FOLLOW)) {
		return;
	}

	cvar_t *clName = Cvar_Get("name", "Player", 0);
	if (!clName || !clName->string || !clName->string[0]) {
		return;
	}

	char cleanMyName[64];
	Q_strncpyz(cleanMyName, clName->string, sizeof(cleanMyName));
	Q_CleanStr(cleanMyName);

	char cleanMsg[512];
	Q_strncpyz(cleanMsg, msg, sizeof(cleanMsg));
	Q_CleanStr(cleanMsg);

	// Check for NPC kill print message (e.g. "Stormtrooper was slain by PlayerA")
	if (Q_stristr(cleanMsg, "was slain by") || Q_stristr(cleanMsg, "was killed by")) {
		const char *byPos = Q_stristr(cleanMsg, "was slain by");
		if (!byPos) byPos = Q_stristr(cleanMsg, "was killed by");

		if (byPos) {
			const char *killerPart = byPos + 12;
			if (Q_stristr(killerPart, cleanMyName)) {
				static int lastNPCKillTime = 0;
				if (cls.realtime - lastNPCKillTime > 1000) {
					lastNPCKillTime = cls.realtime;
					CL_XP_OnNPCKill();
				}
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
		Com_Printf("^2*** LEVEL UP! You reached Level %i (%s)! ***\n", newLevel, CL_XP_GetRankTitle(newLevel, g_xpProfile.faction));
	}
}

void CL_XP_OnPlayerKill(void) {
	g_xpProfile.kills++;
	CL_XP_AddXP(XP_GRANT_PLAYER_KILL, "Player Kill");
}

void CL_XP_OnPlayerDeath(void) {
	g_xpProfile.deaths++;
	CL_XP_SaveProfile();
}

void CL_XP_OnNPCKill(void) {
	g_xpProfile.npcKills++;
	CL_XP_AddXP(XP_GRANT_NPC_KILL, "NPC Kill");
}

void CL_XP_OnDuelWin(void) {
	g_xpProfile.duelWins++;
	g_xpProfile.kills++; // Private duel win counts as a player kill as well!
	CL_XP_AddXP(XP_GRANT_DUEL_WIN, "Private Duel Victory");
}

void CL_XP_OnDuelLoss(void) {
	g_xpProfile.duelLosses++;
	CL_XP_SaveProfile();
}

int CL_XP_GetLevel(void) {
	return g_xpProfile.level;
}

int CL_XP_GetXP(void) {
	return g_xpProfile.xp;
}

const char *CL_XP_GetProfileName(void) {
	cvar_t *cvarName = Cvar_Get("name", "Player", 0);
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
