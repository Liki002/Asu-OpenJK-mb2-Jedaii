/*
===========================================================================
Copyright (C) 2026 AsuTech / OpenJK Contributors
Standalone Client-Side XP & Leveling System with Anti-Cheat Tamper Protection
===========================================================================
*/

#include "cg_xp_profile.h"
#include "cg_local.h"

static xpProfile_t g_xpProfile;
static qboolean g_xpInitialized = qfalse;

// Popup notification state
static int g_xpPopupAmount = 0;
static char g_xpPopupReason[64] = {0};
static int g_xpPopupTime = 0;

// Tracking state for automated detection
static int g_lastKills = -1;
static qboolean g_lastDuelInProgress = qfalse;
static int g_lastDuelWinner = -1;

// Secret salt for anti-cheat hash signature
static const char XP_SECRET_SALT[] = "JEDAII_XP_STANDALONE_SECURE_SALT_2026_x89!";

// -------------------------------------------------------------------------
// Anti-Cheat Signature Calculation
// -------------------------------------------------------------------------
static unsigned int CG_XP_CalculateChecksum(const xpProfile_t *prof) {
	unsigned int hash = 5381;
	const char *p;

	// Hash salt
	for (p = XP_SECRET_SALT; *p; p++) {
		hash = ((hash << 5) + hash) + (unsigned char)(*p);
	}

	// Hash numerical stats
	hash = ((hash << 5) + hash) + (unsigned int)prof->level;
	hash = ((hash << 5) + hash) + (unsigned int)prof->xp;
	hash = ((hash << 5) + hash) + (unsigned int)prof->playerKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->npcKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->duelWins;

	// Hash profile name
	for (p = prof->profileName; *p; p++) {
		hash = ((hash << 5) + hash) + (unsigned char)(*p);
	}

	return hash;
}

// -------------------------------------------------------------------------
// Required XP Calculation
// -------------------------------------------------------------------------
int CG_XP_GetRequiredXP(int level) {
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

static int CG_XP_CalculateLevelFromXP(int totalXP) {
	int lvl;

	if (totalXP <= 0) {
		return 1;
	}

	for (lvl = 1; lvl < MAX_XP_LEVEL; lvl++) {
		if (totalXP < CG_XP_GetRequiredXP(lvl + 1)) {
			return lvl;
		}
	}
	return MAX_XP_LEVEL;
}

void CG_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent) {
	int curLvl = CG_XP_GetLevel();
	int baseXP = CG_XP_GetRequiredXP(curLvl);
	int targetXP = CG_XP_GetRequiredXP(curLvl + 1);

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

// -------------------------------------------------------------------------
// Profile Save & Load with Anti-Cheat Verification
// -------------------------------------------------------------------------
void CG_XP_SaveProfile(void) {
	fileHandle_t f;

	// Calculate tamper-evident signature
	g_xpProfile.checksum = CG_XP_CalculateChecksum(&g_xpProfile);

	trap->FS_Open("xp_profile.dat", &f, FS_WRITE);
	if (!f) {
		trap->Print("^1[XP System] Error: Failed to open xp_profile.dat for writing!\n");
		return;
	}

	trap->FS_Write(&g_xpProfile, sizeof(xpProfile_t), f);
	trap->FS_Close(f);
}

void CG_XP_LoadProfile(void) {
	fileHandle_t f;
	xpProfile_t loaded;

	memset(&g_xpProfile, 0, sizeof(xpProfile_t));
	g_xpProfile.level = 1;
	g_xpProfile.xp = 0;
	Q_strncpyz(g_xpProfile.profileName, "Jedi Warrior", sizeof(g_xpProfile.profileName));

	trap->FS_Open("xp_profile.dat", &f, FS_READ);
	if (!f) {
		trap->Print("^5[RPG MOD] Creating new client XP profile...\n");
		CG_XP_SaveProfile();
		return;
	}

	int bytesRead = trap->FS_Read(&loaded, sizeof(xpProfile_t), f);
	trap->FS_Close(f);

	if (bytesRead < (int)sizeof(xpProfile_t)) {
		trap->Print("^3[RPG MOD] Warning: Corrupted profile file. Resetting profile.\n");
		CG_XP_SaveProfile();
		return;
	}

	// Verify anti-cheat signature
	unsigned int expectedChecksum = CG_XP_CalculateChecksum(&loaded);
	if (loaded.checksum != expectedChecksum) {
		trap->Print("^1[RPG MOD] ANTI-CHEAT WARNING: Tampered XP profile detected! Resetting progress to Level 1.\n");
		CG_XP_SaveProfile();
		return;
	}

	// Verify hardcoded level integrity matches XP
	int expectedLevel = CG_XP_CalculateLevelFromXP(loaded.xp);
	if (loaded.level != expectedLevel) {
		trap->Print(va("^1[RPG MOD] ANTI-CHEAT WARNING: Level mismatch detected! Correcting level to %i.\n", expectedLevel));
		loaded.level = expectedLevel;
	}

	g_xpProfile = loaded;

	// Print prominent console banner
	trap->Print("\n");
	trap->Print("^5=====================================================\n");
	trap->Print("^2  [RPG MOD] Standalone Client XP System LOADED!\n");
	trap->Print(va("^7  Profile Name : ^3%s\n", g_xpProfile.profileName));
	trap->Print(va("^7  Level        : ^3Level %i ^7(Total XP: ^3%i^7)\n", g_xpProfile.level, g_xpProfile.xp));
	trap->Print(va("^7  Duels Won    : ^3%i ^7| Player Kills: ^3%i ^7| NPC Kills: ^3%i\n", g_xpProfile.duelWins, g_xpProfile.playerKills, g_xpProfile.npcKills));
	trap->Print("^7  Type ^3/rpg_status^7 in console for full stats.\n");
	trap->Print("^5=====================================================\n\n");
}

void CG_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	g_lastKills = -1;
	g_lastDuelInProgress = qfalse;
	g_lastDuelWinner = -1;
	CG_XP_LoadProfile();
}

