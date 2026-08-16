#include "sv_ranked_logic.h"
#include "cJSON.h"
#include "server.h"
#include "sv_gameapi.h"
#include "sv_ranked_db.h"
#include <math.h>
#include <ctype.h>
#include "../game/bg_weapons.h"
#include "../game/bg_public.h"
#include "../game/anims.h"

extern cJSON *accountsDB;

rankedParty_t sv_rankedParties[64];

rankedParty_t *SV_Ranked_FindPlayerParty(int clientNum) {
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) return NULL;
    for (int i = 0; i < 64; i++) {
        rankedParty_t *p = &sv_rankedParties[i];
        if (p->active) {
            for (int m = 0; m < p->memberCount; m++) {
                if (p->clientNums[m] == clientNum) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

void SV_Ranked_UpdateParty(int leaderId) {
    if (leaderId < 0 || leaderId >= sv_maxclients->integer) return;
    rankedParty_t *p = &sv_rankedParties[leaderId];
    if (!p->active) {
        for (int i = 0; i < p->memberCount; i++) {
            int cid = p->clientNums[i];
            if (cid >= 0 && cid < sv_maxclients->integer && svs.clients[cid].state >= CS_ACTIVE) {
                SV_SendServerCommand(svs.clients + cid, "party_clear");
            }
        }
        return;
    }

    char infoBuf[MAX_STRING_CHARS];
    Com_sprintf(infoBuf, sizeof(infoBuf), "party_info \"%s\" %d %d %d", p->teamName, p->teamColorIdx, p->score, p->memberCount);

    for (int i = 0; i < p->memberCount; i++) {
        int cid = p->clientNums[i];
        if (cid >= 0 && cid < sv_maxclients->integer && svs.clients[cid].state >= CS_ACTIVE) {
            rankedMatchState_t *r = &sv_rankedPlayers[cid];

            int level = 1;
            if (r->loggedIn && r->username[0]) {
                cJSON *acc = SV_Ranked_GetAccount(r->username);
                cJSON *xpItem = acc ? cJSON_GetObjectItemCaseSensitive(acc, "xp") : NULL;
                if (xpItem && cJSON_IsNumber(xpItem)) {
                    level = SV_Ranked_CalculateLevel(xpItem->valueint);
                }
            }

            int hp = 100;
            int fp = 100;
            int bp = 100;
            playerState_t *ps = SV_GameClientNum(cid);
            if (ps) {
                hp = ps->stats[STAT_HEALTH];
                fp = ps->fd.forcePower;
                bp = (ps->jetpackFuel > 0) ? ps->jetpackFuel : (ps->cloakFuel > 0 ? ps->cloakFuel : ((r->lastBP > 0) ? r->lastBP : ((ps->stats[STAT_ARMOR] > 0) ? ps->stats[STAT_ARMOR] : 100)));
            } else if (r->lastBP > 0) {
                bp = r->lastBP;
            }

            char cleanName[64];
            Q_strncpyz(cleanName, svs.clients[cid].name, sizeof(cleanName));
            for (char *q = cleanName; *q; q++) {
                if (*q == '"') *q = '\'';
            }

            char memStr[128];
            Com_sprintf(memStr, sizeof(memStr), " %d \"%s\" %d %d 100 %d 100 %d 100",
                        cid, cleanName, level, hp, fp, bp);
            Q_strcat(infoBuf, sizeof(infoBuf), memStr);
        }
    }




    for (int i = 0; i < p->memberCount; i++) {
        int cid = p->clientNums[i];
        if (cid >= 0 && cid < sv_maxclients->integer && svs.clients[cid].state >= CS_ACTIVE) {
            SV_SendServerCommand(svs.clients + cid, infoBuf);
        }
    }
}

void SV_Ranked_Party_Heartbeat(void) {
    static int lastCheck = 0;
    if (svs.time - lastCheck < 500) return; // Sync every 500ms
    lastCheck = svs.time;

    for (int i = 0; i < sv_maxclients->integer; i++) {
        rankedParty_t *p = &sv_rankedParties[i];
        if (p->active && p->memberCount > 0) {
            SV_Ranked_UpdateParty(i);
        }
    }
}



/*
==================
SV_Ranked_GetLogUsername
Helper to format a player's username for logging. If the player is a guest,
returns "guest_<CleanedName>" (alphanumeric only, spaces/tabs replaced with single underscore).
==================
*/
static void SV_Ranked_GetLogUsername(int clientNum, char *out, int outSize) {
  if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
    Q_strncpyz(out, "unknown", outSize);
    return;
  }
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (r->loggedIn && r->username[0]) {
    Q_strncpyz(out, r->username, outSize);
  } else {
    char cleanName[MAX_NETNAME];
    Q_strncpyz(cleanName, svs.clients[clientNum].name, sizeof(cleanName));
    
    // Strip colors in place
    Q_CleanStr(cleanName);
    
    // Build single-word representation
    char cleanedWord[MAX_NETNAME];
    int k = 0;
    for (int i = 0; cleanName[i] && k < (int)sizeof(cleanedWord) - 1; i++) {
      char c = cleanName[i];
      if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') {
        cleanedWord[k++] = c;
      } else if (c == ' ' || c == '\t') {
        if (k > 0 && cleanedWord[k-1] != '_') {
          cleanedWord[k++] = '_';
        }
      }
    }
    cleanedWord[k] = '\0';
    
    if (k > 0 && cleanedWord[k-1] == '_') {
      cleanedWord[k-1] = '\0';
    }
    
    if (cleanedWord[0] == '\0') {
      Com_sprintf(out, outSize, "guest_%d", clientNum);
    } else {
      Com_sprintf(out, outSize, "guest_%s", cleanedWord);
    }
  }
}


// ---------------------------------------------------------------------------
// File-scope round state (reset each round)
// ---------------------------------------------------------------------------
static qboolean sv_ranked_firstBlood = qfalse;



/*
==================
SV_Ranked_Logic_Init
==================
*/
void SV_Ranked_Logic_Init(void) {
  sv_ranked_firstBlood = qfalse;
  Com_Printf("[RANKED] Logic initialized.\n");
}

/*
==================
SV_Ranked_Logic_Shutdown
==================
*/
void SV_Ranked_Logic_Shutdown(void) {
  Com_Printf("[RANKED] Logic shutdown.\n");
}

/*
==================
IsThermalMod  – used for bomb-streak detection
==================
*/
static qboolean IsThermalMod(int mod) {
  return (mod == 23 || mod == 24) ? qtrue : qfalse;
}

/*
==================
SV_Ranked_BroadcastOpen
Helper to send centerprints/chats only to players NOT in a private duel.
==================
*/
static void SV_Ranked_BroadcastOpen(const char *fmt, ...) {
  char msg[1024];
  va_list argptr;
  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (svs.clients[i].state && !sv_rankedPlayers[i].inDuel) {
      SV_SendServerCommand(&svs.clients[i], "%s", msg);
    }
  }
}

/*
==================
GetModeDataForAccount  – helper to safely fetch/create per-mode JSON sub-object
==================
*/
static cJSON *GetModeDataForAccount(cJSON *acc) {
  if (!acc)
    return NULL;
  const char *curMode = SV_Ranked_GetActiveMode();

  cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  if (!modesObj) {
    modesObj = cJSON_CreateObject();
    cJSON_AddItemToObject(acc, "modes", modesObj);
  }

  cJSON *modeData = cJSON_GetObjectItemCaseSensitive(modesObj, curMode);
  if (!modeData) {
    modeData = cJSON_CreateObject();
    cJSON_AddNumberToObject(modeData, "elo", 1000);
    cJSON_AddNumberToObject(modeData, "wins", 0);
    cJSON_AddNumberToObject(modeData, "losses", 0);
    cJSON_AddNumberToObject(modeData, "kills", 0);
    cJSON_AddNumberToObject(modeData, "deaths", 0);
    cJSON_AddItemToObject(modesObj, curMode, modeData);
  }
  return modeData;
}

/*
==================
SV_Ranked_GetWeaponName
Translates raw engine MOD strings into human-readable format for JSON tracking
and quests.
==================
*/
const char *SV_Ranked_GetWeaponName(const char *raw) {
  if (!raw || !raw[0])
    return raw;

  if (Q_stricmp(raw, "MOD_SABER") == 0 ||
      Q_stricmp(raw, "MOD_SABER_THROW") == 0)
    return "Lightsaber";
  if (Q_stricmp(raw, "MOD_BLASTER") == 0)
    return "E-11 Rifle";
  if (Q_stricmp(raw, "MOD_BRYAR_PISTOL") == 0 ||
      Q_stricmp(raw, "MOD_BRYAR_PISTOL_ALT") == 0)
    return "Pistol";
  if (Q_stricmp(raw, "MOD_BOWCASTER") == 0)
    return "Bowcaster";
  if (Q_stricmp(raw, "MOD_REPEATER") == 0 ||
      Q_stricmp(raw, "MOD_REPEATER_ALT") == 0 ||
      Q_stricmp(raw, "MOD_REPEATER_ALT_SPLASH") == 0)
    return "Repeater";
  if (Q_stricmp(raw, "MOD_DEMP2") == 0 || Q_stricmp(raw, "MOD_DEMP2_ALT") == 0)
    return "DEMP2";
  if (Q_stricmp(raw, "MOD_FLECHETTE") == 0 ||
      Q_stricmp(raw, "MOD_FLECHETTE_ALT_SPLASH") == 0)
    return "Golan Arms";
  if (Q_stricmp(raw, "MOD_ROCKET_HOMING") == 0 ||
      Q_stricmp(raw, "MOD_ROCKET") == 0 ||
      Q_stricmp(raw, "MOD_ROCKET_SPLASH") == 0 ||
      Q_stricmp(raw, "MOD_ROCKET_HOMING_SPLASH") == 0)
    return "Rocket Launcher";
  if (Q_stricmp(raw, "MOD_THERMAL") == 0 ||
      Q_stricmp(raw, "MOD_THERMAL_SPLASH") == 0)
    return "Thermal Detonator";
  if (Q_stricmp(raw, "MOD_FRAG_NADE") == 0 ||
      Q_stricmp(raw, "MOD_FRAG_NADE_SPLASH") == 0)
    return "Frag Grenade";
  if (Q_stricmp(raw, "MOD_TRIP_MINE_SPLASH") == 0 ||
      Q_stricmp(raw, "MOD_TRIP_MINE") == 0)
    return "Trip Mine";
  if (Q_stricmp(raw, "MOD_MELEE") == 0 ||
      Q_stricmp(raw, "MOD_MELEE_KICK") == 0 ||
      Q_stricmp(raw, "MOD_MELEE_KATA") == 0)
    return "Melee";
  if (Q_stricmp(raw, "MOD_FORCE_DARK") == 0)
    return "Dark Force";
  if (Q_stricmp(raw, "MOD_CRUSH") == 0 || Q_stricmp(raw, "MOD_FORCE_GRIP") == 0)
    return "Force Grip/Crush";
  if (Q_stricmp(raw, "MOD_SHOCKWAVE") == 0)
    return "Force Push/Pull";
  if (Q_stricmp(raw, "MOD_FORCE_LIGHTNING") == 0)
    return "Force Lightning";
  if (Q_stricmp(raw, "MOD_SNIPER") == 0 ||
      Q_stricmp(raw, "MOD_DISRUPTOR_SNIPER") == 0 ||
      Q_stricmp(raw, "MOD_DISRUPTOR") == 0 ||
      Q_stricmp(raw, "MOD_DISRUPTOR_SPLASH") == 0)
    return "Sniper Rifle";
  if (Q_stricmp(raw, "MOD_EE4") == 0 || Q_stricmp(raw, "MOD_EE4_ALT") == 0)
    return "EE-3 Blaster";
  if (Q_stricmp(raw, "MOD_FLAMETHROWER") == 0)
    return "Flamethrower";

  if (Q_stricmp(raw, "MOD_FALLING") == 0 ||
      Q_stricmp(raw, "MOD_SUICIDE") == 0 || Q_stricmp(raw, "MOD_LAVA") == 0 ||
      Q_stricmp(raw, "MOD_WATER") == 0 || Q_stricmp(raw, "MOD_SLIME") == 0 ||
      Q_stricmp(raw, "MOD_TRIGGER_HURT") == 0 ||
      Q_stricmp(raw, "MOD_TRIGGER_HURT_FLAME") == 0 ||
      Q_stricmp(raw, "MOD_TELEFRAG") == 0 ||
      Q_stricmp(raw, "MOD_EXPLOSIVE") == 0 ||
      Q_stricmp(raw, "MOD_SPACE") == 0 ||
      Q_stricmp(raw, "MOD_TARGET_LASER") == 0 ||
      Q_stricmp(raw, "MOD_VEHICLE") == 0 ||
      Q_stricmp(raw, "MOD_UNKNOWN") == 0 ||
      Q_stricmp(raw, "MOD_WENTSPECTATOR") == 0 ||
      Q_stricmp(raw, "MOD_CHANGEDTEAMS") == 0) {
    return "World / Hazard";
  }

  return raw;
}

/*
==================
UpdateAccountStats
  eloDelta  – FR change (can be negative)
  xpDelta   – XP change
  isKill    – 1 if this is a kill credit
  isDeath   – 1 if this is a death credit
  weaponStr – string for weapon tracking
==================
*/
static void UpdateAccountStats(const char *username, const char *displayName,
                               int eloDelta, int xpDelta, int isKill,
                               int isDeath, const char *weaponStr) {
  if (!accountsDB || !username || !username[0])
    return;

  char lowerUser[MAX_AUTH_STRING];
  Q_strncpyz(lowerUser, username, sizeof(lowerUser));
  Q_strlwr(lowerUser);

  cJSON *acc = cJSON_GetObjectItemCaseSensitive(accountsDB, lowerUser);
  if (!acc) {
    Com_Printf("[RANKED] UpdateAccountStats: account '%s' not found!\n",
               lowerUser);
    return;
  }

  // Update live display name (colored in-game name)
  if (displayName && displayName[0]) {
    cJSON *dispPtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
    if (!dispPtr) {
      cJSON_AddStringToObject(acc, "displayName", displayName);
    } else if (strcmp(dispPtr->valuestring, displayName) != 0) {
      cJSON_ReplaceItemInObject(acc, "displayName",
                                cJSON_CreateString(displayName));
    }
  }

  // Global XP
  cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
  if (xpPtr && xpDelta) {
    int oldXp = xpPtr->valueint;
    int newXp = oldXp + xpDelta;
    cJSON_SetNumberValue(xpPtr, newXp);

    int oldLevel = SV_Ranked_CalculateLevel(oldXp);
    int newLevel = SV_Ranked_CalculateLevel(newXp);
    if (newLevel > oldLevel && displayName) {
      for (int i = 0; i < sv_maxclients->integer; i++) {
        if (sv_rankedPlayers[i].loggedIn &&
            Q_stricmp(sv_rankedPlayers[i].username, username) == 0) {
          SV_SendServerCommand(&svs.clients[i],
                               "cp \"^2LEVEL UP!\n^7Level %d\"", newLevel);
          SV_Ranked_CheckLevelAchievements(username, newLevel, &svs.clients[i]);
          break;
        }
      }
      SV_SendServerCommand(NULL, va("print \"^7%s ^7has reached ^3Level %d!\n\"",
                           displayName, newLevel));
      SV_Ranked_Log("LEVEL: %s leveled up to %d", username, newLevel);
      Com_Printf("[RANKED] LevelUp: guid=%s name='%s' old_level=%d new_level=%d\n",
                 username, displayName, oldLevel, newLevel);
    }
    SV_Ranked_SyncClientRPGByName(username);
  }

  cJSON *modeData = GetModeDataForAccount(acc);

  cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
  cJSON *killsPtr = cJSON_GetObjectItemCaseSensitive(modeData, "kills");
  cJSON *deathsPtr = cJSON_GetObjectItemCaseSensitive(modeData, "deaths");

  // Clamp ELO to 0
  if (eloPtr && eloDelta) {
    int oldElo = eloPtr->valueint;
    int newElo = oldElo + eloDelta;
    if (newElo < 0)
      newElo = 0;
    cJSON_SetNumberValue(eloPtr, newElo);
    if (eloDelta != 0) {
      Com_Printf("[RANKED] %s Elo: %d -> %d (%+d)\n", username,
                 oldElo, newElo, eloDelta);
      SV_Ranked_Log("ELO: %s changed from %d to %d (%+d)", username, oldElo, newElo, eloDelta);
    }
  }

  if (killsPtr && isKill) {
    cJSON_SetNumberValue(killsPtr, killsPtr->valueint + 1);
    // Quest 7: Century – reach 100 total kills
    if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open") &&
        killsPtr->valueint == 100) {
      int centClId = -1;
      for (int ci = 0; ci < sv_maxclients->integer; ci++) {
        if (sv_rankedPlayers[ci].loggedIn &&
            Q_stricmp(sv_rankedPlayers[ci].username, username) == 0) {
          centClId = ci;
          break;
        }
      }
      SV_Ranked_Log("QUEST: %s completed Century (100 kills)", username);
      if (centClId != -1) {
        SV_SendServerCommand(&svs.clients[centClId],
                             "cp \"^7%s\n^3QUEST DONE!\"", displayName);
        SV_SendServerCommand(&svs.clients[centClId],
                             "print \"^2QUEST COMPLETE! ^7Century: Reach 100 "
                             "Kills (^5+150 Credits / +50 Elo^7)\n\"");
      }
      UpdateAccountCredits(username, 150);
      if (eloPtr)
        cJSON_SetNumberValue(eloPtr, eloPtr->valueint + 50);
    }
  }
  if (deathsPtr && isDeath)
    cJSON_SetNumberValue(deathsPtr, deathsPtr->valueint + 1);
  // Map weapon strings to readable names for the database
  const char *readableS = weaponStr;
  if (isKill && readableS && readableS[0]) {
    readableS = SV_Ranked_GetWeaponName(readableS);
  }

  // Weapon stat tracking
  if (isKill && readableS && readableS[0]) {
    cJSON *weps = cJSON_GetObjectItemCaseSensitive(modeData, "weapons");
    if (!weps) {
      weps = cJSON_CreateObject();
      cJSON_AddItemToObject(modeData, "weapons", weps);
    }
    cJSON *wepCount = cJSON_GetObjectItemCaseSensitive(weps, readableS);
    if (!wepCount) {
      cJSON_AddNumberToObject(weps, readableS, 1);
    } else {
      cJSON_SetNumberValue(wepCount, wepCount->valueint + 1);

      // Quest 2: 50 Lightsaber Kills
      if (Q_stricmp(readableS, "Lightsaber") == 0 &&
          !Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
        int count = wepCount->valueint;
        if (count == 50) {
          int clId = -1;
          for (int i = 0; i < sv_maxclients->integer; i++) {
            if (sv_rankedPlayers[i].loggedIn &&
                Q_stricmp(sv_rankedPlayers[i].username, username) == 0) {
              clId = i;
              break;
            }
          }
          if (clId != -1) {
            SV_SendServerCommand(&svs.clients[clId],
                                 "cp \"^7%s\n^3QUEST DONE!\"", displayName);
            SV_SendServerCommand(&svs.clients[clId],
                                 "print \"^2QUEST COMPLETE! ^7Get 50 "
                                 "Lightsaber Kills (^5+100 Credits^7)\n\"");
          }
          if (eloPtr)
            cJSON_SetNumberValue(eloPtr, eloPtr->valueint + 50);
        }
      }
    }
  }

  SV_Ranked_SaveAccounts();
}

/*
==================
AddEloToAccount  – simpler helper used by round-end logic
==================
*/
static void AddEloToAccount(const char *username, int eloDelta) {
  if (!accountsDB || !username || !username[0])
    return;

  char lowerUser[MAX_AUTH_STRING];
  Q_strncpyz(lowerUser, username, sizeof(lowerUser));
  Q_strlwr(lowerUser);

  cJSON *acc = cJSON_GetObjectItemCaseSensitive(accountsDB, lowerUser);
  if (!acc)
    return;

  cJSON *modeData = GetModeDataForAccount(acc);
  cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
  if (eloPtr) {
    int newElo = eloPtr->valueint + eloDelta;
    if (newElo < 0)
      newElo = 0;
    Com_Printf("[RANKED] %s FR: %d -> %d (%+d)\n", username, eloPtr->valueint,
               newElo, eloDelta);
    cJSON_SetNumberValue(eloPtr, newElo);
  }

  // Global XP for wins
  if (eloDelta > 0) {
    cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
    if (xpPtr)
      cJSON_SetNumberValue(xpPtr, xpPtr->valueint + 50);
  }

  SV_Ranked_SaveAccounts();
}

