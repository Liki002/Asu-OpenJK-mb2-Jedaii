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
#define XP_GRANT_PERFECT     50    // "Perfect Duel" = HP unchanged + 0 incoming hits (was FLAWLESS)
#define XP_GRANT_QUICKDRAW   25    // Quick Draw: Perfect + < 3.0s total duel duration
#define XP_GRANT_DUEL_KILL   50

// Multi-kill bonus XP (client-only standalone): small bonuses over the base +50
#define XP_MULTI_DOUBLE      10
#define XP_MULTI_TRIPLE      20
#define XP_MULTI_OVERKILL    40
#define XP_MULTI_MONSTER     60
#define XP_MULTI_ULTRA       80

// Kill streak bonus XP (client-only standalone): every tier
#define XP_STREAK_5          25
#define XP_STREAK_10         50
#define XP_STREAK_25         100

// Time windows (milliseconds, engine cls.realtime)
#define MULTIKILL_WINDOW_MS  3000   // kills within this window count as multi-kill chain
#define NOTIF_LIFETIME_MS    4000   // HUD popup notification lifetime
#define MAX_RPG_NOTIFS       4      // simultaneous notifications visible at once
#define QUICKDRAW_WINDOW_MS  3000   // < 3s duel = quick draw
#define RPG_DUEL_LONGEST_MS  60000  // > 60s duel = endurance award (optional)

// HUD popup notification types
typedef enum {
    RPG_NOTIF_XP = 0,
    RPG_NOTIF_MULTIKILL,
    RPG_NOTIF_STREAK,
    RPG_NOTIF_DUEL,
    RPG_NOTIF_LEVELUP,
    RPG_NOTIF_ACHIEVEMENT,
    RPG_NOTIF_MILESTONE,
    RPG_NOTIF_COUNT
} rpgNotifType_t;

// Sound slots (client engine registers these paths into pk3 sound/ on init; -1 = unregistered/missing)
typedef enum {
    RPG_SND_DOUBLE = 0,
    RPG_SND_TRIPLE,
    RPG_SND_OVERKILL,
    RPG_SND_MONSTER,
    RPG_SND_ULTRA,
    RPG_SND_LEVELUP,
    RPG_SND_PERFECT,
    RPG_SND_QUICKDRAW,
    RPG_SND_STREAK,
    RPG_SND_ACHIEVEMENT,
    RPG_SND_XP,
    RPG_SND_COUNT
} rpgSoundSlot_t;

// HUD notification entry (runtime only, not persisted to profile)
typedef struct {
    rpgNotifType_t type;
    char  text[128];
    char  subtext[96];
    int   xpDelta;       // 0 if no xp
    int   startMs;       // cls.realtime when created
    int   lifetimeMs;    // total lifetime (ms)
    vec4_t tint;         // color override (rgba 0-1)
} rpgNotif_t;

// Faction types
typedef enum {
    FACTION_JEDI = 0,
    FACTION_SITH = 1
} rpgFaction_t;

typedef struct {
    int level;
    int xp;
    int kills;       // Player Kills (+50 XP) - ALL modes including duels
    int deaths;      // Player Deaths
    int npcKills;    // NPC Kills (+20 XP)
    int duelWins;    // Private Duel Wins (+100 XP)
    int duelLosses;  // Private Duel Losses
    int faction;     // 0 = Jedi, 1 = Sith

    // Mode Stats (generic - wins / losses per team modes, XP for kills in ALL modes)
    int roundWins;
    int roundLosses;

    // Weapon/Style Breakdown
    int saberKills;    // Saber Kills
    int gunnerKills;   // Blaster / Gun Kills
    int perfectWins;   // Perfect Duels (HP unchanged + 0 incoming hits detected during duel)
    int quickDrawWins; // Quick Draw Perfect Duels (Perfect + < 3.0s total duration)
    int enduranceWins; // Endurance Duel Wins (any duel win > 60s duration = epic saber war)
    int currentDuelStreak;   // Current consecutive Duel Wins
    int bestDuelStreak;      // Lifetime best consecutive Duel Wins
    int shortestDuelMs;      // Fastest-ever duel win from start->win in ms (-1 = none recorded)
    int longestDuelMs;       // Slowest/longest duel win ever recorded (>60s = endurance)

    // Multi-kill lifetime counters (all-time totals)
    int multiDouble;      // Double kills
    int multiTriple;      // Triple kills
    int multiOverkill;    // Overkill (4 in chain)
    int multiMonster;     // Monster kill (5 in chain)
    int multiUltra;       // Ultra kill (6+ in chain)

    // Kill streak lifetime counters (all-time bests)
    int bestKillStreak;   // Lifetime best kill streak (no death between kills)

    // Playtime & session stats
    int totalPlaytimeMs;  // Lifetime total playtime (ms - approximate, not anti-cheat proof but good enough for stats)
    int lastDatePlayed;   // YYYYMMDD last seen - for daily resets if used later

    // Achievement Claim Flags (1 = claimed)
    int achSaberMasterClaimed;
    int achGunnerEliteClaimed;
    int achDuelSpecialistClaimed;
    int achQuickDrawClaimed;
    int achPerfectClaimed;
    int achStreakClaimed;
    int achCenturyClaimed;

    // Audio & Settings
    int soundVolume;      // 0 = Mute, 1 = 25%, 2 = 50%, 3 = 75%, 4 = 100% (default 4)
    int announcerEnabled; // 1 = ON (default), 0 = OFF
    int levelupSndEnabled;// 1 = ON (default), 0 = OFF
    int duelSndEnabled;   // 1 = ON (default), 0 = OFF
    int themeIndex;       // 0=Cyan, 1=Crimson, 2=Purple, 3=Gold, 4=Green, 5=Neon

    // Achievement tracking
    int achievements[128];

    char profileName[64];
    unsigned int checksum;
} clXpProfile_t;