void CG_XP_PrintStatus_f(void) {
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CG_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	trap->Print("\n^5=====================================================\n");
	trap->Print("^2  [RPG MOD] Standalone Client XP & Leveling System\n");
	trap->Print(va("^7  Profile Name  : ^3%s\n", CG_XP_GetProfileName()));
	trap->Print(va("^7  Level         : ^3%i ^7(Max %i)\n", CG_XP_GetLevel(), MAX_XP_LEVEL));
	trap->Print(va("^7  Total XP      : ^3%i\n", CG_XP_GetXP()));
	trap->Print(va("^7  Level Progress: ^3%i / %i XP ^7(%.1f%%)\n", curXP, reqXP, percent * 100.0f));
	trap->Print(va("^7  Player Kills  : ^3%i\n", g_xpProfile.playerKills));
	trap->Print(va("^7  NPC Kills     : ^3%i\n", g_xpProfile.npcKills));
	trap->Print(va("^7  Duels Won     : ^3%i\n", g_xpProfile.duelWins));
	trap->Print("^2  Anti-Cheat Protection: ENABLED & ACTIVE\n");
	trap->Print("^5=====================================================\n\n");
}

// -------------------------------------------------------------------------
// Automated Event Detection & Triggers
// -------------------------------------------------------------------------
void CG_XP_CheckGameEvents(void) {
	if (!cg.snap) {
		g_lastKills = -1;
		g_lastDuelInProgress = qfalse;
		return;
	}

	// 1. Automatic Player Kill tracking via PERS_SCORE
	int currentKills = cg.snap->ps.persistant[PERS_SCORE];
	if (g_lastKills != -1 && currentKills > g_lastKills) {
		int diff = currentKills - g_lastKills;
		int i;
		for (i = 0; i < diff; i++) {
			CG_XP_OnPlayerKill();
		}
	}
	g_lastKills = currentKills;

	// 2. Automatic Duel Victory tracking via duelInProgress
	qboolean currentDuel = (cg.snap->ps.duelInProgress != 0) ? qtrue : qfalse;
	if (g_lastDuelInProgress && !currentDuel) {
		// Duel ended! Check if player survived
		if (cg.snap->ps.stats[STAT_HEALTH] > 0) {
			CG_XP_OnDuelWin();
		}
	}
	g_lastDuelInProgress = currentDuel;

	// 3. Configstring duelWinner check
	if (cgs.duelWinner != g_lastDuelWinner) {
		if (cgs.duelWinner != -1 && cgs.duelWinner == cg.snap->ps.clientNum) {
			CG_XP_OnDuelWin();
		}
		g_lastDuelWinner = cgs.duelWinner;
	}
}