/*
==================
IncrementWinLoss
==================
*/
static void IncrementWinLoss(const char *username, qboolean won, client_t *cl) {
  if (!accountsDB || !username || !username[0])
    return;

  char lowerUser[MAX_AUTH_STRING];
  Q_strncpyz(lowerUser, username, sizeof(lowerUser));
  Q_strlwr(lowerUser);

  cJSON *acc = cJSON_GetObjectItemCaseSensitive(accountsDB, lowerUser);
  if (!acc)
    return;

  cJSON *modeData = GetModeDataForAccount(acc);

  const char *field = won ? "wins" : "losses";
  cJSON *ptr = cJSON_GetObjectItemCaseSensitive(modeData, field);
  if (ptr) {
    cJSON_SetNumberValue(ptr, ptr->valueint + 1);
    Com_Printf("[RANKED] %s %s: %d\n", username, field, ptr->valueint);
  } else {
    cJSON_AddNumberToObject(modeData, field, 1);
    Com_Printf("[RANKED] %s %s: 1 (created)\n", username, field);
  }

  // Open Quests logic: Team Wins
  if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
    if (won) {
      cJSON *tw = cJSON_GetObjectItemCaseSensitive(modeData, "team_wins");
      if (tw) {
        cJSON_SetNumberValue(tw, tw->valueint + 1);
        if (tw->valueint == 10 && cl) {
          SV_SendServerCommand(cl, "cp \"^7%s\n^3QUEST DONE!\"", username);
          SV_SendServerCommand(cl, "print \"^2QUEST COMPLETE! ^7Win 10 Team "
                                   "Rounds (^5+100 XP / +100 Credits^7)\n\"");
          cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
          if (xpPtr)
            cJSON_SetNumberValue(xpPtr, xpPtr->valueint + 100);
          UpdateAccountCredits(username, 100);
        }
      } else {
        cJSON_AddNumberToObject(modeData, "team_wins", 1);
      }
    } else {
      cJSON *tl = cJSON_GetObjectItemCaseSensitive(modeData, "team_losses");
      if (tl) {
        cJSON_SetNumberValue(tl, tl->valueint + 1);
      } else {
        cJSON_AddNumberToObject(modeData, "team_losses", 1);
      }
    }
  }

  SV_Ranked_SaveAccounts();
}

// ===========================================================================
//  KILL PROCESSING
// ===========================================================================

/*
==================
SV_Ranked_ProcessKill
==================
*/
void SV_Ranked_ProcessKill(int killerId, int victimId, int mod,
                           const char *weaponStr,
                           const char *victimNameOverride) {
  // Ranked toggle — do not process any kills when ranked is disabled.
  if (!Cvar_VariableIntegerValue("sv_ranked_enabled"))
    return;

  // Validate player slots for killer only if they are a client.
  // Victim could be an NPC (id >= maxclients).
  if (killerId < 0 || killerId > sv_maxclients->integer + 1)
    return;
  if (victimId < 0)
    return;

  client_t *killerCl = NULL;
  client_t *victimCl = NULL;
  rankedMatchState_t *kState = NULL;
  rankedMatchState_t *vState = NULL;

  if (killerId < sv_maxclients->integer) {
      killerCl = &svs.clients[killerId];
      kState = &sv_rankedPlayers[killerId];
  }
  if (victimId < sv_maxclients->integer) {
      victimCl = &svs.clients[victimId];
      vState = &sv_rankedPlayers[victimId];
  }

  // If killer is a player but not connected/valid state, return
  if (killerCl && !killerCl->state)
    return;

  const char *killerName = killerCl ? killerCl->name : "World";
  const char *victimName = victimCl ? victimCl->name : "NPC";

  Com_Printf("[RANKED] Kill event: %s (id %d) -> %s (id %d) by %s (mod %d)\n",
             killerName, killerId, victimName, victimId,
             weaponStr ? weaponStr : "Unknown", mod);

  // ---- ISOLATE DUEL KILLS ----
  if ((kState && kState->inDuel) || (vState && vState->inDuel)) {
    if (kState && vState && kState->inDuel && kState->duelOpponent == victimId && vState->inDuel &&
        vState->duelOpponent == killerId) {
      SV_Ranked_DuelEnd(killerId, victimId, 0, 0, mod);
    } else if (vState && vState->inDuel && (killerId == victimId ||
                                   killerId == (sv_maxclients->integer + 1))) {
      int oppId = vState->duelOpponent;
      const char *p1Name = (svs.clients[victimId].state >= CS_CONNECTED) ? svs.clients[victimId].name : "Unknown";
      const char *p2Name = (oppId >= 0 && oppId < sv_maxclients->integer && svs.clients[oppId].state >= CS_CONNECTED) ? svs.clients[oppId].name : "Unknown";
      
      SV_Ranked_Log("DUEL_END: %s def. %s (+0 / -0) [Tie: 0, Disc: 0, Cancelled: 1]", p1Name, p2Name);
      
      char u1[64], u2[64];
      SV_Ranked_GetLogUsername(victimId, u1, sizeof(u1));
      SV_Ranked_GetLogUsername(oppId, u2, sizeof(u2));
      SV_Ranked_Log("DUEL: %s def %s | %s +0 (1000) | %s -0 (1000) [Cancelled]", u1, u2, u1, u2);

      SV_SendServerCommand(
          NULL, "print \"^3Duel cancelled due to suicide/world death.\n\"");
      SV_Ranked_DuelStop(oppId, victimId);
    }
    return; // Prevents Duel deaths/suicides/kills from mixing with Open Mode
            // stats
  }

  // ---- NPC BOSS KILLS (Kyle / Rey) ----
  // Detected by clean victim name parsed from the G_PRINT kill line
  // (the old classname-based check was unreliable for MBII NPC entities).
  if (victimId >= sv_maxclients->integer) {
    if (kState && kState->loggedIn && victimNameOverride && victimNameOverride[0]) {
      typedef struct {
        const char *cleanName;
        int xp;
        int cr;
        const char *counterField; // NULL = no dedicated counter
        const char *displayName;
      } rankedBoss_t;
      static const rankedBoss_t bosses[] = {
          {"Kyle Katarn",         250, 100, "kyle_boss_kills", "Kyle Katarn"},
          {"kyle_boss_trainer",   250, 100, "kyle_boss_kills", "Kyle Boss"},
          {"Rey Skywalker",       250, 100, NULL,              "Rey Skywalker"},
      };
      static const int bossCount = (int)(sizeof(bosses) / sizeof(bosses[0]));

      char clean[64];
      Q_strncpyz(clean, victimNameOverride, sizeof(clean));
      Q_StripColor(clean);
      Com_DPrintf("[RANKED] Boss check: victim='%s' clean='%s'\n",
                  victimNameOverride, clean);

      qboolean isBoss = qfalse;
      for (int b = 0; b < bossCount; b++) {
        if (Q_stricmp(clean, bosses[b].cleanName) != 0)
          continue;
        cJSON *acc = SV_Ranked_GetAccount(kState->username);
        if (!acc)
          break;
        if (bosses[b].counterField) {
          cJSON *cnt =
              cJSON_GetObjectItemCaseSensitive(acc, bosses[b].counterField);
          int current = cnt ? cnt->valueint : 0;
          if (cnt)
            cJSON_SetNumberValue(cnt, current + 1);
          else
            cJSON_AddNumberToObject(acc, bosses[b].counterField, current + 1);
        }
        // Use UpdateAccountStats so level-up check fires automatically
        UpdateAccountStats(kState->username, killerName, 0, bosses[b].xp, 0, 0,
                           NULL);
        UpdateAccountCredits(kState->username, bosses[b].cr);

        SV_SendServerCommand(svs.clients + killerId,
                             "cp \"^2BOSS DEFEATED!\n^2+%d XP ^3+%d CR\"",
                             bosses[b].xp, bosses[b].cr);
        SV_SendServerCommand(
            NULL,
            "chat \"^1%s ^7has defeated ^3%s^7! ^2+%d XP ^3+%d Credits\"",
            killerName, bosses[b].displayName, bosses[b].xp, bosses[b].cr);
        SV_Ranked_Log("BOSS: %s defeated %s", killerName, bosses[b].displayName);
        SV_Ranked_SaveAccounts();
        isBoss = qtrue;
        break;
      }

      if (!isBoss) {
        // Standard NPC kill - give 2 XP
        UpdateAccountStats(kState->username, killerName, 0, 2, 0, 0, NULL);
        SV_SendServerCommand(svs.clients + killerId, "chat \"^2NPC Slayed! ^7+2 XP\"");
        SV_Ranked_Log("NPC_SLAY: %s defeated NPC (%s) for 2 XP", killerName, clean);
        SV_Ranked_SaveAccounts();
      }
    }
    return;
  }

  // ---- SUICIDE / WORLD KILL ----
  if (killerId == victimId || killerId == (sv_maxclients->integer + 1)) {
    Com_Printf("[RANKED] Suicide by %s\n", victimName);
    if (vState) {
        vState->roundDeaths++;
        if (vState->loggedIn) {
            cJSON *acc = SV_Ranked_GetAccount(victimName);
            if (acc) {
                const char *mode = SV_Ranked_GetActiveMode();
                cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
                cJSON *modeData = modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, mode) : NULL;
                if (modeData) {
                    cJSON *deathsPtr = cJSON_GetObjectItemCaseSensitive(modeData, "deaths");
                    if (deathsPtr) cJSON_SetNumberValue(deathsPtr, deathsPtr->valueint + 1);
                    else cJSON_AddNumberToObject(modeData, "deaths", 1);
                    SV_Ranked_SaveAccounts();
                }
            }
        }
    }
    return;
  }

  // ---- TEAM KILL DETECTION ----
  int killerTeam = -1;
  int victimTeam = -1;
  if (killerId < sv_maxclients->integer) {
    char *kcs = sv.configstrings[CS_PLAYERS + killerId];
    if (kcs && *kcs) {
      const char *t = Info_ValueForKey(kcs, "team");
      if (!t || !*t)
        t = Info_ValueForKey(kcs, "t");
      if (t && *t) {
        int parsedT = 0;
        if (t[0] == 'r' || t[0] == 'R')
          parsedT = 1;
        else if (t[0] == 'b' || t[0] == 'B')
          parsedT = 2;
        else
          parsedT = atoi(t);
        killerTeam = parsedT;
      }
    }
  }

  if (victimId < sv_maxclients->integer) {
    char *vcs = sv.configstrings[CS_PLAYERS + victimId];
    if (vcs && *vcs) {
      const char *t = Info_ValueForKey(vcs, "team");
      if (!t || !*t)
        t = Info_ValueForKey(vcs, "t");
      if (t && *t) {
        int parsedT = 0;
        if (t[0] == 'r' || t[0] == 'R')
          parsedT = 1;
        else if (t[0] == 'b' || t[0] == 'B')
          parsedT = 2;
        else
          parsedT = atoi(t);
        victimTeam = parsedT;
        if (parsedT == 1 || parsedT == 2)
          vState->latestTeamId = parsedT;
      }
    }
  }

  qboolean isTeamKill =
      (killerTeam == victimTeam && killerTeam > 0) ? qtrue : qfalse;

  if (isTeamKill) {
    Com_Printf("[RANKED] TEAM KILL: %s on %s (team %d)\n", killerName,
               victimName, killerTeam);
    Cbuf_ExecuteText(
        EXEC_APPEND,
        va("sv_centerprint all \"^1TEAM KILL! ^7%s ^7betrayed ^1%s^7!\"\n",
           killerName, victimName));
    if (kState->loggedIn) {
      SV_SendServerCommand(killerCl,
                           "print \"^1WARNING: ^7Team Kill! (-3 Elo)\n\"");
      UpdateAccountStats(kState->username, killerName, -3, 0, 0, 0, NULL);
    }
    // Victim deaths still count
    vState->roundDeaths++;
    if (vState->loggedIn) {
      UpdateAccountStats(vState->username, victimName, 0, 0, 0, 1, NULL);
    }
    return;
  }

  // ---- JAIL FREEKILL ENFORCEMENT ----
  // If attacker is jailed and this is NOT a duel kill → auto-kick
  if (kState->jailExpireTime > 0) {
    if (svs.time < kState->jailExpireTime) {
      // Still jailed — check if this is outside a duel
      if (!kState->inDuel || kState->duelOpponent != victimId) {
        Com_Printf(
            "[RANKED] Jailed player %s committed a freekill on %s. Kicking.\n",
            killerName, victimName);
        SV_SendServerCommand(NULL,
                             "chat \"^1%s^7 was kicked for attacking players "
                             "while on probation.\"",
                             killerName);
        Cbuf_ExecuteText(EXEC_APPEND, va("kick %d\n", killerId));
        return;
      }
    } else {
      // Jail expired — clear it
      kState->jailExpireTime = 0;
      SV_SendServerCommand(
          killerCl,
          "chat \"^2Your probation period has ended. Please be respectful.\"");
    }
  }

  // ---- PROCESS VICTIM ----
  vState->roundDeaths++;
  vState->bombStreak = 0;
  vState->multiKillCount = 0;
  Com_Memset(vState->killsOnPlayers, 0, sizeof(vState->killsOnPlayers));

  if (vState->loggedIn) {
    UpdateAccountStats(vState->username, victimName, 0, 0, 0, 1, NULL);
  }

  // Bounty collection
  if (vState->bountyValue > 0) {
    int bReward = vState->bountyValue;
    Com_Printf("[RANKED] Bounty claimed! %s collected +%d Credits for "
               "eliminating %s\n",
               killerName, bReward, victimName);
    SV_SendServerCommand(
        NULL,
        "chat \"^3BOUNTY CLAIMED! ^7%s ^7collected ^5+%d Credits "
        "^7for eliminating ^1%s!\n\"",
        killerName, bReward, victimName);
    if (kState->loggedIn) {
      UpdateAccountCredits(kState->username, bReward);
      UpdateAccountStats(kState->username, killerName, bReward, 50, 0, 0, NULL);
      // Quest 4: Bounty Hunter – collect 3 bounties (works in ALL modes)
      cJSON *bacc = SV_Ranked_GetAccount(kState->username);
      if (bacc) {
        cJSON *bmodeData = GetModeDataForAccount(bacc);
        cJSON *bqPtr = cJSON_GetObjectItemCaseSensitive(
            bmodeData, "quest_bounties_collected");
        if (bqPtr) {
          cJSON_SetNumberValue(bqPtr, bqPtr->valueint + 1);
          if (bqPtr->valueint == 3) {
            SV_SendServerCommand(killerCl,
                                 "cp \"^7%s\n^3QUEST DONE!\"", killerName);
            SV_SendServerCommand(killerCl,
                                 "print \"^2QUEST COMPLETE! ^7Bounty Hunter: "
                                 "Collect 3 Bounties (^5+100 Credits^7)\n\"");
            UpdateAccountCredits(kState->username, 100);
          }
        } else {
          cJSON_AddNumberToObject(bmodeData, "quest_bounties_collected", 1);
        }
      }
    }
    vState->bountyValue = 0;
  }

  // Streak SHUTDOWN
  int victimStreakWas = vState->killStreak;
  vState->killStreak = 0;
  if (victimStreakWas >= 3) {
    Com_Printf("[RANKED] Streak SHUTDOWN: %s ended %s's %d-kill streak\n",
               killerName, victimName, victimStreakWas);
    SV_SendServerCommand(NULL, "chat \"^5%s ^7ended ^1%s^7's streak!\"",
                         killerName, victimName);
    // Bonus credits for ending a streak
    if (kState->loggedIn) {
      int shutdownBonus = victimStreakWas; // 1 Credit per streak count
      UpdateAccountCredits(kState->username, shutdownBonus);
      SV_SendServerCommand(
          killerCl, "print \"^3STREAK SHUTDOWN! ^7+%d Credits bonus!\n\"",
          shutdownBonus);
    }
  }

  // ---- PROCESS KILLER ----
  kState->roundKills++;
  kState->killsOnPlayers[victimId]++;

  // Kill streaks only count in OPEN mode. In other modes (FA, legends, duel)
  // we keep streak at 0 so non-duel kills don't accidentally fire WANTED
  // announcements or grant streak bounties.
  const qboolean streakModeActive =
      (Q_stricmp(SV_Ranked_GetActiveMode(), "open") == 0) ? qtrue : qfalse;
  if (streakModeActive) {
    kState->killStreak++;
  } else {
    kState->killStreak = 0;
  }

  // Update highest streak in JSON (open mode only)
  if (streakModeActive && kState->loggedIn) {
    cJSON *acc = SV_Ranked_GetAccount(kState->username);
    if (acc) {
      cJSON *modeData = GetModeDataForAccount(acc);
      if (modeData) {
        cJSON *hs =
            cJSON_GetObjectItemCaseSensitive(modeData, "highest_streak");
        if (!hs) {
          cJSON_AddNumberToObject(modeData, "highest_streak",
                                  kState->killStreak);
        } else if (kState->killStreak > hs->valueint) {
        }
      }
    }
  }

  // Check Hot Potato transfer on open mode kill
  extern void SV_Ranked_HotPotato_CheckKill(int victimId, int killerId);
  SV_Ranked_HotPotato_CheckKill(victimId, killerId);

  // Party Wars Kill Score tracking (Party vs Party)
  rankedParty_t *kParty = SV_Ranked_FindPlayerParty(killerId);
  rankedParty_t *vParty = SV_Ranked_FindPlayerParty(victimId);
  if (kParty && vParty && kParty != vParty) {
    kParty->score++;
    SV_SendServerCommand(NULL, va("print \"^3[Party War] ^5%s ^7(^3%s^7) killed ^5%s ^7(^1%s^7)! Team Score: ^3%dP^7!\n\"",
                                 killerName, kParty->teamName, victimName, vParty->teamName, kParty->score));
    SV_Ranked_UpdateParty((int)(kParty - sv_rankedParties));
    SV_Ranked_UpdateParty((int)(vParty - sv_rankedParties));
  }

  if (!kState->loggedIn) {
    return;
  }



  // Base kill reward
  cJSON *xpKillPtr = SV_Ranked_GetSetting("xp_per_kill");
  int xpPerKill = xpKillPtr ? xpKillPtr->valueint : 10;
  cJSON *crKillPtr = SV_Ranked_GetSetting("credits_per_kill");
  int creditsPerKill = crKillPtr ? crKillPtr->valueint : 1;

  UpdateAccountStats(kState->username, killerName, 0, xpPerKill, 1, 0, weaponStr);
  UpdateAccountCredits(kState->username, creditsPerKill);

  const char *mappedWep = SV_Ranked_GetWeaponName(weaponStr);

  // Daily quest: kills
  SV_Ranked_ProgressQuest(kState->username, "kills", 1, killerCl);
  if (mappedWep && !Q_stricmp(mappedWep, "Lightsaber"))
    SV_Ranked_ProgressQuest(kState->username, "saber_kills", 1, killerCl);

  // ---- FIRST BLOOD ----
  if (!sv_ranked_firstBlood) {
    sv_ranked_firstBlood = qtrue;
    Com_Printf("[RANKED] FIRST BLOOD by %s!\n", killerName);
    SV_Ranked_BroadcastOpen("cp \"^1FIRST BLOOD!\n^7%s\"", killerName);
    SV_SendServerCommand(killerCl,
                         "print \"^1FIRST BLOOD! ^7+15 Credits / +5 XP!\n\"");
    UpdateAccountStats(kState->username, killerName, 0, 0, 0, 0, NULL);
    UpdateAccountCredits(kState->username, 15);

    // Daily quest: first bloods
    if (kState->loggedIn)
      SV_Ranked_ProgressQuest(kState->username, "first_bloods", 1, killerCl);
  }

  // ---- MELEE KILL BONUS ----
  if (mappedWep && !Q_stricmp(mappedWep, "Melee")) {
    Com_Printf("[RANKED] MELEE KILL bonus for %s!\n", killerName);
    SV_Ranked_BroadcastOpen("cp \"^1BRAWLER!\n^7%s ^7styled on ^1%s\"",
                            killerName, victimName);
    UpdateAccountStats(kState->username, killerName, 5, 0, 0, 0, NULL);
    SV_Ranked_ProgressQuest(kState->username, "melee_kills", 1, killerCl);
  }

  // ---- BOMB STREAK ----
  if (mappedWep && (!Q_stricmp(mappedWep, "Thermal Detonator") ||
                    !Q_stricmp(mappedWep, "Frag Grenade") ||
                    !Q_stricmp(mappedWep, "Trip Mine"))) {
    kState->bombStreak++;
    Com_Printf("[RANKED] %s bomb streak: %d\n", killerName, kState->bombStreak);
    if (kState->bombStreak == 4) {
      SV_Ranked_BroadcastOpen(
          "cp \"^1BOMBERMAN!\n^7%s ^7landed 4 bomb kills!\"", killerName);
    }
    SV_Ranked_ProgressQuest(kState->username, "bomb_kills", 1, killerCl);
  } else {
    kState->bombStreak = 0;
  }

  // ---- DOMINATION (5 kills on same player) ----
  if (kState->killsOnPlayers[victimId] == 5) {
    Com_Printf("[RANKED] DOMINATION: %s is dominating %s!\n", killerName,
               victimName);
    SV_Ranked_BroadcastOpen("cp \"^3DOMINATION!\n^7%s ^7is crushing ^1%s\"",
                            killerName, victimName);
    SV_SendServerCommand(NULL, "chat \"^3DOMINATION: ^7%s ^7is dominating ^1%s!\"", killerName, victimName);
    // Daily quest: dominations
    if (kState->loggedIn)
      SV_Ranked_ProgressQuest(kState->username, "dominations", 1, killerCl);
  }

  // ---- MULTI KILL ----
  int now = svs.time;
  if (now - kState->lastKillTime < 4000) {
    kState->multiKillCount++;
  } else {
    kState->multiKillCount = 1;
  }
  kState->lastKillTime = now;

  if (kState->multiKillCount == 2) {
    Com_Printf("[RANKED] DOUBLE KILL by %s\n", killerName);
    if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
      SV_SendServerCommand(NULL, "cp \"^5%s\n^1DOUBLE KILL! ^7(+2 Elo)\"",
                           killerName);
    }
    UpdateAccountStats(kState->username, killerName, 2, 0, 0, 0, NULL);
    SV_Ranked_ProgressQuest(kState->username, "double_kills", 1, killerCl);
  } else if (kState->multiKillCount == 3) {
    Com_Printf("[RANKED] TRIPLE KILL by %s\n", killerName);
    if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
      SV_SendServerCommand(NULL, "cp \"^5%s\n^1TRIPLE KILL! ^7(+5 Elo)\"",
                           killerName);
    }
    UpdateAccountStats(kState->username, killerName, 5, 0, 0, 0, NULL);
    SV_Ranked_ProgressQuest(kState->username, "triple_kills", 1, killerCl);
  } else if (kState->multiKillCount == 4) {
    Com_Printf("[RANKED] OVERKILL by %s\n", killerName);
    if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1OVERKILL!\"", killerName);
    }
    UpdateAccountStats(kState->username, killerName, 10, 0, 0, 0, NULL);
  } else if (kState->multiKillCount >= 5) {
    Com_Printf("[RANKED] MONSTER KILL by %s\n", killerName);
    if (!Q_stricmp(SV_Ranked_GetActiveMode(), "open")) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1MONSTER KILL!\"", killerName);
    }
    UpdateAccountStats(kState->username, killerName, 15, 0, 0, 0, NULL);
  }

  // ---- KILL STREAKS (OPEN MODE ONLY) ----
  if (streakModeActive) {
    int streak = kState->killStreak;
    Com_Printf("[RANKED] %s kill streak = %d\n", kState->username, streak);

    if (streak == 3) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^3SPREE!\"", killerName);
    } else if (streak == 5) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1RAMPAGE!\"", killerName);
      SV_SendServerCommand(
          NULL,
          "chat \"^1WANTED! ^7%s ^7is on a 5-kill streak! Worth ^55 Credits^7!\"",
          killerName);
      kState->bountyValue = 5;
      UpdateAccountStats(kState->username, killerName, 5, 0, 0, 0, NULL);
      // Quest 5: Executioner
      if (kState->loggedIn) {
        cJSON *eacc = SV_Ranked_GetAccount(kState->username);
        if (eacc) {
          cJSON *emodeData = GetModeDataForAccount(eacc);
          cJSON *eqPtr =
              cJSON_GetObjectItemCaseSensitive(emodeData, "quest_streaks");
          if (eqPtr) {
            cJSON_SetNumberValue(eqPtr, eqPtr->valueint + 1);
            if (eqPtr->valueint == 3) {
              SV_SendServerCommand(killerCl, "cp \"^7%s\n^3QUEST DONE!\"",
                                   killerName);
              SV_SendServerCommand(
                  killerCl,
                  "print \"^2QUEST COMPLETE! ^7Executioner: Reach 5-Kill Streak "
                  "3 Times (^5+75 Credits / +25 Elo^7)\n\"");
              UpdateAccountCredits(kState->username, 75);
              AddEloToAccount(kState->username, 25);
            }
          } else {
            cJSON_AddNumberToObject(emodeData, "quest_streaks", 1);
          }
        }
      }
    } else if (streak == 8) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1UNSTOPPABLE!\"", killerName);
      UpdateAccountStats(kState->username, killerName, 5, 0, 0, 0, NULL);
    } else if (streak == 10) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1GODLIKE!\"", killerName);
      SV_SendServerCommand(NULL,
                           "chat \"^1WANTED! ^7%s ^7is on a 10-kill streak! "
                           "Worth ^510 Credits + 50 XP^7!\"",
                           killerName);
      kState->bountyValue = 10;
      UpdateAccountStats(kState->username, killerName, 10, 50, 0, 0, NULL);
    } else if (streak == 14) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1FORCE MASTER!\"", killerName);
      UpdateAccountStats(kState->username, killerName, 15, 0, 0, 0, NULL);
    } else if (streak == 20) {
      SV_SendServerCommand(NULL, "cp \"^7%s\n^1LEGENDARY!\"", killerName);
      SV_SendServerCommand(
          NULL,
          "chat \"^1LEGENDARY WANTED! ^7%s ^7is on a 20-kill streak! Worth ^520 "
          "Credits + 100 XP^7! Stop them NOW!\"",
          killerName);
      kState->bountyValue = 20;
      UpdateAccountStats(kState->username, killerName, 20, 100, 0, 0, NULL);
    }
  }

  // ---- ACHIEVEMENT CHECKS ----
  {
    cJSON *acc = SV_Ranked_GetAccount(kState->username);
    if (acc) {
      cJSON *md = GetModeDataForAccount(acc);
      int totalKills =
          md ? (cJSON_GetObjectItemCaseSensitive(md, "kills")
                    ? cJSON_GetObjectItemCaseSensitive(md, "kills")->valueint
                    : 0)
             : 0;
      // Weapon-specific totals are tracked in md->weapons; approximate from
      // session counts
      int meleeKills =
          kState
              ->roundKills; // best session proxy — cumulative tracked via quest
      int bombKills = kState->bombStreak; // running streak proxy
      int dominations = 0;
      cJSON *domPtr =
          md ? cJSON_GetObjectItemCaseSensitive(md, "dominations") : NULL;
      if (domPtr)
        dominations = domPtr->valueint;

      SV_Ranked_CheckKillAchievements(kState->username, totalKills,
                                      kState->killStreak, meleeKills,
                                      bombKills, dominations, killerCl);
      SV_Ranked_CheckEconomyAchievements(kState->username, killerCl);
    }
  }
}

