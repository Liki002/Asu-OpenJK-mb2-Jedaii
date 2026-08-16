#include "cJSON.h"
#include "server.h"
#include "sv_ranked_db.h"
#include "sv_ranked_logic.h"
#include <math.h>

typedef struct {
    const char *shortcode;
    const char *emoji;
} emojiMap_t;

static const emojiMap_t g_emojiMap[] = {
    { ":fire:",      "\x80" },
    { "🔥",           "\x80" },
    { ":potato:",    "\x81" },
    { "🥔",           "\x81" },
    { ":swords:",    "\x82" },
    { ":duel:",      "\x82" },
    { "⚔",           "\x82" },
    { "⚔️",          "\x82" },
    { ":crown:",     "\x83" },
    { ":king:",      "\x83" },
    { "👑",           "\x83" },
    { ":trophy:",    "\x84" },
    { ":winner:",    "\x84" },
    { "🏆",           "\x84" },
    { ":skull:",     "\x85" },
    { ":rip:",       "\x85" },
    { "💀",           "\x85" },
    { ":100:",       "\x86" },
    { "💯",           "\x86" },
    { ":heart:",     "\x87" },
    { ":<3:",        "\x87" },
    { "❤️",          "\x87" },
    { "❤",           "\x87" },
    { ":star:",      "\x88" },
    { "⭐",           "\x88" },
    { ":zap:",       "\x89" },
    { "⚡",           "\x89" },
    { ":flex:",      "\x8a" },
    { "💪",           "\x8a" },
    { ":gg:",        "\x8b" },
    { "🎮",           "\x8b" },
    { ":thumbsup:",  "\x8c" },
    { ":+1:",        "\x8c" },
    { "👍",           "\x8c" },
    { ":target:",    "\x8d" },
    { "🎯",           "\x8d" },
    { ":rocket:",    "\x8e" },
    { "🚀",           "\x8e" },
    { ":poop:",      "\x8f" },
    { "💩",           "\x8f" },
    { NULL,          NULL }
};





void SV_Ranked_TranslateEmojis(const char *inStr, char *outStr, int outSize) {
    if (!inStr || !outStr || outSize <= 0) return;

    char temp[MAX_STRING_CHARS];
    Q_strncpyz(temp, inStr, sizeof(temp));

    for (int i = 0; g_emojiMap[i].shortcode != NULL; i++) {
        const char *sc = g_emojiMap[i].shortcode;
        const char *em = g_emojiMap[i].emoji;
        int scLen = (int)strlen(sc);

        char *match;
        while ((match = strstr(temp, sc)) != NULL) {
            char buf[MAX_STRING_CHARS];
            int prefixLen = (int)(match - temp);
            Q_strncpyz(buf, temp, prefixLen + 1);
            Q_strcat(buf, sizeof(buf), em);
            Q_strcat(buf, sizeof(buf), match + scLen);
            Q_strncpyz(temp, buf, sizeof(temp));
        }
    }
    Q_strncpyz(outStr, temp, outSize);
}

// --- ADMIN HANDLER ---

qboolean SV_Ranked_IsAdmin(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn)
    return qfalse;

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *adminPtr =
      acc ? cJSON_GetObjectItemCaseSensitive(acc, "isAdmin") : NULL;
  return (adminPtr && cJSON_IsTrue(adminPtr)) ? qtrue : qfalse;
}

qboolean SV_Ranked_IsHighAdmin(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn)
    return qfalse;

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *adminPtr =
      acc ? cJSON_GetObjectItemCaseSensitive(acc, "isAdmin") : NULL;
  if (!adminPtr || !cJSON_IsTrue(adminPtr))
    return qfalse;

  cJSON *lvlPtr = cJSON_GetObjectItemCaseSensitive(acc, "adminLevel");
  return (lvlPtr && (lvlPtr->valueint == 1 || lvlPtr->valueint == 2)) ? qtrue : qfalse;
}

// Parse `!roll` — tiered credit gambling with Lucky Charm support
static void SV_Ranked_Cmd_Roll(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in to roll.\"");
    return;
  }

  cJSON *cdPtr = SV_Ranked_GetSetting("roll_cooldown_ms");
  int cooldown = cdPtr ? cdPtr->valueint : 60000;

  int now = svs.time;
  if (r->lastRollTime && (now - r->lastRollTime) < cooldown) {
    SV_SendServerCommand(cl, "chat \"^1You can only roll once every %d seconds.\"", cooldown / 1000);
    return;
  }

  int roll = rand() % 100 + 1;

  // Lucky Charm bonus
  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (acc && r->activeLuckyCharm) {
    cJSON *lcPtr = SV_Ranked_GetSetting("lucky_charm_bonus");
    int lcBonus = lcPtr ? lcPtr->valueint : 10;
    roll += lcBonus;
    if (roll > 100) roll = 100;
  }

  // Tiered outcomes:
  //  1-15 : lose 25 credits
  // 16-35 : lose 10 credits
  // 36-55 : win 10 credits
  // 56-80 : win 25 credits
  // 81-95 : win 50 credits
  // 96-100: win 100 credits (jackpot)
  int creditsChange;
  const char *resultMsg;
  if (roll <= 15) {
    creditsChange = -25;
    resultMsg = "^1Terrible luck!";
  } else if (roll <= 35) {
    creditsChange = -10;
    resultMsg = "^3Bad roll.";
  } else if (roll <= 55) {
    creditsChange = 10;
    resultMsg = "^7Not bad.";
  } else if (roll <= 80) {
    creditsChange = 25;
    resultMsg = "^2Good roll!";
  } else if (roll <= 95) {
    creditsChange = 50;
    resultMsg = "^2Great roll!";
  } else {
    creditsChange = 100;
    resultMsg = "^5JACKPOT!";
  }

  cJSON *credPtr = acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int creds = credPtr ? credPtr->valueint : 0;

  // Clamp loss at current credits (can't go below 0)
  if (creds + creditsChange < 0)
    creditsChange = -creds;

  UpdateAccountCredits(r->username, creditsChange);
  SV_Ranked_SaveAccounts();

  if (creditsChange >= 0) {
    SV_SendServerCommand(cl, "chat \"^7You rolled ^5%d^7. %s ^7+%d credits!\"",
        roll, resultMsg, creditsChange);
  } else {
    SV_SendServerCommand(cl, "chat \"^7You rolled ^5%d^7. %s ^1%d credits.\"",
        roll, resultMsg, creditsChange);
  }

  r->lastRollTime = now;
  SV_Ranked_Log("ROLL: %s rolled %d | Change: %+d | Result: %s", r->username, roll, creditsChange, resultMsg);
}

// Parse `!details` — show player's own account info
static void SV_Ranked_Cmd_Details(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in to use !details.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc) {
    SV_SendServerCommand(cl, "chat \"^1Could not load your account.\"");
    return;
  }

  const char *passStr = "hidden";
  cJSON *passPtr = cJSON_GetObjectItemCaseSensitive(acc, "password");
  if (passPtr && cJSON_IsString(passPtr)) passStr = passPtr->valuestring;

  cJSON *dispPtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  const char *dispName = (dispPtr && dispPtr->valuestring) ? dispPtr->valuestring : cl->name;

  const char *sexStr = "Unknown";
  const char *sexInfo = Info_ValueForKey(cl->userinfo, "sex");
  if (sexInfo && sexInfo[0]) sexStr = sexInfo;

  SV_SendServerCommand(cl, "print \"\n^2--- ^7%s^7's Account Details ^2---\n\"", dispName);
  SV_SendServerCommand(cl, "print \"^2Username: ^7%s\n\"", r->username);
  SV_SendServerCommand(cl, "print \"^2Password: ^7%s\n\"", passStr);
  SV_SendServerCommand(cl, "print \"^3Gender: ^7%s\n\"", sexStr);
  SV_SendServerCommand(cl, "print \"\n^7Use ^6!changepassword ^7to update.\n\n\"");
}

// Parse `!givecredits target amount`
static void SV_Ranked_Cmd_GiveCredits(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(
        cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }
  
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !givecredits <name> <amount>\"");
    return;
  }
  arg++;

  int len = strlen(arg);
  int i = len - 1;
  while (i > 0 && arg[i] == ' ')
    i--;
  while (i > 0 && ((arg[i] >= '0' && arg[i] <= '9') || arg[i] == '-'))
    i--;

  if (i <= 0) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !givecredits <name> <amount>\"");
    return;
  }

  int amount = atoi(&arg[i + 1]);
  char targetName[64];
  Q_strncpyz(targetName, arg, i + 2);
  targetName[i + 1] = '\0';
  for (int k = strlen(targetName) - 1; k >= 0 && targetName[k] == ' '; k--) {
    targetName[k] = '\0';
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient == -1) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  } else if (targetClient == -2) {
    SV_SendServerCommand(
        cl, "chat \"^3Multiple players match that name. Use Client ID.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];
  if (!t->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^1Target player is not logged in.\"");
    return;
  }

  UpdateAccountCredits(t->username, amount);
  SV_Ranked_SaveAccounts();

  SV_SendServerCommand(cl, "chat \"^2You gave ^5%d ^2credits to ^7%s^2.\"",
                       amount, svs.clients[targetClient].name);
  SV_SendServerCommand(
      svs.clients + targetClient,
      "chat \"^2You received ^5%d ^2credits from an admin!\"", amount);
  SV_Ranked_Log("ADMIN: %s gave %d credits to %s", r->username, amount, t->username);
}

// Parse `!setelo target elo`
static void SV_Ranked_Cmd_SetElo(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(
        cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !setelo <name> <amount>\"");
    return;
  }
  arg++;

  int len = strlen(arg);
  int i = len - 1;
  while (i > 0 && arg[i] == ' ')
    i--;
  while (i > 0 && ((arg[i] >= '0' && arg[i] <= '9') || arg[i] == '-'))
    i--;

  if (i <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !setelo <name> <amount>\"");
    return;
  }

  int amount = atoi(&arg[i + 1]);
  char targetName[64];
  Q_strncpyz(targetName, arg, i + 2);
  targetName[i + 1] = '\0';
  for (int k = strlen(targetName) - 1; k >= 0 && targetName[k] == ' '; k--) {
    targetName[k] = '\0';
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];
  if (!t->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^1Target player is not logged in.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(t->username);
  if (acc) {
    // ELO is stored in modes.<activeMode>.elo — not at the top level
    const char *activeMode = SV_Ranked_GetActiveMode();
    cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
    cJSON *modeData = modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, activeMode) : NULL;
    if (modeData) {
      cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
      if (eloPtr) {
        cJSON_SetNumberValue(eloPtr, amount);
      } else {
        cJSON_AddNumberToObject(modeData, "elo", amount);
      }
      SV_Ranked_SaveAccounts();
    } else {
      SV_SendServerCommand(cl, "chat \"^1Could not find mode data for player.\"");
      return;
    }
  }

  SV_SendServerCommand(cl, "chat \"^2Set ELO of ^7%s ^2to ^5%d^2.\"",
                       svs.clients[targetClient].name, amount);
  SV_Ranked_Log("ADMIN: %s set ELO of %s to %d", r->username, t->username, amount);
}