void CG_XP_OnPrintMessage(const char *msg) {
	if (!msg || !msg[0]) {
		return;
	}

	const char *myName = (cgs.clientinfo[cg.clientNum].name[0]) ? cgs.clientinfo[cg.clientNum].name : NULL;

	// Check for Duel Victory print message
	if (Q_stristr((char*)msg, "won the duel") || Q_stristr((char*)msg, "wins the duel") || Q_stristr((char*)msg, "won duel")) {
		if (!myName || Q_stristr((char*)msg, myName)) {
			static int lastDuelMsgTime = 0;
			if (cg.time - lastDuelMsgTime > 2000) {
				lastDuelMsgTime = cg.time;
				CG_XP_OnDuelWin();
			}
		}
	}

	// Check for NPC kill print message
	if (Q_stristr((char*)msg, "was slain by") || Q_stristr((char*)msg, "was killed by") || Q_stristr((char*)msg, "slain by")) {
		if (myName && Q_stristr((char*)msg, myName)) {
			static int lastNPCKillTime = 0;
			if (cg.time - lastNPCKillTime > 500) {
				lastNPCKillTime = cg.time;
				CG_XP_OnNPCKill();
			}
		}
	}
}

// -------------------------------------------------------------------------
// XP Actions & Level Up Handling
// -------------------------------------------------------------------------
void CG_XP_AddXP(int amount, const char *reason) {
	if (amount <= 0) {
		return;
	}

	int oldLevel = g_xpProfile.level;
	g_xpProfile.xp += amount;

	int newLevel = CG_XP_CalculateLevelFromXP(g_xpProfile.xp);
	g_xpProfile.level = newLevel;

	// Save progress securely
	CG_XP_SaveProfile();

	// Popup notification
	g_xpPopupAmount = amount;
	if (reason) {
		Q_strncpyz(g_xpPopupReason, reason, sizeof(g_xpPopupReason));
	} else {
		g_xpPopupReason[0] = '\0';
	}
	g_xpPopupTime = cg.time;

	// Level Up notification
	if (newLevel > oldLevel) {
		trap->Print(va("^2*** LEVEL UP! You reached Level %i! ***\n", newLevel));
		trap->S_StartLocalSound(cgs.media.winnerSound, CHAN_AUTO);
		CG_CenterPrint(va("^2*** LEVEL UP! ***\n^7Reached Level ^3%i", newLevel), SCREEN_HEIGHT * 0.30, 200);
	} else {
		trap->S_StartLocalSound(cgs.media.selectSound, CHAN_AUTO);
	}
}

void CG_XP_OnPlayerKill(void) {
	g_xpProfile.playerKills++;
	CG_XP_AddXP(XP_GRANT_PLAYER_KILL, "Player Kill");
}

void CG_XP_OnNPCKill(void) {
	g_xpProfile.npcKills++;
	CG_XP_AddXP(XP_GRANT_NPC_KILL, "NPC Kill");
}

void CG_XP_OnDuelWin(void) {
	g_xpProfile.duelWins++;
	CG_XP_AddXP(XP_GRANT_DUEL_WIN, "Duel Victory");
}

int CG_XP_GetLevel(void) {
	return g_xpProfile.level;
}

int CG_XP_GetXP(void) {
	return g_xpProfile.xp;
}

const char *CG_XP_GetProfileName(void) {
	if (cg_levelProfileName.string && cg_levelProfileName.string[0]) {
		return cg_levelProfileName.string;
	}
	return g_xpProfile.profileName;
}

void CG_XP_SetProfileName(const char *name) {
	if (name && name[0]) {
		Q_strncpyz(g_xpProfile.profileName, name, sizeof(g_xpProfile.profileName));
		CG_XP_SaveProfile();
	}
}