// ===========================================================================
//  SCOREBOARD OBJECTIVE TRACKING
// ===========================================================================

void SV_Ranked_ProcessScoreboard(const char *text) {
  if (!text || Q_stricmpn(text, "scores ", 7) != 0)
    return;
  if (!accountsDB)
    return;

  const char *parse = text + 7;

  // Format: "scores <numClients> <teamRedScore> <teamBlueScore> ..."
  int numClients = atoi(parse);
  parse = strchr(parse, ' ');
  if (!parse)
    return;
  parse++;
  int redScore = atoi(parse);
  parse = strchr(parse, ' ');
  if (!parse)
    return;
  parse++;
  int blueScore = atoi(parse);

  // Com_Printf("[RANKED] Scoreboard: red=%d blue=%d clients=%d\n", redScore,
  //          blueScore, numClients);

  parse = strchr(parse, ' ');
  while (parse && *parse) {
    parse++; // skip space
    int clientNum = atoi(parse);
    parse = strchr(parse, ' ');
    if (!parse)
      break;
    parse++;
    int pScore = atoi(parse);

    // Skip 12 fields to advance to the next client block
    for (int j = 0; j < 12; j++) {
      parse = strchr(parse, ' ');
      if (!parse)
        break;
      parse++;
    }

    if (clientNum >= 0 && clientNum < sv_maxclients->integer) {
      rankedMatchState_t *mState = &sv_rankedPlayers[clientNum];
      if (mState->loggedIn && mState->username[0]) {
        // Disabled objective score parsing from 'scores' string because MBII
        // alters the string format (what we parse as score is actually ping).

        // Track team from configstring
        char *cs = sv.configstrings[CS_PLAYERS + clientNum];
        if (cs && *cs) {
          const char *teamStr = Info_ValueForKey(cs, "team");
          if (!teamStr || !*teamStr)
            teamStr = Info_ValueForKey(cs, "t");

          if (teamStr && *teamStr) {
            int parsedTeam = 0;
            if (teamStr[0] == 'r' || teamStr[0] == 'R')
              parsedTeam = 1;
            else if (teamStr[0] == 'b' || teamStr[0] == 'B')
              parsedTeam = 2;
            else if (teamStr[0] == 's' || teamStr[0] == 'S')
              parsedTeam = 3;
            else
              parsedTeam = atoi(teamStr);

            if (parsedTeam == 1 || parsedTeam == 2) {
              mState->latestTeamId = parsedTeam;
            }
          }
        }
      }
    }

    if (parse)
      parse = strchr(parse, ' ');
  }

  SV_Ranked_SaveAccounts();
}

// ===========================================================================
//  DISPLAY NAME UPDATE
// ===========================================================================

void SV_Ranked_UpdateDisplayName(int clientNum, const char *newName) {
  if (clientNum < 0 || clientNum >= sv_maxclients->integer || !newName ||
      !newName[0])
    return;

  rankedMatchState_t *mState = &sv_rankedPlayers[clientNum];
  if (!mState->loggedIn || !mState->username[0])
    return;

  if (!accountsDB)
    return;

  char lowerUser[MAX_AUTH_STRING];
  Q_strncpyz(lowerUser, mState->username, sizeof(lowerUser));
  Q_strlwr(lowerUser);

  cJSON *acc = cJSON_GetObjectItemCaseSensitive(accountsDB, lowerUser);
  if (!acc)
    return;

  cJSON *dispPtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  if (!dispPtr) {
    cJSON_AddStringToObject(acc, "displayName", newName);
    Com_Printf("[RANKED] %s display name set to: %s\n", mState->username,
               newName);
  } else if (strcmp(dispPtr->valuestring, newName) != 0) {
    Com_Printf("[RANKED] %s display name updated: %s -> %s\n", mState->username,
               dispPtr->valuestring, newName);
    cJSON_ReplaceItemInObject(acc, "displayName", cJSON_CreateString(newName));
  } else {
    return; // No change
  }

  SV_Ranked_SaveAccounts();
}

// ===========================================================================
//  ROUND END
// ===========================================================================

void SV_Ranked_ProcessRoundEnd(int winnerTeam) {
  if (sv_hotPotatoActive) {
    SV_Ranked_StopHotPotato(qfalse);
  }
  const char *modeStr = SV_Ranked_GetActiveMode();
  Com_Printf("[RANKED] ====== ROUND END ======\n");
  Com_Printf("[RANKED] Winner team: %d | Mode: %s\n", winnerTeam, modeStr);

  // Broadcast the round result to all players
  if (winnerTeam == 1) {
    SV_SendServerCommand(NULL, "chat \"^7[ ^1RANKED ^7] ^1Red Team ^7wins the "
                               "round! ^3Updating ranks...\"");
  } else if (winnerTeam == 2) {
    SV_SendServerCommand(NULL, "chat \"^7[ ^1RANKED ^7] ^4Blue Team ^7wins the "
                               "round! ^3Updating ranks...\"");
  } else {
    SV_SendServerCommand(NULL, "chat \"^7[ ^1RANKED ^7] ^7Round over — it's a "
                               "tie! ^3No rank changes.\"");
  }

  // Handle MVPs
  struct PlayerKD {
    int clientNum;
    int kd;
    int kills;
    int deaths;
  };
  PlayerKD tops[64];
  int numValid = 0;

  for (int i = 0; i < sv_maxclients->integer; i++) {
    client_t *c = &svs.clients[i];
    if (!c->state)
      continue;
    rankedMatchState_t *mState = &sv_rankedPlayers[i];
    if (!mState->loggedIn || !mState->username[0])
      continue;

    int kd = mState->roundKills - mState->roundDeaths;
    if (kd > 0 || mState->roundKills > 0) {
      tops[numValid].clientNum = i;
      tops[numValid].kd = kd;
      tops[numValid].kills = mState->roundKills;
      tops[numValid].deaths = mState->roundDeaths;
      numValid++;
    }
  }

  for (int i = 0; i < numValid - 1; i++) {
    for (int j = i + 1; j < numValid; j++) {
      if (tops[j].kd > tops[i].kd ||
          (tops[j].kd == tops[i].kd && tops[j].kills > tops[i].kills)) {
        PlayerKD temp = tops[i];
        tops[i] = tops[j];
        tops[j] = temp;
      }
    }
  }

  if (numValid > 0) {
    SV_SendServerCommand(NULL, "chat \"^5--- Match MVPs ---\"");
    if (numValid >= 1)
      SV_SendServerCommand(NULL, "chat \"^3MVP: ^7%s ^5(KD: +%d | %dK/%dD)\"",
                           svs.clients[tops[0].clientNum].name, tops[0].kd,
                           tops[0].kills, tops[0].deaths);
    if (numValid >= 2)
      SV_SendServerCommand(NULL, "chat \"^72nd: ^7%s ^5(KD: +%d | %dK/%dD)\"",
                           svs.clients[tops[1].clientNum].name, tops[1].kd,
                           tops[1].kills, tops[1].deaths);
    if (numValid >= 3)
      SV_SendServerCommand(NULL, "chat \"^73rd: ^7%s ^5(KD: +%d | %dK/%dD)\"",
                           svs.clients[tops[2].clientNum].name, tops[2].kd,
                           tops[2].kills, tops[2].deaths);
  }

  for (int i = 0; i < sv_maxclients->integer; i++) {
    rankedMatchState_t *mState = &sv_rankedPlayers[i];
    client_t *cl = &svs.clients[i];

    if (!cl->state)
      continue;

    int thisTeam = mState->latestTeamId;
    // Fallback: if latestTeamId not set (CS_PLAYERS may not carry team in
    // MBII), try reading directly from client userinfo
    if (thisTeam == 0) {
      const char *t = Info_ValueForKey(cl->userinfo, "team");
      if (t && *t) {
        if (t[0] == 'r' || t[0] == 'R')
          thisTeam = 1;
        else if (t[0] == 'b' || t[0] == 'B')
          thisTeam = 2;
      }
      if (thisTeam > 0)
        mState->latestTeamId = thisTeam; // cache it
    }
    qboolean onWinningTeam =
        (thisTeam == winnerTeam && winnerTeam > 0) ? qtrue : qfalse;

    Com_Printf("[RANKED] Player %d (%s) team=%d wins=%s kills=%d deaths=%d "
               "loggedIn=%d\n",
               i, mState->loggedIn ? mState->username : cl->name, thisTeam,
               onWinningTeam ? "YES" : "NO", mState->roundKills,
               mState->roundDeaths, (int)mState->loggedIn);

    if (!mState->loggedIn || !mState->username[0]) {
      goto reset_player;
    }

    if (winnerTeam == 0) {
      // Tie / no winner – just count games played
      Com_Printf("[RANKED] %s round result: TIE (no Elo change)\n",
                 mState->username);
      goto reset_player;
    }

    if (onWinningTeam) {
      // K/D-based Elo for winners (Open mode)
      int frGain;
      if (mState->roundDeaths > mState->roundKills) {
        frGain = 5; // carried by team
        SV_SendServerCommand(
            cl,
            "print \"^2VICTORY! ^7You were carried by your team. (+5 Elo)\n\"");
        Com_Printf("[RANKED] %s WIN (carried): +5 Elo\n", mState->username);
      } else {
        frGain = 15; // positive K/D
        SV_SendServerCommand(cl,
                             "print \"^2VICTORY! ^7Positive K/D! (+15 Elo)\n\"");
        Com_Printf("[RANKED] %s WIN (positive KD): +15 Elo\n", mState->username);
      }
      AddEloToAccount(mState->username, frGain);
      IncrementWinLoss(mState->username, qtrue, cl);
      cJSON *crTeamWinPtr = SV_Ranked_GetSetting("credits_per_team_win");
      int crTeamWin = crTeamWinPtr ? crTeamWinPtr->valueint : 20;
      UpdateAccountCredits(mState->username, crTeamWin); // Credits for Team Win

      // Quest 8: Deathless – win a round with 0 deaths 3 times
      if (mState->roundDeaths == 0 && !Q_stricmp(modeStr, "open")) {
        cJSON *dlacc = SV_Ranked_GetAccount(mState->username);
        if (dlacc) {
          cJSON *dlmodeData = GetModeDataForAccount(dlacc);
          cJSON *dlqPtr =
              cJSON_GetObjectItemCaseSensitive(dlmodeData, "quest_deathless");
          if (dlqPtr) {
            cJSON_SetNumberValue(dlqPtr, dlqPtr->valueint + 1);
            if (dlqPtr->valueint == 3) {
              cJSON *crDeathlessPtr = SV_Ranked_GetSetting("quest_deathless_credits");
              int crDeathless = crDeathlessPtr ? crDeathlessPtr->valueint : 100;
              SV_SendServerCommand(cl, "cp \"^7%s\n^3QUEST DONE!\"", mState->username);
              SV_SendServerCommand(
                  cl, va("print \"^2QUEST COMPLETE! ^7Deathless: Win 3 Rounds "
                      "Without Dying (^5+%d Credits^7)\n\"", crDeathless));
              UpdateAccountCredits(mState->username, crDeathless);
            }
          } else {
            cJSON_AddNumberToObject(dlmodeData, "quest_deathless", 1);
          }
        }
      }

    } else {
      // Losers
      int frLoss;
      if (mState->roundKills > mState->roundDeaths) {
        frLoss = 0;
        SV_SendServerCommand(cl, "chat \"^1DEFEAT. ^7Good fight — positive K/D "
                                 "saves your rank. (-0 Elo)\"");
        Com_Printf("[RANKED] %s LOSS (positive KD): 0 Elo lost\n",
                   mState->username);
      } else {
        frLoss = 10;
        SV_SendServerCommand(cl, "chat \"^1DEFEAT. ^7Negative K/D. (-10 Elo)\"");
        Com_Printf("[RANKED] %s LOSS (negative KD): -10 Elo\n",
                   mState->username);
      }
      if (frLoss > 0) {
        AddEloToAccount(mState->username, -frLoss);
      }
      IncrementWinLoss(mState->username, qfalse, cl);
    }

  reset_player:
    mState->roundKills = 0;
    mState->roundDeaths = 0;
    mState->roundObjective = 0;
    mState->killStreak = 0;
    mState->multiKillCount = 0;
    mState->bombStreak = 0;
    Com_Memset(mState->killsOnPlayers, 0, sizeof(mState->killsOnPlayers));
  }

  // MVP tracking would need a pre-reset snapshot — skipping for now.

  sv_ranked_firstBlood = qfalse;
  Com_Printf("[RANKED] ====== ROUND END COMPLETE ======\n");

  SV_Ranked_SaveAccounts();
}

/*
==================
Duel MMR Mathematics & Tracking
==================
*/
#include <math.h>