// Parse `!setrank <ID/Name> <Rank Title>`
// The first whitespace-delimited token is the player ID or partial name.
// Everything after that is the rank title (supports multi-word titles like "Jedi Master").
static void SV_Ranked_Cmd_SetRank(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(
        cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  // Skip "!setrank "
  const char *arg = strchr(chatText, ' ');
  if (!arg || !*(arg + 1)) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !setrank <ID/Name> <Rank Title>\"");
    return;
  }
  arg++; // skip the space

  // First token = player ID or partial name (stops at first space)
  char targetName[64];
  int tLen = 0;
  while (arg[tLen] && arg[tLen] != ' ' && tLen < 63) {
    targetName[tLen] = arg[tLen];
    tLen++;
  }
  targetName[tLen] = '\0';

  if (!targetName[0]) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !setrank <ID/Name> <Rank Title>\"");
    return;
  }

  // Everything after the first token = rank title
  const char *rankArg = arg + tLen;
  while (*rankArg == ' ') rankArg++; // skip leading spaces

  char rankName[64];
  Q_strncpyz(rankName, rankArg, sizeof(rankName));
  // Trim trailing whitespace
  for (int k = (int)strlen(rankName) - 1; k >= 0 && rankName[k] == ' '; k--)
    rankName[k] = '\0';

  if (!rankName[0]) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !setrank <ID/Name> <Rank Title>\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient == -2) {
    SV_SendServerCommand(
        cl, "chat \"^3Multiple players match. Use client ID instead.\"");
    return;
  }
  if (targetClient < 0) {
    SV_SendServerCommand(
        cl, "chat \"^1Player not found. Use client ID if name has spaces.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];
  if (!t->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^1Target player is not logged in.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(t->username);
  if (acc) {
    cJSON_DeleteItemFromObjectCaseSensitive(acc, "custom_rank_override");
    if (Q_stricmp(rankName, "default") != 0) {
      cJSON_AddStringToObject(acc, "custom_rank_override", rankName);
    }
    SV_Ranked_SaveAccounts();
  }

  SV_SendServerCommand(cl,
                       "chat \"^2Set custom rank for ^7%s ^2to ^5%s^2.\"",
                       svs.clients[targetClient].name, rankName);
  SV_Ranked_Log("ADMIN: %s set rank of %s to '%s'", r->username, t->username, rankName);
}

// Parse `!jail target minutes`
static void SV_Ranked_Cmd_Jail(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(
        cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  char targetName[64];
  int minutes = 0;
  if (sscanf(chatText, "%*s %63s %d", targetName, &minutes) != 2 ||
      minutes <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !jail <player> <minutes>\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];

  if (SV_Ranked_IsAdmin(&svs.clients[targetClient])) {
    SV_SendServerCommand(cl, "chat \"^1You cannot jail another admin.\"");
    return;
  }

  sv_rankedPlayers[targetClient].jailExpireTime = svs.time + (minutes * 60000);
  SV_SendServerCommand(NULL,
                       "chat \"^1%s^7 has been put on probation for ^1%d "
                       "minutes^7 by an admin.\n\"",
                       svs.clients[targetClient].name, minutes);
  SV_SendServerCommand(svs.clients + targetClient,
                       "chat \"^1You are on probation. Any freekills will "
                       "result in a kick.\n\"");
  SV_Ranked_Log("ADMIN: %s jailed %s for %d minutes", r->username, t->username, minutes);
}

// Parse `!unjail target`
static void SV_Ranked_Cmd_Unjail(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(
        cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !unjail <player>\"");
    return;
  }
  arg++;

  int targetClient = SV_Ranked_FindPlayerByNameOrId(arg);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];

  sv_rankedPlayers[targetClient].jailExpireTime = 0;
  SV_SendServerCommand(
      NULL, "chat \"^2%s^7 has been pardoned and removed from probation.\"",
      svs.clients[targetClient].name);
  SV_Ranked_Log("ADMIN: %s unjailed %s", r->username, t->username);
}

static const char *SV_Ranked_ResolveGunItemKey(const char *name) {
  if (!name || !name[0]) return NULL;
  const char *n = name;
  if (!Q_stricmpn(name, "wp_", 3)) {
    n = name + 3;
  }

  if (!Q_stricmp(n, "pistol") || !Q_stricmp(n, "bryar")) return "wp_pistol";
  if (!Q_stricmp(n, "blaster") || !Q_stricmp(n, "e11")) return "wp_blaster";
  if (!Q_stricmp(n, "disruptor") || !Q_stricmp(n, "tenloss")) return "wp_disruptor";
  if (!Q_stricmp(n, "bowcaster") || !Q_stricmp(n, "wookiee")) return "wp_bowcaster";
  if (!Q_stricmp(n, "repeater") || !Q_stricmp(n, "heavy_repeater")) return "wp_repeater";
  if (!Q_stricmp(n, "demp2") || !Q_stricmp(n, "demp")) return "wp_demp2";
  if (!Q_stricmp(n, "flechette") || !Q_stricmp(n, "golan")) return "wp_flechette";
  if (!Q_stricmp(n, "rocket") || !Q_stricmp(n, "rocket_launcher") || !Q_stricmp(n, "merr_sonn") || !Q_stricmp(n, "launcher")) return "wp_rocket";
  if (!Q_stricmp(n, "concussion") || !Q_stricmp(n, "concuss")) return "wp_concussion";

  return NULL;
}

/*
==================
SV_Ranked_Cmd_AdminGiveGun
Grants a weapon item to the target player's Ranked inventory.
==================
*/
void SV_Ranked_Cmd_AdminGiveGun(client_t *cl, const char *target, const char *gunName) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command.\"");
    return;
  }

  const char *itemKey = SV_Ranked_ResolveGunItemKey(gunName);
  if (!itemKey) {
    SV_SendServerCommand(cl, va("chat \"^1Unknown gun '^5%s^1'. Valid weapons: pistol, blaster, disruptor, bowcaster, repeater, demp2, flechette, rocket, concussion\"", gunName));
    return;
  }

  // Add to DB inventory
  SV_Ranked_Cmd_AdminGiveItem(cl, target, itemKey, 1);

  // If online, immediately equip and enforce via grantedWeaponsMask
  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient >= 0) {
    client_t *targetCl = &svs.clients[targetClient];
    if (targetCl->state == CS_ACTIVE) {
      SV_Ranked_GiveWeapon(targetCl, gunName, qtrue);
    }
  }

  Com_Printf("[RANKED ADMIN] %s gave gun '%s' (%s) to '%s'\n", cl->name, gunName, itemKey, target);
}

/*
==================
SV_Ranked_Cmd_AdminGiveAll
Grants ALL weapons and max ammo to target player.
==================
*/
void SV_Ranked_Cmd_AdminGiveAll(client_t *cl, const char *targetName) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1Admin only command.\"");
    return;
  }

  const char *target = targetName;
  if (!target || !target[0]) {
    target = cl->name;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient >= 0) {
    client_t *targetCl = &svs.clients[targetClient];
    if (targetCl->state == CS_ACTIVE) {
      extern void SV_Ranked_ExecuteCheatClientCommand(client_t *cl, const char *cmdString);
      SV_Ranked_ExecuteCheatClientCommand(targetCl, "give all");
      SV_Ranked_ExecuteCheatClientCommand(targetCl, "give force");

      playerState_t *ps = SV_GameClientNum(targetClient);
      if (ps) {
        ps->trueJedi = qfalse;
        ps->trueNonJedi = qtrue;
        ps->stats[STAT_WEAPONS] &= ~(1 << WP_SABER); // Strip saber
      }
      SV_SendServerCommand(targetCl, "chat \"^2Granted ALL weapons and max ammo!\"");
    }
  }

  SV_SendServerCommand(cl, va("chat \"^2Successfully gave ALL weapons to ^7%s^2!\"", target));
  Com_Printf("[RANKED ADMIN] %s gave ALL weapons to %s\n", cl->name, target);
}

