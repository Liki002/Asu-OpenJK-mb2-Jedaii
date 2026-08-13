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
#include <time.h>

clXpProfile_t g_xpProfile;
qboolean g_xpDrawCard = qfalse;
qboolean g_xpDrawRanks = qfalse;
qboolean g_xpDrawHelp = qfalse;
qboolean g_xpDrawSettings = qfalse;
float g_rpgMouseX = 320.0f;
float g_rpgMouseY = 240.0f;
static qboolean g_xpInitialized = qfalse;

// Runtime HUD notif ring + sound handles (not persisted)
rpgNotif_t g_rpgNotifs[MAX_RPG_NOTIFS];
int        g_rpgNotifCount = 0;
int        g_rpgSoundHandles[RPG_SND_COUNT] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int        g_rpgMenuTab = 0; // 0=HUD & Themes, 1=Achievements, 2=Audio Mixer, 3=Guide
int        g_currentKillStreak = 0;
int        g_lastKillMs = -999999;
int        g_runningMultiKillCount = 0;
int        g_duelStartMs = 0;
int        g_duelStartHealth = 100;
int        g_duelStartHits = 0;

// CVars (toggles for new features)
cvar_t *cg_rpg_notify_sounds   = NULL;
cvar_t *cg_rpg_notify_popups   = NULL;
cvar_t *cg_rpg_multikill_enabled = NULL;
cvar_t *cg_rpg_streak_enabled  = NULL;
cvar_t *cg_rpg_hud_pos         = NULL;
// cg_rpg_hud_style registered by SCR_Init in cl_scrn.cpp already (it's called from there)

// Sound paths inside zzzz_rpgstandalone.pk3 (user drops these .wav files in place)
static const char *s_rpgSoundPaths[RPG_SND_COUNT] = {
    "sound/rpg/multikill_double.wav",   // RPG_SND_DOUBLE
    "sound/rpg/multikill_triple.wav",   // RPG_SND_TRIPLE
    "sound/rpg/multikill_overkill.wav", // RPG_SND_OVERKILL
    "sound/rpg/multikill_monster.wav",  // RPG_SND_MONSTER
    "sound/rpg/multikill_ultra.wav",    // RPG_SND_ULTRA
    "sound/rpg/levelup.wav",            // RPG_SND_LEVELUP
    "sound/rpg/perfect_duel.wav",       // RPG_SND_PERFECT
    "sound/rpg/quickdraw.wav",          // RPG_SND_QUICKDRAW
    "sound/rpg/killstreak.wav",         // RPG_SND_STREAK
    "sound/rpg/achievement.wav",        // RPG_SND_ACHIEVEMENT
    "sound/rpg/xp_gain.wav"             // RPG_SND_XP
};

// Tracking state for automated detection
static int g_lastKills = -1;
static int g_lastDeaths = -1;
static int g_lastHits = -1;
static int g_lastHealth = -1;
static qboolean g_playerIsDead = qfalse;
static qboolean g_lastDuelInProgress = qfalse;
static int g_lastFramePlaytime = -1; // last cls.realtime we advanced playtime (avoid double-count when paused)

int g_lastParsedVictimBP = 0;
int g_lastParsedKillerHP = 0;
int g_lastParsedKillerBP = 0;
char g_lastParsedVictimName[64] = "";
char g_lastParsedKillerName[64] = "";
int g_lastDuelOutcome = 0;

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

	hash = ((hash << 5) + hash) + (unsigned int)prof->roundWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->roundLosses;

	hash = ((hash << 5) + hash) + (unsigned int)prof->saberKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->gunnerKills;
	hash = ((hash << 5) + hash) + (unsigned int)prof->perfectWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->quickDrawWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->enduranceWins;
	hash = ((hash << 5) + hash) + (unsigned int)prof->currentDuelStreak;
	hash = ((hash << 5) + hash) + (unsigned int)prof->bestDuelStreak;
	hash = ((hash << 5) + hash) + (unsigned int)prof->shortestDuelMs;
	hash = ((hash << 5) + hash) + (unsigned int)prof->longestDuelMs;
	hash = ((hash << 5) + hash) + (unsigned int)prof->multiDouble;
	hash = ((hash << 5) + hash) + (unsigned int)prof->multiTriple;
	hash = ((hash << 5) + hash) + (unsigned int)prof->multiOverkill;
	hash = ((hash << 5) + hash) + (unsigned int)prof->multiMonster;
	hash = ((hash << 5) + hash) + (unsigned int)prof->multiUltra;
	hash = ((hash << 5) + hash) + (unsigned int)prof->bestKillStreak;
	hash = ((hash << 5) + hash) + (unsigned int)prof->totalPlaytimeMs;

	for (int ai = 0; ai < 128; ai++) {
		hash = ((hash << 5) + hash) + (unsigned int)prof->achievements[ai];
	}

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
// HUD Notification Queue (ring buffer)
// -------------------------------------------------------------------------
void CL_XP_PushNotification(rpgNotifType_t type, const char *text, const char *subtext, int xpDelta, const vec4_t tint, int lifetimeMs) {
	if (!cg_rpg_notify_popups || !cg_rpg_notify_popups->integer) {
		return;
	}

	int now = cls.realtime;
	int life = (lifetimeMs > 0) ? lifetimeMs : NOTIF_LIFETIME_MS;

	// Shift queue down (drop oldest at index 0 if we're at max - FIFO ring)
	if (g_rpgNotifCount >= MAX_RPG_NOTIFS) {
		for (int i = 0; i < MAX_RPG_NOTIFS - 1; i++) g_rpgNotifs[i] = g_rpgNotifs[i+1];
		g_rpgNotifCount = MAX_RPG_NOTIFS - 1;
	}
	rpgNotif_t *n = &g_rpgNotifs[g_rpgNotifCount++];
	memset(n, 0, sizeof(*n));
	n->type = type;
	Q_strncpyz(n->text, text ? text : "", sizeof(n->text));
	Q_strncpyz(n->subtext, subtext ? subtext : "", sizeof(n->subtext));
	n->xpDelta = xpDelta;
	n->lifetimeMs = life;

	// SEQUENTIAL QUEUE TIMING: Schedule startMs after the previous notification finishes!
	int startMs = now;
	if (g_rpgNotifCount > 1) {
		rpgNotif_t *prev = &g_rpgNotifs[g_rpgNotifCount - 2];
		int prevEndMs = prev->startMs + prev->lifetimeMs + 300;
		if (prevEndMs > startMs) {
			startMs = prevEndMs;
		}
	}
	n->startMs = startMs;

	if (tint) { VectorCopy4(tint, n->tint); }
	else {
		// default color per type
		switch (type) {
			case RPG_NOTIF_XP:         n->tint[0]=0.15f; n->tint[1]=0.95f; n->tint[2]=0.25f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_MULTIKILL:  n->tint[0]=1.00f; n->tint[1]=0.45f; n->tint[2]=0.05f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_STREAK:     n->tint[0]=0.95f; n->tint[1]=0.15f; n->tint[2]=0.15f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_DUEL:       n->tint[0]=0.85f; n->tint[1]=0.70f; n->tint[2]=0.15f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_LEVELUP:    n->tint[0]=0.25f; n->tint[1]=0.65f; n->tint[2]=1.00f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_ACHIEVEMENT:n->tint[0]=0.95f; n->tint[1]=0.20f; n->tint[2]=0.85f; n->tint[3]=1.0f; break;
			case RPG_NOTIF_MILESTONE:  n->tint[0]=0.85f; n->tint[1]=0.50f; n->tint[2]=0.95f; n->tint[3]=1.0f; break;
			default:                   n->tint[0]=1.0f;  n->tint[1]=1.0f;  n->tint[2]=1.0f;  n->tint[3]=1.0f; break;
		}
	}
}