static int CalculateDuelElo(int playerMMR, int opponentMMR, qboolean isWinner,
                            int totalPlayerGames) {
  cJSON *kBase = SV_Ranked_GetSetting("elo_k_base");
  cJSON *kNew = SV_Ranked_GetSetting("elo_k_new_player");
  cJSON *kLow = SV_Ranked_GetSetting("elo_k_low_elo");
  cJSON *gamesNew = SV_Ranked_GetSetting("elo_games_for_new");
  cJSON *lowThresh = SV_Ranked_GetSetting("elo_low_thresh");
  cJSON *diffCap = SV_Ranked_GetSetting("elo_diff_cap");
  cJSON *maxSwg = SV_Ranked_GetSetting("elo_max_swing");

  int K = kBase ? kBase->valueint : 15;
  int limitNewGames = gamesNew ? gamesNew->valueint : 40;
  int limitLowElo = lowThresh ? lowThresh->valueint : 3250;

  if (totalPlayerGames < 10) {
    K = 40; // Accelerated placement calibration for first 10 duels
  } else if (totalPlayerGames < limitNewGames) {
    K = kNew ? kNew->valueint : 25;
  } else if (playerMMR < limitLowElo) {
    K = kLow ? kLow->valueint : 20;
  }

  int cap = diffCap ? diffCap->valueint : 1000;
  int effectiveOpponentMMR = opponentMMR;
  if (abs(playerMMR - opponentMMR) > cap) {
    if (playerMMR > opponentMMR) {
      effectiveOpponentMMR = playerMMR - cap;
    } else {
      effectiveOpponentMMR = playerMMR + cap;
    }
  }

  double expectedScore =
      1.0 / (1.0 + pow(10.0, (effectiveOpponentMMR - playerMMR) / 400.0));
  double actualScore = isWinner ? 1.0 : 0.0;

  double eloChange = K * (actualScore - expectedScore);

  // Caps
  int maxSwing = maxSwg ? maxSwg->valueint : 40;
  if (eloChange > maxSwing)
    eloChange = maxSwing;
  if (eloChange < -maxSwing)
    eloChange = -maxSwing;

  // Minimums
  if (isWinner && eloChange < 2)
    eloChange = 2;
  else if (!isWinner && eloChange > -2)
    eloChange = -2;

  return (int)round(eloChange);
}

void SV_Ranked_DuelStart(int p1, int p2) {
  // Ranked toggle — duels still play out but no ELO/stats are tracked.
  if (!Cvar_VariableIntegerValue("sv_ranked_enabled")) {
    Com_Printf("[RANKED] sv_ranked_enabled=0 — duel will not affect stats.\n");
    return;
  }

  if (p1 < 0 || p1 >= sv_maxclients->integer || p2 < 0 ||
      p2 >= sv_maxclients->integer)
    return;

  SV_Ranked_Log("DUEL_START: %s vs %s", svs.clients[p1].name, svs.clients[p2].name);

  rankedMatchState_t *r1 = &sv_rankedPlayers[p1];
  rankedMatchState_t *r2 = &sv_rankedPlayers[p2];

  // Allow duels even if one or both players are temp (no GUID / not logged in).
  // Stats are only saved for logged-in players — see SV_Ranked_DuelEnd.
  if (!r1->loggedIn || !r2->loggedIn) {
    Com_Printf("[RANKED] DuelStart: %s=%s, %s=%s — proceeding (temp players get no stats saved)\n",
      svs.clients[p1].name, r1->loggedIn ? "ranked" : "temp",
      svs.clients[p2].name, r2->loggedIn ? "ranked" : "temp");
  }

  // ---- GHOST DUEL CLEANUP (mirrors JS forceEndDuel / removeDuelState) ----
  // If either player is already mid-duel, silently clear the old opponent's
  // state so they don't get permanently stuck in inDuel=true.
  if (r1->inDuel) {
    int oldOpp = r1->duelOpponent;
    Com_Printf("[RANKED] %s starting new duel – cleaning up abandoned duel "
               "with client %d\n",
               r1->username, oldOpp);
    SV_SendServerCommand(
        NULL,
        "print \"^3%s^7's previous duel was cancelled (new duel started).\n\"",
        svs.clients[p1].name);
    if (oldOpp >= 0 && oldOpp < sv_maxclients->integer) {
      sv_rankedPlayers[oldOpp].inDuel = qfalse;
      sv_rankedPlayers[oldOpp].duelOpponent = -1;
    }
    r1->inDuel = qfalse;
    r1->duelOpponent = -1;
  }

  if (r2->inDuel) {
    int oldOpp = r2->duelOpponent;
    Com_Printf("[RANKED] %s starting new duel – cleaning up abandoned duel "
               "with client %d\n",
               r2->username, oldOpp);
    SV_SendServerCommand(
        NULL,
        "print \"^3%s^7's previous duel was cancelled (new duel started).\n\"",
        svs.clients[p2].name);
    if (oldOpp >= 0 && oldOpp < sv_maxclients->integer) {
      sv_rankedPlayers[oldOpp].inDuel = qfalse;
      sv_rankedPlayers[oldOpp].duelOpponent = -1;
    }
    r2->inDuel = qfalse;
    r2->duelOpponent = -1;
  }
  // -----------------------------------------------------------------------

  playerState_t *ps1 = SV_GameClientNum(p1);
  playerState_t *ps2 = SV_GameClientNum(p2);

  r1->inDuel = qtrue;
  r1->duelOpponent = p2;
  r1->duelStartTime = svs.time;
  r1->lastBP = ps1 ? ps1->stats[STAT_ARMOR] : -1;

  r2->inDuel = qtrue;
  r2->duelOpponent = p1;
  r2->duelStartTime = svs.time;
  r2->lastBP = ps2 ? ps2->stats[STAT_ARMOR] : -1;



  // Announce start with nice broadcast showing both players ELO
  cJSON *a1 = SV_Ranked_GetAccount(r1->username);
  cJSON *a2 = SV_Ranked_GetAccount(r2->username);
  cJSON *m1 = a1 ? GetModeDataForAccount(a1) : NULL;
  cJSON *m2 = a2 ? GetModeDataForAccount(a2) : NULL;
  // Use duel mode data specifically for announcement
  if (a1) {
    cJSON *modes1 = cJSON_GetObjectItemCaseSensitive(a1, "modes");
    if (modes1)
      m1 = cJSON_GetObjectItemCaseSensitive(modes1, "duel");
  }
  if (a2) {
    cJSON *modes2 = cJSON_GetObjectItemCaseSensitive(a2, "modes");
    if (modes2)
      m2 = cJSON_GetObjectItemCaseSensitive(modes2, "duel");
  }
  cJSON *e1ptr = m1 ? cJSON_GetObjectItemCaseSensitive(m1, "elo") : NULL;
  cJSON *e2ptr = m2 ? cJSON_GetObjectItemCaseSensitive(m2, "elo") : NULL;
  int p1mmr = e1ptr ? e1ptr->valueint : 1000;
  int p2mmr = e2ptr ? e2ptr->valueint : 1000;

  SV_SendServerCommand(
      NULL, "print \"^5%s ^7[^2%d Elo^7] ^3VS ^5%s ^7[^2%d Elo^7]\n\"",
      svs.clients[p1].name, p1mmr, svs.clients[p2].name, p2mmr);

  // CP for both duel participants — mimics VICTORY/DEFEAT style
  const char *dTitle1 = SV_Ranked_GetTitle(p1mmr, a1);
  const char *dTitle2 = SV_Ranked_GetTitle(p2mmr, a2);
  SV_SendServerCommand(&svs.clients[p1],
                       "cp \"^7%s\n^3%s  ^2%d Elo\n^3VS\n^7%s\n^3%s  ^2%d Elo\"",
                       svs.clients[p1].name, dTitle1, p1mmr,
                       svs.clients[p2].name, dTitle2, p2mmr);
  SV_SendServerCommand(&svs.clients[p2],
                       "cp \"^7%s\n^3%s  ^2%d Elo\n^3VS\n^7%s\n^3%s  ^2%d Elo\"",
                       svs.clients[p1].name, dTitle1, p1mmr,
                       svs.clients[p2].name, dTitle2, p2mmr);

  char u1[64], u2[64];
  SV_Ranked_GetLogUsername(p1, u1, sizeof(u1));
  SV_Ranked_GetLogUsername(p2, u2, sizeof(u2));
  SV_Ranked_Log("DUEL: Started %s (%d) VS %s (%d)", u1, p1mmr, u2, p2mmr);
}

void SV_Ranked_DuelStop(int p1, int p2) {
  if (p1 >= 0 && p1 < sv_maxclients->integer)
    sv_rankedPlayers[p1].inDuel = qfalse;
  if (p2 >= 0 && p2 < sv_maxclients->integer)
    sv_rankedPlayers[p2].inDuel = qfalse;
}

int SV_Ranked_FindPlayerByNameOrId(const char *identifier) {
  if (!identifier || !identifier[0])
    return -1;

  // 1. Prioritize direct ID match
  int idMatch = atoi(identifier);
  if (idMatch >= 0 && idMatch < sv_maxclients->integer &&
      svs.clients[idMatch].state >= CS_CONNECTED) {
    char idStr[16];
    Q_strncpyz(idStr, va("%d", idMatch), sizeof(idStr));
    if (Q_stricmp(idStr, identifier) == 0) {
      return idMatch;
    }
  }

  char cleanInput[MAX_STRING_CHARS];
  Q_strncpyz(cleanInput, identifier, sizeof(cleanInput));
  Q_StripColor(cleanInput);
  Q_strlwr(cleanInput);
  int inputLen = (int)strlen(cleanInput);

  int prefixMatch = -1;
  int numPrefixes = 0;
  int partialMatches[64];
  int numPartials = 0;

  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (svs.clients[i].state < CS_CONNECTED)
      continue;

    // Clean netname
    char cleanName[MAX_STRING_CHARS];
    Q_strncpyz(cleanName, svs.clients[i].name, sizeof(cleanName));
    Q_StripColor(cleanName);
    Q_strlwr(cleanName);
    int nameLen = (int)strlen(cleanName);

    // Clean display name
    char cleanDisp[MAX_STRING_CHARS] = "";
    if (sv_rankedPlayers[i].displayName[0]) {
      Q_strncpyz(cleanDisp, sv_rankedPlayers[i].displayName, sizeof(cleanDisp));
      Q_StripColor(cleanDisp);
      Q_strlwr(cleanDisp);
    }
    int dispLen = (int)strlen(cleanDisp);

    // 1. Exact match
    if (strcmp(cleanName, cleanInput) == 0 || (cleanDisp[0] && strcmp(cleanDisp, cleanInput) == 0)) {
      return i;
    }

    // 2. Truncation / Prefix match
    int minLenName = (nameLen < inputLen) ? nameLen : inputLen;
    if (minLenName >= 3 && strncmp(cleanName, cleanInput, minLenName) == 0) {
      prefixMatch = i;
      numPrefixes++;
    }
    if (dispLen > 0) {
      int minLenDisp = (dispLen < inputLen) ? dispLen : inputLen;
      if (minLenDisp >= 3 && strncmp(cleanDisp, cleanInput, minLenDisp) == 0) {
        prefixMatch = i;
        numPrefixes++;
      }
    }

    // 3. Substring match
    if (strstr(cleanName, cleanInput) || (cleanDisp[0] && strstr(cleanDisp, cleanInput))) {
      if (numPartials < 64) {
        partialMatches[numPartials++] = i;
      }
    }
  }

  if (numPrefixes == 1) {
    return prefixMatch;
  }
  if (numPartials == 1) {
    return partialMatches[0];
  }
  if (numPrefixes > 1) {
    return prefixMatch;
  }
  if (numPartials > 1) {
    return partialMatches[0];
  }

  return -1;
}

static void AnnounceSpecialDuelResults(int winnerId, int loserId) {
  rankedMatchState_t *rWin = &sv_rankedPlayers[winnerId];

  if (!rWin->loggedIn)
    return;

  cJSON *accWin = SV_Ranked_GetAccount(rWin->username);
  if (!accWin)
    return;

  cJSON *winMsg = cJSON_GetObjectItemCaseSensitive(accWin, "winMsg");
  if (winMsg && winMsg->valuestring && winMsg->valuestring[0]) {
    char formattedMsg[512];
    Q_strncpyz(formattedMsg, winMsg->valuestring, sizeof(formattedMsg));

    // Basic string helper to split long messages
    int len = strlen(formattedMsg);
    if (len > 30) {
      for (int i = 30; i < len; i++) {
        if (formattedMsg[i] == ' ') {
          formattedMsg[i] = '\n';
          break;
        }
      }
    }

    SV_SendServerCommand(svs.clients + winnerId, "cp \"%s\"", formattedMsg);
    SV_SendServerCommand(svs.clients + loserId, "cp \"%s\"", formattedMsg);
    SV_Ranked_Log("WINMSG: Sent custom message '%s' for %s", formattedMsg, rWin->username);
  }

  // Custom win sound feature removed — unreliable playback in MBII engine.
}

static const char *SV_Ranked_WeaponName(int weapon) {
  switch (weapon) {
    case WP_SABER:          return "Saber";
    case WP_BRYAR_PISTOL:   return "Pistol";
    case WP_BLASTER:        return "Blaster";
    case WP_DISRUPTOR:      return "Disruptor";
    case WP_BOWCASTER:      return "Bowcaster";
    case WP_REPEATER:       return "Repeater";
    case WP_DEMP2:          return "Demp2";
    case WP_FLECHETTE:      return "Flechette";
    case WP_ROCKET_LAUNCHER:return "Rocket";
    case WP_THERMAL:        return "Thermal";
    case WP_TRIP_MINE:      return "TripMine";
    case WP_DET_PACK:       return "DetPack";
    case WP_CONCUSSION:     return "Concussion";
    case WP_BRYAR_OLD:      return "BryarOld";
    case WP_STUN_BATON:     return "StunBaton";
    case WP_EMPLACED_GUN:   return "EmplacedGun";
    case WP_TURRET:         return "Turret";
    case WP_MELEE:          return "Melee";
    default:                return "None";
  }
}

static const char *SV_Ranked_SaberStyleName(int style) {
  switch (style) {
    case SS_FAST:   return "Fast";
    case SS_MEDIUM: return "Medium";
    case SS_STRONG: return "Strong";
    case SS_DESANN: return "Desann";
    case SS_TAVION: return "Tavion";
    case SS_STAFF:  return "Staff";
    case SS_DUAL:   return "Duals";
    default:        return "None";
  }
}