// Runtime globals (not persisted, used each frame in engine)
extern rpgNotif_t g_rpgNotifs[MAX_RPG_NOTIFS];      // Ring buffer of active HUD popup notifications
extern int        g_rpgNotifCount;
extern int        g_rpgSoundHandles[RPG_SND_COUNT];  // S_RegisterSound handles (-1 if file missing)
extern int        g_rpgMenuTab;                      // Active tab in !rpgmenu (0=HUD, 1=Ach, 2=Audio, 3=Guide)
extern int        g_currentKillStreak;               // current in-game kill streak (reset on death)
extern int        g_lastKillMs;                      // cls.realtime of last player kill (for multi-kill window)
extern int        g_runningMultiKillCount;           // kills in current multi-kill chain (resets after 3s idle)
extern int        g_duelStartMs;                     // cls.realtime when current duel started (0 = no duel)
extern int        g_duelStartHealth;                 // HP at duel start snapshot (if duelInProgress)
extern int        g_duelStartHits;                   // PERS_HITS counter at duel start snapshot

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
void CL_XP_OnDuelWin(qboolean perfect, qboolean quickDraw, int duelDurationMs);
void CL_XP_OnDuelLoss(void);
void CL_XP_OnRoundWin(void);
void CL_XP_OnRoundLoss(void);
void CL_XP_ClaimAchievement(int achievementId);

// --- HUD Popup Notifications + Sounds (standalone client-only) ---
void CL_XP_PushNotification(rpgNotifType_t type, const char *text, const char *subtext,
                            int xpDelta, const vec4_t tint, int lifetimeMs);
void CL_XP_TickNotifications(void);                 // Call every frame from SCR_DrawRPGHUDOverlay
void CL_XP_DrawNotifications(void);                 // Call from SCR_DrawRPGHUDOverlay after positioning
void CL_XP_RegisterSounds(void);                    // Call once in CL_XP_Init after renderer is ready
void CL_XP_PlaySound(rpgSoundSlot_t slot);          // Safe no-op if sound file missing from pk3

// Draw helpers for cgame-independent screen (in SCR space)
void SCR_DrawRPGNotificationCard(const rpgNotif_t *notif, int posIndex, int visibleCount);

int  CL_XP_GetLevel(void);
int  CL_XP_GetXP(void);
int  CL_XP_GetRequiredXP(int level);
void CL_XP_GetLevelProgress(int *currentLevelXP, int *nextLevelXP, float *percent);

const char *CL_XP_GetProfileName(void);
void CL_XP_SetProfileName(const char *name);
const char *CL_XP_GetRankTitle(int level, int faction);

extern clXpProfile_t g_xpProfile;
extern qboolean g_xpDrawCard;
extern qboolean g_xpDrawRanks;
extern qboolean g_xpDrawHelp;
extern qboolean g_xpDrawSettings;
extern float g_rpgMouseX;
extern float g_rpgMouseY;
extern int g_roundWon;
extern cvar_t *cg_rpg_notify_sounds;
extern cvar_t *cg_rpg_notify_popups;
extern cvar_t *cg_rpg_hud_style;
extern cvar_t *cg_rpg_hud_pos;
extern cvar_t *cg_rpg_avatar;
extern cvar_t *cg_rpg_toast_pos;
extern cvar_t *cg_rpg_notif_pos;
extern cvar_t *cg_rpg_duel_popups;

extern int g_lastParsedVictimBP;
extern int g_lastParsedKillerHP;
extern int g_lastParsedKillerBP;
extern cvar_t *cg_rpg_multikill_enabled;
extern cvar_t *cg_rpg_streak_enabled;
