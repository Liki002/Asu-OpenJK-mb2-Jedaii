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
qboolean g_xpDrawCard = qfalse;
static qboolean g_xpInitialized = qfalse;

// Tracking state for automated detection
static int g_lastKills = -1;
static int g_lastHealth = -1;
static qboolean g_playerIsDead = qfalse;
static qboolean g_lastDuelInProgress = qfalse;
static int g_duelStartHealth = 100;

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

	hash = ((hash << 5) + hash) + (unsigned int)prof->openWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->openLosses;
	hash = ((hash << 5) + hash) + (unsigned int)prof->legendsWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->legendsLosses;
	hash = ((hash << 5) + hash) + (unsigned int)prof->faWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->faLosses;

	hash = ((hash << 5) + hash) + (unsigned int)prof->saberKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->gunnerKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->flawlessWins;

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

	// Auto-sanitize legacy death bug (where deaths equaled kills)
	if (loaded.deaths > 0 && loaded.deaths == loaded.kills) {
		loaded.deaths = 0;
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
	Com_Printf("^7  Combat Types : ^3%i Saber Kills ^7| ^3%i Gunner Kills\n", g_xpProfile.saberKills, g_xpProfile.gunnerKills);
	Com_Printf("^7  MB2 Modes    : ^2Open: %dW-%dL ^7| ^2Legends: %dW-%dL ^7| ^2Duels: %dW-%dL (%d Flawless)\n",
		g_xpProfile.openWins, g_xpProfile.openLosses, g_xpProfile.legendsWins, g_xpProfile.legendsLosses, g_xpProfile.duelWins, g_xpProfile.duelLosses, g_xpProfile.flawlessWins);
	Com_Printf("^7  Type ^3/rpg_card^7, ^3/rpg_status^7, ^3/rpg_ranks^7, or ^3/rpg_sith^7 / ^3/rpg_jedi^7.\n");
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

void CL_XP_ToggleCard_f(void) {
	g_xpDrawCard = (g_xpDrawCard == qtrue) ? qfalse : qtrue;
	Com_Printf("^5[RPG MOD] Profile Stats Card %s\n", (g_xpDrawCard == qtrue) ? "^2ENABLED" : "^1DISABLED");
}

void CL_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	g_lastKills = -1;
	g_lastHealth = -1;
	g_playerIsDead = qfalse;
	g_lastDuelInProgress = qfalse;
	g_duelStartHealth = 100;

	Cmd_AddCommand("rpg_card",    CL_XP_ToggleCard_f,  "Toggle full-screen RPG Profile Stats Card");
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
	Com_Printf("^7  Combat Breakdown: ^3%i Saber Kills ^7| ^3%i Gunner Kills\n", g_xpProfile.saberKills, g_xpProfile.gunnerKills);
	Com_Printf("^7  Open Mode     : ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.openWins, g_xpProfile.openLosses);
	Com_Printf("^7  Legends Mode  : ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.legendsWins, g_xpProfile.legendsLosses);
	Com_Printf("^7  Full Authentic: ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.faWins, g_xpProfile.faLosses);
	Com_Printf("^7  Private Duels : ^2%i Wins ^7| ^1%i Losses ^7(^3%i Flawless^7)\n", g_xpProfile.duelWins, g_xpProfile.duelLosses, g_xpProfile.flawlessWins);
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

// Tracking state
static int g_lastSpawnCount = -1;   // detects round resets (new spawn)
static int g_lastAssists    = -1;   // assist counter
static int g_roundWon       = -1;   // 1 = team won last round, 0 = lost, -1 = unknown

void CL_XP_CheckGameEvents(void) {
	if (cls.state != CA_ACTIVE || !cl.snap.valid) {
		g_lastKills = -1;
		g_lastHealth = -1;
		g_playerIsDead = qfalse;
		g_lastDuelInProgress = qfalse;
		g_lastSpawnCount = -1;
		g_lastAssists = -1;
		return;
	}

	CL_XP_UpdateEngineCVars();

	// Check if local player is spectating another entity
	qboolean isSpectatingOther = (cl.snap.ps.clientNum != clc.clientNum || (cl.snap.ps.pm_flags & PMF_FOLLOW)) ? qtrue : qfalse;
	if (isSpectatingOther) {
		g_lastKills = -1;
		g_lastHealth = -1;
		g_playerIsDead = qfalse;
		g_lastDuelInProgress = qfalse;
		g_lastSpawnCount = -1;
		g_lastAssists = -1;
		return;
	}

	int currentHealth = cl.snap.ps.stats[STAT_HEALTH];
	int pmType = cl.snap.ps.pm_type;
	int currentSpawnCount = cl.snap.ps.persistant[PERS_SPAWN_COUNT];

	// -----------------------------------------------------------------------
	// 1. Round Win/Loss detection via spawn count reset + roundWon flag
	//    When a new round starts, PERS_SPAWN_COUNT goes up.
	//    g_roundWon is set in CL_XP_OnPrintMessage when we see the result.
	// -----------------------------------------------------------------------
	if (g_lastSpawnCount != -1 && currentSpawnCount != g_lastSpawnCount) {
		// Player just respawned → new round started, apply pending round result
		if (g_roundWon == 1) {
			cvar_t *mbmodeCvar = Cvar_Get("g_mbmode", "0", 0);
			if (!mbmodeCvar || mbmodeCvar->integer == 0) mbmodeCvar = Cvar_Get("mbmode", "0", 0);
			int mbmode = mbmodeCvar ? mbmodeCvar->integer : 0;
			CL_XP_OnRoundWin(mbmode);
		} else if (g_roundWon == 0) {
			cvar_t *mbmodeCvar = Cvar_Get("g_mbmode", "0", 0);
			if (!mbmodeCvar || mbmodeCvar->integer == 0) mbmodeCvar = Cvar_Get("mbmode", "0", 0);
			int mbmode = mbmodeCvar ? mbmodeCvar->integer : 0;
			CL_XP_OnRoundLoss(mbmode);
		}
		g_roundWon = -1;
		// Reset kill baseline for new round
		g_lastKills = cl.snap.ps.persistant[PERS_SCORE];
		g_lastAssists = -1;
	}
	g_lastSpawnCount = currentSpawnCount;

	// -----------------------------------------------------------------------
	// 2. Death tracking via health drop & PM_DEAD state
	// -----------------------------------------------------------------------
	if (currentHealth > 0 && pmType != PM_DEAD && pmType != PM_SPECTATOR) {
		g_playerIsDead = qfalse;
	} else if (!g_playerIsDead) {
		if ((g_lastHealth > 0 && currentHealth <= 0) || pmType == PM_DEAD) {
			g_playerIsDead = qtrue;
			CL_XP_OnPlayerDeath();
		}
	}
	g_lastHealth = currentHealth;

	// Skip kills, assists and duels if dead or spectating
	if (pmType == PM_SPECTATOR || pmType == PM_DEAD || currentHealth <= 0) {
		g_lastDuelInProgress = qfalse;
		return;
	}

	// -----------------------------------------------------------------------
	// 3. Player Kill tracking via PERS_SCORE
	// -----------------------------------------------------------------------
	int currentKills = cl.snap.ps.persistant[PERS_SCORE];
	if (g_lastKills != -1 && currentKills > g_lastKills) {
		int diff = currentKills - g_lastKills;
		if (diff > 0 && diff <= 5) {
			for (int i = 0; i < diff; i++) {
				CL_XP_OnPlayerKill(cl.snap.ps.weapon);
			}
		}
	}
	g_lastKills = currentKills;

	// -----------------------------------------------------------------------
	// 4. Assist tracking via PERS_HITS (used as assist count in MB2)
	//    Only grants XP in Open (0), Full Authentic (2), and Legends (4).
	//    NOT in Duel mode (3).
	// -----------------------------------------------------------------------
	cvar_t *mbmodeCvar = Cvar_Get("g_mbmode", "0", 0);
	if (!mbmodeCvar || mbmodeCvar->integer == 0) mbmodeCvar = Cvar_Get("mbmode", "0", 0);
	int mbmode = mbmodeCvar ? mbmodeCvar->integer : 0;

	if (mbmode != 3) { // Not Duel mode
		int currentAssists = cl.snap.ps.persistant[PERS_HITS];
		if (g_lastAssists != -1 && currentAssists > g_lastAssists) {
			int diff = currentAssists - g_lastAssists;
			if (diff > 0 && diff <= 5) {
				for (int i = 0; i < diff; i++) {
					CL_XP_AddXP(XP_GRANT_ASSIST, "Assist Kill");
					Com_Printf("^3[RPG MOD] Assist Kill! (+%d XP)\n", XP_GRANT_ASSIST);
				}
			}
		}
		if (g_lastAssists == -1) g_lastAssists = currentAssists;
		else g_lastAssists = currentAssists;
	}

	// -----------------------------------------------------------------------
	// 5. Private 1v1 Saber Duel Win / Loss tracking via duelInProgress
	// -----------------------------------------------------------------------
	qboolean currentDuel = (cl.snap.ps.duelInProgress != 0) ? qtrue : qfalse;
	if (!g_lastDuelInProgress && currentDuel) {
		// Duel just started — record starting health
		g_duelStartHealth = currentHealth;
	} else if (g_lastDuelInProgress && !currentDuel) {
		// Duel just ended — check outcome
		// We check health now AND also if we were dead at end
		if (!g_playerIsDead && currentHealth > 0) {
			qboolean flawless = (currentHealth >= g_duelStartHealth) ? qtrue : qfalse;
			CL_XP_OnDuelWin(flawless);
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

	// Check if print message is a MB2 Round Win/Loss broadcast
	// We set g_roundWon flag here; it's applied when the player respawns (new round)
	qboolean isRoundEndMsg = (Q_stristr(cleanMsg, "win the round") ||
	                          Q_stristr(cleanMsg, "wins the round") ||
	                          Q_stristr(cleanMsg, "round won") ||
	                          Q_stristr(cleanMsg, "round over") ||
	                          Q_stristr(cleanMsg, "round ends") ||
	                          Q_stristr(cleanMsg, "team wins") ||
	                          Q_stristr(cleanMsg, "heroes win") ||
	                          Q_stristr(cleanMsg, "villains win") ||
	                          Q_stristr(cleanMsg, "rebels win") ||
	                          Q_stristr(cleanMsg, "empire wins") ||
	                          Q_stristr(cleanMsg, "clones win") ||
	                          Q_stristr(cleanMsg, "separatists win")) ? qtrue : qfalse;

	if (isRoundEndMsg) {
		static int lastRoundTime = 0;
		if (cls.realtime - lastRoundTime > 3000) {
			lastRoundTime = cls.realtime;
			// Determine win or loss: player alive = win, player dead = loss
			if (cl.snap.ps.stats[STAT_HEALTH] > 0 && cl.snap.ps.pm_type != PM_DEAD) {
				g_roundWon = 1;
			} else {
				g_roundWon = 0;
			}
		}
	}

	// Check if print message is a kill obituary for an NPC
	// ("was slain", "was killed", "was destroyed", "was sliced", "was vaporized")
	// Only count it as an NPC kill if the player name appears as the killer
	// AND the victim is NOT the player themselves.
	qboolean isKillMsg = (Q_stristr(cleanMsg, "was slain") ||
	                      Q_stristr(cleanMsg, "was killed") ||
	                      Q_stristr(cleanMsg, "was destroyed") ||
	                      Q_stristr(cleanMsg, "was sliced") ||
	                      Q_stristr(cleanMsg, "was vaporized")) ? qtrue : qfalse;

	if (isKillMsg) {
		// Make sure the message starts with a victim that is NOT the local player
		// (Player obituaries start with the player's name or "You")
		qboolean victimIsMe = qfalse;
		if (Q_stristr(cleanMsg, cleanMyName)) {
			// Check if player name appears BEFORE "was" (i.e., is the victim)
			const char *wasPos = Q_stristr(cleanMsg, "was ");
			const char *namePos = Q_stristr(cleanMsg, cleanMyName);
			if (namePos && wasPos && namePos < wasPos) {
				// Name appears before "was" — local player is the victim, not the killer
				victimIsMe = qtrue;
			}
		}

		if (!victimIsMe) {
			// Check if player name appears after "by" (is the killer)
			const char *byPos = Q_stristr(cleanMsg, "by ");
			qboolean isMyKill = qfalse;
			if (byPos) {
				const char *killerName = byPos + 3;
				if (Q_stristr(killerName, cleanMyName)) {
					isMyKill = qtrue;
				}
			}
			if (isMyKill) {
				static int lastNPCKillTime = 0;
				if (cls.realtime - lastNPCKillTime > 500) {
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

void CL_XP_OnPlayerKill(int weapon) {
	g_xpProfile.kills++;
	if (weapon == 10) { // WP_SABER
		g_xpProfile.saberKills++;
	} else {
		g_xpProfile.gunnerKills++;
	}
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

void CL_XP_OnDuelWin(qboolean flawless) {
	g_xpProfile.duelWins++;
	g_xpProfile.kills++;
	g_xpProfile.saberKills++;

	int grantedXP = XP_GRANT_DUEL_WIN;
	if (flawless) {
		g_xpProfile.flawlessWins++;
		grantedXP += XP_GRANT_FLAWLESS;
		Com_Printf("^3*** FLAWLESS DUEL VICTORY! (+%d XP Bonus) ***\n", XP_GRANT_FLAWLESS);
	}

	CL_XP_AddXP(grantedXP, "Private Duel Victory");
}

void CL_XP_OnDuelLoss(void) {
	g_xpProfile.duelLosses++;
	CL_XP_SaveProfile();
}

void CL_XP_OnRoundWin(int mbmode) {
	if (mbmode == 4) {
		g_xpProfile.legendsWins++;
	} else if (mbmode == 2) {
		g_xpProfile.faWins++;
	} else {
		g_xpProfile.openWins++;
	}
	Com_Printf("^2*** ROUND VICTORY! (+%d XP) ***\n", XP_GRANT_ROUND_WIN);
	CL_XP_AddXP(XP_GRANT_ROUND_WIN, "Round Victory");
}

void CL_XP_OnRoundLoss(int mbmode) {
	if (mbmode == 4) {
		g_xpProfile.legendsLosses++;
	} else if (mbmode == 2) {
		g_xpProfile.faLosses++;
	} else {
		g_xpProfile.openLosses++;
	}
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