void SV_Ranked_DuelEnd(int winnerId, int loserId, int isTie, int isDisconnect,
                       int mod) {
  Com_Printf(
      "[RANKED] DuelEnd ENTERED: winner=%d loser=%d tie=%d disc=%d mod=%d\n",
      winnerId, loserId, isTie, isDisconnect, mod);

  if (winnerId < 0 || winnerId >= sv_maxclients->integer || loserId < 0 ||
      loserId >= sv_maxclients->integer) {
    Com_Printf("[RANKED] DuelEnd: invalid IDs, returning.\n");
    return;
  }

  rankedMatchState_t *rWin = &sv_rankedPlayers[winnerId];
  rankedMatchState_t *rLose = &sv_rankedPlayers[loserId];

  int winHealth = 0;
  int winArmor = 0;
  int winWeapon = 0;
  int winStyle = 0;
  int winForce = 0;
  int loseWeapon = 0;
  int loseStyle = 0;

  playerState_t *psWin = SV_GameClientNum(winnerId);
  playerState_t *psLose = SV_GameClientNum(loserId);
  if (psWin) {
    winHealth = psWin->stats[STAT_HEALTH];
    winArmor = (psWin->weapon == WP_SABER || psWin->fd.forcePower > 0 || psWin->stats[STAT_ARMOR] <= 0) ? psWin->fd.forcePower : psWin->stats[STAT_ARMOR];

    winWeapon = psWin->weapon;
    winStyle = psWin->fd.saberAnimLevel;
    winForce = psWin->fd.forcePower;
  }
  if (psLose) {
    loseWeapon = psLose->weapon;
    loseStyle = psLose->fd.saberAnimLevel;
  }

  const char *winWeaponStr = SV_Ranked_WeaponName(winWeapon);
  const char *winStyleStr = SV_Ranked_SaberStyleName(winStyle);
  const char *loseWeaponStr = SV_Ranked_WeaponName(loseWeapon);
  const char *loseStyleStr = SV_Ranked_SaberStyleName(loseStyle);

  if (!rWin->inDuel || rWin->duelOpponent != loserId)
    return;

  rWin->inDuel = qfalse;
  rLose->inDuel = qfalse;

  if (isTie) {
    SV_SendServerCommand(NULL, "chat \"^7Duel tied between %s and %s\"",
                         svs.clients[winnerId].name, svs.clients[loserId].name);
    return;
  }

  // Casual Mode Check (if either player has ranked disabled)
  if (!rWin->rankedEnabled || !rLose->rankedEnabled) {
    SV_SendServerCommand(NULL, va("print \"^3[CASUAL DUEL] ^5%s ^7defeated ^6%s ^7(Casual Match - No Elo Changed)\n\"",
                                 svs.clients[winnerId].name, svs.clients[loserId].name));
    if (rWin->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[winnerId]);
    if (rLose->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[loserId]);
    return;
  }

  // Unengaged / Accidental Match Void Check:
  // If BOTH players still have >= 90 BP when the match ended (no real combat occurred / accidental death), void the match.
  int currentWinBP = (psWin && psWin->jetpackFuel > 0) ? psWin->jetpackFuel : (rWin->lastBP > 0 ? rWin->lastBP : winArmor);
  int currentLoseBP = (rLose->lastBP > 0) ? rLose->lastBP : ((psLose && psLose->jetpackFuel > 0) ? psLose->jetpackFuel : 100);
  if (currentWinBP >= 90 && currentLoseBP >= 90 && !isDisconnect) {
    SV_SendServerCommand(NULL, va("print \"^3[DUEL VOIDED] ^7Duel between ^5%s ^7and ^5%s ^7ended before combat engaged (Both had full BP). No Elo changed.\n\"",
                                 svs.clients[winnerId].name, svs.clients[loserId].name));
    Com_Printf("[RANKED] Duel VOIDED: winBP=%d loseBP=%d (no combat engaged)\n", currentWinBP, currentLoseBP);
    if (rWin->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[winnerId]);
    if (rLose->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[loserId]);
    return;
  }

  // Get accounts — NULL for temp (no-GUID / not logged-in) players.
  // Ranked players still earn full Elo even when their opponent is temp.
  cJSON *aWin  = (rWin->loggedIn  && rWin->username[0])  ? SV_Ranked_GetAccount(rWin->username)  : NULL;
  cJSON *aLose = (rLose->loggedIn && rLose->username[0]) ? SV_Ranked_GetAccount(rLose->username) : NULL;

  if (!aWin && !aLose)
    Com_Printf("[RANKED] DuelEnd: both players are temp — no stats saved.\n");

  // Get/create duel mode data per-player (remains NULL for temp players)
  cJSON *mWin  = NULL;
  cJSON *mLose = NULL;
  if (aWin) {
    cJSON *modesWin = cJSON_GetObjectItemCaseSensitive(aWin, "modes");
    if (modesWin) {
      mWin = cJSON_GetObjectItemCaseSensitive(modesWin, "duel");
      if (!mWin) {
        mWin = cJSON_CreateObject();
        cJSON_AddNumberToObject(mWin, "elo", 1000);
        cJSON_AddNumberToObject(mWin, "wins", 0);
        cJSON_AddNumberToObject(mWin, "losses", 0);
        cJSON_AddNumberToObject(mWin, "kills", 0);
        cJSON_AddNumberToObject(mWin, "deaths", 0);
        cJSON_AddItemToObject(modesWin, "duel", mWin);
      }
    }
  }
  if (aLose) {
    cJSON *modesLose = cJSON_GetObjectItemCaseSensitive(aLose, "modes");
    if (modesLose) {
      mLose = cJSON_GetObjectItemCaseSensitive(modesLose, "duel");
      if (!mLose) {
        mLose = cJSON_CreateObject();
        cJSON_AddNumberToObject(mLose, "elo", 1000);
        cJSON_AddNumberToObject(mLose, "wins", 0);
        cJSON_AddNumberToObject(mLose, "losses", 0);
        cJSON_AddNumberToObject(mLose, "kills", 0);
        cJSON_AddNumberToObject(mLose, "deaths", 0);
        cJSON_AddItemToObject(modesLose, "duel", mLose);
      }
    }
  }

  // MMR/stats default to in-memory session ELO (starts at 1000) — overwritten below if player has an account
  int wMmr = rWin->tempElo > 0 ? rWin->tempElo : 1000;
  int lMmr = rLose->tempElo > 0 ? rLose->tempElo : 1000;
  int wWins = 0, wLosses = 0, lWins = 0, lLosses = 0;
  int wStreak = 0, lStreak = 0;

  cJSON *ptr;
  if (mWin) {
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mWin, "elo")))    wMmr    = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mWin, "wins")))   wWins   = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mWin, "losses"))) wLosses = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mWin, "streak"))) wStreak = ptr->valueint;
  }
  if (mLose) {
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mLose, "elo")))    lMmr    = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mLose, "wins")))   lWins   = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mLose, "losses"))) lLosses = ptr->valueint;
    if ((ptr = cJSON_GetObjectItemCaseSensitive(mLose, "streak"))) lStreak = ptr->valueint;
  }

  int newWMmr = wMmr + CalculateDuelElo(wMmr, lMmr, qtrue, wWins + wLosses);
  int newLMmr = lMmr + CalculateDuelElo(lMmr, wMmr, qfalse, lWins + lLosses);

  int winEloChange = newWMmr - wMmr;
  int loseEloChange = newLMmr - lMmr;

  int streakBonus = wStreak / 2;
  if (streakBonus > 10)
    streakBonus = 10;
  winEloChange += streakBonus;

  int upsetBonus = 0;
  if (lMmr - wMmr > 150) {
    upsetBonus = (lMmr - wMmr) / 40;
    if (upsetBonus > 12)
      upsetBonus = 12;
    winEloChange += upsetBonus;
  }

  Com_Printf("[RANKED] DuelEnd: Bonuses applied. winEloChange now %d\n",
             winEloChange);

  // Elo Boost shop effect (+15%)
  if (rWin->activeEloBoost) {
    winEloChange = (int)((double)winEloChange * 1.15);
    if (winEloChange < 1)
      winEloChange = 1;
    rWin->activeEloBoost = 0;
    SV_SendServerCommand(
        svs.clients + winnerId,
        "print \"^5[Elo Boost] ^7+15%% Elo bonus applied this duel!\n\"");
  }

  // Anti-Farm consecutive rematch tracking
  const char *loserIdentifier = (rLose->loggedIn && rLose->username[0]) ? rLose->username : svs.clients[loserId].name;
  if (rWin->lastDuelOpponent[0] && !Q_stricmp(rWin->lastDuelOpponent, loserIdentifier)) {
    rWin->consecutiveSameOpponentCount++;
  } else {
    Q_strncpyz(rWin->lastDuelOpponent, loserIdentifier, sizeof(rWin->lastDuelOpponent));
    rWin->consecutiveSameOpponentCount = 1;
  }

  double rematchMultiplier = 1.0;
  if (rWin->consecutiveSameOpponentCount == 3) rematchMultiplier = 0.75;
  else if (rWin->consecutiveSameOpponentCount == 4) rematchMultiplier = 0.50;
  else if (rWin->consecutiveSameOpponentCount >= 5) rematchMultiplier = 0.25;

  if (rematchMultiplier < 1.0) {
    winEloChange = (int)round((double)winEloChange * rematchMultiplier);
    if (winEloChange < 1) winEloChange = 1;
    loseEloChange = (int)round((double)loseEloChange * rematchMultiplier);
    if (loseEloChange > -1) loseEloChange = -1;
    SV_SendServerCommand(svs.clients + winnerId, va("print \"^3[Anti-Farm] ^7Consecutive duel #%d vs same opponent (Elo scaled to %d%%)\n\"", rWin->consecutiveSameOpponentCount, (int)(rematchMultiplier * 100)));
  }

  // Close Fight Mitigation: If loser brought winner below 25 HP or 20 BP, reduce loser's penalty by 30%
  if (winHealth < 25 || (winArmor > 0 && winArmor < 20)) {
    loseEloChange = (int)round((double)loseEloChange * 0.70);
    if (loseEloChange > -1) loseEloChange = -1;
    SV_SendServerCommand(NULL, va("print \"^6[VALIANT EFFORT] ^5%s ^7fought hard and nearly defeated ^5%s^7 (-30%% Elo Loss Shielded)!\n\"",
                                 svs.clients[loserId].name, svs.clients[winnerId].name));
  }

  // Flawless Defense Bonus (+2 Elo): 100 HP and >= 90 BP
  if (winHealth >= 100 && winArmor >= 90) {
    winEloChange += 2;
    SV_SendServerCommand(NULL, va("print \"^3[FLAWLESS DEFENSE] ^5%s ^7dominated ^1%s ^7with an impenetrable defense (100 HP, %d BP)! (+2 Bonus Elo)\n\"",
                                 svs.clients[winnerId].name, svs.clients[loserId].name, winArmor));
  }

  // Clutch Comeback Bonus (+3 Elo): Winner dropped below 15 HP or 10 BP
  if (winHealth <= 15 || (winArmor > 0 && winArmor <= 10)) {
    winEloChange += 3;
    SV_SendServerCommand(NULL, va("print \"^2[CLUTCH COMEBACK] ^5%s ^7survived on the brink (%d HP, %d BP) and defeated ^1%s^7! (+3 Bonus Elo)\n\"",
                                 svs.clients[winnerId].name, winHealth, winArmor, svs.clients[loserId].name));
  }

  // Loser Mitigation
  int cappedLStreak = lStreak > 20 ? 20 : lStreak;
  double lossReductionPercent = cappedLStreak * 0.025;
  loseEloChange =
      (int)round((double)loseEloChange * (1.0 - lossReductionPercent));

  // Duration tracking
  int durationSecs = (svs.time - rWin->duelStartTime) / 1000;

  // Ultra-fast duel cap: <3s duels give minimal Elo swing
  if (durationSecs < 3 && !isDisconnect) {
    winEloChange = 1;
    loseEloChange = -1;
  }

  // Existing duration scaling for 3-9s duels
  if (durationSecs >= 3 && durationSecs < 10 && !isDisconnect) {
    double mod = (double)durationSecs / 10.0;
    loseEloChange = (int)round((double)loseEloChange * mod);
    if (loseEloChange > -1)
      loseEloChange = -1;
  }

  if (isDisconnect) {
    // Disconnect Penalty for leaving duel
    loseEloChange -= 5;
  }

  // 1. Guest/Temp player matching overrides (Visual ELO for guests, safety for registered)
  bool isGuestWin = !rWin->loggedIn;
  bool isGuestLose = !rLose->loggedIn;

  if (isGuestWin || isGuestLose) {
    if (!isGuestWin) {
      // Winner is registered, opponent is guest -> registered gets +1 ELO
      winEloChange = 1;
    }
    if (!isGuestLose) {
      // Loser is registered, opponent is guest -> registered gets -1 ELO
      loseEloChange = -1;
    }
  } else {
    // 2. Provisional Calibration Dampening (Anti-Smurf Safeguard for registered vs registered)
    // If the winner is a new account (total games < 20), dampen the registered loser's ELO loss.
    int winnerGames = wWins + wLosses;
    if (winnerGames < 20 && loseEloChange < 0) {
      double scale = (double)winnerGames / 20.0;
      if (scale < 0.0) scale = 0.0;
      int dampLoss = (int)round((double)loseEloChange * scale);
      // Ensure the loser loses at least -1 ELO if they had a negative change
      if (dampLoss > -1) {
        dampLoss = -1;
      }
      loseEloChange = dampLoss;
    }
  }

  // Update in-memory session ELO for both players
  rWin->tempElo = wMmr + winEloChange;
  rLose->tempElo = lMmr + loseEloChange;

  SV_SendServerCommand(svs.clients + winnerId,
                       "cp \"^2VICTORY!\n^2+%d ^7(%d)\"", winEloChange,
                       wMmr + winEloChange);
  SV_SendServerCommand(svs.clients + loserId, "cp \"^1DEFEAT.\n^1%d ^7(%d)\"",
                       loseEloChange, lMmr + loseEloChange);

  SV_SendServerCommand(svs.clients + winnerId,
                       "chat \"^2VICTORY! ^7Elo: ^2+%d ^7(now %d)\"",
                       winEloChange, wMmr + winEloChange);
  SV_SendServerCommand(svs.clients + loserId,
                       "chat \"^1DEFEAT. ^7Elo: ^1%d ^7(now %d)\"",
                       loseEloChange, lMmr + loseEloChange);

  // ---- DUEL BADGES & FLAVOR ----
  const char *modBadge = NULL;
  if (!isTie && !isDisconnect) {
    switch (mod) {
      case MOD_SABER:
        modBadge = "Saber Master";
        break;
      case MOD_MELEE:
        modBadge = "Brawler";
        break;
      case MOD_FORCE_DARK:
        modBadge = "Dark Lord";
        break;
      case MOD_SUICIDE:
        modBadge = "Forfeit";
        break;
      default:
        break;
    }

    if (modBadge) {
      SV_SendServerCommand(
          NULL,
          "chat \"^5%s ^7won with ^3%s^7! Badge: ^2%s\"",
          svs.clients[winnerId].name,
          mod == MOD_SABER      ? "a SABER STRIKE"
          : mod == MOD_MELEE    ? "a KICK"
          : mod == MOD_FORCE_DARK ? "DARK SIDE POWERS"
          :                       "UNKNOWN",
          modBadge);
    }

    if (durationSecs < 8) {
      SV_SendServerCommand(
          NULL,
          "print \"^3SPEED DEMON! ^7%s ^7won in ^3%d seconds^7!\n\"",
          svs.clients[winnerId].name, durationSecs);
    } else if (durationSecs > 120) {
      SV_SendServerCommand(
          NULL,
          "print \"^5MARATHON! ^7%s ^7won after ^5%d seconds^7!\n\"",
          svs.clients[winnerId].name, durationSecs);
    }
  }

  SV_Ranked_Log("DUEL_END: %s def. %s (+%d / %d) [Tie: %d, Disc: %d, Mod: %d, Dur: %ds, W_HP: %d, W_AP: %d, W_FP: %d, W_WP: %s, W_St: %s, L_WP: %s, L_St: %s]",
                svs.clients[winnerId].name, svs.clients[loserId].name,
                winEloChange, loseEloChange, isTie, isDisconnect, mod,
                durationSecs, winHealth, winArmor, winForce, winWeaponStr, winStyleStr,
                loseWeaponStr, loseStyleStr);

  // Record duel kill/death and credits — only for logged-in (ranked) players
  cJSON *wCrPtr = SV_Ranked_GetSetting("duel_win_credits");
  cJSON *lCrPtr = SV_Ranked_GetSetting("duel_loss_credits");
  int wCr = wCrPtr ? wCrPtr->valueint : 25;
  int lCr = lCrPtr ? lCrPtr->valueint : 5;
  cJSON *xpDuelWinPtr = SV_Ranked_GetSetting("xp_per_duel_win");
  int xpPerDuelWin = xpDuelWinPtr ? xpDuelWinPtr->valueint : 50;

  if (rWin->loggedIn && aWin) {
    UpdateAccountStats(rWin->username, svs.clients[winnerId].name, 0, xpPerDuelWin, 1, 0, "Lightsaber");
    UpdateAccountCredits(rWin->username, wCr);
    // Daily quest: duel wins
    SV_Ranked_ProgressQuest(rWin->username, "duel_wins", 1, &svs.clients[winnerId]);
  }
  if (rLose->loggedIn && aLose) {
    UpdateAccountStats(rLose->username, svs.clients[loserId].name, 0, 0, 0, 1, NULL);
    UpdateAccountCredits(rLose->username, lCr);
  }

  int loseHealth = (psLose && psLose->stats[STAT_HEALTH] > 0) ? psLose->stats[STAT_HEALTH] : 0;
  int loseBP = (rLose->lastBP > 0) ? rLose->lastBP : ((psLose && psLose->stats[STAT_ARMOR] > 0) ? psLose->stats[STAT_ARMOR] : 0);

  // Toast notifications — winner sees green card with remaining HP and BP, loser sees red card with their own stats
  SV_SendServerCommand(svs.clients + winnerId,
                       va("toast_win %d %d %d \"%s\" %d %d",
                          winEloChange,
                          wCr,
                          xpPerDuelWin,
                          svs.clients[loserId].name,
                          winHealth,
                          winArmor));

  SV_SendServerCommand(svs.clients + loserId,
                       va("toast_lose %d %d %d \"%s\" %d %d",
                          loseEloChange,
                          lCr,
                          0,
                          svs.clients[winnerId].name,
                          loseHealth,
                          loseBP));

  int duelDurationSec = (rWin->duelStartTime > 0) ? (svs.time - rWin->duelStartTime) / 1000 : 0;

  // Record Full Server Duel Analysis

  SV_Ranked_Log("DUEL_ANALYSIS: Winner '%s' [HP:%d, BP:%d, Wep:%s, Style:%s, ELO:+%d] defeated Loser '%s' [Wep:%s, Style:%s, ELO:%d, Duration:%ds]",
                svs.clients[winnerId].name, winHealth, winArmor, winWeaponStr, winStyleStr, winEloChange,
                svs.clients[loserId].name, loseWeaponStr, loseStyleStr, loseEloChange, duelDurationSec);

  extern void SV_Ranked_DB_RecordDuelAnalysis(const char *winnerName, const char *loserName, int winHealth, int winBP, int winEloDelta, int loseEloDelta, int durationSec);
  if (rWin->loggedIn && rWin->username[0]) {
    SV_Ranked_DB_RecordDuelAnalysis(rWin->username, (rLose->loggedIn && rLose->username[0]) ? rLose->username : svs.clients[loserId].name, winHealth, winArmor, winEloChange, loseEloChange, duelDurationSec);
  }




  // Check Hot Potato transfer on duel kill
  extern void SV_Ranked_HotPotato_CheckKill(int victimId, int killerId);
  SV_Ranked_HotPotato_CheckKill(loserId, winnerId);


  // ---- CLAIM BOUNTY ON LOSER ----
  if (rLose->bountyValue > 0) {
    int bVal = rLose->bountyValue;
    SV_SendServerCommand(svs.clients + winnerId,
                         "cp \"^2VICTORY! ^3BOUNTY CLAIMED!\n^2+%d ^7Credits\"",
                         bVal);
    SV_SendServerCommand(
        NULL,
        "chat \"^2%s ^7claimed the bounty on ^1%s^7, earning ^5%d credits^7!\"",
        svs.clients[winnerId].name, svs.clients[loserId].name, bVal);
    rLose->bountyValue = 0;
    if (rWin->loggedIn && aWin) {
      UpdateAccountCredits(rWin->username, bVal);
      // Daily quest: claim bounty in duel
      SV_Ranked_ProgressQuest(rWin->username, "d_bounties", 1, &svs.clients[winnerId]);
      // Achievement: bounty hunter
      SV_Ranked_GrantAchievement(rWin->username, "bounty_claim", &svs.clients[winnerId]);
    }
  }

  // ---- DUEL WIN STREAK MILESTONES (every 5 wins) ----
  {
    int newStreak = wStreak + 1;
    if (newStreak > 0 && newStreak % 5 == 0) {
      cJSON *sBountyPtr = SV_Ranked_GetSetting("streak_bounty_factor");
      int sBounty = sBountyPtr ? sBountyPtr->valueint : 50;
      cJSON *sCreditsPtr = SV_Ranked_GetSetting("streak_credits_factor");
      int sCredits = sCreditsPtr ? sCreditsPtr->valueint : 25;
      cJSON *sXpPtr = SV_Ranked_GetSetting("streak_xp_factor");
      int sXp = sXpPtr ? sXpPtr->valueint : 50;

      int tier      = newStreak / 5;
      int bountyAdd = tier * sBounty;
      int crBonus   = tier * sCredits;
      int xpBonus   = tier * sXp;
      rWin->bountyValue += bountyAdd;
      SV_SendServerCommand(NULL,
        "chat \"^1WANTED! ^7%s ^7is on a ^1%d-win duel streak! "
        "Bounty: ^3%d^7 | ^2+%d CR ^3+%d XP^7!\"",
        svs.clients[winnerId].name, newStreak, rWin->bountyValue, crBonus, xpBonus);
      if (rWin->loggedIn && aWin) {
        UpdateAccountCredits(rWin->username, crBonus);
        UpdateAccountStats(rWin->username, svs.clients[winnerId].name, 0, xpBonus, 0, 0, NULL);
      }
      SV_Ranked_Log("STREAK: %s hit %d-win streak (tier %d, bounty %d, +%d CR, +%d XP)",
                    rWin->username, newStreak, tier, rWin->bountyValue, crBonus, xpBonus);
    }
  }

  AnnounceSpecialDuelResults(winnerId, loserId);

  // ---- WINNER RANK UP ----
  if (rWin->loggedIn && aWin) {
    const char *oldRankWin = SV_Ranked_GetTitle(wMmr, aWin);
    const char *newRankWin = SV_Ranked_GetTitle(wMmr + winEloChange, aWin);
    if (Q_stricmp(oldRankWin, newRankWin) != 0 && (winEloChange > 0)) {
      SV_SendServerCommand(NULL,
                           "chat \"^2PROMOTION! ^7%s ^7has reached ^5%s^7!\"",
                           svs.clients[winnerId].name, newRankWin);
      SV_SendServerCommand(svs.clients + winnerId, "cp \"^2PROMOTED!\n^5%s\"",
                           newRankWin);
      cJSON *crPromoPtr = SV_Ranked_GetSetting("credits_per_promotion");
      int crPromo = crPromoPtr ? crPromoPtr->valueint : 500;
      UpdateAccountCredits(rWin->username, crPromo);
      // Daily quest: get promoted via dueling
      SV_Ranked_ProgressQuest(rWin->username, "d_rankups", 1, &svs.clients[winnerId]);
      SV_Ranked_Log("RANK: %s PROMOTED from %s to %s", rWin->username, oldRankWin, newRankWin);
      // Achievement: first rank up
      SV_Ranked_GrantAchievement(rWin->username, "rank_up", &svs.clients[winnerId]);
    }
  }

  // ---- DUEL ACHIEVEMENT CHECKS ----
  {
    cJSON *wAcc = SV_Ranked_GetAccount(rWin->username);
    if (wAcc) {
      cJSON *wMd = GetModeDataForAccount(wAcc);
      int duelWins = 0;
      if (wMd) {
        cJSON *wPtr = cJSON_GetObjectItemCaseSensitive(wMd, "wins");
        if (wPtr)
          duelWins = wPtr->valueint;
      }
      SV_Ranked_CheckDuelAchievements(rWin->username, duelWins,
                                      wMmr + winEloChange,
                                      &svs.clients[winnerId]);
      SV_Ranked_CheckEconomyAchievements(rWin->username,
                                         &svs.clients[winnerId]);
    }
  }

  // ---- LOSER RANK DEMOTION ----
  if (rLose->loggedIn && aLose) {
    const char *oldRankLose = SV_Ranked_GetTitle(lMmr, aLose);
    const char *newRankLose = SV_Ranked_GetTitle(lMmr + loseEloChange, aLose);
    if (Q_stricmp(oldRankLose, newRankLose) != 0 && (loseEloChange < 0)) {
      SV_SendServerCommand(NULL,
                           "chat \"^1DEMOTION. ^7%s ^7has dropped to ^1%s^7.\"",
                           svs.clients[loserId].name, newRankLose);
      SV_SendServerCommand(svs.clients + loserId, "cp \"^1DEMOTED\n^1%s\"",
                           newRankLose);
      SV_Ranked_Log("RANK: %s DEMOTED from %s to %s", rLose->username, oldRankLose, newRankLose);
    }
  }

  // Resolve active bets
  for (int i = 0; i < sv_maxclients->integer; i++) {
    client_t *c = svs.clients + i;
    if (c->state >= CS_CONNECTED && sv_rankedPlayers[i].loggedIn &&
        sv_rankedPlayers[i].currentBetAmount > 0) {
      int betTarget = sv_rankedPlayers[i].currentBetTarget;
      // if this bet is on either player in THIS duel
      if (betTarget == winnerId || betTarget == loserId) {
        if (isTie) {
          UpdateAccountCredits(sv_rankedPlayers[i].username,
                               sv_rankedPlayers[i].currentBetAmount);
          SV_SendServerCommand(
              c, "chat \"^3Your bet was refunded due to a tie.\"");
        } else if (betTarget == winnerId) {
          int winnings = sv_rankedPlayers[i].currentBetAmount * 2;
          UpdateAccountCredits(sv_rankedPlayers[i].username, winnings);
          SV_SendServerCommand(
              c, "chat \"^2You won ^5%d ^2credits from your bet on %s!\"",
              winnings, svs.clients[winnerId].name);
        } else {
          SV_SendServerCommand(
              c, "chat \"^1You lost your bet of ^5%d ^1credits on %s.\"",
              sv_rankedPlayers[i].currentBetAmount, svs.clients[loserId].name);
        }
        sv_rankedPlayers[i].currentBetTarget = -1;
        sv_rankedPlayers[i].currentBetAmount = 0;
      }
    }
  }

  // Update ranked player JSON data (skipped for temp players whose mWin/mLose are NULL)
  if (mWin && rWin->loggedIn) {
    cJSON *eloWinPtr = cJSON_GetObjectItemCaseSensitive(mWin, "elo");
    if (eloWinPtr) cJSON_SetNumberValue(eloWinPtr, wMmr + winEloChange);
    else           cJSON_AddNumberToObject(mWin, "elo", wMmr + winEloChange);

    if (cJSON_GetObjectItemCaseSensitive(mWin, "wins"))
      cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(mWin, "wins"), wWins + 1);
    else
      cJSON_AddNumberToObject(mWin, "wins", wWins + 1);

    if (cJSON_GetObjectItemCaseSensitive(mWin, "streak"))
      cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(mWin, "streak"), wStreak + 1);
    else
      cJSON_AddNumberToObject(mWin, "streak", wStreak + 1);

    cJSON *hs = cJSON_GetObjectItemCaseSensitive(mWin, "highest_streak");
    if (hs) {
      if (wStreak + 1 > hs->valueint) cJSON_SetNumberValue(hs, wStreak + 1);
    } else {
      cJSON_AddNumberToObject(mWin, "highest_streak", wStreak + 1);
    }

    // ---- DUEL STATS (duration & MOD badges) ----
    if (durationSecs < 8 && !isDisconnect) {
      cJSON *sd = cJSON_GetObjectItemCaseSensitive(mWin, "speed_demon_wins");
      if (sd) cJSON_SetNumberValue(sd, sd->valueint + 1);
      else cJSON_AddNumberToObject(mWin, "speed_demon_wins", 1);
    }
    if (durationSecs > 120 && !isDisconnect) {
      cJSON *mw = cJSON_GetObjectItemCaseSensitive(mWin, "marathon_wins");
      if (mw) cJSON_SetNumberValue(mw, mw->valueint + 1);
      else cJSON_AddNumberToObject(mWin, "marathon_wins", 1);
    }

    // Total duel time for average calculation
    cJSON *tdt = cJSON_GetObjectItemCaseSensitive(mWin, "total_duel_time");
    if (tdt) cJSON_SetNumberValue(tdt, tdt->valueint + durationSecs);
    else cJSON_AddNumberToObject(mWin, "total_duel_time", durationSecs);

    cJSON *tdc = cJSON_GetObjectItemCaseSensitive(mWin, "total_duel_count");
    if (tdc) cJSON_SetNumberValue(tdc, tdc->valueint + 1);
    else cJSON_AddNumberToObject(mWin, "total_duel_count", 1);

    // MOD win counters
    if (!isDisconnect) {
      const char *modField = NULL;
      switch (mod) {
        case MOD_SABER:      modField = "mod_saber_wins";      break;
        case MOD_MELEE:      modField = "mod_melee_wins";      break;
        case MOD_FORCE_DARK: modField = "mod_force_dark_wins"; break;
        case MOD_SUICIDE:    modField = "mod_suicide_wins";    break;
        default:             modField = "mod_other_wins";      break;
      }
      if (modField) {
        cJSON *mc = cJSON_GetObjectItemCaseSensitive(mWin, modField);
        if (mc) cJSON_SetNumberValue(mc, mc->valueint + 1);
        else cJSON_AddNumberToObject(mWin, modField, 1);
      }
    }
  }

  if (mLose && rLose->loggedIn) {
    cJSON *eloLosePtr = cJSON_GetObjectItemCaseSensitive(mLose, "elo");
    if (eloLosePtr) cJSON_SetNumberValue(eloLosePtr, lMmr + loseEloChange);
    else            cJSON_AddNumberToObject(mLose, "elo", lMmr + loseEloChange);

    if (cJSON_GetObjectItemCaseSensitive(mLose, "losses"))
      cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(mLose, "losses"), lLosses + 1);
    else
      cJSON_AddNumberToObject(mLose, "losses", lLosses + 1);

    // Reset loser streak to 0 in JSON (create field if missing)
    if (cJSON_GetObjectItemCaseSensitive(mLose, "streak"))
      cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(mLose, "streak"), 0);
    else
      cJSON_AddNumberToObject(mLose, "streak", 0);
  }

  // ---- RIVAL TRACKING — per-player, works even when opponent is temp ----
  {
    extern void SV_Ranked_TrackRival(const char *username, const char *opponentGuid, const char *opponentName);
    char cleanLose[64], cleanWin[64];
    Q_strncpyz(cleanLose, svs.clients[loserId].name,  sizeof(cleanLose)); Q_StripColor(cleanLose);
    Q_strncpyz(cleanWin,  svs.clients[winnerId].name, sizeof(cleanWin));  Q_StripColor(cleanWin);

    if (rWin->loggedIn && aWin) {
      cJSON *lGuidPtr = aLose ? cJSON_GetObjectItemCaseSensitive(aLose, "engine_guid") : NULL;
      const char *lIdent = (lGuidPtr && lGuidPtr->valuestring && lGuidPtr->valuestring[0])
                               ? lGuidPtr->valuestring : va("temp:%s", cleanLose);
      SV_Ranked_TrackRival(rWin->username, lIdent, svs.clients[loserId].name);
    }
    if (rLose->loggedIn && aLose) {
      cJSON *wGuidPtr = aWin ? cJSON_GetObjectItemCaseSensitive(aWin, "engine_guid") : NULL;
      const char *wIdent = (wGuidPtr && wGuidPtr->valuestring && wGuidPtr->valuestring[0])
                               ? wGuidPtr->valuestring : va("temp:%s", cleanWin);
      SV_Ranked_TrackRival(rLose->username, wIdent, svs.clients[winnerId].name);
    }
  }

  if (rWin->loggedIn || rLose->loggedIn)
    SV_Ranked_SaveAccounts();

  // Instant live sync to winner and loser HUDs
  if (rWin->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[winnerId]);
  if (rLose->loggedIn) SV_Ranked_SyncClientRPG(&svs.clients[loserId]);

  char uWin[64], uLose[64];
  SV_Ranked_GetLogUsername(winnerId, uWin, sizeof(uWin));
  SV_Ranked_GetLogUsername(loserId, uLose, sizeof(uLose));
  SV_Ranked_Log("DUEL: %s def %s | %s %+d (%d) | %s %d (%d)",
                uWin, uLose,
                uWin, winEloChange, wMmr + winEloChange,
                uLose, loseEloChange, lMmr + loseEloChange);

  Com_Printf("[RANKED] DuelEnd complete: %s defeated %s\n", uWin, uLose);

  Com_Printf("[RANKED] DuelHistory: guid_winner=%s guid_loser=%s winner_name='%s' loser_name='%s' winner_elo_change=%d loser_elo_change=%d winner_new_elo=%d loser_new_elo=%d duration_seconds=%d winner_health=%d winner_bp=%d winner_force=%d winner_weapon='%s' winner_style='%s' loser_weapon='%s' loser_style='%s'\n",
             uWin,
             uLose,
             svs.clients[winnerId].name,
             svs.clients[loserId].name,
             winEloChange,
             loseEloChange,
             wMmr + winEloChange,
             lMmr + loseEloChange,
             durationSecs,
             winHealth,
             winArmor,
             winForce,
             winWeaponStr,
             winStyleStr,
             loseWeaponStr,
             loseStyleStr);

}