/*
==================
SV_Ranked_Cmd_Yeet
Yeets the target player (propels them vertically and horizontally across the room).
==================
*/
static void SV_Ranked_Cmd_Yeet(client_t *cl, const char *chatText) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  const char *arg = strchr(chatText, ' ');
  if (!arg || *(arg + 1) == '\0') {
    SV_SendServerCommand(cl, "chat \"^1Usage: !yeet <player>\"");
    return;
  }
  arg++;

  while (*arg == ' ' || *arg == '\t') arg++;

  int targetClient = SV_Ranked_FindPlayerByNameOrId(arg);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *ps = SV_GameClientNum(targetClient);

  if (!ps || ps->pm_type == PM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Player is not alive or is in spectator mode.\"");
    return;
  }

  // Set high upward and horizontal velocities (yeet)
  ps->velocity[2] += 1200 + (rand() % 600);
  ps->velocity[0] += (rand() % 2400) - 1200;
  ps->velocity[1] += (rand() % 2400) - 1200;

  // Deduct 1 HP as visual penalty feedback (if they have > 1 health remaining)
  if (ps->stats[STAT_HEALTH] > 1) {
    ps->stats[STAT_HEALTH] -= 1;
  }

  // Broadcast the yeet
  SV_SendServerCommand(NULL, va("chat \"^1%s^7 was YEETED across the room by High Admin ^1%s^7!\"", targetCl->name, cl->name));
  
  // Log the action
  SV_Ranked_Log("ADMIN: High Admin %s yeeted %s", r->username, targetCl->name);
  Com_Printf("[RANKED ADMIN] High Admin %s yeeted %s\n", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_AdminFreeze
Freezes target player using PM_FREEZE force power.
==================
*/
void SV_Ranked_Cmd_AdminFreeze(client_t *cl, const char *target) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *ps = SV_GameClientNum(targetClient);
  if (!ps || ps->pm_type == PM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Player is not alive or in spectator mode.\"");
    return;
  }

  sv_rankedPlayers[targetClient].isFrozen = qtrue;
  VectorCopy(ps->origin, sv_rankedPlayers[targetClient].frozenOrigin);
  ps->pm_type = PM_NORMAL;
  VectorClear(ps->velocity);
  ps->weaponTime = 1000;
  ps->saberMove = 0;
  ps->forceHandExtend = HANDEXTEND_NONE;
  ps->fd.forcePower = 0;
  ps->saberHolstered = 2;

  SV_SendServerCommand(NULL, va("chat \"^5Force Freeze! ^1%s^7 has been frozen in time by High Admin ^1%s^7!\"", targetCl->name, cl->name));
  SV_Ranked_Log("ADMIN: High Admin %s froze %s", cl->name, targetCl->name);
  Com_Printf("[RANKED ADMIN] High Admin %s froze %s\n", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_AdminUnfreeze
Unfreezes target player.
==================
*/
void SV_Ranked_Cmd_AdminUnfreeze(client_t *cl, const char *target) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *ps = SV_GameClientNum(targetClient);

  sv_rankedPlayers[targetClient].isFrozen = qfalse;
  if (ps) {
    if (ps->pm_type == PM_FREEZE) {
      ps->pm_type = PM_NORMAL;
    }
    ps->weaponTime = 0;
    ps->saberHolstered = 0;
  }

  SV_SendServerCommand(NULL, va("chat \"^2Force Release! ^1%s^7 was unfrozen by High Admin ^1%s^7.\"", targetCl->name, cl->name));
  SV_Ranked_Log("ADMIN: High Admin %s unfroze %s", cl->name, targetCl->name);
  Com_Printf("[RANKED ADMIN] High Admin %s unfroze %s\n", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_Bring
Teleports target player 64 units in front of the Admin.
==================
*/
void SV_Ranked_Cmd_Bring(client_t *cl, const char *target) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  int adminNum = cl - svs.clients;
  if (targetClient == adminNum) {
    SV_SendServerCommand(cl, "chat \"^1Cannot bring yourself.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *psAdmin = SV_GameClientNum(adminNum);
  playerState_t *psTarget = SV_GameClientNum(targetClient);

  if (!psAdmin || !psTarget || psTarget->pm_type == PM_SPECTATOR || psTarget->stats[STAT_HEALTH] <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Target is not alive or is in spectator mode.\"");
    return;
  }

  float yaw = DEG2RAD(psAdmin->viewangles[YAW]);
  vec3_t forward;
  forward[0] = cosf(yaw);
  forward[1] = sinf(yaw);
  forward[2] = 0.0f;

  psTarget->origin[0] = psAdmin->origin[0] + forward[0] * 64.0f;
  psTarget->origin[1] = psAdmin->origin[1] + forward[1] * 64.0f;
  psTarget->origin[2] = psAdmin->origin[2];
  VectorClear(psTarget->velocity);

  if (sv_rankedPlayers[targetClient].isFrozen) {
    VectorCopy(psTarget->origin, sv_rankedPlayers[targetClient].frozenOrigin);
  }

  SV_SendServerCommand(NULL, va("chat \"^1%s^7 was brought to High Admin ^1%s^7!\"", targetCl->name, cl->name));
  SV_Ranked_Log("ADMIN: High Admin %s brought %s", cl->name, targetCl->name);
  Com_Printf("[RANKED ADMIN] High Admin %s brought %s\n", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_Goto
Teleports Admin 64 units in front of the target player.
==================
*/
void SV_Ranked_Cmd_Goto(client_t *cl, const char *target) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  int adminNum = cl - svs.clients;
  if (targetClient == adminNum) {
    SV_SendServerCommand(cl, "chat \"^1Cannot goto yourself.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *psAdmin = SV_GameClientNum(adminNum);
  playerState_t *psTarget = SV_GameClientNum(targetClient);

  if (!psAdmin || !psTarget || psTarget->pm_type == PM_SPECTATOR || psTarget->stats[STAT_HEALTH] <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Target is not alive or is in spectator mode.\"");
    return;
  }

  float yaw = DEG2RAD(psTarget->viewangles[YAW]);
  vec3_t forward;
  forward[0] = cosf(yaw);
  forward[1] = sinf(yaw);
  forward[2] = 0.0f;

  psAdmin->origin[0] = psTarget->origin[0] + forward[0] * 64.0f;
  psAdmin->origin[1] = psTarget->origin[1] + forward[1] * 64.0f;
  psAdmin->origin[2] = psTarget->origin[2];
  VectorClear(psAdmin->velocity);

  SV_SendServerCommand(cl, va("chat \"^2Teleported to ^5%s^2.\"", targetCl->name));
  SV_Ranked_Log("ADMIN: High Admin %s went to %s", cl->name, targetCl->name);
  Com_Printf("[RANKED ADMIN] High Admin %s went to %s\n", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_GiveForce
Grants force power to target player.
==================
*/
void SV_Ranked_Cmd_GiveForce(client_t *cl, const char *target, const char *power, int level) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires High Admin).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  extern qboolean SV_Ranked_GiveForcePower(client_t *cl, const char *powerName, int level, qboolean showMsg);
  if (SV_Ranked_GiveForcePower(targetCl, power, level, qtrue)) {
    SV_SendServerCommand(cl, va("chat \"^2Granted ^5%s^2 (Level %d) to ^5%s^2!\"", power, level, targetCl->name));
    SV_Ranked_Log("ADMIN: High Admin %s gave force %s lvl %d to %s", cl->name, power, level, targetCl->name);
  }
}

/*
==================
SV_Ranked_Cmd_GodForce
Toggles infinite force energy for target player.
==================
*/
void SV_Ranked_Cmd_GodForce(client_t *cl, const char *target) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires High Admin).\"");
    return;
  }

  const char *tName = target;
  if (!tName || !tName[0]) tName = cl->name;

  int targetClient = SV_Ranked_FindPlayerByNameOrId(tName);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  client_t *targetCl = &svs.clients[targetClient];
  sv_rankedPlayers[targetClient].godForce = sv_rankedPlayers[targetClient].godForce ? qfalse : qtrue;

  const char *st = sv_rankedPlayers[targetClient].godForce ? "^2ENABLED" : "^1DISABLED";
  SV_SendServerCommand(cl, va("chat \"^5Infinite Force Energy for ^7%s %s^5!\"", targetCl->name, st));
  if (cl != targetCl) {
    SV_SendServerCommand(targetCl, va("chat \"^5Infinite Force Energy %s ^5by High Admin ^7%s^5!\"", st, cl->name));
  }
  SV_Ranked_Log("ADMIN: High Admin %s toggled godForce for %s", cl->name, targetCl->name);
}

/*
==================
SV_Ranked_Cmd_Speed
Sets movement speed multiplier for target player.
==================
*/
void SV_Ranked_Cmd_Speed(client_t *cl, const char *target, float multiplier) {
  if (!SV_Ranked_IsHighAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1You do not have permission to use this command (requires Admin Level 1).\"");
    return;
  }

  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  if (multiplier <= 0.05f) {
    multiplier = 1.0f;
  }

  client_t *targetCl = &svs.clients[targetClient];
  playerState_t *ps = SV_GameClientNum(targetClient);

  sv_rankedPlayers[targetClient].speedMultiplier = multiplier;
  if (ps) {
    ps->speed = 250.0f * multiplier;
  }

  SV_SendServerCommand(NULL, va("chat \"^2Speed for ^1%s^2 set to ^5%.1fx ^2by High Admin ^1%s^2!\"", targetCl->name, multiplier, cl->name));
  SV_Ranked_Log("ADMIN: High Admin %s set speed for %s to %.1fx", cl->name, targetCl->name, multiplier);
  Com_Printf("[RANKED ADMIN] High Admin %s set speed for %s to %.1fx\n", cl->name, targetCl->name, multiplier);
}



/*
==================
SV_Ranked_Cmd_Lives
Shows remaining lives to self.
==================
*/
void SV_Ranked_Cmd_Lives(client_t *cl) {
  int clientNum = cl - svs.clients;
  if (!sv_rankedPlayers[clientNum].livesActive) {
    SV_SendServerCommand(cl, "chat \"^5Lives system is not active for you (unlimited respawns).\"");
  } else {
    SV_SendServerCommand(cl, va("chat \"^2You have ^5%d ^2lives remaining.\"", sv_rankedPlayers[clientNum].remainingLives));
  }
}


// Parse `!wanted` — show top 5 players by duel win streak + live bounty
static void SV_Ranked_Cmd_Wanted(client_t *cl) {
  extern cJSON *accountsDB;
  if (!accountsDB || !accountsDB->child) {
    SV_SendServerCommand(cl, "chat \"^1No player data found.\"");
    return;
  }

  // Collect name/duel-streak/bounty, always reading from modes.duel.streak
  typedef struct {
    char name[64];
    char username[64];
    int  streak;
    int  bounty;
  } WantedEntry;
  WantedEntry top[5];
  int topCount = 0;
  Com_Memset(top, 0, sizeof(top));

  cJSON *player = accountsDB->child;
  while (player) {
    cJSON *modesObj  = cJSON_GetObjectItemCaseSensitive(player, "modes");
    cJSON *duelData  = modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, "duel") : NULL;
    cJSON *streakPtr = duelData ? cJSON_GetObjectItemCaseSensitive(duelData, "streak") : NULL;
    cJSON *namePtr   = cJSON_GetObjectItemCaseSensitive(player, "displayName");
    cJSON *userPtr   = cJSON_GetObjectItemCaseSensitive(player, "username");
    if (!namePtr || !namePtr->valuestring || !namePtr->valuestring[0]) {
      player = player->next;
      continue;
    }
    if (!streakPtr || streakPtr->valueint <= 0) {
      player = player->next;
      continue;
    }
    int streak = streakPtr->valueint;

    // Look up live in-memory bounty for this account (if they're online)
    int liveBounty = 0;
    if (userPtr && userPtr->valuestring && userPtr->valuestring[0]) {
      for (int i = 0; i < sv_maxclients->integer; i++) {
        if (sv_rankedPlayers[i].loggedIn &&
            Q_stricmp(sv_rankedPlayers[i].username, userPtr->valuestring) == 0) {
          liveBounty = sv_rankedPlayers[i].bountyValue;
          break;
        }
      }
    }

    // Insert-sort into top[5] by duel streak
    for (int i = 0; i < 5; i++) {
      if (streak > top[i].streak) {
        for (int j = 4; j > i; j--)
          top[j] = top[j - 1];
        Q_strncpyz(top[i].name,     namePtr->valuestring, sizeof(top[i].name));
        Q_strncpyz(top[i].username, userPtr ? userPtr->valuestring : "", sizeof(top[i].username));
        top[i].streak = streak;
        top[i].bounty = liveBounty;
        if (topCount < 5)
          topCount++;
        break;
      }
    }
    player = player->next;
  }

  if (topCount == 0) {
    SV_SendServerCommand(cl, "chat \"^1No wanted players at the moment.\"");
    return;
  }
  SV_SendServerCommand(cl, "bounty_clear");
  SV_SendServerCommand(cl, "print \"^1=== WANTED LIST (Duel Streaks) ===\n\"");
  for (int i = 0; i < topCount; i++) {
    SV_SendServerCommand(cl, va("bounty_entry %d %d %d \"%s\"",
      i + 1, top[i].streak, top[i].bounty, top[i].name));
    if (top[i].bounty > 0) {
      SV_SendServerCommand(cl,
        "print \"^5#%d^7: ^7%s^7 | Streak: ^5%d^7 | Bounty: ^3%d CR^7\n\"",
        i + 1, top[i].name, top[i].streak, top[i].bounty);
    } else {
      SV_SendServerCommand(cl,
        "print \"^5#%d^7: ^7%s^7 | Streak: ^5%d\n\"",
        i + 1, top[i].name, top[i].streak);
    }
  }
  SV_SendServerCommand(cl, "bounty_open 1");
}