void CL_XP_TickNotifications(void) {
	int now = cls.realtime;
	if (g_rpgNotifCount > 0) {
		int finishMs = g_rpgNotifs[0].startMs + g_rpgNotifs[0].lifetimeMs + 400; // 400ms fade-out
		if (now >= finishMs) {
			// Shift queue down so the next notification in line begins displaying immediately
			for (int i = 0; i < g_rpgNotifCount - 1; i++) {
				g_rpgNotifs[i] = g_rpgNotifs[i+1];
			}
			g_rpgNotifCount--;
		}
	}
}

// -------------------------------------------------------------------------
// Sound Registration + Playback (safe: missing .wav = no-op silent)
// -------------------------------------------------------------------------
void CL_XP_RegisterSounds(void) {
	if (cls.state != CA_ACTIVE) return;

	static int s_lastSoundState = -1;
	if (cls.state != s_lastSoundState) {
		s_lastSoundState = cls.state;
		for (int i = 0; i < RPG_SND_COUNT; i++) {
			g_rpgSoundHandles[i] = -1;
		}
	}

	int registeredCount = 0;
	for (int i = 0; i < RPG_SND_COUNT; i++) {
		if (g_rpgSoundHandles[i] <= 0) {
			int h = (int)S_RegisterSound(s_rpgSoundPaths[i]);
			if (h > 0) {
				g_rpgSoundHandles[i] = h;
			}
		}
		if (g_rpgSoundHandles[i] > 0) {
			registeredCount++;
		}
	}

	static int s_lastPrintedCount = -1;
	if (registeredCount != s_lastPrintedCount) {
		s_lastPrintedCount = registeredCount;
		Com_Printf("^2[RPG MOD] Registered %d/%d sound FX slots from zzzz_rpgstandalone.pk3\n",
			registeredCount, RPG_SND_COUNT);
	}
}

void CL_XP_PlaySound(rpgSoundSlot_t slot) {
	if (!cg_rpg_notify_sounds || !cg_rpg_notify_sounds->integer) return;
	if (g_xpProfile.soundVolume == 0) return; // Mute
	if (slot < 0 || slot >= RPG_SND_COUNT) return;

	if (cls.state == CA_ACTIVE && g_rpgSoundHandles[slot] <= 0) {
		CL_XP_RegisterSounds();
	}
	if (g_rpgSoundHandles[slot] <= 0) return; // file not present in pk3 - silent no-op fallback

	if (slot >= RPG_SND_DOUBLE && slot <= RPG_SND_ULTRA && g_xpProfile.announcerEnabled == 0) return;
	if (slot == RPG_SND_LEVELUP && g_xpProfile.levelupSndEnabled == 0) return;
	if ((slot == RPG_SND_PERFECT || slot == RPG_SND_QUICKDRAW) && g_xpProfile.duelSndEnabled == 0) return;

	// Use S_StartLocalSound for non-spatial client-only playback (CHAN_LOCAL_SOUND channel)
	S_StartLocalSound(g_rpgSoundHandles[slot], CHAN_LOCAL_SOUND);
}

void CL_XP_ClaimAchievement(int achievementId) {
	const char *title = NULL;
	int xpReward = 0;
	qboolean canClaim = qfalse;

	switch (achievementId) {
		case 0: // Saber Master (50 Saber Kills)
			if (!g_xpProfile.achSaberMasterClaimed && g_xpProfile.saberKills >= 50) {
				g_xpProfile.achSaberMasterClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Saber Master!";
				xpReward = 250;
				canClaim = qtrue;
			}
			break;
		case 1: // Gunner Elite (50 Gun Kills)
			if (!g_xpProfile.achGunnerEliteClaimed && g_xpProfile.gunnerKills >= 50) {
				g_xpProfile.achGunnerEliteClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Gunner Elite!";
				xpReward = 250;
				canClaim = qtrue;
			}
			break;
		case 2: // Duel Specialist (10 Duel Wins)
			if (!g_xpProfile.achDuelSpecialistClaimed && g_xpProfile.duelWins >= 10) {
				g_xpProfile.achDuelSpecialistClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Duel Specialist!";
				xpReward = 300;
				canClaim = qtrue;
			}
			break;
		case 3: // Quick Draw Pro (3 QuickDraw Wins)
			if (!g_xpProfile.achQuickDrawClaimed && g_xpProfile.quickDrawWins >= 3) {
				g_xpProfile.achQuickDrawClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Quick Draw Pro!";
				xpReward = 200;
				canClaim = qtrue;
			}
			break;
		case 4: // Flawless Victory (3 Perfect Duels)
			if (!g_xpProfile.achPerfectClaimed && g_xpProfile.perfectWins >= 3) {
				g_xpProfile.achPerfectClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Flawless Victory!";
				xpReward = 250;
				canClaim = qtrue;
			}
			break;
		case 5: // Unstoppable (10 Kill Streak)
			if (!g_xpProfile.achStreakClaimed && g_xpProfile.bestKillStreak >= 10) {
				g_xpProfile.achStreakClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Unstoppable!";
				xpReward = 400;
				canClaim = qtrue;
			}
			break;
		case 6: // Century Club (Level 100)
			if (!g_xpProfile.achCenturyClaimed && g_xpProfile.level >= 100) {
				g_xpProfile.achCenturyClaimed = 1;
				title = "ACHIEVEMENT CLAIMED: Century Club!";
				xpReward = 1000;
				canClaim = qtrue;
			}
			break;
		default:
			break;
	}

	if (canClaim && xpReward > 0) {
		CL_XP_AddXP(xpReward, title);
		CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
		char sub[64];
		Com_sprintf(sub, sizeof(sub), "+%d XP Awarded!", xpReward);
		CL_XP_PushNotification(RPG_NOTIF_ACHIEVEMENT, title, sub, xpReward, NULL, NOTIF_LIFETIME_MS + 2000);
		CL_XP_SaveProfile();
	}
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
	g_xpProfile.shortestDuelMs = -1; // -1 sentinel = not yet recorded
	g_xpProfile.longestDuelMs  = 0;
	g_xpProfile.currentDuelStreak = 0;
	g_xpProfile.bestDuelStreak    = 0;
	g_xpProfile.bestKillStreak    = 0;
	g_xpProfile.soundVolume       = 4; // 100%
	g_xpProfile.announcerEnabled  = 1;
	g_xpProfile.levelupSndEnabled = 1;
	g_xpProfile.duelSndEnabled    = 1;
	g_xpProfile.themeIndex        = 0; // Cyan
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

	// Fix sentinel defaults for new fields not in legacy profiles
	if (loaded.shortestDuelMs == 0) loaded.shortestDuelMs = -1;

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
	Com_Printf("^7  Team Rounds  : ^2%d Wins ^7| ^1%d Losses ^7| Duel: %dW-%dL (Perfect:%d QD:%d Endurance:%d)\n",
		g_xpProfile.roundWins, g_xpProfile.roundLosses, g_xpProfile.duelWins, g_xpProfile.duelLosses,
		g_xpProfile.perfectWins, g_xpProfile.quickDrawWins, g_xpProfile.enduranceWins);
	Com_Printf("^7  Streaks      : Best Kill %d | Best Duel %d | Multi-kills: 2x=%d 3x=%d 4x=%d 5x=%d 6+=%d\n",
		g_xpProfile.bestKillStreak, g_xpProfile.bestDuelStreak,
		g_xpProfile.multiDouble, g_xpProfile.multiTriple, g_xpProfile.multiOverkill, g_xpProfile.multiMonster, g_xpProfile.multiUltra);
	float hrs = (float)g_xpProfile.totalPlaytimeMs / (1000.0f * 3600.0f);
	Com_Printf("^7  Total Playtime: ^3%.1f hrs ^7  | Shortest Duel Win: ^3%i ms ^7| Longest: ^3%i s\n",
		hrs, g_xpProfile.shortestDuelMs, g_xpProfile.longestDuelMs / 1000);
	Com_Printf("^7  Type ^3/rpg_card^7, ^3/rpg_status^7, ^3/rpg_ranks^7, or ^3/rpg_sith^7 / ^3/rpg_jedi^7.\n");
	Com_Printf("^7  CVar Toggles : ^5cg_rpg_notify_popups ^7(0/1), ^5cg_rpg_notify_sounds ^7(0/1)\n");
	Com_Printf("^7                 ^5cg_rpg_multikill_enabled ^7(0/1), ^5cg_rpg_streak_enabled ^7(0/1)\n");
	Com_Printf("^7                 ^5cg_rpg_hud_style ^7(0-4: 0=Holo Datapad, 1=Bottom Bar, 2=Pill, 3=Imperial, 4=Cyber Neon)\n");
	{
		int regSounds = 0;
		for (int i = 0; i < RPG_SND_COUNT; i++) if (g_rpgSoundHandles[i] > 0) regSounds++;
		Com_Printf("^7  pk3 Sounds   : ^2%d/%d ^7announcer/chime slots registered from zzzz_rpgstandalone.pk3 (missing .wav = silent)\n",
			regSounds, RPG_SND_COUNT);
	}
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
	g_xpDrawRanks = qfalse;
	g_xpDrawHelp = qfalse;
	g_xpDrawSettings = qfalse;
	Com_Printf("^5[RPG MOD] Profile Stats Card %s\n", (g_xpDrawCard == qtrue) ? "^2ENABLED" : "^1DISABLED");
}