void SV_Ranked_DuelDisconnectCheck(int clientNum) {
  if (clientNum < 0 || clientNum >= sv_maxclients->integer)
    return;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (r->inDuel) {
    int opp = r->duelOpponent;
    SV_SendServerCommand(NULL,
                         "print \"^1%s ^7disconnected while dueling ^5%s^7!\n\"",
                         svs.clients[clientNum].name, svs.clients[opp].name);
    SV_Ranked_DuelEnd(opp, clientNum, 0, 1, -1);
  }
}

void SV_Ranked_SetAdmin(int targetClient, int adminId) {
  if (targetClient < 0 || targetClient >= sv_maxclients->integer)
    return;

  extern cJSON *accountsDB;
  rankedMatchState_t *rAdmin = &sv_rankedPlayers[targetClient];
  if (rAdmin->loggedIn && accountsDB) {
    char lowerUser[MAX_AUTH_STRING];
    Q_strncpyz(lowerUser, rAdmin->username, sizeof(lowerUser));
    Q_strlwr(lowerUser);
    cJSON *adminAcc = cJSON_GetObjectItemCaseSensitive(accountsDB, lowerUser);
    if (adminAcc) {
      cJSON *isAdminPtr = cJSON_GetObjectItemCaseSensitive(adminAcc, "isAdmin");
      if (!isAdminPtr)
        cJSON_AddTrueToObject(adminAcc, "isAdmin");
      else
        cJSON_ReplaceItemInObject(adminAcc, "isAdmin", cJSON_CreateBool(1));

      cJSON *adminLvlPtr =
          cJSON_GetObjectItemCaseSensitive(adminAcc, "adminLevel");
      if (!adminLvlPtr)
        cJSON_AddNumberToObject(adminAcc, "adminLevel", adminId);
      else
        cJSON_SetNumberValue(adminLvlPtr, adminId);

      SV_Ranked_SaveAccounts();
      Com_Printf("[RANKED] SMOD: %s granted admin level %d\n", rAdmin->username,
                 adminId);
      SV_SendServerCommand(&svs.clients[targetClient],
                           "print \"^5[RANKED] ^7You have been recognized as "
                           "^3Admin Level %d^7.\n\"",
                           adminId);
    }
  }
}

void SV_Ranked_ProcessPrivateDuel(int d1, int d2) {
  if (d1 < 0 || d1 >= sv_maxclients->integer || d2 < 0 ||
      d2 >= sv_maxclients->integer)
    return;

  client_t *c1 = &svs.clients[d1];
  client_t *c2 = &svs.clients[d2];

  extern cJSON *accountsDB;
  int elo1 = 1000, elo2 = 1000;

  if (accountsDB) {
    rankedMatchState_t *r1 = &sv_rankedPlayers[d1];
    rankedMatchState_t *r2 = &sv_rankedPlayers[d2];
    if (r1->loggedIn) {
      cJSON *a1 = SV_Ranked_GetAccount(r1->username);
      if (a1) {
        cJSON *m1 = cJSON_GetObjectItemCaseSensitive(a1, "modes");
        if (m1) {
          cJSON *d1Data = cJSON_GetObjectItemCaseSensitive(m1, "duel");
          if (d1Data) {
            cJSON *e1 = cJSON_GetObjectItemCaseSensitive(d1Data, "elo");
            if (e1)
              elo1 = e1->valueint;
          }
        }
      }
    }
    if (r2->loggedIn) {
      cJSON *a2 = SV_Ranked_GetAccount(r2->username);
      if (a2) {
        cJSON *m2 = cJSON_GetObjectItemCaseSensitive(a2, "modes");
        if (m2) {
          cJSON *d2Data = cJSON_GetObjectItemCaseSensitive(m2, "duel");
          if (d2Data) {
            cJSON *e2 = cJSON_GetObjectItemCaseSensitive(d2Data, "elo");
            if (e2)
              elo2 = e2->valueint;
          }
        }
      }
    }
  }
  // THE BROADCAST ITSELF!
  SV_SendServerCommand(
      NULL, "print \"^5[RANKED] ^3%s ^7(%d ELO) ^1VS ^3%s ^7(%d ELO)!\n\"",
      c1->name, elo1, c2->name, elo2);
  Com_Printf("[RANKED] Private Duel Started: %s (%d ELO) vs %s (%d ELO)\n",
             c1->name, elo1, c2->name, elo2);
}

// ===========================================================================
//  HOT POTATO MODE
//  Players vote to start. Holder earns points/credits every 10 seconds.
//  Killing the holder steals the potato.
// ===========================================================================

qboolean sv_hotPotatoActive = qfalse;
qboolean sv_hotPotatoEnabled = qfalse;
static int sv_hotPotatoHolder = -1;  // clientNum of current holder
static int sv_hotPotatoNextTick = 0; // svs.time when next tick fires
static int sv_hotPotatoDuration = 0; // how many ticks holder has held
static int sv_hotPotatoSessionBestTicks = 0;
static char sv_hotPotatoSessionBestName[MAX_NAME_LENGTH] = "None";

#define HP_TICK_MS 10000
#define HP_POINTS_PER_TICK 10
#define HP_CREDITS_PER_TICK 5
#define HP_KILL_POINTS 2
#define HP_KILL_CREDITS 2

// Returns qtrue if clientNum is valid, connected, not spectating, not in a duel
static qboolean HP_IsValidCandidate(int clientNum) {
  if (clientNum < 0 || clientNum >= sv_maxclients->integer)
    return qfalse;
  client_t *cl = &svs.clients[clientNum];
  if (cl->state < CS_ACTIVE)
    return qfalse;
  // Skip spectators (team 3 in JKA = TEAM_SPECTATOR)
  playerState_t *ps = SV_GameClientNum(clientNum);
  if (!ps)
    return qfalse;
  if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR)
    return qfalse;
  if (sv_rankedPlayers[clientNum].inDuel)
    return qfalse;
  return qtrue;
}

static void HP_UpdateTopPotato(int clientNum) {
  if (clientNum < 0 || sv_hotPotatoDuration <= 0)
    return;

  // Track session best (MVP)
  if (sv_hotPotatoDuration > sv_hotPotatoSessionBestTicks) {
    sv_hotPotatoSessionBestTicks = sv_hotPotatoDuration;
    Q_strncpyz(sv_hotPotatoSessionBestName, svs.clients[clientNum].name,
               sizeof(sv_hotPotatoSessionBestName));
  }

  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn || r->isTemp)
    return;

  extern cJSON *accountsDB;
  if (!accountsDB)
    return;

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (acc) {
    cJSON *maxPotato =
        cJSON_GetObjectItemCaseSensitive(acc, "max_potato_ticks");
    if (maxPotato) {
      if (sv_hotPotatoDuration > maxPotato->valueint) {
        cJSON_SetNumberValue(maxPotato, sv_hotPotatoDuration);
      }
    } else {
      cJSON_AddNumberToObject(acc, "max_potato_ticks", sv_hotPotatoDuration);
    }
  }
}

static void HP_PickRandom(void) {
  int candidates[MAX_CLIENTS];
  int count = 0;
  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (i == sv_hotPotatoHolder)
      continue; // don't re-pick current holder
    if (HP_IsValidCandidate(i)) {
      candidates[count++] = i;
    }
  }

  if (count == 0) {
    // Only announce the pause once — suppress repeated spam every 10s
    if (sv_hotPotatoHolder != -1) {
      SV_SendServerCommand(NULL, "chat \"^1[Hot Potato] ^7No eligible players — mode paused.\"");
      SV_Ranked_Log("POTATO: Mode paused - no eligible players");
    }
    sv_hotPotatoHolder = -1;
    SV_SendServerCommand(NULL, "potato_holder -1");
    return;
  }


  if (sv_hotPotatoHolder >= 0) {
    HP_UpdateTopPotato(sv_hotPotatoHolder);
  }

  int newHolder = candidates[rand() % count];
  int oldHolder = sv_hotPotatoHolder;
  sv_hotPotatoHolder = newHolder;
  sv_hotPotatoDuration = 0;
  sv_hotPotatoNextTick = svs.time + HP_TICK_MS;

  SV_SendServerCommand(NULL, va("potato_holder %d", sv_hotPotatoHolder));

  if (oldHolder >= 0 && svs.clients[oldHolder].state >= CS_CONNECTED) {
    SV_SendServerCommand(
        NULL, "chat \"^1%s ^7dropped the Potato! ^2%s ^7picked it up!\"",
        svs.clients[oldHolder].name, svs.clients[newHolder].name);
    SV_Ranked_Log("POTATO: %s dropped -> %s picked up", svs.clients[oldHolder].name, svs.clients[newHolder].name);
  } else {
    SV_SendServerCommand(NULL,
                         "chat \"^1HOT POTATO EVENT! ^2%s ^7is the ^1HOT "
                         "POTATO^7! Kill them to steal it!\"",
                         svs.clients[newHolder].name);
    SV_Ranked_Log("POTATO: Event started - %s is the holder", svs.clients[newHolder].name);
  }

  // Center print the holder
  SV_SendServerCommand(
      &svs.clients[newHolder],
      "cp \"^1YOU ARE THE HOT POTATO!\n^7Survive to earn points!\"");
}

void SV_Ranked_HotPotato_CheckKill(int victimId, int killerId) {
  if (!sv_hotPotatoActive || sv_hotPotatoHolder < 0)
    return;

  if (victimId == sv_hotPotatoHolder) {
    int oldHolder = sv_hotPotatoHolder;
    HP_UpdateTopPotato(oldHolder);

    if (killerId >= 0 && killerId < sv_maxclients->integer && killerId != victimId && HP_IsValidCandidate(killerId)) {
      sv_hotPotatoHolder = killerId;
      sv_hotPotatoDuration = 0;
      sv_hotPotatoNextTick = svs.time + HP_TICK_MS;

      SV_SendServerCommand(NULL, va("potato_holder %d", sv_hotPotatoHolder));
      SV_SendServerCommand(NULL, va("chat \"^1%s ^7killed the Potato Holder! ^2%s ^7stole the ^1HOT POTATO^7!\"",
                           svs.clients[victimId].name, svs.clients[killerId].name));
      SV_SendServerCommand(&svs.clients[killerId], "cp \"^1YOU STOLE THE HOT POTATO!\n^7Survive to earn points!\"");
      SV_Ranked_Log("POTATO: %s killed holder %s and stole potato", svs.clients[killerId].name, svs.clients[victimId].name);
    } else {
      HP_PickRandom();
    }
  }
}


void SV_Ranked_StartHotPotato(void) {
  sv_hotPotatoEnabled = qtrue;
  if (sv_hotPotatoActive) {
    SV_SendServerCommand(NULL, "chat \"^1[Hot Potato] Already running!\"");
    return;
  }

  // Only valid in duel mode
  const char *currentMode = SV_Ranked_GetActiveMode();
  if (Q_stricmp(currentMode, "duel") != 0) {
    SV_SendServerCommand(
        NULL,
        "chat \"^1[Hot Potato] ^7This mode only works in ^3Duel Mode^7!\"");
    return;
  }

  // Need at least 2 eligible players
  int count = 0;
  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (HP_IsValidCandidate(i))
      count++;
  }
  if (count < 2) {
    SV_SendServerCommand(NULL, "chat \"^1[Hot Potato] ^7Need at least 2 "
                               "active players to start.\"");
    return;
  }

  sv_hotPotatoActive = qtrue;
  sv_hotPotatoHolder = -1;
  sv_hotPotatoDuration = 0;
  sv_hotPotatoSessionBestTicks = 0;
  Q_strncpyz(sv_hotPotatoSessionBestName, "None", sizeof(sv_hotPotatoSessionBestName));
  HP_PickRandom();
}

void SV_Ranked_StopHotPotato(qboolean disableCompletely) {
  if (!sv_hotPotatoActive)
    return;

  if (sv_hotPotatoHolder >= 0) {
    HP_UpdateTopPotato(sv_hotPotatoHolder);
  }

  sv_hotPotatoActive = qfalse;
  sv_hotPotatoDuration = 0;

  if (sv_hotPotatoSessionBestTicks > 0) {
    SV_SendServerCommand(
        NULL, "chat \"^1[Hot Potato] ^7Round MVP: ^2%s ^7held the potato for "
              "^1%d ^7ticks!\"",
        sv_hotPotatoSessionBestName, sv_hotPotatoSessionBestTicks);
  }

  sv_hotPotatoHolder = -1;
  SV_SendServerCommand(NULL, "potato_holder -1");

  // Bulk-save all accounts now so all in-memory credit changes are flushed

  SV_Ranked_SaveAccounts();

  if (disableCompletely) {
    sv_hotPotatoEnabled = qfalse;
    SV_SendServerCommand(NULL,
                         "chat \"^1Hot Potato Event Ended! Credits saved.\"");
  } else {
    SV_SendServerCommand(NULL,
                         "chat \"^1Hot Potato paused for round transition. Credits saved.\"");
  }
}

void SV_Ranked_ProcessRoundStart(void) {
  if (sv_hotPotatoEnabled && !sv_hotPotatoActive) {
    int count = 0;
    for (int i = 0; i < sv_maxclients->integer; i++) {
      if (HP_IsValidCandidate(i))
        count++;
    }
    if (count >= 2) {
      SV_Ranked_StartHotPotato();
    } else {
      Com_Printf("[RANKED] Hot Potato: not enough players to start round (%d/2)\n", count);
    }
  }
}