// Parse `!inventory` — show owned items from JSON account
void SV_Ranked_Cmd_Inventory(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(
        cl, "chat \"^3You must be logged in to view your inventory.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *inv = acc ? cJSON_GetObjectItemCaseSensitive(acc, "inventory") : NULL;

  SV_SendServerCommand(cl, "inv_clear");
  SV_SendServerCommand(cl, "print \"^6=== Your Inventory ===\n\"");
  if (!inv || cJSON_GetArraySize(inv) == 0) {
    SV_SendServerCommand(cl, "print \"^7  (empty)\n\"");
  } else {
    cJSON *item = NULL;
    int count = 0;
    cJSON_ArrayForEach(item, inv) {
      const char *itemName = item->string;
      int qty = item->valueint;
      if (qty > 0) {
        const char *dispName = SV_Ranked_GetItemDisplayName(itemName);
        SV_SendServerCommand(cl, "print \"^7  %s ^5x%d\n\"", dispName, qty);
        SV_SendServerCommand(cl, "inv_entry %d \"%s\" \"%s\"", qty, itemName, dispName);
        count++;
      }
    }
    if (count == 0) {
      SV_SendServerCommand(cl, "print \"^7  (empty)\n\"");
    }
  }
  SV_SendServerCommand(cl, "inv_open");
}


// Parse `!buy itemname` — purchase from shop
static void SV_Ranked_Cmd_Buy(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl,
                         "chat \"^3You must be logged in to buy items.\"");
    return;
  }

  const char *arg = strchr(chatText, ' ');
  if (!arg || *(arg + 1) == '\0') {
    SV_SendServerCommand(cl, "chat \"^1Usage: !buy <item_name>\"");
    return;
  }
  arg++;

  // Forward to the DB shop handler
  SV_Ranked_ShopBuy(cl, arg);
}

// Parse `!sell itemname` — sell an inventory item
static void SV_Ranked_Cmd_Sell(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl,
                         "chat \"^3You must be logged in to sell items.\"");
    return;
  }

  const char *arg = strchr(chatText, ' ');
  if (!arg || *(arg + 1) == '\0') {
    SV_SendServerCommand(cl, "chat \"^1Usage: !sell <item_name>\"");
    return;
  }
  arg++;

  SV_Ranked_ShopSell(cl, arg);
}

// Parse `!use itemname` — use an inventory item
static void SV_Ranked_Cmd_Use(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl,
                         "chat \"^3You must be logged in to use items.\"");
    return;
  }

  const char *arg = strchr(chatText, ' ');
  if (!arg || *(arg + 1) == '\0') {
    SV_SendServerCommand(cl, "chat \"^1Usage: !use <item_name>\"");
    return;
  }
  arg++;

  SV_Ranked_ShopUse(cl, arg);
}

// ===========================================================================
//  HOT POTATO VOTE SYSTEM
// ===========================================================================
static qboolean sv_ranked_voteActive = qfalse;
static int sv_ranked_votesFor = 0;
static int sv_ranked_votesAgainst = 0;
static int sv_ranked_voteEndTime = 0;
static int sv_ranked_voterFlags[64]; // bitfield-style: 1 = voted

static void SV_Ranked_Cmd_Vote(client_t *cl, const char *chatText) {
  char arg1[32];
  if (sscanf(chatText, "%*s %31s", arg1) != 1) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !vote hotpotato | !vote yes | !vote no\"");
    return;
  }

  int initiator = cl - svs.clients;

  if (!Q_stricmp(arg1, "hotpotato")) {
    if (sv_ranked_voteActive && svs.time < sv_ranked_voteEndTime) {
      SV_SendServerCommand(cl, "chat \"^1A vote is already in progress.\"");
      return;
    }

    if (sv_hotPotatoActive) {
      SV_SendServerCommand(cl, "chat \"^1Hot Potato mode is already active!\"");
      return;
    }

    if (Q_stricmp(SV_Ranked_GetActiveMode(), "duel") != 0) {
      SV_SendServerCommand(cl, "chat \"^1Hot Potato only works in ^3Duel Mode^1!\"");
      return;
    }

    sv_ranked_voteActive = qtrue;
    sv_ranked_votesFor = 1;
    sv_ranked_votesAgainst = 0;
    sv_ranked_voteEndTime = svs.time + 60000; // 60 second window
    Com_Memset(sv_ranked_voterFlags, 0, sizeof(sv_ranked_voterFlags));
    sv_ranked_voterFlags[initiator] = 1;

    SV_SendServerCommand(NULL, "chat \"^7A vote has started for ^1HOT POTATO MODE^7! Type ^2!vote yes^7 or ^1!vote no^7! (60s)\"");
    return;
  }

  if (!sv_ranked_voteActive || svs.time >= sv_ranked_voteEndTime) {
    SV_SendServerCommand(cl, "chat \"^1There is no active vote.\"");
    return;
  }

  if (sv_ranked_voterFlags[initiator]) {
    SV_SendServerCommand(cl, "chat \"^1You have already voted.\"");
    return;
  }
  
  if (!Q_stricmp(arg1, "yes")) {
    sv_ranked_votesFor++;
    SV_SendServerCommand(NULL, "chat \"^7%s^7 votes ^2YES^7.\"", cl->name);
  } else if (!Q_stricmp(arg1, "no")) {
    sv_ranked_votesAgainst++;
    SV_SendServerCommand(NULL, "chat \"^7%s^7 votes ^1NO^7.\"", cl->name);
  } else {
    SV_SendServerCommand(cl, "chat \"^1Usage: !vote hotpotato | !vote yes | !vote no\"");
    return;
  }

  sv_ranked_voterFlags[initiator] = 1;

  int connected = 0;
  for (int i = 0; i < sv_maxclients->integer; i++) {
    if (svs.clients[i].state >= CS_CONNECTED) connected++;
  }
  
  int required = (connected * 50) / 100;
  if (required < 2) required = 2; // For testing

  if (sv_ranked_votesFor >= required) {
    SV_SendServerCommand(NULL, "chat \"^2Vote PASSED! Hot Potato mode is starting!\"");
    SV_Ranked_StartHotPotato();
    sv_ranked_voteActive = qfalse;
  }
}

void SV_Ranked_Vote_Frame(void) {
  if (!sv_ranked_voteActive) return;

  if (svs.time >= sv_ranked_voteEndTime) {
    SV_SendServerCommand(NULL, "chat \"^1Vote FAILED! Not enough votes to start Hot Potato.\"");
    sv_ranked_voteActive = qfalse;
  }
}

// Parse `!send target amount`
static void SV_Ranked_Cmd_Send(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl,
                         "chat \"^3You must be logged in to send credits.\"");
    return;
  }

  // Skip "!send " or "!s "
  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !send <name> <amount>\"");
    return;
  }
  arg++; // skip space

  // Find the last space to split target Name and amount
  // E.g. "my target name 50" -> split at the last space
  // E.g. "my target name  50" -> we trim trailing spaces before the number
  
  // Trim trailing spaces off the command string first to prevent " 50 " issues
  char argClean[256];
  Q_strncpyz(argClean, arg, sizeof(argClean));
  int len = strlen(argClean);
  while (len > 0 && argClean[len - 1] == ' ') {
      argClean[len - 1] = '\0';
      len--;
  }

  const char *lastSpace = strrchr(argClean, ' ');
  if (!lastSpace) {
      SV_SendServerCommand(cl, "chat \"^1Usage: !send <name> <amount>\"");
      return;
  }

  int amount = atoi(lastSpace + 1);
  if (amount <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Invalid amount.\"");
    return;
  }

  char targetName[64];
  int nameLen = lastSpace - argClean;
  if (nameLen >= sizeof(targetName)) nameLen = sizeof(targetName) - 1;
  Q_strncpyz(targetName, argClean, nameLen + 1);

  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient == -1) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  } else if (targetClient == -2) {
    SV_SendServerCommand(cl, "chat \"^3Multiple players match that name. Use "
                             "Client ID or a more specific name.\n\"");
    return;
  }

  if (targetClient == clientNum) {
    SV_SendServerCommand(cl,
                         "chat \"^1You cannot send credits to yourself.\"");
    return;
  }

  rankedMatchState_t *t = &sv_rankedPlayers[targetClient];
  if (!t->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^1Target player is not logged in.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *credPtr =
      acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int creds = credPtr ? credPtr->valueint : 0;

  if (creds < amount) {
    SV_SendServerCommand(
        cl, "chat \"^1Insufficient credits. You only have %d.\"", creds);
    return;
  }

  UpdateAccountCredits(r->username, -amount);
  UpdateAccountCredits(t->username, amount);

  SV_SendServerCommand(
      cl, "chat \"^2Successfully sent ^5%d ^2credits to ^7%s^2.\"", amount,
      svs.clients[targetClient].name);
  SV_SendServerCommand(
      svs.clients + targetClient,
      "chat \"^3[BANK] ^2You received ^5%d ^2credits from ^7%s^2.\"", amount,
      cl->name);
  SV_Ranked_Log("BANK: %s sent %d credits to %s", r->username, amount, t->username);
  SV_Ranked_SaveAccounts();
}