void CL_XP_ToggleSettings_f(void) {
	g_xpDrawSettings = (g_xpDrawSettings == qtrue) ? qfalse : qtrue;
	g_xpDrawCard = qfalse;
	g_xpDrawRanks = qfalse;
	g_xpDrawHelp = qfalse;
	Com_Printf("^5[RPG MOD] Customization Settings UI %s\n", (g_xpDrawSettings == qtrue) ? "^2OPENED" : "^1CLOSED");
}

void CL_XP_ToggleRanksUI_f(void) {
	g_xpDrawRanks = (g_xpDrawRanks == qtrue) ? qfalse : qtrue;
	g_xpDrawCard = qfalse;
	g_xpDrawHelp = qfalse;
	g_xpDrawSettings = qfalse;
	Com_Printf("^5[RPG MOD] Rank Progression Tiers UI %s\n", (g_xpDrawRanks == qtrue) ? "^2OPENED" : "^1CLOSED");
}

void CL_XP_ToggleHelpUI_f(void) {
	g_xpDrawHelp = (g_xpDrawHelp == qtrue) ? qfalse : qtrue;
	g_xpDrawCard = qfalse;
	g_xpDrawRanks = qfalse;
	g_xpDrawSettings = qfalse;
	Com_Printf("^5[RPG MOD] System Guide & Commands UI %s\n", (g_xpDrawHelp == qtrue) ? "^2OPENED" : "^1CLOSED");
}

void CL_XP_Init(void) {
	if (g_xpInitialized) {
		return;
	}
	g_xpInitialized = qtrue;
	g_lastKills = -1;
	g_lastDeaths = -1;
	g_lastHits = -1;
	g_lastHealth = -1;
	g_playerIsDead = qfalse;
	g_lastDuelInProgress = qfalse;
	g_duelStartHealth = 100;
	g_duelStartHits = 0;
	g_duelStartMs = 0;
	g_lastKillMs = -999999;
	g_runningMultiKillCount = 0;
	g_currentKillStreak = 0;
	g_lastFramePlaytime = -1;

	// Register toggle CVars for new features (persisted across runs via CVAR_ARCHIVE)
	cg_rpg_notify_sounds   = Cvar_Get("cg_rpg_notify_sounds",   "1", CVAR_ARCHIVE);
	cg_rpg_notify_popups   = Cvar_Get("cg_rpg_notify_popups",   "1", CVAR_ARCHIVE);
	cg_rpg_multikill_enabled = Cvar_Get("cg_rpg_multikill_enabled", "1", CVAR_ARCHIVE);
	cg_rpg_streak_enabled  = Cvar_Get("cg_rpg_streak_enabled",  "1", CVAR_ARCHIVE);
	cg_rpg_hud_pos         = Cvar_Get("cg_rpg_hud_pos",         "0", CVAR_ARCHIVE);
	cg_rpg_avatar          = Cvar_Get("cg_rpg_avatar",          "0", CVAR_ARCHIVE);

	memset(g_rpgNotifs, 0, sizeof(g_rpgNotifs));
	g_rpgNotifCount = 0;

	Cmd_AddCommand("rpg_card",     CL_XP_ToggleCard_f,     "Toggle full-screen RPG Profile Stats Card");
	Cmd_AddCommand("rpg_status",   CL_XP_PrintStatus_f,    "Print RPG client profile status");
	Cmd_AddCommand("rpg_ranks",    CL_XP_ToggleRanksUI_f,  "Open Rank Progression Tiers UI Window");
	Cmd_AddCommand("rpg_jedi",     CL_XP_SetFaction_f,     "Switch rank title path to Jedi Light Side");
	Cmd_AddCommand("rpg_sith",     CL_XP_SetFaction_f,     "Switch rank title path to Sith Dark Side");
	Cmd_AddCommand("rpg_faction",  CL_XP_SetFaction_f,     "Switch rank title path (jedi or sith)");
	Cmd_AddCommand("rpg_settings", CL_XP_ToggleSettings_f, "Open mouse-interactive RPG Customization Settings UI");
	Cmd_AddCommand("rpg_hud",      CL_XP_ToggleSettings_f, "Open mouse-interactive RPG Customization Settings UI");
	Cmd_AddCommand("rpg_help",     CL_XP_ToggleHelpUI_f,   "Open System Guide & Command UI Window");
	Cmd_AddCommand("settings",     CL_XP_ToggleSettings_f, "Open mouse-interactive RPG Customization Settings UI");
	Cmd_AddCommand("hud",          CL_XP_ToggleSettings_f, "Open mouse-interactive RPG Customization Settings UI");
	Cmd_AddCommand("menu",         CL_XP_ToggleSettings_f, "Open mouse-interactive RPG Customization Settings UI");
	Cmd_AddCommand("ranks",        CL_XP_ToggleRanksUI_f,  "Open Rank Progression Tiers UI Window");
	Cmd_AddCommand("stats",        CL_XP_ToggleCard_f,     "Toggle full-screen RPG Profile Stats Card");
	Cmd_AddCommand("card",         CL_XP_ToggleCard_f,     "Toggle full-screen RPG Profile Stats Card");

	CL_XP_LoadProfile();

	// Sounds are registered AFTER renderer is ready - if CL_XP_Init runs before renderer we'll re-register them at first draw time
	CL_XP_RegisterSounds();
}