// Called from SV_Ranked_Logic_Frame on a per-tick basis
static void HP_Tick(void) {
  if (sv_hotPotatoHolder < 0) {
    HP_PickRandom();
    return;
  }

  // Validate that holder is still eligible
  if (!HP_IsValidCandidate(sv_hotPotatoHolder)) {
    HP_PickRandom();
    return;
  }

  sv_hotPotatoDuration++;
  const char *holderName = svs.clients[sv_hotPotatoHolder].name;

  // Milestone broadcasts
  if (sv_hotPotatoDuration == 3) {
    SV_SendServerCommand(NULL,
                         "chat \"^3NOTICE: ^7%s ^7has held the potato for "
                         "^130 seconds^7! Someone kill them!\"",
                         holderName);
  } else if (sv_hotPotatoDuration == 6) {
    SV_SendServerCommand(NULL,
                         "chat \"^1WARNING: ^7%s ^7is DOMINATING with the "
                         "potato! (1 Minute)\"",
                         holderName);
  } else if (sv_hotPotatoDuration == 12) {
    SV_SendServerCommand(NULL,
                         "chat \"^1UNSTOPPABLE: ^7%s ^7has held the potato "
                         "for ^12 MINUTES^7!\"",
                         holderName);
  }

  // Reward the holder (accumulate in-memory; saved on mode end or disconnect)
  rankedMatchState_t *r = &sv_rankedPlayers[sv_hotPotatoHolder];
  if (r->loggedIn && !r->isTemp) {
    UpdateAccountCredits(r->username, HP_CREDITS_PER_TICK);
    // NO SaveAccounts() here — batch-saved on StopHotPotato or disconnect
    SV_SendServerCommand(
        &svs.clients[sv_hotPotatoHolder],
        "cp \"^2+%d Potato Credits\n^7(hold streak: ^5%d^7 ticks)\"",
        HP_CREDITS_PER_TICK, sv_hotPotatoDuration);
  }
}

// ===========================================================================
//  NATIVE TRIVIA SYSTEM
// ===========================================================================
#include "sv_ranked_trivia.h"

static int sv_triviaNextQuestionTime = 0;
int sv_triviaActiveQuestionIndex = -1; // -1 means no active question

void SV_Ranked_Trivia_Frame(void) {
  if (!Cvar_VariableIntegerValue("sv_ranked_enabled"))
    return;

  int now = svs.time;
  if (sv_triviaNextQuestionTime == 0) {
    sv_triviaNextQuestionTime =
        now + 60000; // First question 60s after map start
  }

  if (now >= sv_triviaNextQuestionTime) {
    if (sv_rankedTriviaCount > 0) {
      sv_triviaActiveQuestionIndex = rand() % sv_rankedTriviaCount;
      SV_SendServerCommand(
          NULL, "chat \"^6Trivia Time! ^7Question: %s\"",
          sv_rankedTriviaQuestions[sv_triviaActiveQuestionIndex].question);
      SV_SendServerCommand(NULL, "chat \"^7Answer using ^3#<your answer>\"");
    }
    sv_triviaNextQuestionTime = now + 280000; // Next question in ~4.6 mins
  }
}

void SV_Ranked_Trivia_HandleAnswer(client_t *cl, const char *message) {
  if (sv_triviaActiveQuestionIndex < 0 ||
      sv_triviaActiveQuestionIndex >= sv_rankedTriviaCount)
    return;
  if (!cl || !cl->state)
    return;

  // Check cooldown for player (simple static array)
  static int triviaCooldowns[MAX_CLIENTS] = {0};
  int clientNum = cl - svs.clients;
  int now = svs.time;
  if (triviaCooldowns[clientNum] > now) {
    SV_SendServerCommand(
        cl, va("print \"^1Please wait %d seconds before answering again.\n\"",
               (triviaCooldowns[clientNum] - now) / 1000));
    return;
  }
  triviaCooldowns[clientNum] = now + 10000; // 10 second cooldown

  // Check answer
  const char *guess = message + 1; // skip '#'
  while (*guess == ' ')
    guess++; // skip leading spaces

  // Simple case-insensitive match against all valid answers
  qboolean correct = qfalse;
  for (int i = 0; i < 6; i++) {
    const char *validAns =
        sv_rankedTriviaQuestions[sv_triviaActiveQuestionIndex].answers[i];
    if (!validAns)
      break;
    if (!Q_stricmp(guess, validAns)) {
      correct = qtrue;
      break;
    }
  }

  if (correct) {
    rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
    if (r->loggedIn) {
      cJSON *acc = SV_Ranked_GetAccount(r->username);
      if (acc) {
        cJSON *tw = cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins");
        int current = tw ? tw->valueint : 0;
        if (tw)
          cJSON_SetNumberValue(tw, current + 1);
        else
          cJSON_AddNumberToObject(acc, "trivia_wins", 1);

        cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
        if (xpPtr)
          cJSON_SetNumberValue(xpPtr, xpPtr->valueint + 100);
        else
          cJSON_AddNumberToObject(acc, "xp", 100);

        cJSON *crPtr = cJSON_GetObjectItemCaseSensitive(acc, "credits");
        if (crPtr)
          cJSON_SetNumberValue(crPtr, crPtr->valueint + 50);
        else
          cJSON_AddNumberToObject(acc, "credits", 50);

        SV_Ranked_SaveAccounts();
      }
    }

    SV_SendServerCommand(
        NULL,
        "chat \"^2%s ^7answered the trivia correctly! ^2+100 XP ^5+50 Credits\"",
        cl->name);
    SV_Ranked_Log("TRIVIA: %s answered correctly (Index: %d)", cl->name, sv_triviaActiveQuestionIndex);
    sv_triviaActiveQuestionIndex = -1;        // Question answered
    sv_triviaNextQuestionTime = now + 280000; // Reset timer
  } else {
    SV_SendServerCommand(cl, "chat \"^1Incorrect answer. Try again!\"");
    SV_Ranked_Log("TRIVIA: %s attempted incorrect answer: '%s'", cl->name, guess);
  }
}

// ===========================================================================
//  NATIVE ADVENTURE SYSTEM
// ===========================================================================
#include "sv_ranked_adventure.h"

#define ADV_COOLDOWN_MS (5 * 60 * 1000) // 5 minutes between adventures

static void Adventure_DisplayNode(client_t *cl, int nodeIdx) {
  if (nodeIdx < 0 || nodeIdx >= sv_rankedAdventureNodeCount)
    return;
  rankedAdvNode_t *node = &sv_rankedAdventureNodes[nodeIdx];

  char safeDesc[1024];
  Q_strncpyz(safeDesc, node->description, sizeof(safeDesc));
  for (int i = 0; safeDesc[i]; i++) {
    if (safeDesc[i] == '"') safeDesc[i] = '\'';
  }

  char c1[256] = "", c2[256] = "", c3[256] = "";
  if (node->numChoices > 0 && node->choices[0].text) Q_strncpyz(c1, node->choices[0].text, sizeof(c1));
  if (node->numChoices > 1 && node->choices[1].text) Q_strncpyz(c2, node->choices[1].text, sizeof(c2));
  if (node->numChoices > 2 && node->choices[2].text) Q_strncpyz(c3, node->choices[2].text, sizeof(c3));

  for (int i = 0; c1[i]; i++) if (c1[i] == '"') c1[i] = '\'';
  for (int i = 0; c2[i]; i++) if (c2[i] == '"') c2[i] = '\'';
  for (int i = 0; c3[i]; i++) if (c3[i] == '"') c3[i] = '\'';

  SV_SendServerCommand(cl, va("adv_node \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
    node->id[0] ? node->id : "Quest", safeDesc, c1, c2, c3));

  SV_SendServerCommand(cl, "chat \"^6--- Adventure ---\"");
  SV_SendServerCommand(cl, va("chat \"%s\"", safeDesc));
}

static void Adventure_EndAdventure(client_t *cl, int outcomeNodeIdx) {
  if (!cl || outcomeNodeIdx < 0 || outcomeNodeIdx >= sv_rankedAdventureNodeCount)
    return;
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  rankedAdvNode_t *node = &sv_rankedAdventureNodes[outcomeNodeIdx];

  r->adventureNodeIdx = -1;
  r->adventureCooldownEnd = svs.time + ADV_COOLDOWN_MS;

  int cr = node->outcomeCredits;
  int xp = node->outcomeXp;

  char safeDesc[1024];
  Q_strncpyz(safeDesc, node->description, sizeof(safeDesc));
  for (int i = 0; safeDesc[i]; i++) {
    if (safeDesc[i] == '"') safeDesc[i] = '\'';
  }

  char endText[1024];
  Com_sprintf(endText, sizeof(endText), "%s\n^7Reward: ^2+%d XP^7, ^5+%d Credits", safeDesc, xp, cr);

  SV_SendServerCommand(cl, va("adv_node \"%s\" \"%s\" \"\" \"\" \"\"",
    node->id[0] ? node->id : "Outcome", endText));

  SV_SendServerCommand(cl, "chat \"^6--- Adventure End ---\"");
  SV_SendServerCommand(cl, va("chat \"%s\"", safeDesc));
  SV_SendServerCommand(cl, va("chat \"^7Reward: ^2%d XP^7, ^5%d Credits\"", xp, cr));


  if (r->loggedIn && !r->isTemp && r->username[0]) {
    if (cr != 0)
      UpdateAccountCredits(r->username, cr);
    if (xp != 0)
      UpdateAccountStats(r->username, cl->name, 0, xp, 0, 0, NULL);
  }
  SV_Ranked_Log("ADVENTURE: %s finished '%s' (reward: %d XP, %d CR)",
                cl->name, node->id, xp, cr);
}

void SV_Ranked_Adventure_Start(client_t *cl) {
  if (!cl)
    return;
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (r->adventureNodeIdx >= 0) {
    SV_SendServerCommand(cl, "chat \"^1You are already on an adventure!\"");
    return;
  }
  if (r->adventureCooldownEnd > svs.time) {
    int msLeft = r->adventureCooldownEnd - svs.time;
    int minutesLeft = (msLeft + 59999) / 60000;
    SV_SendServerCommand(cl,
        "chat \"^1You must wait ^7%d ^1more minute(s) for your next adventure.\"",
        minutesLeft);
    return;
  }
  if (sv_rankedAdventureStartCount <= 0) {
    SV_SendServerCommand(cl, "chat \"^1No adventures available.\"");
    return;
  }

  int pick = rand() % sv_rankedAdventureStartCount;
  int startIdx = sv_rankedAdventureStartIndices[pick];
  r->adventureNodeIdx = startIdx;

  SV_Ranked_Log("ADVENTURE: %s started '%s'", cl->name,
                sv_rankedAdventureNodes[startIdx].id);
  Adventure_DisplayNode(cl, startIdx);
}

void SV_Ranked_Adventure_Choose(client_t *cl, int choiceIndex) {
  if (!cl)
    return;
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (r->adventureNodeIdx < 0) {
    SV_SendServerCommand(cl,
        "chat \"^1You are not on an adventure. Type ^2!adventure ^1to start.\"");
    return;
  }

  rankedAdvNode_t *cur = &sv_rankedAdventureNodes[r->adventureNodeIdx];
  if (choiceIndex < 1 || choiceIndex > cur->numChoices) {
    SV_SendServerCommand(cl, "chat \"^1That is not a valid choice.\"");
    return;
  }

  int destIdx = cur->choices[choiceIndex - 1].destIndex;
  if (destIdx < 0 || destIdx >= sv_rankedAdventureNodeCount) {
    SV_SendServerCommand(cl, "chat \"^1Invalid destination node.\"");
    r->adventureNodeIdx = -1;
    return;
  }

  rankedAdvNode_t *next = &sv_rankedAdventureNodes[destIdx];
  if (next->hasOutcome) {
    Adventure_EndAdventure(cl, destIdx);
  } else {
    r->adventureNodeIdx = destIdx;
    Adventure_DisplayNode(cl, destIdx);
  }
}

// Called every SV_Frame — drives the 10-second potato tick and trivia
void SV_Ranked_Logic_Frame(void) {
  SV_Ranked_Trivia_Frame();
  SV_Ranked_Vote_Frame();

  // Passive XP logic (1 XP every 180 seconds = 3 minutes)
  static int lastPassiveXPTime = 0;
  if (lastPassiveXPTime == 0) {
    lastPassiveXPTime = svs.time;
  }
  if (svs.time - lastPassiveXPTime >= 180000) {
    lastPassiveXPTime = svs.time;
    qboolean saved = qfalse;
    for (int i = 0; i < sv_maxclients->integer; i++) {
      client_t *cl = &svs.clients[i];
      if (cl->state == CS_ACTIVE && sv_rankedPlayers[i].loggedIn) {
        UpdateAccountStats(sv_rankedPlayers[i].username, cl->name, 0, 1, 0, 0, NULL);
        saved = qtrue;
      }
    }
    if (saved) {
      SV_Ranked_SaveAccounts();
    }
  }

  // Enforce PM_FREEZE and grantedWeaponsMask for active clients
  if (sv_maxclients) {
    for (int i = 0; i < sv_maxclients->integer; i++) {
      client_t *cl = &svs.clients[i];
      if (cl->state == CS_ACTIVE) {
        playerState_t *ps = SV_GameClientNum(i);
        if (ps && ps->stats[STAT_HEALTH] > 0) {
          if (sv_rankedPlayers[i].isFrozen) {
            ps->pm_type = PM_NORMAL;
            VectorCopy(sv_rankedPlayers[i].frozenOrigin, ps->origin);
            VectorClear(ps->velocity);
            ps->weaponTime = 1000;
            ps->saberMove = 0;
            ps->saberBlocked = 0;
            ps->forceHandExtend = HANDEXTEND_NONE;
            ps->fd.forcePower = 0;
            ps->saberHolstered = 2;
          }
          if (sv_rankedPlayers[i].grantedWeaponsMask != 0) {
            ps->trueJedi = qfalse;
            ps->trueNonJedi = qtrue;
            ps->stats[STAT_WEAPONS] |= sv_rankedPlayers[i].grantedWeaponsMask;
            for (int a = 0; a < MAX_AMMO; a++) {
              if (ps->ammo[a] < 10) {
                ps->ammo[a] = 300;
              }
            }
          }
          if (sv_rankedPlayers[i].godForce) {
            ps->fd.forcePower = 100;
          }
          if (sv_rankedPlayers[i].grantedForcePowersMask != 0) {
            ps->fd.forcePowersKnown |= sv_rankedPlayers[i].grantedForcePowersMask;
            for (int fp = 0; fp < 18; fp++) {
              if (sv_rankedPlayers[i].grantedForcePowersMask & (1 << fp)) {
                if (sv_rankedPlayers[i].grantedForceLevels[fp] > 0) {
                  ps->fd.forcePowerLevel[fp] = sv_rankedPlayers[i].grantedForceLevels[fp];
                  ps->fd.forcePowerBaseLevel[fp] = sv_rankedPlayers[i].grantedForceLevels[fp];
                }
              }
            }
          }
          if (sv_rankedPlayers[i].speedMultiplier > 1.01f || sv_rankedPlayers[i].speedMultiplier < 0.99f) {
            float horizSpeed = sqrtf(ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1]);
            float targetSpeed = 250.0f * sv_rankedPlayers[i].speedMultiplier;
            if (horizSpeed > 30.0f && horizSpeed < targetSpeed) {
              float scale = targetSpeed / horizSpeed;
              if (scale > 1.35f) scale = 1.35f;
              ps->velocity[0] *= scale;
              ps->velocity[1] *= scale;
            }
          }
          if (sv_rankedPlayers[i].livesActive) {
            if (ps->stats[STAT_HEALTH] <= 0) {
              if (!sv_rankedPlayers[i].wasDeadLastFrame) {
                sv_rankedPlayers[i].wasDeadLastFrame = qtrue;
                if (sv_rankedPlayers[i].remainingLives > 0) {
                  sv_rankedPlayers[i].remainingLives--;
                  SV_SendServerCommand(cl, va("chat \"^1You died! ^5%d ^1lives remaining.\"", sv_rankedPlayers[i].remainingLives));
                }
              }
              if (sv_rankedPlayers[i].remainingLives <= 0) {
                ps->pm_type = PM_SPECTATOR;
                SV_SendServerCommand(cl, "chat \"^1OUT OF LIVES! You are now spectating until the next match.\"");
              }
            } else {
              sv_rankedPlayers[i].wasDeadLastFrame = qfalse;
              if (sv_rankedPlayers[i].remainingLives <= 0) {
                ps->pm_type = PM_SPECTATOR;
              }
            }
          }
        } else if (ps && ps->stats[STAT_HEALTH] <= 0) {
          sv_rankedPlayers[i].grantedWeaponsMask = 0;
          if (sv_rankedPlayers[i].livesActive && sv_rankedPlayers[i].remainingLives <= 0) {
            ps->pm_type = PM_SPECTATOR;
          }
        }
      }
    }
  }

  if (!sv_hotPotatoActive)
    return;
  if (svs.time < sv_hotPotatoNextTick)
    return;

  sv_hotPotatoNextTick = svs.time + HP_TICK_MS;
  HP_Tick();
}

// Call this when the potato holder disconnects
void SV_Ranked_HotPotatoDisconnect(int clientNum) {
  if (!sv_hotPotatoActive)
    return;
  if (clientNum != sv_hotPotatoHolder)
    return;
  // Save outstanding credits for this player before re-picking
  HP_UpdateTopPotato(clientNum);
  SV_Ranked_SaveAccounts();
  HP_PickRandom();
}

// Call from SV_Ranked_ProcessKill — passes potato on kill, or rewards holder
// kill
void SV_Ranked_HotPotatoHandleKill(int killerId, int victimId) {
  if (!sv_hotPotatoActive)
    return;

  if (victimId == sv_hotPotatoHolder) {
    // Killer steals the potato (if valid and not self)
    if (killerId == victimId || killerId < 0) {
      HP_PickRandom();
    } else if (HP_IsValidCandidate(killerId)) {
      HP_UpdateTopPotato(victimId);
      int prev = victimId;
      sv_hotPotatoHolder = killerId;
      sv_hotPotatoDuration = 0;
      sv_hotPotatoNextTick = svs.time + HP_TICK_MS;
      SV_SendServerCommand(
          NULL, "chat \"^1%s ^7dropped the Potato! ^2%s ^7picked it up!\"",
          svs.clients[prev].name, svs.clients[killerId].name);
      SV_SendServerCommand(
          &svs.clients[killerId],
          "cp \"^1YOU ARE THE HOT POTATO!\n^7Survive to earn points!\"");
    } else {
      HP_PickRandom();
    }
  } else if (killerId == sv_hotPotatoHolder) {
    // Holder scored a kill — small bonus
    rankedMatchState_t *r = &sv_rankedPlayers[killerId];
    if (r->loggedIn && !r->isTemp) {
      UpdateAccountCredits(r->username, HP_KILL_CREDITS);
      SV_SendServerCommand(&svs.clients[killerId],
                           "cp \"^1HOT POTATO KILL!\n^2+%d Credits\"",
                           HP_KILL_CREDITS);
    }
  }
}

void SV_Ranked_MapChange(void) {
  // Clear any active duels
  for (int i = 0; i < 64; i++) {
    if (sv_rankedPlayers[i].inDuel) {
      int oppId = sv_rankedPlayers[i].duelOpponent;
      if (oppId > i) { // Only log once per duel pair
        const char *p1Name = (svs.clients[i].state >= CS_CONNECTED) ? svs.clients[i].name : "Unknown";
        const char *p2Name = (oppId >= 0 && oppId < sv_maxclients->integer && svs.clients[oppId].state >= CS_CONNECTED) ? svs.clients[oppId].name : "Unknown";
        SV_Ranked_Log("DUEL_END: %s def. %s (+0 / -0) [Tie: 0, Disc: 0, Cancelled: 1, MapChange: 1]", p1Name, p2Name);
        
        char u1[64], u2[64];
        SV_Ranked_GetLogUsername(i, u1, sizeof(u1));
        SV_Ranked_GetLogUsername(oppId, u2, sizeof(u2));
        SV_Ranked_Log("DUEL: %s def %s | %s +0 (1000) | %s -0 (1000) [Cancelled]", u1, u2, u1, u2);
      }
    }
    sv_rankedPlayers[i].inDuel = qfalse;
    sv_rankedPlayers[i].duelOpponent = -1;
    sv_rankedPlayers[i].isFrozen = qfalse;
    sv_rankedPlayers[i].grantedWeaponsMask = 0;
    sv_rankedPlayers[i].burnExpireTime = 0;
    sv_rankedPlayers[i].burnNextDamageTime = 0;
    sv_rankedPlayers[i].speedMultiplier = 1.0f;
  }
  // Reset trivia question timer and active question (preserve or reschedule gracefully)
  sv_triviaActiveQuestionIndex = -1;
  {
    int remainingTime = sv_triviaNextQuestionTime - svs.time;
    if (remainingTime < 60000) {
      sv_triviaNextQuestionTime = svs.time + 120000;
    }
  }
  // Reset/stop Hot Potato
  sv_hotPotatoActive = qfalse;
  sv_hotPotatoEnabled = qfalse;
  sv_hotPotatoHolder = -1;
  sv_hotPotatoDuration = 0;
  sv_hotPotatoSessionBestTicks = 0;
  Q_strncpyz(sv_hotPotatoSessionBestName, "None", sizeof(sv_hotPotatoSessionBestName));

  Com_Printf("[RANKED] Map change cleanup complete. Volatile states reset.\n");
}