// Parse `!bet target amount`
static void SV_Ranked_Cmd_Bet(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in to bet.\"");
    return;
  }

  if (r->currentBetAmount > 0) {
    SV_SendServerCommand(cl, "chat \"^1You already have an active bet.\"");
    return;
  }

  // Skip "!bet "
  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl, "chat \"^1Usage: !bet <name> <amount>\"");
    return;
  }
  arg++; // skip space

  // Find the last space using safe indexing
  char argClean[256];
  Q_strncpyz(argClean, arg, sizeof(argClean));
  int len = strlen(argClean);
  while (len > 0 && argClean[len - 1] == ' ') {
      argClean[len - 1] = '\0';
      len--;
  }

  const char *lastSpace = strrchr(argClean, ' ');
  if (!lastSpace) {
      SV_SendServerCommand(cl, "chat \"^1Usage: !bet <name> <amount>\"");
      return;
  }

  int amount = atoi(lastSpace + 1);
  if (amount <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Invalid amount.\"");
    return;
  }

  char targetName[64];
  int nameLen = lastSpace - argClean;
  if (nameLen >= sizeof(targetName)) nameLen = sizeof(targetName) - 1;
  Q_strncpyz(targetName, argClean, nameLen + 1);

  cJSON *betMinPtr = SV_Ranked_GetSetting("bet_min_amount");
  int betMin = betMinPtr ? betMinPtr->valueint : 1;
  cJSON *betMaxPtr = SV_Ranked_GetSetting("bet_max_amount");
  int betMax = betMaxPtr ? betMaxPtr->valueint : 10000;

  if (amount < betMin) {
    SV_SendServerCommand(cl, va("chat \"^1Bet amount must be at least %d.\"", betMin));
    return;
  }
  if (amount > betMax) {
    SV_SendServerCommand(cl, va("chat \"^1Maximum bet is %d credits.\"", betMax));
    return;
  }

  // Find target BEFORE any credit checks so we can do self-check early
  int targetClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (targetClient == -1) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  } else if (targetClient == -2) {
    SV_SendServerCommand(cl, "chat \"^3Multiple players match that name. Use "
                             "Client ID or a more specific name.\n\"");
    return;
  }

  // Self-bet guard MUST come before inDuel check and credit deduction
  if (targetClient == clientNum) {
    SV_SendServerCommand(cl, "chat \"^1You cannot bet on yourself.\"");
    return;
  }

  if (!sv_rankedPlayers[targetClient].inDuel) {
    SV_SendServerCommand(
        cl, "chat \"^1That player is not currently in a duel.\"");
    return;
  }

  // Credit check last, after all other guards pass
  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *credPtr =
      acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int creds = credPtr ? credPtr->valueint : 0;

  if (creds < amount) {
    SV_SendServerCommand(
        cl, "chat \"^1Not enough credits. You have ^5%d^1, bet requires ^5%d^1.\"",
        creds, amount);
    return;
  }

  UpdateAccountCredits(r->username, -amount);
  SV_Ranked_SaveAccounts();

  r->currentBetTarget = targetClient;
  r->currentBetAmount = amount;

  SV_SendServerCommand(
      cl, "chat \"^2You bet ^5%d ^2credits on ^7%s^2 to win their duel!\"",
      amount, svs.clients[targetClient].name);
}

// Parse `!changepassword newpass`
static void SV_Ranked_Cmd_ChangePassword(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn || r->isTemp) {
    SV_SendServerCommand(
        cl, "chat \"^3You must be logged in to change your password.\"");
    return;
  }

  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !changepassword <new_password>\"");
    return;
  }

  // skip space
  while (*arg == ' ')
    arg++;

  if (!arg[0] || strlen(arg) < 3) {
    SV_SendServerCommand(
        cl, "chat \"^1New password must be at least 3 characters long.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (acc) {
    cJSON *passObj = cJSON_GetObjectItemCaseSensitive(acc, "password");
    if (passObj) {
      cJSON_SetValuestring(passObj, arg);
      SV_Ranked_SaveAccounts();
      SV_SendServerCommand(cl, "chat \"^2Password successfully changed.\"");
      Com_Printf("RANKED: Client %d changed their password.\n", clientNum);
    }
  }
}

// Parse `!changeusername <new_username>`
static void SV_Ranked_Cmd_ChangeUsername(client_t *cl, const char *chatText) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn || r->isTemp) {
    SV_SendServerCommand(
        cl, "chat \"^3You must be logged in to change your username.\"");
    return;
  }

  const char *arg = strchr(chatText, ' ');
  if (!arg) {
    SV_SendServerCommand(cl,
                         "chat \"^1Usage: !changeusername <new_username>\"");
    return;
  }

  while (*arg == ' ')
    arg++;

  if (!arg[0] || strlen(arg) < 2 || strlen(arg) >= MAX_AUTH_STRING) {
    SV_SendServerCommand(
        cl, "chat \"^1Username must be 2-63 characters.\"");
    return;
  }

  if (!Ranked_IsValidAuthString(arg)) {
    SV_SendServerCommand(
        cl, "chat \"^1Invalid username. Use only letters, numbers, _, -\"");
    return;
  }

  char newKey[MAX_AUTH_STRING];
  Ranked_SafeUserKey(arg, newKey, sizeof(newKey));

  if (!Q_stricmp(newKey, r->username)) {
    SV_SendServerCommand(cl, "chat \"^1New username is the same as current.\"");
    return;
  }

  // Check if new username is already taken
  if (SV_Ranked_GetAccount(newKey)) {
    SV_SendServerCommand(cl, "chat \"^1Username '^7%s^1' is already taken.\"", newKey);
    return;
  }

  // Rename account in DB
  extern cJSON *accountsDB;
  if (!accountsDB) {
    SV_SendServerCommand(cl, "chat \"^1Database not loaded.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc) {
    SV_SendServerCommand(cl, "chat \"^1Account not found.\"");
    return;
  }

  // Detach old account entry
  cJSON *detached = cJSON_DetachItemFromObject(accountsDB, r->username);
  if (!detached) {
    SV_SendServerCommand(cl, "chat \"^1Failed to detach old username.\"");
    return;
  }

  // Update the username field inside the account object
  cJSON *uPtr = cJSON_GetObjectItemCaseSensitive(detached, "username");
  if (uPtr) {
    cJSON_SetValuestring(uPtr, newKey);
  } else {
    cJSON_AddStringToObject(detached, "username", newKey);
  }

  // Re-attach with new key
  cJSON_AddItemToObject(accountsDB, newKey, detached);
  SV_Ranked_SaveAccounts();

  // Update session state
  Q_strncpyz(r->username, newKey, sizeof(r->username));

  SV_SendServerCommand(cl, "chat \"^2Username changed to ^7%s^2!\"", newKey);
  SV_Ranked_Log("RENAME: client %d renamed to '%s'", clientNum, newKey);
}