// -------------------------------------------------------------------------
// HUD Rendering
// -------------------------------------------------------------------------
void CG_XP_DrawHUD(void) {
	if (!g_xpInitialized) {
		CG_XP_Init();
	}

	// Always run game event checks for kills and duel victories
	CG_XP_CheckGameEvents();

	// Check if explicitly disabled via console
	if (cg_drawLevelProfile.string && cg_drawLevelProfile.string[0] == '0') {
		return;
	}

	float x = cg_levelProfileX.value;
	float y = cg_levelProfileY.value;

	if (x <= 0.0f) x = 10.0f;
	if (y <= 0.0f) y = 10.0f;

	float w = 210.0f;
	float h = 50.0f;

	vec4_t bgColor = {0.05f, 0.05f, 0.08f, 0.85f};
	vec4_t borderColor = {0.2f, 0.6f, 1.0f, 0.9f};
	vec4_t barBgColor = {0.1f, 0.1f, 0.15f, 0.95f};
	vec4_t barFillColor = {0.1f, 0.7f, 1.0f, 0.95f};

	// Draw Background Box & Border
	CG_FillRect(x, y, w, h, bgColor);
	CG_DrawRect(x, y, w, h, 1.0f, borderColor);

	// Determine Profile Badge Icon shader by Level Tier
	qhandle_t badgeShader = 0;
	int level = CG_XP_GetLevel();

	if (level >= 75) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/hud/w_icon_saberstaff");
	} else if (level >= 50) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/hud/w_icon_duallightsaber");
	} else if (level >= 25) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/mp/pduel_icon_double");
	} else {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/mp/pduel_icon_lone");
	}

	// Draw Profile Badge Icon (fallback to white box if missing)
	if (!badgeShader) {
		badgeShader = cgs.media.whiteShader;
	}
	if (badgeShader) {
		CG_DrawPic(x + 5.0f, y + 5.0f, 40.0f, 40.0f, badgeShader);
	}

	// Draw Level & Profile Name
	const char *profName = CG_XP_GetProfileName();
	char headerStr[128];
	Com_sprintf(headerStr, sizeof(headerStr), "^3Lvl %i ^7| %s", level, profName);
	CG_DrawStringExt((int)(x + 50.0f), (int)(y + 6.0f), headerStr, colorWhite, qtrue, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0);

	// Draw XP Progress Bar
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CG_XP_GetLevelProgress(&curXP, &reqXP, &percent);

	float barX = x + 50.0f;
	float barY = y + 24.0f;
	float barW = 150.0f;
	float barH = 10.0f;

	CG_FillRect(barX, barY, barW, barH, barBgColor);
	if (percent > 0.0f) {
		CG_FillRect(barX, barY, barW * percent, barH, barFillColor);
	}
	CG_DrawRect(barX, barY, barW, barH, 1.0f, colorWhite);

	// Draw XP Text below bar
	char xpText[64];
	Com_sprintf(xpText, sizeof(xpText), "^7%i / %i XP", curXP, reqXP);
	CG_DrawStringExt((int)(barX), (int)(y + 36.0f), xpText, colorWhite, qtrue, qfalse, 8, 10, 0);

	// Render Recent XP Popup Notification (+50 XP) for 3 seconds
	if (g_xpPopupTime > 0 && (cg.time - g_xpPopupTime) < 3000) {
		float alpha = 1.0f - ((float)(cg.time - g_xpPopupTime) / 3000.0f);
		vec4_t popupColor = {0.2f, 1.0f, 0.3f, alpha};

		char popupStr[128];
		if (g_xpPopupReason[0]) {
			Com_sprintf(popupStr, sizeof(popupStr), "+%i XP (%s)", g_xpPopupAmount, g_xpPopupReason);
		} else {
			Com_sprintf(popupStr, sizeof(popupStr), "+%i XP", g_xpPopupAmount);
		}
		CG_DrawStringExt((int)(x + w + 10.0f), (int)(y + 15.0f), popupStr, popupColor, qtrue, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0);
	}
}
