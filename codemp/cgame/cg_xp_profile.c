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
		trap->Print("^5[XP System] Creating new client XP profile...\n");
		CG_XP_SaveProfile();
		return;
	}

	int bytesRead = trap->FS_Read(&loaded, sizeof(xpProfile_t), f);
	trap->FS_Close(f);

	if (bytesRead < (int)sizeof(xpProfile_t)) {
		trap->Print("^3[XP System] Warning: Corrupted profile file. Resetting profile.\n");
		CG_XP_SaveProfile();
		return;
	}

	// Verify anti-cheat signature
	unsigned int expectedChecksum = CG_XP_CalculateChecksum(&loaded);
	if (loaded.checksum != expectedChecksum) {
		trap->Print("^1[XP System] ANTI-CHEAT WARNING: Tampered XP profile detected! Resetting progress to Level 1.\n");
		CG_XP_SaveProfile();
		return;
	}

	// Verify hardcoded level integrity matches XP
	int expectedLevel = CG_XP_CalculateLevelFromXP(loaded.xp);
	if (loaded.level != expectedLevel) {
		trap->Print(va("^1[XP System] ANTI-CHEAT WARNING: Level mismatch detected! Correcting level to %i.\n", expectedLevel));
		loaded.level = expectedLevel;
	}

	g_xpProfile = loaded;
	trap->Print(va("^2[XP System] Profile loaded successfully! Level: %i | XP: %i | Duel Wins: %i\n",
		g_xpProfile.level, g_xpProfile.xp, g_xpProfile.duelWins));
}

void CG_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	CG_XP_LoadProfile();
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
	if (!cg_drawLevelProfile.integer) {
		return;
	}

	if (!g_xpInitialized) {
		CG_XP_Init();
	}

	float x = cg_levelProfileX.value;
	float y = cg_levelProfileY.value;
	float w = 210.0f;
	float h = 50.0f;

	vec4_t bgColor = {0.05f, 0.05f, 0.08f, 0.75f};
	vec4_t borderColor = {0.2f, 0.6f, 1.0f, 0.8f};
	vec4_t barBgColor = {0.1f, 0.1f, 0.15f, 0.9f};
	vec4_t barFillColor = {0.1f, 0.7f, 1.0f, 0.9f};

	// Draw Background Box & Border
	CG_FillRect(x, y, w, h, bgColor);
	CG_DrawRect(x, y, w, h, 1.0f, borderColor);

	// Determine Profile Badge Icon shader by Level Tier
	qhandle_t badgeShader = 0;
	int level = CG_XP_GetLevel();

	if (level >= 75) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/2d/hud_rank3");
	} else if (level >= 50) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/2d/hud_rank2");
	} else if (level >= 25) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/2d/hud_rank1");
	} else if (level >= 10) {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/mp/pduel_icon_double");
	} else {
		badgeShader = trap->R_RegisterShaderNoMip("gfx/mp/pduel_icon_lone");
	}

	// Draw Profile Badge Icon
	if (badgeShader) {
		CG_DrawPic(x + 5.0f, y + 5.0f, 40.0f, 40.0f, badgeShader);
	}

	// Draw Level & Profile Name
	const char *profName = CG_XP_GetProfileName();
	char headerStr[128];
	Com_sprintf(headerStr, sizeof(headerStr), "^3Lvl %i ^7| %s", level, profName);
	CG_DrawStringExt((int)(x + 50.0f), (int)(y + 6.0f), headerStr, colorWhite, qtrue, qtrue, 12, 12, 0);

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
	CG_DrawStringExt((int)(barX), (int)(y + 36.0f), xpText, colorWhite, qtrue, qfalse, 9, 9, 0);

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
		CG_DrawStringExt((int)(x + w + 10.0f), (int)(y + 15.0f), popupStr, popupColor, qtrue, qtrue, 12, 12, 0);
	}
}