qboolean SV_Ranked_ProcessCommand(client_t *cl, const char *chatText) {
  if (!chatText || chatText[0] == '\0')
    return qfalse;

  // Intercept Trivia Answers
  if (chatText[0] == '#') {
      SV_Ranked_Trivia_HandleAnswer(cl, chatText);
      return qtrue; // Hide the answer attempt from public chat to prevent copying
  }

  // Global ranked toggle — if disabled, silently swallow all ranked commands.
  if (!Cvar_VariableIntegerValue("sv_ranked_enabled")) {
    // Only respond to !help so players know the system is offline.
    if (chatText[0] == '!' && (!Q_stricmpn(chatText, "!help", 5) || !Q_stricmpn(chatText, "!cmds", 5))) {
      SV_SendServerCommand(cl, "chat \"^1Ranked system is currently disabled by the server admin.\"");
      return qtrue;
    }
    return qfalse;
  }

  char cmdSpace[MAX_STRING_CHARS];
  Q_strncpyz(cmdSpace, chatText, sizeof(cmdSpace));
  char *spacePos = strchr(cmdSpace, ' ');
  if (spacePos)
    *spacePos = '\0'; // Get first token

  if (!Q_stricmp(cmdSpace, "!stats") || !Q_stricmp(cmdSpace, "!info")) {
    const char *arg = strchr(chatText, ' ');
    if (arg && *(arg + 1) != '\0') {
      SV_Ranked_ShowStatsTarget(cl, arg + 1);
    } else {
      SV_Ranked_ShowStats(cl);
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!ranked")) {
    int clientNum = cl - svs.clients;
    sv_rankedPlayers[clientNum].rankedEnabled = qtrue;
    SV_SendServerCommand(cl, "chat \"^2[RANKED] ^7Ranked mode is now ^2ENABLED^7. Your duels will count towards Elo & stats.\"");
    SV_SendServerCommand(cl, "ranked_status 1");
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!unranked") || !Q_stricmp(cmdSpace, "!casual")) {
    int clientNum = cl - svs.clients;
    sv_rankedPlayers[clientNum].rankedEnabled = qfalse;
    SV_SendServerCommand(cl, "chat \"^3[RANKED] ^7Ranked mode is now ^1DISABLED (Casual)^7. Your duels will be friendly matches with no Elo changes.\"");
    SV_SendServerCommand(cl, "ranked_status 0");
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!toggleranked") || !Q_stricmp(cmdSpace, "!tr")) {
    int clientNum = cl - svs.clients;
    sv_rankedPlayers[clientNum].rankedEnabled = (qboolean)!sv_rankedPlayers[clientNum].rankedEnabled;
    if (sv_rankedPlayers[clientNum].rankedEnabled) {
      SV_SendServerCommand(cl, "chat \"^2[RANKED] ^7Ranked mode is now ^2ENABLED^7.\"");
      SV_SendServerCommand(cl, "ranked_status 1");
    } else {
      SV_SendServerCommand(cl, "chat \"^3[RANKED] ^7Ranked mode is now ^1DISABLED (Casual)^7.\"");
      SV_SendServerCommand(cl, "ranked_status 0");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!top") || !Q_stricmp(cmdSpace, "!t") ||
             !Q_stricmp(cmdSpace, "!topelo")) {
    SV_Ranked_ShowTop(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!toppotato")) {
    SV_Ranked_ShowTopPotato(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!topcredits") || !Q_stricmp(cmdSpace, "!topcr")) {
    SV_Ranked_ShowTopCredits(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!rank") || !Q_stricmp(cmdSpace, "!r") ||
             !Q_stricmp(cmdSpace, "!ra")) {
    SV_Ranked_ShowRank(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!ranks")) {
    SV_Ranked_ShowRankThresholds(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!quests")) {
    SV_Ranked_ShowQuests(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!logout")) {
    SV_Ranked_Logout(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!changepassword") ||
             !Q_stricmp(cmdSpace, "!passwd")) {
    SV_Ranked_Cmd_ChangePassword(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!changeusername")) {
    SV_Ranked_Cmd_ChangeUsername(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!achievements") ||
             !Q_stricmp(cmdSpace, "!ach")) {
    SV_Ranked_ShowAchievements(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!credits") || !Q_stricmp(cmdSpace, "!cr")) {
    SV_Ranked_ShowCredits(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!shop") || !Q_stricmp(cmdSpace, "!sh")) {
    SV_Ranked_ShowShop(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!inventory") ||
             !Q_stricmp(cmdSpace, "!inv") || !Q_stricmp(cmdSpace, "!i")) {
    SV_Ranked_Cmd_Inventory(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!buy")) {
    SV_Ranked_Cmd_Buy(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!sell") || !Q_stricmp(cmdSpace, "!sl")) {
    SV_Ranked_Cmd_Sell(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!use") || !Q_stricmp(cmdSpace, "!u")) {
    SV_Ranked_Cmd_Use(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!wanted") || !Q_stricmp(cmdSpace, "!w")) {
    SV_Ranked_Cmd_Wanted(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!vote")) {
    SV_Ranked_Cmd_Vote(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!bounty")) {
    char targetName[MAX_STRING_CHARS];
    int amount = 0;
    if (sscanf(chatText, "!bounty %63s %d", targetName, &amount) == 2) {
      SV_Ranked_SetBounty(cl, targetName, amount);
    } else {
      SV_SendServerCommand(cl,
                           "chat \"^3Usage: !bounty <playername> <amount>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!bet") || !Q_stricmp(cmdSpace, "!b")) {
    SV_Ranked_Cmd_Bet(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!bountylist") ||
             !Q_stricmp(cmdSpace, "!bounties")) {
    SV_Ranked_ShowBountyList(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!send") || !Q_stricmp(cmdSpace, "!s")) {
    SV_Ranked_Cmd_Send(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!roll") || !Q_stricmp(cmdSpace, "!rl")) {
    SV_Ranked_Cmd_Roll(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!setwinmsg")) {
    const char *arg = strchr(chatText, ' ');
    if (arg && *(arg + 1) != '\0') {
      SV_Ranked_Cmd_SetWinMsg(cl, arg + 1);
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !setwinmsg <message>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!giveitem") || !Q_stricmp(cmdSpace, "!givegun") || !Q_stricmp(cmdSpace, "!giveall") || !Q_stricmp(cmdSpace, "!giveguns") ||
             !Q_stricmp(cmdSpace, "!yeet") || !Q_stricmp(cmdSpace, "!slap") ||
             !Q_stricmp(cmdSpace, "!giveforce") || !Q_stricmp(cmdSpace, "!grantforce") ||
             !Q_stricmp(cmdSpace, "!godforce") || !Q_stricmp(cmdSpace, "!infforce") || !Q_stricmp(cmdSpace, "!speed")) {
    SV_SendServerCommand(cl, "chat \"^1Command disabled on this server to comply with MovieBattles II ToS.\"");
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!freeze")) {
    char target[64];
    if (sscanf(chatText, "%*s %63s", target) == 1) {
      SV_Ranked_Cmd_AdminFreeze(cl, target);
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !freeze <player>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!unfreeze")) {
    char target[64];
    if (sscanf(chatText, "%*s %63s", target) == 1) {
      SV_Ranked_Cmd_AdminUnfreeze(cl, target);
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !unfreeze <player>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!givecredits") || !Q_stricmp(cmdSpace, "!gc")) {
    SV_Ranked_Cmd_GiveCredits(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!setelo")) {
    SV_Ranked_Cmd_SetElo(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!setrank")) {
    SV_Ranked_Cmd_SetRank(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!jail")) {
    SV_Ranked_Cmd_Jail(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!unjail")) {
    SV_Ranked_Cmd_Unjail(cl, chatText);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!forcepotato")) {
    if (SV_Ranked_IsAdmin(cl)) {
      SV_Ranked_StartHotPotato();
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!stoppotato")) {
    if (SV_Ranked_IsAdmin(cl)) {
      SV_Ranked_StopHotPotato(qtrue);
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!bring")) {
    char target[64];
    if (sscanf(chatText, "%*s %63s", target) == 1) {
      SV_Ranked_Cmd_Bring(cl, target);
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !bring <player>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!goto")) {
    char target[64];
    if (sscanf(chatText, "%*s %63s", target) == 1) {
      SV_Ranked_Cmd_Goto(cl, target);
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !goto <player>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!lives") || !Q_stricmp(cmdSpace, "!mylives")) {
    SV_Ranked_Cmd_Lives(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!details") || !Q_stricmp(cmdSpace, "!myinfo")) {
    SV_Ranked_Cmd_Details(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!cp")) {
    if (!SV_Ranked_IsAdmin(cl)) {
      SV_SendServerCommand(cl, "chat \"^1Admin only.\"");
      return qtrue;
    }
    const char *arg = strchr(chatText, ' ');
    if (!arg || !*(arg + 1)) {
      SV_SendServerCommand(cl, "chat \"^1Usage: !cp all <msg>  OR  !cp <name/id> <msg>\"");
      return qtrue;
    }
    // skip past !cp
    while (*arg == ' ') arg++;

    // parse target (first word after !cp)
    char target[64];
    int ti = 0;
    while (*arg && *arg != ' ' && ti < 63) {
      target[ti++] = *arg++;
    }
    target[ti] = '\0';
    while (*arg == ' ') arg++;

    if (!target[0] || !arg[0]) {
      SV_SendServerCommand(cl, "chat \"^1Usage: !cp all <msg>  OR  !cp <name/id> <msg>\"");
      return qtrue;
    }

    char wrapped[1024];
    SV_CP_WrapMessage(arg, wrapped, sizeof(wrapped));

    if (!Q_stricmp(target, "all")) {
      for (int i = 0; i < sv_maxclients->integer; i++) {
        client_t *c = &svs.clients[i];
        if (c->state >= CS_ACTIVE)
          SV_SendServerCommand(c, "cp \"%s\n\"", wrapped);
      }
      SV_Ranked_Log("ADMIN_CP: %s broadcast: %s",
                    sv_rankedPlayers[cl - svs.clients].username[0]
                        ? sv_rankedPlayers[cl - svs.clients].username
                        : cl->name,
                    wrapped);
    } else {
      int tid = SV_Ranked_FindPlayerByNameOrId(target);
      if (tid < 0 || tid >= sv_maxclients->integer ||
          svs.clients[tid].state < CS_ACTIVE) {
        SV_SendServerCommand(cl, "chat \"^1Player '^7%s^1' not found or not active.\"", target);
        return qtrue;
      }
      SV_SendServerCommand(&svs.clients[tid], "cp \"%s\n\"", wrapped);
      SV_Ranked_Log("ADMIN_CP: %s -> %s: %s",
                    sv_rankedPlayers[cl - svs.clients].username[0]
                        ? sv_rankedPlayers[cl - svs.clients].username
                        : cl->name,
                    svs.clients[tid].name, wrapped);
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!adventure") || !Q_stricmp(cmdSpace, "!adv")) {
    SV_Ranked_Adventure_Start(cl);
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "!choose") || !Q_stricmp(cmdSpace, "!c")) {
    const char *arg = strchr(chatText, ' ');
    if (arg && *(arg + 1) != '\0') {
      extern void SV_Ranked_Adventure_Choose(client_t *cl, int choiceIndex);
      SV_Ranked_Adventure_Choose(cl, atoi(arg + 1));
    } else {
      SV_SendServerCommand(cl, "chat \"^1Usage: !choose <choice_number>\"");
    }
    return qtrue;
  } else if (!Q_stricmp(cmdSpace, "duel_bp") || !Q_stricmp(cmdSpace, "my_bp")) {
    int val1 = atoi(Cmd_Argv(1));
    if (Cmd_Argc() >= 3) {
      int val2 = atoi(Cmd_Argv(2));
      if (val1 >= 0 && val1 < sv_maxclients->integer && val2 >= 0) {
        sv_rankedPlayers[val1].lastBP = val2;
      }
    } else {
      if (val1 >= 0) {
        sv_rankedPlayers[cl - svs.clients].lastBP = val1;
      }
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!createparty") || !Q_stricmp(cmdSpace, "!party") || !Q_stricmp(cmdSpace, "!cp")) {
    int clientNum = cl - svs.clients;
    char teamName[64] = "Party";
    char colorOrIdsBuf[128] = "";

    int numParsed = sscanf(chatText, "%*s %63s %127s", teamName, colorOrIdsBuf);
    if (numParsed < 1) {
      SV_SendServerCommand(cl, "chat \"^3Usage: ^5!createparty <TeamName> [color]\n^7Colors: ^5blue, red, green, yellow, purple, orange, black, white\"");
      return qtrue;
    }

    rankedParty_t *p = &sv_rankedParties[clientNum];
    memset(p, 0, sizeof(rankedParty_t));
    p->active = qtrue;
    Q_strncpyz(p->teamName, teamName, sizeof(p->teamName));
    p->teamColorIdx = clientNum % 8;
    p->score = 0;

    if (colorOrIdsBuf[0]) {
      int cIdx = -1;
      if (!Q_stricmp(colorOrIdsBuf, "blue")) cIdx = 0;
      else if (!Q_stricmp(colorOrIdsBuf, "red")) cIdx = 1;
      else if (!Q_stricmp(colorOrIdsBuf, "green")) cIdx = 2;
      else if (!Q_stricmp(colorOrIdsBuf, "yellow")) cIdx = 3;
      else if (!Q_stricmp(colorOrIdsBuf, "purple")) cIdx = 4;
      else if (!Q_stricmp(colorOrIdsBuf, "orange")) cIdx = 5;
      else if (!Q_stricmp(colorOrIdsBuf, "black")) cIdx = 6;
      else if (!Q_stricmp(colorOrIdsBuf, "white")) cIdx = 7;

      if (cIdx >= 0) p->teamColorIdx = cIdx;
    }

    p->clientNums[0] = clientNum;
    p->memberCount = 1;

    const qboolean isAdmin = SV_Ranked_IsAdmin(cl);
    if (isAdmin && colorOrIdsBuf[0] && strchr(colorOrIdsBuf, '.')) {
      char *token = strtok(colorOrIdsBuf, ".,; ");
      while (token && p->memberCount < MAX_PARTY_MEMBERS) {
        int id = atoi(token);
        if (id >= 0 && id < sv_maxclients->integer && id != clientNum) {
          qboolean alreadyIn = qfalse;
          for (int j = 0; j < p->memberCount; j++) {
            if (p->clientNums[j] == id) { alreadyIn = qtrue; break; }
          }
          if (!alreadyIn) {
            p->clientNums[p->memberCount++] = id;
          }
        }
        token = strtok(NULL, ".,; ");
      }
    }

    SV_Ranked_UpdateParty(clientNum);
    SV_SendServerCommand(cl, va("chat \"^2Party '^5%s^2' created! Use ^3!inviteparty <ID>^2 to invite members.\"", p->teamName));
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!partycolor") || !Q_stricmp(cmdSpace, "!teamcolor")) {
    int clientNum = cl - svs.clients;
    rankedParty_t *p = &sv_rankedParties[clientNum];
    if (!p->active) {
      SV_SendServerCommand(cl, "chat \"^1You are not party leader!\"");
      return qtrue;
    }
    char colStr[32] = "";
    if (sscanf(chatText, "%*s %31s", colStr) == 1) {
      int cIdx = -1;
      if (!Q_stricmp(colStr, "blue")) cIdx = 0;
      else if (!Q_stricmp(colStr, "red")) cIdx = 1;
      else if (!Q_stricmp(colStr, "green")) cIdx = 2;
      else if (!Q_stricmp(colStr, "yellow")) cIdx = 3;
      else if (!Q_stricmp(colStr, "purple")) cIdx = 4;
      else if (!Q_stricmp(colStr, "orange")) cIdx = 5;
      else if (!Q_stricmp(colStr, "black")) cIdx = 6;
      else if (!Q_stricmp(colStr, "white")) cIdx = 7;

      if (cIdx >= 0) {
        p->teamColorIdx = cIdx;
        SV_Ranked_UpdateParty(clientNum);
        SV_SendServerCommand(cl, va("chat \"^2Party shield color updated to ^5%s^2!\"", colStr));
      } else {
        SV_SendServerCommand(cl, "chat \"^3Colors: ^5blue, red, green, yellow, purple, orange, black, white\"");
      }
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "duel_bp") || !Q_stricmp(cmdSpace, "my_bp")) {
    int val1 = atoi(Cmd_Argv(1));
    int cNum = (int)(cl - svs.clients);
    if (Cmd_Argc() >= 3) {
      int val2 = atoi(Cmd_Argv(2));
      if (val1 >= 0 && val1 < sv_maxclients->integer && val2 >= 0) {
        sv_rankedPlayers[val1].lastBP = val2;
        Com_Printf("[RANKED] Client %d (%s) reported live combat BP: %d\n", val1, svs.clients[val1].name, val2);
      }
    } else {
      if (val1 >= 0) {
        sv_rankedPlayers[cNum].lastBP = val1;
        Com_Printf("[RANKED] Client %d (%s) reported live combat BP: %d\n", cNum, cl->name, val1);
      }
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!createparty") || !Q_stricmp(cmdSpace, "!party") || !Q_stricmp(cmdSpace, "!cp")) {
    int clientNum = cl - svs.clients;
    char teamName[64] = "Party";
    char colorOrIdsBuf[128] = "";

    int numParsed = sscanf(chatText, "%*s %63s %127s", teamName, colorOrIdsBuf);
    if (numParsed >= 1 && teamName[0] != '\0') {
      const char *colorNames[8] = { "blue", "red", "green", "yellow", "purple", "orange", "black", "white" };
      int selectedColor = 0; // default Blue
      for (int c = 0; c < 8; c++) {
        if (!Q_stricmp(colorOrIdsBuf, colorNames[c])) {
          selectedColor = c;
          break;
        }
      }

      rankedParty_t *p = &sv_rankedParties[clientNum];
      p->active = qtrue;
      Q_strncpyz(p->teamName, teamName, sizeof(p->teamName));
      p->teamColorIdx = selectedColor;
      p->score = 0;
      p->memberCount = 1;
      p->clientNums[0] = clientNum;

      SV_Ranked_UpdateParty(clientNum);
      SV_SendServerCommand(cl, va("chat \"^2Party '^5%s^2' created with shield color ^5%s^2!\"", p->teamName, colorNames[selectedColor]));
      SV_SendServerCommand(NULL, va("chat \"^3[NEW TEAM] ^5%s ^7formed party '^3%s^7'!\"", cl->name, p->teamName));
    } else {
      SV_SendServerCommand(cl, "chat \"^3Usage: ^5!createparty <TeamName> [Color]\"");
      SV_SendServerCommand(cl, "chat \"^7Colors: ^5blue, red, green, yellow, purple, orange, black, white\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!disbandparty") || !Q_stricmp(cmdSpace, "!dp")) {
    int clientNum = cl - svs.clients;
    rankedParty_t *p = &sv_rankedParties[clientNum];
    if (!p->active) {
      SV_SendServerCommand(cl, "chat \"^1You are not the leader of an active party!\"");
      return qtrue;
    }

    p->active = qfalse;
    SV_Ranked_UpdateParty(clientNum);
    p->memberCount = 0;
    SV_SendServerCommand(NULL, va("chat \"^1[PARTY] ^5%s ^7has disbanded party '^3%s^7'!\"", cl->name, p->teamName));
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!leaveparty") || !Q_stricmp(cmdSpace, "!lp")) {
    int clientNum = cl - svs.clients;
    qboolean left = qfalse;

    for (int i = 0; i < sv_maxclients->integer; i++) {
      rankedParty_t *p = &sv_rankedParties[i];
      if (p->active) {
        for (int j = 0; j < p->memberCount; j++) {
          if (p->clientNums[j] == clientNum) {
            if (i == clientNum) {
              p->active = qfalse;
              SV_Ranked_UpdateParty(i);
              p->memberCount = 0;
              SV_SendServerCommand(NULL, va("chat \"^1[PARTY] ^5%s ^7disbanded party '^3%s^7'!\"", cl->name, p->teamName));
            } else {
              for (int k = j; k < p->memberCount - 1; k++) {
                p->clientNums[k] = p->clientNums[k + 1];
              }
              p->memberCount--;
              SV_SendServerCommand(cl, "party_clear");
              SV_Ranked_UpdateParty(i);
              SV_SendServerCommand(svs.clients + i, va("chat \"^1[PARTY] ^5%s ^7has left your party.\"", cl->name));
              SV_SendServerCommand(cl, va("chat \"^2You left party '^5%s^2'.\"", p->teamName));
            }
            left = qtrue;
            break;
          }
        }
        if (left) break;
      }
    }
    if (!left) {
      SV_SendServerCommand(cl, "chat \"^1You are not currently in a party!\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!inviteparty") || !Q_stricmp(cmdSpace, "!ip")) {
    int clientNum = cl - svs.clients;
    rankedParty_t *p = &sv_rankedParties[clientNum];
    if (!p->active) {
      SV_SendServerCommand(cl, "chat \"^1You don't have an active party! Use ^3!createparty <Name>^1 first.\"");
      return qtrue;
    }
    if (p->memberCount >= MAX_PARTY_MEMBERS) {
      SV_SendServerCommand(cl, "chat \"^1Your party is full! (Max 6 members)\"");
      return qtrue;
    }

    int targetId = -1;
    if (sscanf(chatText, "%*s %d", &targetId) == 1 && targetId >= 0 && targetId < sv_maxclients->integer && svs.clients[targetId].state >= CS_ACTIVE) {
      sv_rankedPlayers[targetId].pendingPartyLeader = clientNum;
      SV_SendServerCommand(svs.clients + targetId, va("party_invite_req %d \"%s\" \"%s\"", clientNum, cl->name, p->teamName));
      SV_SendServerCommand(svs.clients + targetId, va("chat \"^3[PARTY INVITE] ^5%s ^7invited you to join team '^3%s^7'!\n^7Type ^2!acceptparty^7 or ^2!ap^7 to join!\"", cl->name, p->teamName));
      SV_SendServerCommand(cl, va("chat \"^2Invited ^5%s ^2to your party!\"", svs.clients[targetId].name));
    } else {
      SV_SendServerCommand(cl, "chat \"^3Usage: ^5!inviteparty <PlayerID>\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!acceptparty") || !Q_stricmp(cmdSpace, "!ap")) {
    int clientNum = cl - svs.clients;
    int leaderId = sv_rankedPlayers[clientNum].pendingPartyLeader;
    if (leaderId >= 0 && leaderId < sv_maxclients->integer && sv_rankedParties[leaderId].active) {

      rankedParty_t *p = &sv_rankedParties[leaderId];
      if (p->memberCount < MAX_PARTY_MEMBERS) {
        qboolean alreadyIn = qfalse;
        for (int j = 0; j < p->memberCount; j++) {
          if (p->clientNums[j] == clientNum) { alreadyIn = qtrue; break; }
        }
        if (!alreadyIn) {
          p->clientNums[p->memberCount++] = clientNum;
          sv_rankedPlayers[clientNum].pendingPartyLeader = -1;
          SV_Ranked_UpdateParty(leaderId);
          SV_SendServerCommand(cl, va("chat \"^2You joined party '^5%s^2'!\"", p->teamName));
          return qtrue;
        }
      } else {
        SV_SendServerCommand(cl, "chat \"^1That party is full!\"");
      }
    } else {
      SV_SendServerCommand(cl, "chat \"^7You don't have any pending party invites.\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!parties") || !Q_stricmp(cmdSpace, "!partylist")) {
    int activeCount = 0;
    SV_SendServerCommand(cl, "party_list_clear");
    SV_SendServerCommand(cl, "print \"\n^5--- ^2Active Server Parties ^5---\n\"");
    for (int i = 0; i < sv_maxclients->integer; i++) {
      rankedParty_t *p = &sv_rankedParties[i];
      if (p->active && svs.clients[i].state >= CS_ACTIVE) {
        const char *colorNames[8] = { "Blue", "Red", "Green", "Yellow", "Purple", "Orange", "Black", "White" };
        const char *colName = (p->teamColorIdx >= 0 && p->teamColorIdx < 8) ? colorNames[p->teamColorIdx] : "Blue";
        SV_SendServerCommand(cl, va("party_list_item %d \"%s\" %d %d \"%s\"",
                                    i, p->teamName, p->teamColorIdx, p->memberCount, svs.clients[i].name));
        SV_SendServerCommand(cl, va("print \"^3Party #%d: ^7'%s^7' | Leader: %s ^7| Members: ^2%d/%d ^7| Color: ^5%s\n\"",
                                    i, p->teamName, svs.clients[i].name, p->memberCount, MAX_PARTY_MEMBERS, colName));
        activeCount++;
      }
    }
    if (activeCount == 0) {
      SV_SendServerCommand(cl, "print \"^7No active parties right now. Type ^3!party ^7to create one!\n\"");
    }
    SV_SendServerCommand(cl, "print \"^5---------------------------------\n\n\"");
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!requestparty") || !Q_stricmp(cmdSpace, "!joinparty") || !Q_stricmp(cmdSpace, "!rp")) {
    const char *arg = strchr(chatText, ' ');
    if (arg && *(arg + 1) != '\0') {
      int leaderId = SV_Ranked_FindPlayerByNameOrId(arg + 1);
      if (leaderId >= 0 && leaderId < sv_maxclients->integer && sv_rankedParties[leaderId].active) {
        rankedParty_t *p = &sv_rankedParties[leaderId];
        if (p->memberCount >= MAX_PARTY_MEMBERS) {
          SV_SendServerCommand(cl, "chat \"^1That party is full!\"");
          return qtrue;
        }
        int clientNum = cl - svs.clients;
        sv_rankedPlayers[leaderId].pendingPartyJoinRequester = clientNum;
        // Notify Leader
        SV_SendServerCommand(svs.clients + leaderId, va("party_join_req %d \"%s\"", clientNum, cl->name));
        SV_SendServerCommand(svs.clients + leaderId, va("chat \"^3[PARTY] ^7%s has requested to join your party! Type ^2!acceptjoin %d\"", cl->name, clientNum));
        SV_SendServerCommand(svs.clients + leaderId, va("cp \"^3JOIN REQUEST:\n^7%s wants to join!\"", cl->name));
        SV_SendServerCommand(cl, va("chat \"^2Requested to join party '^5%s^2' (Leader: %s). Waiting for approval...\"", p->teamName, svs.clients[leaderId].name));
      } else {
        SV_SendServerCommand(cl, "chat \"^1Party or leader not found! Type ^3!parties ^1to view active teams.\"");
      }
    } else {
      SV_SendServerCommand(cl, "chat \"^3Usage: ^5!joinparty <LeaderName/ID>\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!acceptjoin") || !Q_stricmp(cmdSpace, "!aj")) {
    int clientNum = cl - svs.clients;
    rankedParty_t *p = &sv_rankedParties[clientNum];
    if (!p->active) {
      SV_SendServerCommand(cl, "chat \"^1You are not the leader of an active party!\"");
      return qtrue;
    }
    int targetId = sv_rankedPlayers[clientNum].pendingPartyJoinRequester;
    const char *arg = strchr(chatText, ' ');
    if (arg && *(arg + 1) != '\0') {
      targetId = SV_Ranked_FindPlayerByNameOrId(arg + 1);
    }
    if (targetId >= 0 && targetId < sv_maxclients->integer && svs.clients[targetId].state >= CS_ACTIVE) {
      if (p->memberCount < MAX_PARTY_MEMBERS) {
        qboolean alreadyIn = qfalse;
        for (int j = 0; j < p->memberCount; j++) {
          if (p->clientNums[j] == targetId) { alreadyIn = qtrue; break; }
        }
        if (!alreadyIn) {
          p->clientNums[p->memberCount++] = targetId;
          sv_rankedPlayers[clientNum].pendingPartyJoinRequester = -1;
          SV_SendServerCommand(cl, "party_join_req -1 \"\"");
          SV_Ranked_UpdateParty(clientNum);
          SV_SendServerCommand(svs.clients + targetId, va("chat \"^2You joined party '^5%s^2'!\"", p->teamName));
          SV_SendServerCommand(cl, va("chat \"^2Accepted ^5%s ^2into your party!\"", svs.clients[targetId].name));
          return qtrue;
        }
      } else {
        SV_SendServerCommand(cl, "chat \"^1Party is full!\"");
      }
    } else {
      SV_SendServerCommand(cl, "chat \"^7No pending join requests.\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!declineparty")) {
    int clientNum = cl - svs.clients;
    sv_rankedPlayers[clientNum].pendingPartyLeader = -1;
    SV_SendServerCommand(cl, "chat \"^1Party invite declined.\"");
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!startevent") || !Q_stricmp(cmdSpace, "!event")) {
    const qboolean isAdmin = SV_Ranked_IsAdmin(cl);
    if (!isAdmin) {
      SV_SendServerCommand(cl, "chat \"^1Only admins can start 3v3v3 Pit Events!\"");
      return qtrue;
    }
    SV_SendServerCommand(NULL, "cp \"^33v3v3 PIT BATTLE EVENT STARTED!\n^2Get ready with your parties!\"");
    SV_SendServerCommand(NULL, "chat \"^3[EVENT] ^23v3v3 Pit Battle Event initiated by ^5%s^2!\"", cl->name);
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!disbandparty") || !Q_stricmp(cmdSpace, "!leaveparty") || !Q_stricmp(cmdSpace, "!dp")) {
    int clientNum = cl - svs.clients;
    rankedParty_t *p = &sv_rankedParties[clientNum];
    if (p->active) {
      p->active = qfalse;
      SV_Ranked_UpdateParty(clientNum);
      SV_SendServerCommand(cl, "chat \"^1Party disbanded.\"");
    } else {
      for (int i = 0; i < svs.numSnapshotEntities; i++) {
        rankedParty_t *party = &sv_rankedParties[i];
        if (party->active) {
          for (int j = 0; j < party->memberCount; j++) {
            if (party->clientNums[j] == clientNum) {
              for (int k = j; k < party->memberCount - 1; k++) {
                party->clientNums[k] = party->clientNums[k + 1];
              }
              party->memberCount--;
              SV_Ranked_UpdateParty(i);
              SV_SendServerCommand(cl, "chat \"^1You left the party.\"");
              SV_SendServerCommand(svs.clients + clientNum, "party_clear");
              return qtrue;
            }
          }
        }
      }
      SV_SendServerCommand(cl, "chat \"^7You are not in a party.\"");
    }
    return qtrue;

  } else if (!Q_stricmp(cmdSpace, "!cmds") || !Q_stricmp(cmdSpace, "!help")) {



    const qboolean isAdmin = SV_Ranked_IsAdmin(cl);

    SV_SendServerCommand(cl, "print \"\n^5====== RANKED COMMANDS ======\n\"");

    // -- Account --
    SV_SendServerCommand(cl, "print \"\n^5[Account]\n\"");
    SV_SendServerCommand(cl, "print \"^2/login <user> <pass>    ^7Register or log in\n\"");
    SV_SendServerCommand(cl, "print \"^2/logout / !logout       ^7Log out\n\"");
    SV_SendServerCommand(cl, "print \"^5!changepassword/!passwd ^7Change password\n\"");
    SV_SendServerCommand(cl, "print \"^5!changeusername        ^7Change username\n\"");
    SV_SendServerCommand(cl, "print \"^3!details / !myinfo      ^7View account info\n\"");

    // -- Stats / Leaderboards --
    SV_SendServerCommand(cl, "print \"\n^5[Stats & Leaderboards]\n\"");
    SV_SendServerCommand(cl, "print \"^3!stats [name] / !info   ^7View ranked stats\n\"");
    SV_SendServerCommand(cl, "print \"^3!rank / !r              ^7View your rank title\n\"");
    SV_SendServerCommand(cl, "print \"^3!ranks                 ^7View all rank thresholds\n\"");
    SV_SendServerCommand(cl, "print \"^3!top / !t               ^7Top 5 by Elo\n\"");
    SV_SendServerCommand(cl, "print \"^3!topcredits / !topcr    ^7Top 5 Wealthiest\n\"");
    SV_SendServerCommand(cl, "print \"^3!toppotato              ^7Top 5 Hot Potato\n\"");
    SV_SendServerCommand(cl, "print \"^3!wanted / !w            ^7Top 5 by duel streak\n\"");

    // -- Quests & Achievements --
    SV_SendServerCommand(cl, "print \"\n^5[Quests & Achievements]\n\"");
    SV_SendServerCommand(cl, "print \"^3!quests                 ^7View daily quests\n\"");
    SV_SendServerCommand(cl, "print \"^5!achievements / !ach    ^7View achievements\n\"");

    // -- Economy --
    SV_SendServerCommand(cl, "print \"\n^5[Economy]\n\"");
    SV_SendServerCommand(cl, "print \"^6!credits / !cr          ^7Credits balance\n\"");
    SV_SendServerCommand(cl, "print \"^6!send <name> <amt> / !s ^7Send credits to player\n\"");
    SV_SendServerCommand(cl, "print \"^6!shop / !sh             ^7Credits shop\n\"");
    SV_SendServerCommand(cl, "print \"^6!inventory / !inv / !i  ^7Your item inventory\n\"");
    SV_SendServerCommand(cl, "print \"^6!buy <item>             ^7Buy from shop\n\"");
    SV_SendServerCommand(cl, "print \"^6!sell <item> / !sl      ^7Sell an item\n\"");
    SV_SendServerCommand(cl, "print \"^6!use <item> / !u        ^7Use an item\n\"");
    SV_SendServerCommand(cl, "print \"^6!setwinmsg <msg>        ^7Set custom win msg\n\"");

    // -- Bounty / Bet / Roll --
    SV_SendServerCommand(cl, "print \"\n^5[Bounties & Betting]\n\"");
    SV_SendServerCommand(cl, "print \"^1!bounty <name> <amt>    ^7Place a bounty\n\"");
    SV_SendServerCommand(cl, "print \"^1!bountylist / !bounties ^7All active bounties\n\"");
    SV_SendServerCommand(cl, "print \"^6!bet <name> <amt> / !b  ^7Bet on a duel\n\"");
    SV_SendServerCommand(cl, "print \"^6!roll / !rl             ^7Tiered Gamble (60s cd)\n\"");

    // -- Activities --
    SV_SendServerCommand(cl, "print \"\n^5[Activities]\n\"");
    SV_SendServerCommand(cl, "print \"^2!adventure / !adv       ^7Start a random adventure\n\"");
    SV_SendServerCommand(cl, "print \"^2!choose <n> / !c <n>    ^7Pick an adventure choice\n\"");
    SV_SendServerCommand(cl, "print \"^3#<answer>               ^7Answer active trivia question\n\"");
    SV_SendServerCommand(cl, "print \"^7!vote hotpotato/yes/no  ^7Start/Vote on Hot Potato Mode\n\"");
    // -- Admin --
    if (isAdmin) {
      SV_SendServerCommand(cl, "print \"\n^1[ADMIN_COMMANDS]\n\"");
      SV_SendServerCommand(cl, "print \"^1!cp all <msg>           ^7Centerprint broadcast\n\"");
      SV_SendServerCommand(cl, "print \"^1!cp <name/id> <msg>    ^7Centerprint to target\n\"");
      SV_SendServerCommand(cl, "print \"^1!forcepotato            ^7Force-start Hot Potato\n\"");
      SV_SendServerCommand(cl, "print \"^1!stoppotato             ^7Stop Hot Potato\n\"");
      SV_SendServerCommand(cl, "print \"^1!givecredits <name> <n> ^7Grant credits  (alias !gc)\n\"");
      SV_SendServerCommand(cl, "print \"^1!setelo <name> <n>      ^7Set player Elo\n\"");
      SV_SendServerCommand(cl, "print \"^1!setrank <name> <rank>  ^7Set player rank title\n\"");
      SV_SendServerCommand(cl, "print \"^1!bring <name>           ^7Teleport player in front of you (Level 1 Admin)\n\"");
      SV_SendServerCommand(cl, "print \"^1!goto <name>            ^7Teleport to player (Level 1 Admin)\n\"");
      SV_SendServerCommand(cl, "print \"^1!jail <name> <min>      ^7Put on probation\n\"");
      SV_SendServerCommand(cl, "print \"^1!unjail <name>          ^7Clear probation\n\"");
    }

    SV_SendServerCommand(cl, "print \"^5=============================\n\n\"");
    return qtrue;
  }

  return qfalse;
}