/*
==================
SV_Ranked_IsNameInvalidOrOffensive
Checks if a name is invalid (empty, caret-only, or contains offensive words).
==================
*/
qboolean SV_Ranked_IsNameInvalidOrOffensive(const char *name) {
  if (!name || !name[0]) {
    return qtrue;
  }

  char cleanName[MAX_NETNAME];
  Q_strncpyz(cleanName, name, sizeof(cleanName));
  Q_CleanStr(cleanName);

  // Check if the clean name has no printable characters
  int printableCount = 0;
  for (int i = 0; cleanName[i] != '\0'; i++) {
    if ((unsigned char)cleanName[i] > 32) {
      printableCount++;
    }
  }
  if (printableCount == 0) {
    return qtrue;
  }

  // Normalize characters for case-insensitive and leet-speak checks
  char normalized[MAX_NETNAME];
  int nIdx = 0;
  for (int i = 0; cleanName[i] != '\0'; i++) {
    char c = cleanName[i];
    if (c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';
    }
    
    // Leetspeak substitutions
    if (c == '1' || c == '!' || c == '|' || c == 'l') {
      c = 'i';
    } else if (c == '3') {
      c = 'e';
    } else if (c == '4' || c == '@') {
      c = 'a';
    } else if (c == '0') {
      c = 'o';
    } else if (c == '5' || c == '$') {
      c = 's';
    }
    
    // Keep only alphabetical characters
    if (c >= 'a' && c <= 'z') {
      normalized[nIdx++] = c;
    }
  }
  normalized[nIdx] = '\0';

  // Check if contains "nigger" or "niger"
  if (strstr(normalized, "nigger") || strstr(normalized, "niger")) {
    return qtrue;
  }

  return qfalse;
}

// Helper to resolve weapon ID from name
static int SV_Ranked_GetWeaponIdByName(const char *name) {
  const char *n = name;
  if (!Q_stricmpn(name, "wp_", 3)) {
    n = name + 3;
  }
  
  if (!Q_stricmp(n, "stun_baton") || !Q_stricmp(n, "baton")) return WP_STUN_BATON;
  if (!Q_stricmp(n, "melee") || !Q_stricmp(n, "fist")) return WP_MELEE;
  if (!Q_stricmp(n, "saber") || !Q_stricmp(n, "lightsaber")) return WP_SABER;
  if (!Q_stricmp(n, "pistol") || !Q_stricmp(n, "bryar")) return WP_BRYAR_PISTOL;
  if (!Q_stricmp(n, "blaster") || !Q_stricmp(n, "e11")) return WP_BLASTER;
  if (!Q_stricmp(n, "disruptor") || !Q_stricmp(n, "tenloss")) return WP_DISRUPTOR;
  if (!Q_stricmp(n, "bowcaster") || !Q_stricmp(n, "wookiee")) return WP_BOWCASTER;
  if (!Q_stricmp(n, "repeater") || !Q_stricmp(n, "heavy_repeater")) return WP_REPEATER;
  if (!Q_stricmp(n, "demp2") || !Q_stricmp(n, "demp")) return WP_DEMP2;
  if (!Q_stricmp(n, "flechette") || !Q_stricmp(n, "golan")) return WP_FLECHETTE;
  if (!Q_stricmp(n, "rocket") || !Q_stricmp(n, "rocket_launcher") || !Q_stricmp(n, "merr_sonn") || !Q_stricmp(n, "launcher")) return WP_ROCKET_LAUNCHER;
  if (!Q_stricmp(n, "thermal") || !Q_stricmp(n, "grenade")) return WP_THERMAL;
  if (!Q_stricmp(n, "trip_mine") || !Q_stricmp(n, "tripmine") || !Q_stricmp(n, "mine")) return WP_TRIP_MINE;
  if (!Q_stricmp(n, "det_pack") || !Q_stricmp(n, "detpack") || !Q_stricmp(n, "det")) return WP_DET_PACK;
  if (!Q_stricmp(n, "concussion") || !Q_stricmp(n, "concuss")) return WP_CONCUSSION;
  
  return WP_NONE;
}

// Helper to map weapon ID to ammo index and max capacity
static void SV_Ranked_GetWeaponAmmo(int wp, int *ammoIdx, int *maxAmmo) {
  *ammoIdx = AMMO_NONE;
  *maxAmmo = 0;
  
  switch (wp) {
    case WP_BRYAR_PISTOL:
    case WP_BLASTER:
    case WP_BRYAR_OLD:
      *ammoIdx = AMMO_BLASTER;
      *maxAmmo = 300;
      break;
    case WP_DISRUPTOR:
    case WP_BOWCASTER:
    case WP_DEMP2:
      *ammoIdx = AMMO_POWERCELL;
      *maxAmmo = 150;
      break;
    case WP_REPEATER:
    case WP_FLECHETTE:
    case WP_CONCUSSION:
      *ammoIdx = AMMO_METAL_BOLTS;
      *maxAmmo = 400;
      break;
    case WP_ROCKET_LAUNCHER:
      *ammoIdx = AMMO_ROCKETS;
      *maxAmmo = 10;
      break;
    case WP_THERMAL:
      *ammoIdx = AMMO_THERMAL;
      *maxAmmo = 5;
      break;
    case WP_TRIP_MINE:
      *ammoIdx = AMMO_TRIPMINE;
      *maxAmmo = 5;
      break;
    case WP_DET_PACK:
      *ammoIdx = AMMO_DETPACK;
      *maxAmmo = 5;
      break;
    default:
      break;
  }
}

/*
==================
SV_Ranked_ExecuteCheatClientCommand
Temporarily enables sv_cheats for a single Game VM command call and immediately restores sv_cheats to 0.
==================
*/
void SV_Ranked_ExecuteCheatClientCommand(client_t *cl, const char *cmdString) {
  if (!cl || cl->state != CS_ACTIVE) return;

  int clientNum = cl - svs.clients;
  cvar_t *sv_cheats = Cvar_Get("sv_cheats", "0", CVAR_SYSTEMINFO);
  int oldCheats = sv_cheats ? sv_cheats->integer : 0;

  // Temporarily set sv_cheats to 1 for this Game VM call only
  Cvar_Set2("sv_cheats", "1", 0, qtrue);
  Cvar_Set2("g_cheats", "1", 0, qtrue);

  // Tokenize the command string for GVM_ClientCommand
  Cmd_TokenizeString(cmdString);

  // Pass command directly to MovieBattles II Game DLL
  GVM_ClientCommand(clientNum);

  // Instantly restore sv_cheats to original value (0) so normal players CANNOT use god/noclip
  Cvar_Set2("sv_cheats", oldCheats ? "1" : "0", 0, qtrue);
  Cvar_Set2("g_cheats", oldCheats ? "1" : "0", 0, qtrue);
}

/*
==================
SV_Ranked_GiveWeapon
Gives the specified weapon and maximum ammo to the playerState_t.
==================
*/
qboolean SV_Ranked_GiveWeapon(client_t *cl, const char *weaponName, qboolean showMsg) {
  if (!cl || cl->state != CS_ACTIVE || !cl->gentity) {
    if (showMsg && cl && cl->state == CS_ACTIVE) {
      SV_SendServerCommand(cl, "chat \"^1Error: You must be alive to receive a weapon.\"");
    }
    return qfalse;
  }
  
  playerState_t *ps = SV_GameClientNum(cl - svs.clients);
  if (!ps || ps->pm_type == PM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0) {
    if (showMsg) {
      SV_SendServerCommand(cl, "chat \"^1Error: You must be alive to receive a weapon.\"");
    }
    return qfalse;
  }
  
  int wp = SV_Ranked_GetWeaponIdByName(weaponName);
  if (wp == WP_NONE) {
    if (showMsg) {
      SV_SendServerCommand(cl, "chat \"^1Error: Unknown weapon.\"");
    }
    return qfalse;
  }
  
  int ammoIdx = AMMO_NONE;
  int maxAmmo = 0;
  SV_Ranked_GetWeaponAmmo(wp, &ammoIdx, &maxAmmo);
  
  int clientNum = cl - svs.clients;
  // Store weapon bit in grantedWeaponsMask so engine frame loop continuously maintains it against MB2 resets
  sv_rankedPlayers[clientNum].grantedWeaponsMask |= (1 << wp);
  sv_rankedPlayers[clientNum].lastGrantedWeapon = wp;

  Cvar_Set2("g_duelWeaponDisable", "0", 0, qtrue);
  Cvar_Set2("g_weaponDisable", "0", 0, qtrue);

  ps->trueJedi = qfalse;
  ps->trueNonJedi = qtrue;

  // Strip saber so player holds the gun instead of saber
  ps->stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

  // Grant weapon bit in the playerState bitmask
  ps->stats[STAT_WEAPONS] |= (1 << wp);
  
  // Fill the ammo pool for this weapon and general ammo pool
  if (ammoIdx != AMMO_NONE) {
    ps->ammo[ammoIdx] = maxAmmo;
  }
  for (int a = 0; a < MAX_AMMO; a++) {
    if (ps->ammo[a] < 50) {
      ps->ammo[a] = 300;
    }
  }
  
  // Switch client focus to this weapon
  ps->weapon = wp;
  ps->weaponstate = WEAPON_RAISING;

  // Map weapon ID to Quake3/MB2 pickup item classname
  static const char *wpEntityNames[] = {
      "weapon_none", "weapon_stun_baton", "weapon_melee", "weapon_saber",
      "weapon_bryar_pistol", "weapon_blaster", "weapon_disruptor",
      "weapon_bowcaster", "weapon_repeater", "weapon_demp2", "weapon_flechette",
      "weapon_rocket_launcher", "weapon_thermal", "weapon_trip_mine",
      "weapon_det_pack", "weapon_concussion"
  };
  if (wp > 0 && wp < 16) {
    SV_Ranked_ExecuteCheatClientCommand(cl, va("give %s", wpEntityNames[wp]));
    SV_Ranked_ExecuteCheatClientCommand(cl, "give ammo");
  }

  // Force client weapon selection command
  SV_SendServerCommand(cl, va("weapon %d", wp));

  if (showMsg) {
    SV_SendServerCommand(cl, va("chat \"^2Equipped ^5%s ^2(Saber Stripped)!\"", weaponName));
  }
  
  return qtrue;
}

/*
==================
SV_Ranked_GetForcePowerIdByName
Maps string force power names to FP_ enum indices.
==================
*/
int SV_Ranked_GetForcePowerIdByName(const char *name) {
  if (!name || !name[0]) return -1;
  const char *n = name;
  if (!Q_stricmpn(n, "fp_", 3)) n += 3;
  else if (!Q_stricmpn(n, "force_", 6)) n += 6;

  if (!Q_stricmp(n, "heal")) return 0; // FP_HEAL
  if (!Q_stricmp(n, "jump") || !Q_stricmp(n, "levitation")) return 1; // FP_LEVITATION
  if (!Q_stricmp(n, "speed")) return 2; // FP_SPEED
  if (!Q_stricmp(n, "push")) return 3; // FP_PUSH
  if (!Q_stricmp(n, "pull")) return 4; // FP_PULL
  if (!Q_stricmp(n, "mindtrick") || !Q_stricmp(n, "telepathy") || !Q_stricmp(n, "trick")) return 5; // FP_TELEPATHY
  if (!Q_stricmp(n, "grip")) return 6; // FP_GRIP
  if (!Q_stricmp(n, "lightning")) return 7; // FP_LIGHTNING
  if (!Q_stricmp(n, "rage")) return 8; // FP_RAGE
  if (!Q_stricmp(n, "protect")) return 9; // FP_PROTECT
  if (!Q_stricmp(n, "absorb")) return 10; // FP_ABSORB
  if (!Q_stricmp(n, "teamheal")) return 11; // FP_TEAM_HEAL
  if (!Q_stricmp(n, "teamforce")) return 12; // FP_TEAM_FORCE
  if (!Q_stricmp(n, "drain")) return 13; // FP_DRAIN
  if (!Q_stricmp(n, "see") || !Q_stricmp(n, "seeing")) return 14; // FP_SEE
  if (!Q_stricmp(n, "saber")) return 15; // FP_SABER_OFFENSE
  if (!Q_stricmp(n, "defense")) return 16; // FP_SABER_DEFENSE
  if (!Q_stricmp(n, "saberthrow") || !Q_stricmp(n, "throw")) return 17; // FP_SABERTHROW

  return -1;
}

/*
==================
SV_Ranked_GiveForcePower
Grants a force power to client and maintains it across frames.
==================
*/
qboolean SV_Ranked_GiveForcePower(client_t *cl, const char *powerName, int level, qboolean showMsg) {
  if (!cl || cl->state != CS_ACTIVE || !cl->gentity) {
    return qfalse;
  }
  playerState_t *ps = SV_GameClientNum(cl - svs.clients);
  if (!ps || ps->pm_type == PM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0) {
    return qfalse;
  }

  if (level < 1) level = 1;
  if (level > 3) level = 3;

  int clientNum = cl - svs.clients;

  Cvar_Set2("g_forcePowerDisable", "0", 0, qtrue);
  Cvar_Set2("g_duelForcePowerDisable", "0", 0, qtrue);

  // Execute native Game VM give force command while temporarily bypassing sv_cheats for this tick
  SV_Ranked_ExecuteCheatClientCommand(cl, "give force");

  // Set force side to Neutral (0) so MB2 does not block Dark/Light powers
  ps->fd.forceSide = 0;
  ps->fd.forceRank = 7;
  ps->fd.forcePowerMax = 100;
  ps->fd.forcePower = 100;

  if (!Q_stricmp(powerName, "all")) {
    for (int fp = 0; fp < 18; fp++) {
      sv_rankedPlayers[clientNum].grantedForcePowersMask |= (1 << fp);
      sv_rankedPlayers[clientNum].grantedForceLevels[fp] = level;
      ps->fd.forcePowersKnown |= (1 << fp);
      ps->fd.forcePowerLevel[fp] = level;
      ps->fd.forcePowerBaseLevel[fp] = level;
    }
    if (showMsg) {
      SV_SendServerCommand(cl, va("chat \"^2Granted ALL Force Powers (Level %d)!\"", level));
    }
    return qtrue;
  }

  int fpId = SV_Ranked_GetForcePowerIdByName(powerName);
  if (fpId < 0 || fpId >= 18) {
    if (showMsg) {
      SV_SendServerCommand(cl, va("chat \"^1Unknown force power '^5%s^1'. Valid: lightning, grip, drain, heal, rage, protect, absorb, push, pull, mindtrick, speed, seeing, all\"", powerName));
    }
    return qfalse;
  }

  sv_rankedPlayers[clientNum].grantedForcePowersMask |= (1 << fpId);
  sv_rankedPlayers[clientNum].grantedForceLevels[fpId] = level;
  ps->fd.forcePowersKnown |= (1 << fpId);
  ps->fd.forcePowerLevel[fpId] = level;
  ps->fd.forcePowerBaseLevel[fpId] = level;
  ps->fd.forcePowerSelected = fpId;

  if (showMsg) {
    SV_SendServerCommand(cl, va("chat \"^2Granted Force Power ^5%s ^2(Level %d)! Use on force wheel or keybind!\"", powerName, level));
  }
  return qtrue;
}

/*
==================
SV_Ranked_SyncClientRPG
Calculates current client XP in level, XP needed for next level, and Level, then sends `rpg_sync` command to client.
==================
*/
void SV_Ranked_SyncClientRPG(client_t *cl) {
  if (!cl || cl->state < CS_CONNECTED)
    return;

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn || r->isTemp || !r->username[0]) {
    SV_SendServerCommand(cl, "rpg_sync 0 1000 1 1000 \"Padawan\" \"Guest\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc) {
    SV_SendServerCommand(cl, "rpg_sync 0 1000 1 1000 \"Padawan\" \"Guest\"");
    return;
  }

  cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
  int totalXp = (xpPtr && cJSON_IsNumber(xpPtr)) ? xpPtr->valueint : 0;
  if (totalXp < 0) totalXp = 0;

  // Standard database level calculation (xp_per_level, default 1000 XP per level)
  cJSON *xpl = SV_Ranked_GetSetting("xp_per_level");
  int perLevel = (xpl && xpl->valueint > 0) ? xpl->valueint : 1000;
  int level = SV_Ranked_CalculateLevel(totalXp);
  int currentXPInLevel = (perLevel > 0) ? (totalXp % perLevel) : 0;
  int xpNeededForNext = perLevel;

  cJSON *modes = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  cJSON *duel = modes ? cJSON_GetObjectItemCaseSensitive(modes, "duel") : NULL;
  cJSON *eloPtr = duel ? cJSON_GetObjectItemCaseSensitive(duel, "elo") : NULL;
  int fr = (eloPtr && cJSON_IsNumber(eloPtr)) ? eloPtr->valueint : 1000;
  const char *rankTitle = SV_Ranked_GetTitle(fr, acc);
  if (!rankTitle) rankTitle = "Padawan";

  cJSON *dispPtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  const char *dispName = (dispPtr && dispPtr->valuestring && dispPtr->valuestring[0]) ? dispPtr->valuestring : cl->name;

  SV_SendServerCommand(cl, va("rpg_sync %d %d %d %d \"%s\" \"%s\"", currentXPInLevel, xpNeededForNext, level, fr, rankTitle, dispName));

  cJSON *passPtr = cJSON_GetObjectItemCaseSensitive(acc, "password");
  const char *password = (passPtr && passPtr->valuestring) ? passPtr->valuestring : "hidden";
  SV_SendServerCommand(cl, va("rpg_creds \"%s\" \"%s\"", r->username, password));

  // Silent sync of full account stats (Kills, Deaths, Wins, Losses) to HUD without opening full modal
  cJSON *credPtr = cJSON_GetObjectItemCaseSensitive(acc, "credits");
  int credits = (credPtr && cJSON_IsNumber(credPtr)) ? credPtr->valueint : 0;
  int wins = 0, losses = 0, kills = 0, deaths = 0, streak = 0;
  if (duel) {
    cJSON *w = cJSON_GetObjectItemCaseSensitive(duel, "wins"); if (w) wins = w->valueint;
    cJSON *l = cJSON_GetObjectItemCaseSensitive(duel, "losses"); if (l) losses = l->valueint;
    cJSON *k = cJSON_GetObjectItemCaseSensitive(duel, "kills"); if (k) kills = k->valueint;
    cJSON *d = cJSON_GetObjectItemCaseSensitive(duel, "deaths"); if (d) deaths = d->valueint;
    cJSON *s = cJSON_GetObjectItemCaseSensitive(duel, "highest_streak"); if (s) streak = s->valueint;
  }
  SV_SendServerCommand(cl, va("stats_sync %d %d %d %d %d %d %d %d %d %d %d %d \"%s\" \"%s\" \"%s\"",
       totalXp, level, credits, fr,
       wins, losses, kills, deaths,
       0, streak, 0, 0,
       rankTitle, dispName, "None"));
}

void SV_Ranked_SyncClientRPGByName(const char *username) {
  if (!username || !username[0])
    return;
  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (svs.clients[i].state >= CS_CONNECTED && sv_rankedPlayers[i].loggedIn) {
      if (!Q_stricmp(sv_rankedPlayers[i].username, username)) {
        SV_Ranked_SyncClientRPG(&svs.clients[i]);
      }
    }
  }
}