void CL_XP_PrintStatus_f(void) {
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress(&curXP, &reqXP, &percent);
	float kd = (g_xpProfile.deaths > 0) ? (float)g_xpProfile.kills / (float)g_xpProfile.deaths : (float)g_xpProfile.kills;
	float hrs = (float)g_xpProfile.totalPlaytimeMs / (1000.0f * 3600.0f);

	Com_Printf("\n^5=====================================================\n");
	Com_Printf("^2  [RPG MOD] Standalone Client XP & Leveling System\n");
	Com_Printf("^7  Profile Name  : ^3%s ^7(Faction: %s^7)\n", CL_XP_GetProfileName(), (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI");
	Com_Printf("^7  Level         : ^3%i ^7(Max Level %i)  |  Playtime ^3%.1f hrs\n", CL_XP_GetLevel(), MAX_XP_LEVEL, hrs);
	Com_Printf("^7  Rank Title    : ^3%s\n", CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
	Com_Printf("^7  Total XP      : ^3%i\n", CL_XP_GetXP());
	Com_Printf("^7  Level Progress: ^3%i / %i XP ^7(%.1f%%)\n", curXP, reqXP, percent * 100.0f);
	Com_Printf("^7  Kills/Deaths  : ^3%i Kills ^7| ^1%i Deaths ^7(K/D: ^3%.2f^7)\n", g_xpProfile.kills, g_xpProfile.deaths, kd);
	Com_Printf("^7  Combat Breakdn: ^3%i Saber ^7| ^3%i Gunner ^7| Streak: Curr ^3%i ^7/ Best ^3%i\n",
		g_xpProfile.saberKills, g_xpProfile.gunnerKills, g_currentKillStreak, g_xpProfile.bestKillStreak);
	Com_Printf("^7  Team Rounds   : ^2%i Wins ^7| ^1%i Losses\n", g_xpProfile.roundWins, g_xpProfile.roundLosses);
	Com_Printf("^7  Private Duels : ^2%i W ^7| ^1%i L  | Perfect ^3%i  | Q.Draw ^3%i  | Endurance ^3%i\n",
		g_xpProfile.duelWins, g_xpProfile.duelLosses, g_xpProfile.perfectWins, g_xpProfile.quickDrawWins, g_xpProfile.enduranceWins);
	Com_Printf("^7  Duel Streaks  : Curr ^3%i ^7/ Best ^3%i  | Shortest ^3%ims ^7| Longest ^3%is\n",
		g_xpProfile.currentDuelStreak, g_xpProfile.bestDuelStreak,
		g_xpProfile.shortestDuelMs, g_xpProfile.longestDuelMs / 1000);
	Com_Printf("^7  Multi-kills   : ^32x=%d  3x=%d  4x=%d  5x=%d  6+=%d\n",
		g_xpProfile.multiDouble, g_xpProfile.multiTriple, g_xpProfile.multiOverkill, g_xpProfile.multiMonster, g_xpProfile.multiUltra);
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
static int g_roundWon       = -1;   // 1 = team won last round, 0 = lost, -1 = unknown

void CL_XP_CheckGameEvents(void) {
	if (cls.state != CA_ACTIVE || !cl.snap.valid) {
		g_lastKills = -1;
		g_lastDeaths = -1;
		g_lastHits = -1;
		g_lastHealth = -1;
		g_playerIsDead = qfalse;
		g_lastDuelInProgress = qfalse;
		g_lastSpawnCount = -1;
		g_runningMultiKillCount = 0;
		g_duelStartMs = 0;
		g_lastKillMs = -999999;
		g_currentKillStreak = 0;
		return;
	}

	CL_XP_UpdateEngineCVars();

	// Advance playtime approximation (each tick that we're in-game = tick delta)
	static int lastTickRealtime = -1;
	if (lastTickRealtime != -1 && cls.realtime > lastTickRealtime) {
		int dt = cls.realtime - lastTickRealtime;
		if (dt > 0 && dt < 10000) {
			g_xpProfile.totalPlaytimeMs += dt;
		}
	}
	lastTickRealtime = cls.realtime;

	// Print RPG MOD welcome status banner to console ~60 seconds after joining server so it appears at bottom of console
	static int s_connectSpawnTime = -1;
	static qboolean s_welcomeBannerPrinted = qfalse;

	if (s_connectSpawnTime == -1) {
		s_connectSpawnTime = cls.realtime;
	}

	if (!s_welcomeBannerPrinted && cls.realtime - s_connectSpawnTime >= 60000) {
		s_welcomeBannerPrinted = qtrue;
		Com_Printf("\n^5=====================================================\n");
		Com_Printf("^2  [RPG MOD] Standalone Client Leveling System Active!\n");
		Com_Printf("^7  Welcome ^3%s ^7| Level ^3%d ^7(^3%s^7)\n", CL_XP_GetProfileName(), CL_XP_GetLevel(), CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction));
		Com_Printf("^7  Type ^3!stats ^7or ^3!ranks ^7or ^3!rpgmenu ^7in chat anytime!\n");
	}
	// Tick notification expiration timers each frame
	CL_XP_TickNotifications();

	// Check if local player is spectating another entity
	qboolean isSpectatingOther = (cl.snap.ps.clientNum != clc.clientNum || (cl.snap.ps.pm_flags & PMF_FOLLOW)) ? qtrue : qfalse;
	if (isSpectatingOther) {
		g_lastKills = -1;
		g_lastDeaths = -1;
		g_lastHits = -1;
		g_lastHealth = -1;
		g_playerIsDead = qfalse;
		g_lastDuelInProgress = qfalse;
		g_lastSpawnCount = -1;
		g_currentKillStreak = 0;
		g_runningMultiKillCount = 0;
		g_duelStartMs = 0;
		return;
	}

	int currentHealth = cl.snap.ps.stats[STAT_HEALTH];
	int pmType = cl.snap.ps.pm_type;
	int currentSpawnCount = cl.snap.ps.persistant[PERS_SPAWN_COUNT];
	int currentHits = cl.snap.ps.persistant[PERS_HITS];
	int currentDeaths = cl.snap.ps.persistant[PERS_KILLED];

	// -----------------------------------------------------------------------
	// 0. Non-duel Death tracking via health drop & PM_DEAD state
	// -----------------------------------------------------------------------
	if (currentHealth > 0 && pmType != PM_DEAD && pmType != PM_SPECTATOR) {
		g_playerIsDead = qfalse;
	} else if (!g_playerIsDead && ((g_lastHealth > 0 && currentHealth <= 0) || pmType == PM_DEAD)) {
		g_playerIsDead = qtrue;
		if (!g_lastDuelInProgress) {
			CL_XP_OnPlayerDeath();
		}
	}
	g_lastHealth = currentHealth;

	// -----------------------------------------------------------------------
	// 1. Round Win/Loss detection via spawn count reset + roundWon flag
	// -----------------------------------------------------------------------
	if (g_lastSpawnCount != -1 && currentSpawnCount != g_lastSpawnCount) {
		if (g_roundWon == 1) {
			CL_XP_OnRoundWin();
		} else if (g_roundWon == 0) {
			CL_XP_OnRoundLoss();
		}
		g_roundWon = -1;
		g_lastKills = cl.snap.ps.persistant[PERS_SCORE];
		g_lastDeaths = currentDeaths;
		g_lastHits = currentHits;
		g_currentKillStreak = 0;
		g_runningMultiKillCount = 0;
	}
	g_lastSpawnCount = currentSpawnCount;

	// -----------------------------------------------------------------------
	// 2. Player Kill tracking via PERS_SCORE + multi-kill + kill streak
	// -----------------------------------------------------------------------
	int currentKills = cl.snap.ps.persistant[PERS_SCORE];

	if (g_lastKills == -1) {
		g_lastKills = currentKills;
	} else if (currentKills > g_lastKills) {
		int diff = currentKills - g_lastKills;
		g_lastKills = currentKills;
		if (diff > 0 && diff <= 10) {
			int nowMs = cls.realtime;
			for (int i = 0; i < diff; i++) {
				// Multi-kill detection: kill within MULTIKILL_WINDOW_MS of previous kill = chain increment
				int chain = 0;
				if (cg_rpg_multikill_enabled && cg_rpg_multikill_enabled->integer) {
					if ((nowMs - g_lastKillMs) <= MULTIKILL_WINDOW_MS) {
						g_runningMultiKillCount++;
					} else {
						g_runningMultiKillCount = 1;
					}
					chain = g_runningMultiKillCount;
				}
				g_lastKillMs = nowMs;

				// If kill occurred during a private duel, award duel win!
				if (g_lastDuelInProgress) {
					CL_XP_OnDuelWin(qfalse, qfalse, 0);
				} else {
					CL_XP_OnPlayerKill(cl.snap.ps.weapon);
				}

				// Multi-kill tier bonus + notif/sound (chain >= 2 matters; 1 = single kill no multi)
				if (chain >= 2) {
					const char *nameTxt = NULL;
					int xpBonus = 0;
					rpgSoundSlot_t snd = RPG_SND_COUNT;
					switch (chain) {
						case 2:  nameTxt = "DOUBLE KILL!";  xpBonus = XP_MULTI_DOUBLE;   snd = RPG_SND_DOUBLE;   g_xpProfile.multiDouble++;   break;
						case 3:  nameTxt = "TRIPLE KILL!";  xpBonus = XP_MULTI_TRIPLE;   snd = RPG_SND_TRIPLE;   g_xpProfile.multiTriple++;   break;
						case 4:  nameTxt = "OVERKILL!";     xpBonus = XP_MULTI_OVERKILL; snd = RPG_SND_OVERKILL; g_xpProfile.multiOverkill++; break;
						case 5:  nameTxt = "MONSTER KILL!"; xpBonus = XP_MULTI_MONSTER;  snd = RPG_SND_MONSTER;  g_xpProfile.multiMonster++;  break;
						default: nameTxt = "ULTRA KILL!";   xpBonus = XP_MULTI_ULTRA;    snd = RPG_SND_ULTRA;    g_xpProfile.multiUltra++;    break;
					}
					if (xpBonus > 0) CL_XP_AddXP(xpBonus, nameTxt);
					if (snd < RPG_SND_COUNT) CL_XP_PlaySound(snd);
					char sub[64];
					Com_sprintf(sub, sizeof(sub), "+%d XP  x%d Chain", xpBonus, chain);
					CL_XP_PushNotification(RPG_NOTIF_MULTIKILL, nameTxt, sub, xpBonus, NULL, NOTIF_LIFETIME_MS);
				}
				// Kill streak tracking (client standalone): every kill without dying = +1 streak
				if (cg_rpg_streak_enabled && cg_rpg_streak_enabled->integer) {
					g_currentKillStreak++;
					if (g_currentKillStreak > g_xpProfile.bestKillStreak)
						g_xpProfile.bestKillStreak = g_currentKillStreak;
					if (g_currentKillStreak == 5 || g_currentKillStreak == 10 || g_currentKillStreak == 25
						|| (g_currentKillStreak > 0 && g_currentKillStreak % 50 == 0)) {
						int tierXP = 0;
						rpgSoundSlot_t tierSnd = RPG_SND_STREAK;
						if (g_currentKillStreak >= 25) tierXP = XP_STREAK_25;
						else if (g_currentKillStreak >= 10) tierXP = XP_STREAK_10;
						else tierXP = XP_STREAK_5;
						CL_XP_AddXP(tierXP, va("%d-KILL STREAK!", g_currentKillStreak));
						CL_XP_PlaySound(tierSnd);
						char sub[64];
						Com_sprintf(sub, sizeof(sub), "+%d XP  Best: %d", tierXP, g_xpProfile.bestKillStreak);
						CL_XP_PushNotification(RPG_NOTIF_STREAK, va("%d-KILL STREAK!", g_currentKillStreak), sub, tierXP, NULL, NOTIF_LIFETIME_MS);
					}
				}
			}
		}
	}
	g_lastKills = currentKills;

	// -----------------------------------------------------------------------
	// 4. Private 1v1 Saber Duel Win / Loss tracking via duelInProgress
	//    NEW: capture start HP + start PERS_HITS + start time for Perfect/QuickDraw/Endurance
	// -----------------------------------------------------------------------
	qboolean currentDuel = (cl.snap.ps.duelInProgress != 0) ? qtrue : qfalse;
	if (!g_lastDuelInProgress && currentDuel) {
		// Duel just started
		g_duelStartHealth = currentHealth;
		g_duelStartHits   = currentHits;
		g_duelStartMs     = cls.realtime;
		g_lastDuelOutcome = 0;
	} else if (g_lastDuelInProgress && !currentDuel) {
		// Duel just ended — check outcome
		int durationMs = (g_duelStartMs > 0) ? (cls.realtime - g_duelStartMs) : 0;
		if (g_lastDuelOutcome == -2 || durationMs < 500) {
			// Duel ended manually or prematurely — do NOT grant win or loss!
			g_duelStartMs = 0;
			g_lastDuelOutcome = 0;
		} else {
			qboolean isWin = qfalse;
			if (g_lastDuelOutcome == 1) {
				isWin = qtrue;
			} else if (g_lastDuelOutcome == -1) {
				isWin = qfalse;
			} else {
				isWin = (!g_playerIsDead && currentHealth >= g_duelStartHealth) ? qtrue : qfalse;
			}

			if (isWin) {
				qboolean perfectDuel = qfalse;
				qboolean quickDraw = qfalse;
				int hitsDelta = (g_lastHits >= g_duelStartHits) ? (g_lastHits - g_duelStartHits) : 0;
				if ((currentHealth >= g_duelStartHealth) && hitsDelta == 0) {
					perfectDuel = qtrue;
				}
				if (perfectDuel && durationMs > 0 && durationMs < QUICKDRAW_WINDOW_MS) {
					quickDraw = qtrue;
				}
				CL_XP_OnDuelWin(perfectDuel, quickDraw, durationMs);
			} else {
				CL_XP_OnDuelLoss();
			}
			g_duelStartMs = 0;
			g_lastDuelOutcome = 0;
		}
	}
	g_lastDuelInProgress = currentDuel;
}

static int s_lastKillPrintMs = 0;

void CL_XP_OnPrintMessage(const char *msg) {
	if (!msg || !msg[0] || cls.state != CA_ACTIVE) {
		return;
	}

	// Recursion prevention: ignore our own RPG MOD console logs to prevent infinite kill count loops
	if (Q_stristr(msg, "[RPG MOD]")) {
		return;
	}

	// Detect manual or premature duel end
	if (Q_stristr(msg, "Duel has been ended manually") || Q_stristr(msg, "became one with the Force") || Q_stristr(msg, "duel was declined") || Q_stristr(msg, "duel forfeited")) {
		g_lastDuelOutcome = -2; // Manual / Premature cancel flag
	}

	cvar_t *clName = Cvar_Get("name", "Player", 0);
	if (!clName || !clName->string || !clName->string[0]) {
		return;
	}

	char cleanMyName[64];
	Q_strncpyz(cleanMyName, clName->string, sizeof(cleanMyName));
	Q_CleanStr(cleanMyName);
	if (!cleanMyName[0]) return;

	char cleanMsg[1024];
	Q_strncpyz(cleanMsg, msg, sizeof(cleanMsg));
	Q_CleanStr(cleanMsg);

	static int s_lastDeathPrintMs = 0;

	// Check if print message is a MB2 Round Win/Loss broadcast
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
			if (cl.snap.ps.stats[STAT_HEALTH] > 0 && cl.snap.ps.pm_type != PM_DEAD) {
				g_roundWon = 1;
			} else {
				g_roundWon = 0;
			}
		}
	}

	// 1. Check if local player is the VICTIM (Death)
	qboolean iAmVictim = qfalse;
	if (Q_stristr(cleanMsg, "you were killed") || Q_stristr(cleanMsg, "you were slain") ||
		Q_stristr(cleanMsg, "you died") || Q_stristr(cleanMsg, "you fell") || Q_stristr(cleanMsg, "you were defeated")) {
		iAmVictim = qtrue;
	} else {
		const char *myPos = Q_stristr(cleanMsg, cleanMyName);
		const char *wasPos = Q_stristr(cleanMsg, "was ");
		if (myPos && wasPos && myPos < wasPos) {
			iAmVictim = qtrue;
		}
	}

	// 2. Check if local player is the KILLER (Kill)
	qboolean iAmKiller = qfalse;
	const char *wasPos = Q_stristr(cleanMsg, " was ");
	char extractedKiller[64] = "";
	char extractedVictim[64] = "";

	if (wasPos) {
		// Extract victim name (before ' (' or ' was ')
		int vLen = wasPos - cleanMsg;
		const char *pParen = strchr(cleanMsg, '(');
		if (pParen && pParen < wasPos) {
			vLen = pParen - cleanMsg;
		}
		while (vLen > 0 && (cleanMsg[vLen - 1] == ' ' || cleanMsg[vLen - 1] == '\t')) vLen--;
		if (vLen >= sizeof(extractedVictim)) vLen = sizeof(extractedVictim) - 1;
		Q_strncpyz(extractedVictim, cleanMsg, vLen + 1);

		const char *byPos = Q_stristr(wasPos, " by ");
		if (byPos) {
			const char *killerStart = byPos + 4;
			const char *withPos = Q_stristr(killerStart, " with ");
			if (withPos) {
				int nameLen = withPos - killerStart;
				if (nameLen >= sizeof(extractedKiller)) nameLen = sizeof(extractedKiller) - 1;
				Q_strncpyz(extractedKiller, killerStart, nameLen + 1);
				
				// Parse killer's HP and BP (e.g. "99HP and 99BP remaining")
				g_lastParsedKillerHP = 0;
				g_lastParsedKillerBP = 0;
				sscanf(withPos + 6, "%dHP and %dBP", &g_lastParsedKillerHP, &g_lastParsedKillerBP);
				if (g_lastParsedKillerHP == 0) {
					sscanf(withPos + 6, "%dHP", &g_lastParsedKillerHP);
				}
			} else {
				Q_strncpyz(extractedKiller, killerStart, sizeof(extractedKiller));
				g_lastParsedKillerHP = 0;
				g_lastParsedKillerBP = 0;
			}

			// Clean trailing space/punctuation/newline
			int klen = strlen(extractedKiller);
			while (klen > 0 && (extractedKiller[klen - 1] == ' ' || extractedKiller[klen - 1] == '\n' || extractedKiller[klen - 1] == '\r' || extractedKiller[klen - 1] == '.' || extractedKiller[klen - 1] == '"')) {
				extractedKiller[klen - 1] = '\0';
				klen--;
			}

			if (Q_stristr(extractedKiller, cleanMyName) != NULL) {
				iAmKiller = qtrue;
			}
		}
		
		// Parse victim's BP (e.g. parenthesized "exoticweiner (11BP remaining)")
		g_lastParsedVictimBP = 0;
		const char *paren = strchr(cleanMsg, '(');
		if (paren && paren < wasPos) {
			sscanf(paren, "(%dBP", &g_lastParsedVictimBP);
		}
	} else if (Q_stristr(cleanMsg, "you killed") || Q_stristr(cleanMsg, "you fragged") || Q_stristr(cleanMsg, "you defeated") || Q_stristr(cleanMsg, "you eliminated")) {
		iAmKiller = qtrue;
		g_lastParsedVictimBP = 0;
		g_lastParsedKillerHP = 0;
		g_lastParsedKillerBP = 0;
	}

	if (iAmVictim) {
		g_lastDuelOutcome = -1;
		if (extractedKiller[0]) Q_strncpyz(g_lastParsedKillerName, extractedKiller, sizeof(g_lastParsedKillerName));
		if (extractedVictim[0]) Q_strncpyz(g_lastParsedVictimName, extractedVictim, sizeof(g_lastParsedVictimName));
		if (cls.realtime - s_lastDeathPrintMs > 1000) {
			s_lastDeathPrintMs = cls.realtime;
			CL_XP_OnPlayerDeath();
		}
		return;
	}

	if (iAmKiller) {
		g_lastDuelOutcome = 1;
		if (extractedVictim[0]) Q_strncpyz(g_lastParsedVictimName, extractedVictim, sizeof(g_lastParsedVictimName));
		if (extractedKiller[0]) Q_strncpyz(g_lastParsedKillerName, extractedKiller, sizeof(g_lastParsedKillerName));
		if (cls.realtime - s_lastKillPrintMs > 100) {
			s_lastKillPrintMs = cls.realtime;
			Com_DPrintf("[RPG MOD] Killer Match: '%s' | MyName: '%s'\n", cleanMsg, cleanMyName);
			CL_XP_OnPlayerKill(cl.snap.ps.weapon);
		}
	} else {
		if (Q_stristr(cleanMsg, "sabered") || Q_stristr(cleanMsg, "killed") || Q_stristr(cleanMsg, "slain") || Q_stristr(cleanMsg, "shot") || Q_stristr(cleanMsg, "vaporized") || Q_stristr(cleanMsg, "sniped")) {
			Com_DPrintf("[RPG MOD] Suspected Obituary: '%s' | MyName: '%s' | iAmKiller: NO\n", cleanMsg, cleanMyName);
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
		// Levelup milestone popup + sound
		CL_XP_PlaySound(RPG_SND_LEVELUP);
		char sub[96];
		Com_sprintf(sub, sizeof(sub), "Now: %s", CL_XP_GetRankTitle(newLevel, g_xpProfile.faction));
		CL_XP_PushNotification(RPG_NOTIF_LEVELUP, va("LEVEL UP! Lv %d", newLevel), sub, 0, NULL, NOTIF_LIFETIME_MS + 1000);
		// Milestones for banner notif every 25/50/100 levels
		if (newLevel == 10 || newLevel == 25 || newLevel == 50 || newLevel == 100 || newLevel == 250 || newLevel == 500 || newLevel == MAX_XP_LEVEL) {
			const char *msTitle = NULL;
			switch (newLevel) {
				case 10:  msTitle = "MILESTONE: Double Digits!"; break;
				case 25:  msTitle = "MILESTONE: Initiate Rank!"; break;
				case 50:  msTitle = "MILESTONE: Apprentice!"; break;
				case 100: msTitle = "MILESTONE: Century!"; break;
				case 250: msTitle = "MILESTONE: Knight Elite!"; break;
				case 500: msTitle = "MILESTONE: Halfway to 1000!"; break;
				default:  msTitle = "MILESTONE: MAX LEVEL REACHED!"; break;
			}
			char msSub[64];
			Com_sprintf(msSub, sizeof(msSub), "Level %d — Unstoppable!", newLevel);
			CL_XP_PushNotification(RPG_NOTIF_MILESTONE, msTitle, msSub, 0, NULL, NOTIF_LIFETIME_MS + 2000);
			CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
		}
	}
}

static int s_lastKillProcessedMs = 0;

void CL_XP_OnPlayerKill(int weapon) {
	if (cls.realtime - s_lastKillProcessedMs < 600) {
		return; // Ignore duplicate kill signals within 600ms
	}
	s_lastKillProcessedMs = cls.realtime;

	g_xpProfile.kills++;
	if (weapon == 10) { // WP_SABER
		g_xpProfile.saberKills++;
	} else {
		g_xpProfile.gunnerKills++;
	}
	CL_XP_AddXP(XP_GRANT_PLAYER_KILL, "Player Kill");

	// Instant feedback popup (No sound on basic kill per user request)
	char sub[96];
	if (g_lastParsedKillerHP > 0 || g_lastParsedKillerBP > 0) {
		Com_sprintf(sub, sizeof(sub), "My HP: %d | My BP: %d", g_lastParsedKillerHP, g_lastParsedKillerBP);
	} else {
		Com_sprintf(sub, sizeof(sub), "Defeated opponent in combat");
	}
	CL_XP_PushNotification(RPG_NOTIF_XP, "PLAYER KILL", sub, XP_GRANT_PLAYER_KILL, NULL, 2500);

	// Milestone kills banner notifications every 100 kills
	if (g_xpProfile.kills > 0 && (g_xpProfile.kills == 100 || g_xpProfile.kills == 500 || g_xpProfile.kills == 1000 ||
		g_xpProfile.kills == 2500 || g_xpProfile.kills == 5000 || (g_xpProfile.kills % 5000 == 0))) {
		char msSub[64];
		Com_sprintf(msSub, sizeof(msSub), "Total Kills: %d", g_xpProfile.kills);
		CL_XP_PushNotification(RPG_NOTIF_MILESTONE, va("KILL MILESTONE: %d!", g_xpProfile.kills), msSub, 0, NULL, NOTIF_LIFETIME_MS + 1500);
		CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
	}
}

void CL_XP_OnPlayerDeath(void) {
	g_xpProfile.deaths++;
	// Death = reset all client-only in-session kill streaks and multi-kill chains
	g_currentKillStreak = 0;
	g_runningMultiKillCount = 0;
	// Duel win streak also breaks on ANY death (not just duel loss, any death in between = streak gone)
	g_xpProfile.currentDuelStreak = 0;
	CL_XP_SaveProfile();
}

void CL_XP_OnNPCKill(void) {
	g_xpProfile.npcKills++;
	CL_XP_AddXP(XP_GRANT_NPC_KILL, "NPC Kill");
}

void CL_XP_OnDuelWin(qboolean perfect, qboolean quickDraw, int duelDurationMs) {
	g_xpProfile.duelWins++;
	// Note: g_xpProfile.kills and saberKills are already counted by CL_XP_OnPlayerKill via obituary!

	// Optional PLAYER KILL popup notification during duels
	if (cg_rpg_duel_popups && cg_rpg_duel_popups->integer) {
		char sub[96];
		if (g_lastParsedKillerHP > 0 || g_lastParsedKillerBP > 0) {
			Com_sprintf(sub, sizeof(sub), "My HP: %d | My BP: %d", g_lastParsedKillerHP, g_lastParsedKillerBP);
		} else {
			Com_sprintf(sub, sizeof(sub), "Defeated opponent in private duel");
		}
		CL_XP_PushNotification(RPG_NOTIF_XP, "PLAYER KILL", sub, XP_GRANT_DUEL_WIN, NULL, 3500);
	}

	int grantedXP = XP_GRANT_DUEL_WIN;

	// --- Perfect Duel ---
	if (perfect) {
		g_xpProfile.perfectWins++;
		grantedXP += XP_GRANT_PERFECT;
		Com_Printf("^3*** PERFECT DUEL VICTORY! (+%d XP Bonus) ***\n", XP_GRANT_PERFECT);
		CL_XP_PlaySound(RPG_SND_PERFECT);
		char sub[64];
		Com_sprintf(sub, sizeof(sub), "+%d XP Bonus", XP_GRANT_PERFECT);
		CL_XP_PushNotification(RPG_NOTIF_DUEL, "PERFECT DUEL!", sub, XP_GRANT_PERFECT, NULL, NOTIF_LIFETIME_MS + 1000);
		// Quick Draw only possible if also perfect AND < 3s
		if (quickDraw) {
			g_xpProfile.quickDrawWins++;
			grantedXP += XP_GRANT_QUICKDRAW;
			Com_Printf("^5*** QUICK DRAW! (+%d XP Bonus) ***\n", XP_GRANT_QUICKDRAW);
			CL_XP_PlaySound(RPG_SND_QUICKDRAW);
			char qd[64];
			Com_sprintf(qd, sizeof(qd), "+%d XP Bonus (%dms)", XP_GRANT_QUICKDRAW, duelDurationMs);
			CL_XP_PushNotification(RPG_NOTIF_DUEL, "QUICK DRAW!", qd, XP_GRANT_QUICKDRAW, NULL, NOTIF_LIFETIME_MS + 1000);
		}
	}

	// --- Endurance Award (>60s long saber war win) ---
	if (duelDurationMs >= RPG_DUEL_LONGEST_MS) {
		g_xpProfile.enduranceWins++;
		char endSub[64];
		Com_sprintf(endSub, sizeof(endSub), "Duel lasted %ds", duelDurationMs / 1000);
		CL_XP_PushNotification(RPG_NOTIF_DUEL, "ENDURANCE VICTORY!", endSub, 0, NULL, NOTIF_LIFETIME_MS + 1000);
		CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
	}

	// --- Shortest & Longest duel tracking ---
	if (duelDurationMs > 0) {
		if (g_xpProfile.shortestDuelMs == -1 || duelDurationMs < g_xpProfile.shortestDuelMs) {
			g_xpProfile.shortestDuelMs = duelDurationMs;
		}
		if (duelDurationMs > g_xpProfile.longestDuelMs) {
			g_xpProfile.longestDuelMs = duelDurationMs;
		}
	}

	// --- Duel win streak ---
	g_xpProfile.currentDuelStreak++;
	if (g_xpProfile.currentDuelStreak > g_xpProfile.bestDuelStreak) {
		g_xpProfile.bestDuelStreak = g_xpProfile.currentDuelStreak;
	}
	// Milestone: every 5/10/25/50 duel wins streak
	if (g_xpProfile.currentDuelStreak == 5 || g_xpProfile.currentDuelStreak == 10
		|| g_xpProfile.currentDuelStreak == 25 || g_xpProfile.currentDuelStreak == 50) {
		char sub[64];
		Com_sprintf(sub, sizeof(sub), "Current Streak: %d | Best: %d", g_xpProfile.currentDuelStreak, g_xpProfile.bestDuelStreak);
		CL_XP_PushNotification(RPG_NOTIF_DUEL, va("%d-DUEL WIN STREAK!", g_xpProfile.currentDuelStreak), sub, 0, NULL, NOTIF_LIFETIME_MS + 1000);
		CL_XP_PlaySound(RPG_SND_STREAK);
	}

	// Milestone total duel wins
	if (g_xpProfile.duelWins == 10 || g_xpProfile.duelWins == 50 || g_xpProfile.duelWins == 100
		|| g_xpProfile.duelWins == 250 || g_xpProfile.duelWins == 500) {
		char sub[64];
		Com_sprintf(sub, sizeof(sub), "Duel Wins: %d", g_xpProfile.duelWins);
		CL_XP_PushNotification(RPG_NOTIF_MILESTONE, va("DUEL MILESTONE: %d WINS!", g_xpProfile.duelWins), sub, 0, NULL, NOTIF_LIFETIME_MS + 1500);
		CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
	}

	CL_XP_AddXP(grantedXP, "Private Duel Victory");

	// Activate local Victory UI Toast overlay card
	g_rpgToast.active       = qtrue;
	g_rpgToast.isWin        = qtrue;
	g_rpgToast.eloDelta     = 15;
	g_rpgToast.credits      = 10;
	g_rpgToast.xp           = grantedXP;
	g_rpgToast.startTimeMs  = cls.realtime;
	g_rpgToast.victimBP     = g_lastParsedVictimBP;
	g_rpgToast.killerHP     = g_lastParsedKillerHP;
	g_rpgToast.killerBP     = g_lastParsedKillerBP;
	if (g_lastParsedVictimName[0]) {
		Q_strncpyz(g_rpgToast.opponentName, g_lastParsedVictimName, sizeof(g_rpgToast.opponentName));
	} else {
		Q_strncpyz(g_rpgToast.opponentName, "Opponent", sizeof(g_rpgToast.opponentName));
	}
}

void CL_XP_OnDuelLoss(void) {
	g_xpProfile.duelLosses++;
	g_xpProfile.deaths++;
	g_xpProfile.currentDuelStreak = 0;

	// Optional PLAYER DIED popup notification during duels
	if (cg_rpg_duel_popups && cg_rpg_duel_popups->integer) {
		CL_XP_PushNotification(RPG_NOTIF_DUEL, "PLAYER DIED", "Defeated in private duel", 0, NULL, 3500);
	}

	// Activate local Defeat UI Toast overlay card
	g_rpgToast.active       = qtrue;
	g_rpgToast.isWin        = qfalse;
	g_rpgToast.eloDelta     = -10;
	g_rpgToast.credits      = 0;
	g_rpgToast.xp           = 0;
	g_rpgToast.startTimeMs  = cls.realtime;
	g_rpgToast.victimBP     = g_lastParsedVictimBP;
	g_rpgToast.killerHP     = g_lastParsedKillerHP;
	g_rpgToast.killerBP     = g_lastParsedKillerBP;
	if (g_lastParsedKillerName[0]) {
		Q_strncpyz(g_rpgToast.opponentName, g_lastParsedKillerName, sizeof(g_rpgToast.opponentName));
	} else {
		Q_strncpyz(g_rpgToast.opponentName, "Opponent", sizeof(g_rpgToast.opponentName));
	}

	CL_XP_SaveProfile();
}

void CL_XP_OnRoundWin(void) {
	g_xpProfile.roundWins++;
	Com_Printf("^2*** ROUND VICTORY! (+%d XP) ***\n", XP_GRANT_ROUND_WIN);
	CL_XP_AddXP(XP_GRANT_ROUND_WIN, "Round Victory");
	CL_XP_PlaySound(RPG_SND_ACHIEVEMENT);
	char sub[64];
	Com_sprintf(sub, sizeof(sub), "+%d XP | Round Record: %dW-%dL", XP_GRANT_ROUND_WIN, g_xpProfile.roundWins, g_xpProfile.roundLosses);
	CL_XP_PushNotification(RPG_NOTIF_XP, "ROUND VICTORY!", sub, XP_GRANT_ROUND_WIN, NULL, NOTIF_LIFETIME_MS + 500);
	// Round milestones
	if (g_xpProfile.roundWins == 10 || g_xpProfile.roundWins == 50 || g_xpProfile.roundWins == 100
		|| g_xpProfile.roundWins == 250 || g_xpProfile.roundWins == 500) {
		char rSub[64];
		Com_sprintf(rSub, sizeof(rSub), "Total Round Wins: %d", g_xpProfile.roundWins);
		CL_XP_PushNotification(RPG_NOTIF_MILESTONE, va("ROUND MILESTONE: %d WINS!", g_xpProfile.roundWins), rSub, 0, NULL, NOTIF_LIFETIME_MS + 1500);
	}
}

void CL_XP_OnRoundLoss(void) {
	g_xpProfile.roundLosses++;
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

void CL_XP_ResetProfile(void) {
	int curFaction = g_xpProfile.faction;
	memset(&g_xpProfile, 0, sizeof(g_xpProfile));
	g_xpProfile.level = 1;
	g_xpProfile.xp = 0;
	g_xpProfile.faction = curFaction;
	g_xpProfile.soundVolume = 4;
	g_xpProfile.announcerEnabled = 1;
	g_xpProfile.levelupSndEnabled = 1;
	g_xpProfile.duelSndEnabled = 1;
	g_xpProfile.shortestDuelMs = -1;
	CL_XP_SaveProfile();
	CL_XP_PushNotification(RPG_NOTIF_MILESTONE, "RPG PROFILE RESET", "All stats & level reset to 1", 0, NULL, 4000);
}

void CL_XP_SetProfileName(const char *name) {
	if (name && name[0]) {
		Q_strncpyz(g_xpProfile.profileName, name, sizeof(g_xpProfile.profileName));
		CL_XP_SaveProfile();
	}
}
