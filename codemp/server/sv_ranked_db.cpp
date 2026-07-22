#include "sv_ranked_db.h"
#include "cJSON.h"
#include "game/bg_public.h"
#include "qcommon/qcommon.h"
#include "sv_ranked_logic.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <string>
#include <vector>

// NOTE: This ranked system extends the classic JKA MAX_CLIENTS (often 32)
// to 64. The rest of this codebase already uses 64 for ranked state arrays, so
// keep the definition consistent here to avoid ODR/link issues.
static const int RANKED_MAX_CLIENTS = 64;

// -----------------------------------------------------------------------------
// Persistent JSON databases (stored on disk via FS_* in the current game dir)
// -----------------------------------------------------------------------------
static const char *RANKED_ACCOUNTS_PATH = "ranked/accounts.json";
static const char *RANKED_CONFIG_PATH = "ranked/config.json";
static const char *RANKED_LOG_PATH = "ranked/ranked.log";

typedef struct shopItem_s {
  const char *key;
  const char *display;
  int price;
  int sellBack;
} shopItem_t;

static const shopItem_t sv_shopItems[] = {
    {"xp_boost", "^5XP Boost         ^7(+50% XP for 1 round)", 500, 200},
    {"cr_boost", "^5Credit Boost      ^7(+50% credits for 1 round)", 400, 150},
    {"lucky_charm", "^5Lucky Charm       ^7(+10% roll luck)", 300, 100},
    {"yoda_scroll", "^2Yoda's Scroll     ^7(wisdom from Master Yoda)", 800,
     200},
    {"jedi_holocron", "^6Jedi Holocron     ^7(secrets of the Light Side)", 600,
     150},
    {"sith_holocron", "^1Sith Holocron     ^7(secrets of the Dark Side)", 600,
     150},
    {"jedi_manual", "^3Jedi Manual       ^7(combat training insight)", 400,
     100},
    {"elo_boost", "^5Elo Boost         ^7(+15% FR for next duel win)", 1200,
     500},
    {"jedaii_secret", "^5Jedaii Secret     ^7(forbidden knowledge...)", 1000000,
     0},
    {"win_msg", "^5Custom Win Msg    ^7(Unlock !setwinmsg)", 20000, 0},
    {"wp_pistol", "^5Bryar Pistol     ^7(Equip Bryar Pistol)", 1000, 400},
    {"wp_blaster", "^5E-11 Blaster     ^7(Equip Blaster Rifle)", 1500, 600},
    {"wp_disruptor", "^5Disruptor Rifle  ^7(Equip Disruptor)", 2500, 1000},
    {"wp_bowcaster", "^5Bowcaster        ^7(Equip Bowcaster)", 2000, 800},
    {"wp_repeater", "^5Heavy Repeater   ^7(Equip Heavy Repeater)", 2500, 1000},
    {"wp_demp2", "^5DEMP2            ^7(Equip DEMP2 Pistol)", 2000, 800},
    {"wp_flechette", "^5Flechette        ^7(Equip Golan Flechette)", 3000, 1200},
    {"wp_rocket", "^5Rocket Launcher  ^7(Equip Rocket Launcher)", 5000, 2000},
    {"wp_concussion", "^5Concussion       ^7(Equip Concussion Rifle)", 4000, 1600},
    {NULL, NULL, 0, 0}};

cJSON *accountsDB = NULL; // exported (referenced by sv_ranked_logic/cmds)
static cJSON *rankedConfig = NULL;

rankedMatchState_t sv_rankedPlayers[RANKED_MAX_CLIENTS];

// Provided by sv_ranked_cmds.cpp (used for admin-only DB commands)
extern qboolean SV_Ranked_IsAdmin(client_t *cl);

// -----------------------------------------------------------------------------
// Persistent Logging
// -----------------------------------------------------------------------------
void SV_Ranked_Log(const char *fmt, ...) {
  char msg[1024];
  va_list argptr;
  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  // Timestamp
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char timeBuf[32];
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", t);

  // Build log line
  char logLine[1100];
  Com_sprintf(logLine, sizeof(logLine), "[%s] %s\n", timeBuf, msg);

  // Append to ranked/ranked.log
  fileHandle_t f;
  FS_FOpenFileByMode(RANKED_LOG_PATH, &f, FS_APPEND);
  if (f) {
    FS_Write(logLine, (int)strlen(logLine), f);
    FS_FCloseFile(f);
  }

  // Mirror to server console with [RANKED] prefix — only if debug is enabled
  // to avoid spamming the main console (e.g. database save messages)
  if (Cvar_VariableIntegerValue("sv_ranked_debug")) {
    Com_Printf("[RANKED] %s\n", msg);
  }
}

void SV_Ranked_DebugLog(const char *fmt, ...) {
  if (!Cvar_VariableIntegerValue("sv_ranked_debug")) return;
  char msg[1024];
  va_list argptr;
  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);
  Com_Printf("[RANKED-DEBUG] %s\n", msg);
}

static void Ranked_ToLower(char *s) {
  if (!s)
    return;
  for (; *s; ++s) {
    *s = (char)tolower((unsigned char)*s);
  }
}

void Ranked_SafeUserKey(const char *username, char *out, int outSize) {
  if (!out || outSize <= 0)
    return;
  out[0] = '\0';
  if (!username || !username[0])
    return;

  Q_strncpyz(out, username, outSize);
  Ranked_ToLower(out);

  // Strip spaces (usernames should be key-friendly)
  for (int i = 0; out[i]; ++i) {
    if ((unsigned char)out[i] <= ' ')
      out[i] = '_';
  }
}

static cJSON *Ranked_ParseOrEmptyObject(const char *jsonText, const char *tag) {
  if (!jsonText || !jsonText[0]) {
    return cJSON_CreateObject();
  }
  cJSON *root = cJSON_Parse(jsonText);
  if (!root || !cJSON_IsObject(root)) {
    Com_Printf("[RANKED] %s: invalid JSON, starting new DB.\n", tag);
    if (root)
      cJSON_Delete(root);
    return cJSON_CreateObject();
  }
  return root;
}

static void Ranked_WriteJSON(const char *path, cJSON *root, const char *tag) {
  if (!root)
    return;
  char *printed = cJSON_Print(root);
  if (!printed) {
    Com_Printf("[RANKED] %s: cJSON_Print failed, not writing '%s'\n", tag,
               path);
    return;
  }
  FS_WriteFile(path, printed, (int)strlen(printed));
  cJSON_free(printed);
}

static void Ranked_EnsureDefaultConfig(void) {
  if (!rankedConfig) {
    rankedConfig = cJSON_CreateObject();
  }

  // -------------------------------------------------------------------------
  // _help: Human-readable descriptions for every config key.
  // This object is written to ranked/config.json so server admins can
  // understand what each field does without reading source code.
  // -------------------------------------------------------------------------
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "_help")) {
    cJSON *help = cJSON_CreateObject();
    cJSON_AddStringToObject(
        help, "enabled",
        "1 = ranked system active, 0 = fully disabled (no XP/ELO/commands). "
        "Also controllable at runtime via Cvar sv_ranked_enabled.");
    cJSON_AddStringToObject(
        help, "rank_titles",
        "Array of colored rank title strings, indexed by rank_thresholds. "
        "Uses JKA color codes (^1=red, ^2=green, ^3=yellow, ^5=cyan, "
        "^6=orange).");
    cJSON_AddStringToObject(help, "rank_thresholds",
                            "FR (ELO) values required to reach each rank. Must "
                            "have same length as rank_titles.");
    cJSON_AddStringToObject(help, "elo_k_base",
                            "Base K-factor for ELO calculation. Higher = more "
                            "volatile FR changes. Default: 32.");
    cJSON_AddStringToObject(help, "elo_k_new_player",
                            "K-factor used for players with fewer than "
                            "elo_games_for_new games played. Default: 48.");
    cJSON_AddStringToObject(help, "elo_k_low_elo",
                            "K-factor used for players below elo_low_thresh "
                            "FR. Protects low-ranked players. Default: 16.");
    cJSON_AddStringToObject(help, "elo_games_for_new",
                            "Number of games before a player is no longer "
                            "considered 'new'. Default: 10.");
    cJSON_AddStringToObject(help, "elo_low_thresh",
                            "FR value below which elo_k_low_elo is used "
                            "instead of elo_k_base. Default: 800.");
    cJSON_AddStringToObject(
        help, "elo_diff_cap",
        "Maximum FR difference used in ELO expected-score calculation. "
        "Prevents extreme swings. Default: 600.");
    cJSON_AddStringToObject(
        help, "elo_max_swing",
        "Hard cap on FR gained or lost per single duel. Default: 50.");
    cJSON_AddStringToObject(
        help, "duel_win_credits",
        "Credits awarded to the winner of a ranked duel. Default: 25.");
    cJSON_AddStringToObject(help, "duel_loss_credits",
                            "Credits awarded as consolation to the loser of a "
                            "ranked duel. Default: 10.");
    cJSON_AddStringToObject(help, "roll_cooldown_ms",
                            "Cooldown in milliseconds between uses of the "
                            "!roll command. Default: 60000 (60 sec).");
    cJSON_AddStringToObject(help, "lucky_charm_bonus",
                            "Extra percentage points added to roll luck when "
                            "lucky_charm item is active. Default: 10.");
    cJSON_AddStringToObject(help, "xp_per_kill", "XP awarded on a standard player kill. Default: 10.");
    cJSON_AddStringToObject(help, "credits_per_kill", "Credits awarded on a standard player kill. Default: 1.");
    cJSON_AddStringToObject(help, "xp_per_duel_win", "XP awarded to the winner of a ranked duel. Default: 50.");
    cJSON_AddStringToObject(help, "credits_per_team_win", "Credits awarded to winning team members at round end. Default: 20.");
    cJSON_AddStringToObject(help, "credits_per_promotion", "Credits awarded when promoting to a new rank title. Default: 500.");
    cJSON_AddStringToObject(help, "quest_deathless_credits", "Credits awarded for completing the Deathless quest. Default: 100.");
    cJSON_AddStringToObject(help, "streak_bounty_factor", "Base credits added to wanted bounty per streak tier. Default: 50.");
    cJSON_AddStringToObject(help, "streak_credits_factor", "Base credits awarded on streak milestones per tier. Default: 25.");
    cJSON_AddStringToObject(help, "streak_xp_factor", "Base XP awarded on streak milestones per tier. Default: 50.");
    cJSON_AddStringToObject(help, "bet_min_amount", "Minimum credit amount allowed for placing a duel bet. Default: 1.");
    cJSON_AddStringToObject(help, "bet_max_amount", "Maximum credit amount allowed for placing a duel bet. Default: 10000.");
    cJSON_AddStringToObject(help, "shop_prices", "Object mapping shop item keys to their purchase price in credits.");
    cJSON_AddStringToObject(help, "shop_sellback", "Object mapping shop item keys to their sell-back value in credits.");
    cJSON_AddItemToObject(rankedConfig, "_help", help);
  }

  // Global enabled flag — synced with sv_ranked_enabled Cvar on load/save.
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "enabled")) {
    cJSON_AddNumberToObject(rankedConfig, "enabled", 1);
  }

  // Rank titles (configurable)
  // Used by SV_Ranked_GetTitle(fr). Keep arrays aligned by index.
  // If you already have these keys in ranked/config.json, they will be
  // preserved.
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_titles")) {
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateString("^3Youngling"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("^2Padawan"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("^5Jedi Knight"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("^6Jedi Master"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("^1Grand Master"));
    cJSON_AddItemToObject(rankedConfig, "rank_titles", arr);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_thresholds")) {
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(1500));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(2000));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(2500));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(3000));
    cJSON_AddItemToObject(rankedConfig, "rank_thresholds", arr);
  }

  // Elo tuning
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_k_base")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_k_base", 32);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_k_new_player")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_k_new_player", 48);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_k_low_elo")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_k_low_elo", 16);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_games_for_new")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_games_for_new", 10);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_low_thresh")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_low_thresh", 800);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_diff_cap")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_diff_cap", 600);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "elo_max_swing")) {
    cJSON_AddNumberToObject(rankedConfig, "elo_max_swing", 50);
  }

  // Duel economy
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "duel_win_credits")) {
    cJSON_AddNumberToObject(rankedConfig, "duel_win_credits", 25);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "duel_loss_credits")) {
    cJSON_AddNumberToObject(rankedConfig, "duel_loss_credits", 10);
  }

  // Roll / Lucky charm
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "roll_cooldown_ms")) {
    cJSON_AddNumberToObject(rankedConfig, "roll_cooldown_ms", 60000);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "lucky_charm_bonus")) {
    cJSON_AddNumberToObject(rankedConfig, "lucky_charm_bonus", 10);
  }

  // Leveling
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "xp_per_level")) {
    cJSON_AddNumberToObject(rankedConfig, "xp_per_level", 1000);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "xp_per_kill")) {
    cJSON_AddNumberToObject(rankedConfig, "xp_per_kill", 10);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "credits_per_kill")) {
    cJSON_AddNumberToObject(rankedConfig, "credits_per_kill", 1);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "xp_per_duel_win")) {
    cJSON_AddNumberToObject(rankedConfig, "xp_per_duel_win", 50);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "credits_per_team_win")) {
    cJSON_AddNumberToObject(rankedConfig, "credits_per_team_win", 20);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "credits_per_promotion")) {
    cJSON_AddNumberToObject(rankedConfig, "credits_per_promotion", 500);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "quest_deathless_credits")) {
    cJSON_AddNumberToObject(rankedConfig, "quest_deathless_credits", 100);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "streak_bounty_factor")) {
    cJSON_AddNumberToObject(rankedConfig, "streak_bounty_factor", 50);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "streak_credits_factor")) {
    cJSON_AddNumberToObject(rankedConfig, "streak_credits_factor", 25);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "streak_xp_factor")) {
    cJSON_AddNumberToObject(rankedConfig, "streak_xp_factor", 50);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "bet_min_amount")) {
    cJSON_AddNumberToObject(rankedConfig, "bet_min_amount", 1);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "bet_max_amount")) {
    cJSON_AddNumberToObject(rankedConfig, "bet_max_amount", 10000);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "shop_prices")) {
    cJSON *pricesObj = cJSON_CreateObject();
    for (int i = 0; sv_shopItems[i].key != NULL; i++) {
      cJSON_AddNumberToObject(pricesObj, sv_shopItems[i].key, sv_shopItems[i].price);
    }
    cJSON_AddItemToObject(rankedConfig, "shop_prices", pricesObj);
  }
  if (!cJSON_GetObjectItemCaseSensitive(rankedConfig, "shop_sellback")) {
    cJSON *sellbackObj = cJSON_CreateObject();
    for (int i = 0; sv_shopItems[i].key != NULL; i++) {
      cJSON_AddNumberToObject(sellbackObj, sv_shopItems[i].key, sv_shopItems[i].sellBack);
    }
    cJSON_AddItemToObject(rankedConfig, "shop_sellback", sellbackObj);
  }
  if (cJSON_GetObjectItemCaseSensitive(rankedConfig, "_help")) {
    cJSON *h = cJSON_GetObjectItemCaseSensitive(rankedConfig, "_help");
    if (!cJSON_GetObjectItemCaseSensitive(h, "xp_per_level"))
      cJSON_AddStringToObject(h, "xp_per_level",
        "XP required per level. Default: 1000. E.g. 500 = faster leveling.");
  }
}

// -----------------------------------------------------------------------------
// Core DB API
// -----------------------------------------------------------------------------
cJSON *SV_Ranked_GetAccount(const char *username) {
  if (!accountsDB || !username || !username[0])
    return NULL;
  char key[MAX_AUTH_STRING];
  Ranked_SafeUserKey(username, key, sizeof(key));
  return cJSON_GetObjectItemCaseSensitive(accountsDB, key);
}

cJSON *SV_Ranked_GetAccountByGUID(const char *guid) {
  if (!accountsDB || !guid || !guid[0])
    return NULL;
  for (cJSON *child = accountsDB->child; child; child = child->next) {
    cJSON *eg = cJSON_GetObjectItemCaseSensitive(child, "engine_guid");
    if (eg && cJSON_IsString(eg) && eg->valuestring &&
        !Q_stricmp(eg->valuestring, guid)) {
      return child;
    }
    cJSON *g = cJSON_GetObjectItemCaseSensitive(child, "guid");
    if (g && cJSON_IsString(g) && g->valuestring &&
        !Q_stricmp(g->valuestring, guid)) {
      return child;
    }
  }
  return NULL;
}

void SV_Ranked_GenerateRandomUsername(char *out, int size) {
  if (!out || size <= 0)
    return;
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  int n = size - 1;
  if (n > 12)
    n = 12;
  Q_strncpyz(out, "user_", size);
  int baseLen = (int)strlen(out);
  for (int i = baseLen; i < n; ++i) {
    out[i] = alphabet[rand() % (int)(sizeof(alphabet) - 1)];
  }
  out[n] = '\0';
}

void SV_Ranked_GenerateRandomPassword(char *out, int size) {
  if (!out || size <= 0)
    return;
  static const char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int n = size - 1;
  if (n > 14)
    n = 14;
  for (int i = 0; i < n; ++i) {
    out[i] = alphabet[rand() % (int)(sizeof(alphabet) - 1)];
  }
  out[n] = '\0';
}

void SV_Ranked_GenerateUUID(char *out, int size) {
    if (!out || size < 37) return;
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            out[i] = '-';
        } else if (i == 14) {
            out[i] = '4'; // UUID v4 spec
        } else if (i == 19) {
            out[i] = hex[(rand() % 4) + 8]; // UUID v4 spec (8, 9, a, b)
        } else {
            out[i] = hex[rand() % 16];
        }
    }
    out[36] = '\0';
}

const char *SV_Ranked_GetActiveMode(void) {
  // Use g_Authenticity cvar to split persistent ranking buckets (MBII
  // standard).
  int auth = (int)Cvar_VariableValue("g_Authenticity");

  if (auth == 3) {
    return "duel";
  }

  // Fallback to gametype check if authenticity is not set or not duel
  if (sv_gametype && (sv_gametype->integer == GT_DUEL ||
                      sv_gametype->integer == GT_POWERDUEL)) {
    return "duel";
  }

  return "open";
}

void SV_Ranked_LoadAccounts(void) {
  void *buf = NULL;
  long len = FS_ReadFile(RANKED_ACCOUNTS_PATH, &buf);
  if (len <= 0 || !buf) {
    if (accountsDB)
      cJSON_Delete(accountsDB);
    accountsDB = cJSON_CreateObject();
    return;
  }

  cJSON *root = Ranked_ParseOrEmptyObject((const char *)buf, "accounts");
  FS_FreeFile(buf);

  if (accountsDB)
    cJSON_Delete(accountsDB);
  accountsDB = root;
}

// Forward declaration — defined later in this file
void SV_Ranked_SyncTopLevelFields(cJSON *acc);

void SV_Ranked_SaveAccounts(void) {
  if (!accountsDB)
    return;

  // Sync all accounts before saving to ensure the JSON matches SQL schema for external tools
  for (cJSON *acc = accountsDB->child; acc; acc = acc->next) {
    SV_Ranked_SyncTopLevelFields(acc);
  }

  Ranked_WriteJSON(RANKED_ACCOUNTS_PATH, accountsDB, "accounts");
  if (Cvar_VariableIntegerValue("sv_ranked_debug")) {
    SV_Ranked_Log("DATABASE: Saved %d accounts to accounts.json", cJSON_GetArraySize(accountsDB));
  }
}

void SV_Ranked_LoadConfig(void) {
  void *buf = NULL;
  long len = FS_ReadFile(RANKED_CONFIG_PATH, &buf);
  if (len <= 0 || !buf) {
    if (rankedConfig)
      cJSON_Delete(rankedConfig);
    rankedConfig = cJSON_CreateObject();
    Ranked_EnsureDefaultConfig();
    SV_Ranked_SaveConfig();
    // Sync Cvar to default (1 = enabled)
    Cvar_Set("sv_ranked_enabled", "1");
    return;
  }

  cJSON *root = Ranked_ParseOrEmptyObject((const char *)buf, "config");
  FS_FreeFile(buf);

  if (rankedConfig)
    cJSON_Delete(rankedConfig);
  rankedConfig = root;
  Ranked_EnsureDefaultConfig();

  // Sync Cvar from config.json "enabled" key
  cJSON *enabledKey = cJSON_GetObjectItemCaseSensitive(rankedConfig, "enabled");
  if (enabledKey && cJSON_IsNumber(enabledKey)) {
    Cvar_Set("sv_ranked_enabled", enabledKey->valueint ? "1" : "0");
    Com_Printf("[RANKED] sv_ranked_enabled set to %d (from config.json)\n",
               enabledKey->valueint);
  }
}

void SV_Ranked_SaveConfig(void) {
  Ranked_EnsureDefaultConfig();

  // Sync sv_ranked_enabled Cvar -> config.json "enabled" key before saving
  cJSON *enabledKey = cJSON_GetObjectItemCaseSensitive(rankedConfig, "enabled");
  int cvEnabled = (int)Cvar_VariableValue("sv_ranked_enabled");
  if (enabledKey)
    cJSON_SetNumberValue(enabledKey, cvEnabled);
  else
    cJSON_AddNumberToObject(rankedConfig, "enabled", cvEnabled);

  // Sync active_mode so external APIs can track it
  cJSON *modeKey =
      cJSON_GetObjectItemCaseSensitive(rankedConfig, "active_mode");
  const char *currentMode = SV_Ranked_GetActiveMode();
  if (modeKey)
    cJSON_SetValuestring(modeKey, currentMode);
  else
    cJSON_AddStringToObject(rankedConfig, "active_mode", currentMode);

  Ranked_WriteJSON(RANKED_CONFIG_PATH, rankedConfig, "config");
}

cJSON *SV_Ranked_GetSetting(const char *key) {
  if (!rankedConfig || !key || !key[0])
    return NULL;
  return cJSON_GetObjectItemCaseSensitive(rankedConfig, key);
}

int SV_Ranked_GetShopPrice(const char *itemKey, int defaultPrice) {
  cJSON *shopPrices = SV_Ranked_GetSetting("shop_prices");
  if (shopPrices) {
    cJSON *pricePtr = cJSON_GetObjectItemCaseSensitive(shopPrices, itemKey);
    if (pricePtr && cJSON_IsNumber(pricePtr)) {
      return pricePtr->valueint;
    }
  }
  return defaultPrice;
}

int SV_Ranked_GetShopSellBack(const char *itemKey, int defaultSellBack) {
  cJSON *shopSellBack = SV_Ranked_GetSetting("shop_sellback");
  if (shopSellBack) {
    cJSON *sellBackPtr = cJSON_GetObjectItemCaseSensitive(shopSellBack, itemKey);
    if (sellBackPtr && cJSON_IsNumber(sellBackPtr)) {
      return sellBackPtr->valueint;
    }
  }
  return defaultSellBack;
}

void SV_Ranked_Init(void) {
  srand((unsigned int)time(NULL));
  Com_Memset(sv_rankedPlayers, 0, sizeof(sv_rankedPlayers));
  for (int i = 0; i < RANKED_MAX_CLIENTS; ++i) {
    sv_rankedPlayers[i].duelOpponent = -1;
    sv_rankedPlayers[i].currentBetTarget = -1;
  }

  SV_Ranked_LoadAccounts();
  SV_Ranked_LoadConfig();
  SV_Ranked_Logic_Init();
  SV_Ranked_SaveConfig(); // Force save to update active_mode for API

  Cvar_Get("sv_ranked_debug", "0", CVAR_ARCHIVE);

  Com_Printf("[RANKED] DB initialized.\n");
}

void SV_Ranked_Shutdown(void) {
  SV_Ranked_SaveAccounts();
  SV_Ranked_SaveConfig();

  SV_Ranked_Logic_Shutdown();

  if (accountsDB) {
    cJSON_Delete(accountsDB);
    accountsDB = NULL;
  }
  if (rankedConfig) {
    cJSON_Delete(rankedConfig);
    rankedConfig = NULL;
  }
  Com_Printf("[RANKED] DB shutdown.\n");
}

// -----------------------------------------------------------------------------
// Authentication / session state
// -----------------------------------------------------------------------------
qboolean Ranked_IsValidAuthString(const char *s) {
  if (!s || !s[0])
    return qfalse;
  // Allow a-z 0-9 _ . - (no spaces)
  for (const char *p = s; *p; ++p) {
    char c = *p;
    if ((unsigned char)c <= ' ')
      return qfalse;
    if (isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')
      continue;
    return qfalse;
  }
  return qtrue;
}

static cJSON *Ranked_EnsureModeData(cJSON *acc, const char *mode) {
  if (!acc || !mode || !mode[0])
    return NULL;

  cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  if (!modesObj) {
    modesObj = cJSON_CreateObject();
    cJSON_AddItemToObject(acc, "modes", modesObj);
  }

  cJSON *modeData = cJSON_GetObjectItemCaseSensitive(modesObj, mode);
  if (!modeData) {
    modeData = cJSON_CreateObject();
    cJSON_AddNumberToObject(modeData, "elo", 1000);
    cJSON_AddNumberToObject(modeData, "wins", 0);
    cJSON_AddNumberToObject(modeData, "losses", 0);
    cJSON_AddNumberToObject(modeData, "kills", 0);
    cJSON_AddNumberToObject(modeData, "deaths", 0);
    cJSON_AddItemToObject(modesObj, mode, modeData);
  }
  return modeData;
}

void SV_Ranked_LoginOrRegister(client_t *cl, const char *username,
                               const char *password) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);

  if (!Ranked_IsValidAuthString(username) ||
      !Ranked_IsValidAuthString(password)) {
    SV_SendServerCommand(cl, "chat \"^1Invalid username/password. Use only "
                             "letters/numbers and _.-\"");
    return;
  }

  if (!accountsDB) {
    SV_Ranked_LoadAccounts();
  }

  char key[MAX_AUTH_STRING];
  Ranked_SafeUserKey(username, key, sizeof(key));

  cJSON *acc = cJSON_GetObjectItemCaseSensitive(accountsDB, key);
  if (!acc) {
    // Register
    acc = cJSON_CreateObject();
    cJSON_AddStringToObject(acc, "username", key);
    cJSON_AddStringToObject(acc, "password", password);
    cJSON_AddNumberToObject(acc, "xp", 0);
    cJSON_AddNumberToObject(acc, "credits", 0);
    cJSON_AddNumberToObject(acc, "max_potato_ticks", 0);

    const char *guid = Info_ValueForKey(cl->userinfo, "cl_guid");
    if (guid && guid[0])
      cJSON_AddStringToObject(acc, "guid", guid);

    // Track player's colored name (for nicer prints)
    if (cl->name && cl->name[0])
      cJSON_AddStringToObject(acc, "displayName", cl->name);

    Ranked_EnsureModeData(acc, SV_Ranked_GetActiveMode());
    cJSON_AddItemToObject(accountsDB, key, acc);

    SV_SendServerCommand(cl, "chat \"^2Registered account ^7%s^2. Logged in.\"",
                         key);
  } else {
    // Login
    cJSON *pw = cJSON_GetObjectItemCaseSensitive(acc, "password");
    if (!pw || !cJSON_IsString(pw) || !pw->valuestring ||
        Q_stricmp(pw->valuestring, password) != 0) {
      SV_SendServerCommand(cl, "chat \"^1Incorrect password.\"");
      return;
    }

    // keep displayName up to date
    if (cl->name && cl->name[0]) {
      cJSON *disp = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
      if (!disp) {
        cJSON_AddStringToObject(acc, "displayName", cl->name);
      } else if (cJSON_IsString(disp) && disp->valuestring &&
                 strcmp(disp->valuestring, cl->name) != 0) {
        cJSON_ReplaceItemInObject(acc, "displayName",
                                  cJSON_CreateString(cl->name));
      }
    }

    // Auto-update GUID if it changed
    const char *curGuid = Info_ValueForKey(cl->userinfo, "ja_guid");
    if (!curGuid || !curGuid[0])
      curGuid = Info_ValueForKey(cl->userinfo, "cl_guid");
    if (curGuid && curGuid[0]) {
      cJSON *gPtr = cJSON_GetObjectItemCaseSensitive(acc, "guid");
      if (!gPtr) {
        cJSON_AddStringToObject(acc, "guid", curGuid);
      } else if (cJSON_IsString(gPtr) && gPtr->valuestring &&
                 Q_stricmp(gPtr->valuestring, curGuid) != 0) {
        cJSON_ReplaceItemInObject(acc, "guid", cJSON_CreateString(curGuid));
        SV_SendServerCommand(cl, "print \"^5[RANKED] ^7GUID updated for this account.\n\"");
        SV_Ranked_Log("GUID-UPDATE: user='%s' newGUID='%s'", key, curGuid);
      }
    }

    Ranked_EnsureModeData(acc, SV_Ranked_GetActiveMode());
    SV_SendServerCommand(cl, "chat \"^2Logged in as ^7%s^2.\"", key);
  }

  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  r->loggedIn = qtrue;
  r->isTemp = qfalse;
  Q_strncpyz(r->username, key, sizeof(r->username));
  SV_Ranked_SyncClientRPG(cl);

  cJSON *modes = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  cJSON *duel = modes ? cJSON_GetObjectItemCaseSensitive(modes, "duel") : NULL;
  cJSON *eloPtr = duel ? cJSON_GetObjectItemCaseSensitive(duel, "elo") : NULL;
  r->tempElo = eloPtr ? eloPtr->valueint : 1000;

  SV_Ranked_Log("%s: user='%s' ip='%s'", acc ? "LOGIN" : "REGISTER", key,
                Info_ValueForKey(cl->userinfo, "ip"));

  SV_Ranked_SaveAccounts();
}

void SV_Ranked_Logout(client_t *cl) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You are not logged in.\"");
    return;
  }
  r->loggedIn = qfalse;
  r->isTemp = qtrue;
  r->username[0] = '\0';

  SV_Ranked_Log("LOGOUT: user='%s' clientNum=%d", r->username, clientNum);
  SV_SendServerCommand(cl, "chat \"^2Logged out.\"");
}

void SV_Ranked_ClientConnect(client_t *cl) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  Com_Memset(r, 0, sizeof(*r));
  r->loggedIn = qfalse;
  r->isTemp = qtrue;
  r->tempElo = 1000;
  r->duelOpponent = -1;
  r->currentBetTarget = -1;
  r->adventureNodeIdx = -1;
  r->adventureCooldownEnd = 0;

  // Attempt GUID-based auto registration/login
  SV_Ranked_AutoRegisterByGUID(cl);
}

void SV_Ranked_ClientDisconnect(client_t *cl) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  // If in a duel, penalize for combat logging
  SV_Ranked_DuelDisconnectCheck(clientNum);

  if (r->loggedIn && r->username[0]) {
    SV_Ranked_Log("DISCONNECT: user='%s' clientNum=%d", r->username, clientNum);
  }

  r->loggedIn = qfalse;
  r->isTemp = qtrue;
  r->inDuel = qfalse;
  r->duelOpponent = -1;
  r->username[0] = '\0';

  // Persist any last-minute DB changes
  SV_Ranked_SaveAccounts();
}

// -----------------------------------------------------------------------------
// GUID Auto-Registration
// -----------------------------------------------------------------------------
void SV_Ranked_AutoRegisterByGUID(client_t *cl) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  // Already logged in — skip
  if (r->loggedIn)
    return;

  // Attempt to find a GUID (check both cl_guid and ja_guid for MB2
  // compatibility)
  const char *guid = Info_ValueForKey(cl->userinfo, "ja_guid");
  if (!guid || !guid[0]) {
    guid = Info_ValueForKey(cl->userinfo, "cl_guid");
  }

  if (!guid || !guid[0]) {
    SV_SendServerCommand(cl, "print \"^5[RANKED] ^3No GUID detected. Login "
                             "manually: /login <user> <pass>\n\"");
    return;
  }

  if (!accountsDB)
    SV_Ranked_LoadAccounts();

  // Look up existing account by GUID
  cJSON *acc = SV_Ranked_GetAccountByGUID(guid);
  if (acc) {
    // --- AUTO-LOGIN ---
    cJSON *uPtr = cJSON_GetObjectItemCaseSensitive(acc, "username");
    if (!uPtr || !uPtr->valuestring)
      return;

    Q_strncpyz(r->username, uPtr->valuestring, sizeof(r->username));
    r->loggedIn = qtrue;
    r->isTemp = qfalse;

    cJSON *modes = cJSON_GetObjectItemCaseSensitive(acc, "modes");
    cJSON *duel = modes ? cJSON_GetObjectItemCaseSensitive(modes, "duel") : NULL;
    cJSON *eloPtr = duel ? cJSON_GetObjectItemCaseSensitive(duel, "elo") : NULL;
    r->tempElo = eloPtr ? eloPtr->valueint : 1000;

    // Keep displayName in sync
    if (cl->name && cl->name[0]) {
      cJSON *disp = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
      if (!disp)
        cJSON_AddStringToObject(acc, "displayName", cl->name);
      else if (strcmp(disp->valuestring, cl->name) != 0)
        cJSON_ReplaceItemInObject(acc, "displayName",
                                  cJSON_CreateString(cl->name));
    }

    Ranked_EnsureModeData(acc, SV_Ranked_GetActiveMode());
    SV_Ranked_SaveAccounts();

    SV_SendServerCommand(cl,
                         "print \"^5[RANKED] ^2Welcome back, ^7%s^2! "
                         "Auto-logged in via GUID.\n\"",
                         r->username);
    SV_Ranked_Log("AUTO-LOGIN: user='%s' GUID='%s' ip='%s'", r->username, guid,
                  Info_ValueForKey(cl->userinfo, "ip"));
    SV_Ranked_CheckAndRefreshDailyQuests(r->username, cl);
    SV_Ranked_SyncClientRPG(cl);

  } else {
    // --- AUTO-REGISTER (new GUID) ---
    char newUser[MAX_AUTH_STRING];
    char newPass[MAX_AUTH_STRING];

    // Generate unique username (ranked_XXXX style)
    do {
      Q_strncpyz(newUser, "ranked_", sizeof(newUser));
      static const char alpha[] = "abcdefghijklmnopqrstuvwxyz0123456789";
      for (int i = 7; i < 11; ++i)
        newUser[i] = alpha[rand() % (int)(sizeof(alpha) - 1)];
      newUser[11] = '\0';
    } while (SV_Ranked_GetAccount(newUser)); // ensure uniqueness

    SV_Ranked_GenerateRandomPassword(newPass, sizeof(newPass));

    cJSON *newAcc = cJSON_CreateObject();
    cJSON_AddStringToObject(newAcc, "username", newUser);
    cJSON_AddStringToObject(newAcc, "password", newPass);
    cJSON_AddStringToObject(newAcc, "engine_guid", guid);
    cJSON_AddNumberToObject(newAcc, "xp", 0);
    cJSON_AddNumberToObject(newAcc, "credits", 0);
    cJSON_AddNumberToObject(newAcc, "max_potato_ticks", 0);
    if (cl->name && cl->name[0])
      cJSON_AddStringToObject(newAcc, "displayName", cl->name);
    Ranked_EnsureModeData(newAcc, SV_Ranked_GetActiveMode());
    cJSON_AddItemToObject(accountsDB, newUser, newAcc);

    Q_strncpyz(r->username, newUser, sizeof(r->username));
    r->loggedIn = qtrue;
    r->isTemp = qfalse;
    r->tempElo = 1000;

    SV_Ranked_SaveAccounts();

    // Tell the player their credentials privately
    SV_SendServerCommand(
        cl, "print \"^5[RANKED] ^2New ranked account created!\n\"");
    SV_SendServerCommand(cl,
                         "print \"^5[RANKED] ^7User: ^2%s  ^7Pass: ^2%s\n\"",
                         newUser, newPass);
    SV_SendServerCommand(cl,
                         "print \"^5[RANKED] ^3Save these! Use "
                         "^7/changepassword <newpass> ^3to set your own.\n\"");
    SV_Ranked_Log("AUTO-REGISTER: user='%s' GUID='%s' ip='%s'", newUser, guid,
                  Info_ValueForKey(cl->userinfo, "ip"));
    SV_Ranked_CheckAndRefreshDailyQuests(r->username, cl);
    SV_Ranked_SyncClientRPG(cl);
  }
}

// -----------------------------------------------------------------------------
// Economy / leveling helpers
// -----------------------------------------------------------------------------
void UpdateAccountCredits(const char *username, int credDelta) {
  if (!accountsDB || !username || !username[0] || credDelta == 0)
    return;

  cJSON *acc = SV_Ranked_GetAccount(username);
  if (!acc)
    return;

  cJSON *cred = cJSON_GetObjectItemCaseSensitive(acc, "credits");
  if (!cred) {
    cred = cJSON_AddNumberToObject(acc, "credits", 0);
  }

  int newVal = cred->valueint + credDelta;
  if (newVal < 0)
    newVal = 0;
  cJSON_SetNumberValue(cred, newVal);

  // Trigger economy achievement checks on any credit gain.
  // We pass NULL for client_t here since we may not have context;
  // the grant function will notify only if the player is online.
  if (credDelta > 0) {
    // Find the online client for this username to show the notification
    extern rankedMatchState_t sv_rankedPlayers[64];
    extern cvar_t *sv_maxclients;
    for (int i = 0; i < sv_maxclients->integer; i++) {
      if (sv_rankedPlayers[i].loggedIn &&
          !Q_stricmp(sv_rankedPlayers[i].username, username)) {
        SV_Ranked_CheckEconomyAchievements(username, &svs.clients[i]);
        break;
      }
    }
  }
}

/*
==================
SV_Ranked_SyncTopLevelFields
  Ensures that the JSON account object contains top-level fields
  that match the SQL schema provided by the user.
==================
*/
void SV_Ranked_SyncTopLevelFields(cJSON *acc) {
    if (!acc) return;

    const char *currentMode = SV_Ranked_GetActiveMode();
    cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
    cJSON *modeData = modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, currentMode) : NULL;

    // 1. Initialized
    if (!cJSON_GetObjectItemCaseSensitive(acc, "initialized")) {
        cJSON_AddNumberToObject(acc, "initialized", 1);
    }

    // 2. Username (ensure top level)
    if (!cJSON_GetObjectItemCaseSensitive(acc, "username") && acc->string) {
        cJSON_AddStringToObject(acc, "username", acc->string);
    }

    // 3. GUID Generation / Sync
    // "engine_guid" = what engine gave us (cl_guid).
    // "guid" = internal string, generate "AUTH_..." if missing.
    // "persistent_id" = generate UUID if missing.
    cJSON *eguid = cJSON_GetObjectItemCaseSensitive(acc, "engine_guid");
    cJSON *oldGuid = cJSON_GetObjectItemCaseSensitive(acc, "guid");
    
    // If old JSON has "guid" which is actually the engine guid, migrate it:
    if (!eguid && oldGuid && cJSON_IsString(oldGuid) && oldGuid->valuestring) {
        // Assume oldGuid is engine guid if it doesn't start with AUTH_ or TEMP_
        if (strncmp(oldGuid->valuestring, "AUTH_", 5) != 0 && strncmp(oldGuid->valuestring, "TEMP_", 5) != 0) {
            cJSON_AddStringToObject(acc, "engine_guid", oldGuid->valuestring);
            // Now we need to generate a real AUTH_ guid below.
            cJSON_DeleteItemFromObject(acc, "guid");
            oldGuid = NULL;
        }
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "guid")) {
        char newGuid[64];
        char u[40];
        SV_Ranked_GenerateUUID(u, sizeof(u));
        Com_sprintf(newGuid, sizeof(newGuid), "AUTH_%s", u);
        cJSON_AddStringToObject(acc, "guid", newGuid);
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "persistent_id")) {
        char u[40];
        SV_Ranked_GenerateUUID(u, sizeof(u));
        cJSON_AddStringToObject(acc, "persistent_id", u);
    }

    // 4. Password to password_hash
    cJSON *pw = cJSON_GetObjectItemCaseSensitive(acc, "password");
    if (pw && !cJSON_GetObjectItemCaseSensitive(acc, "password_hash")) {
        cJSON_AddStringToObject(acc, "password_hash", pw->valuestring);
    }

    // 5. Clean Name (strip colors)
    cJSON *dispPtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
    if (dispPtr && dispPtr->valuestring) {
        char clean[MAX_AUTH_STRING];
        Q_strncpyz(clean, dispPtr->valuestring, sizeof(clean));
        Q_CleanStr(clean);
        if (!cJSON_GetObjectItemCaseSensitive(acc, "clean_name")) {
            cJSON_AddStringToObject(acc, "clean_name", clean);
        } else {
            cJSON_ReplaceItemInObject(acc, "clean_name", cJSON_CreateString(clean));
        }
    }

    // 6. MMR (Alias for active mode ELO)
    int mmr = 1000;
    if (modeData) {
        cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
        if (eloPtr) mmr = eloPtr->valueint;
    }
    if (!cJSON_GetObjectItemCaseSensitive(acc, "mmr")) {
        cJSON_AddNumberToObject(acc, "mmr", mmr);
    } else {
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "mmr"), mmr);
    }

    // 7. Wins / Losses / Kills / Deaths (Sync from active mode)
    const char *fields[] = {"wins", "losses", "kills", "deaths"};
    for (int i = 0; i < 4; i++) {
        int val = 0;
        if (modeData) {
            cJSON *p = cJSON_GetObjectItemCaseSensitive(modeData, fields[i]);
            if (p) val = p->valueint;
        }
        if (!cJSON_GetObjectItemCaseSensitive(acc, fields[i])) {
            cJSON_AddNumberToObject(acc, fields[i], val);
        } else {
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, fields[i]), val);
        }
    }

    // 8. Level & Rank
    cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
    int xp = xpPtr ? xpPtr->valueint : 0;
    int level = SV_Ranked_CalculateLevel(xp);
    const char *rankStr = SV_Ranked_GetTitle(mmr);

    if (!cJSON_GetObjectItemCaseSensitive(acc, "level")) {
        cJSON_AddNumberToObject(acc, "level", level);
    } else {
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "level"), level);
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "rank")) {
        cJSON_AddStringToObject(acc, "rank", rankStr);
    } else {
        cJSON_ReplaceItemInObject(acc, "rank", cJSON_CreateString(rankStr));
    }

    // 9. Last Login (Timestamp alias)
    cJSON *llt = cJSON_GetObjectItemCaseSensitive(acc, "last_login_time");
    if (llt) {
        if (!cJSON_GetObjectItemCaseSensitive(acc, "last_login")) {
            cJSON_AddNumberToObject(acc, "last_login", llt->valuedouble);
        } else {
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "last_login"), llt->valuedouble);
        }
    }

    // 10. Default JSON strings for TEXT columns
    if (!cJSON_GetObjectItemCaseSensitive(acc, "aliases")) cJSON_AddStringToObject(acc, "aliases", "");
    if (!cJSON_GetObjectItemCaseSensitive(acc, "inventory")) cJSON_AddStringToObject(acc, "inventory", "{}");
    if (!cJSON_GetObjectItemCaseSensitive(acc, "achievements")) cJSON_AddItemToObject(acc, "achievements", cJSON_CreateArray());

    // 11. Aliases for ints
    int hsVal = 0;
    if (modeData) {
        cJSON *hs = cJSON_GetObjectItemCaseSensitive(modeData, "highest_streak");
        if (hs) hsVal = hs->valueint;
    }
    if (!cJSON_GetObjectItemCaseSensitive(acc, "longest_streak")) cJSON_AddNumberToObject(acc, "longest_streak", hsVal);
    else cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "longest_streak"), hsVal);

    cJSON *mpt = cJSON_GetObjectItemCaseSensitive(acc, "max_potato_ticks");
    if (mpt) {
        if (!cJSON_GetObjectItemCaseSensitive(acc, "potato_points")) cJSON_AddNumberToObject(acc, "potato_points", mpt->valueint);
        else cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "potato_points"), mpt->valueint);
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "kyle_boss_kills")) cJSON_AddNumberToObject(acc, "kyle_boss_kills", 0);
    if (!cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins")) cJSON_AddNumberToObject(acc, "trivia_wins", 0);
    if (!cJSON_GetObjectItemCaseSensitive(acc, "rival_guid")) cJSON_AddStringToObject(acc, "rival_guid", "None");
    if (!cJSON_GetObjectItemCaseSensitive(acc, "rival_name")) cJSON_AddStringToObject(acc, "rival_name", "None");

    // 12. In Duel & Team (Real-time sync if online)
    int inDuel = 0;
    const char *teamStr = "free";
    
    // Find the online client to get current state
    extern rankedMatchState_t sv_rankedPlayers[64];
    extern cvar_t *sv_maxclients;
    for (int i = 0; i < sv_maxclients->integer; i++) {
        if (svs.clients[i].state && sv_rankedPlayers[i].loggedIn && 
            !Q_stricmp(sv_rankedPlayers[i].username, acc->string)) {
            inDuel = sv_rankedPlayers[i].inDuel ? 1 : 0;
            if (sv_rankedPlayers[i].latestTeamId == 1) teamStr = "red";
            else if (sv_rankedPlayers[i].latestTeamId == 2) teamStr = "blue";
            break;
        }
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "in_duel")) {
        cJSON_AddNumberToObject(acc, "in_duel", inDuel);
    } else {
        cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(acc, "in_duel"), inDuel);
    }

    if (!cJSON_GetObjectItemCaseSensitive(acc, "team")) {
        cJSON_AddStringToObject(acc, "team", teamStr);
    } else {
        cJSON_ReplaceItemInObject(acc, "team", cJSON_CreateString(teamStr));
    }
}

int SV_Ranked_CalculateLevel(int xp) {
  if (xp < 0)
    xp = 0;
  cJSON *xpl = SV_Ranked_GetSetting("xp_per_level");
  int perLevel = (xpl && xpl->valueint > 0) ? xpl->valueint : 1000;
  return (xp / perLevel) + 1;
}

const char *SV_Ranked_GetTitle(int fr, cJSON *acc) {
  extern cJSON *rankedConfig;

  // 1. Check for custom rank override FIRST
  if (acc) {
    cJSON *overrideRank = cJSON_GetObjectItemCaseSensitive(acc, "custom_rank_override");
    if (overrideRank && cJSON_IsString(overrideRank) && overrideRank->valuestring && overrideRank->valuestring[0]) {
      return overrideRank->valuestring;
    }
  }

  // 2. Compute dynamic rank if valid arrays exist in the config
  //   "rank_titles": ["Youngling", "Padawan", ...]
  //   "rank_thresholds": [0, 1500, ...]
  if (rankedConfig) {
    cJSON *titles =
        cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_titles");
    cJSON *thresh =
        cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_thresholds");
    if (titles && thresh && cJSON_IsArray(titles) && cJSON_IsArray(thresh)) {
      const int tCount = cJSON_GetArraySize(titles);
      const int thCount = cJSON_GetArraySize(thresh);
      const int n = (tCount < thCount) ? tCount : thCount;

      if (n > 0) {
        int idx = 0;
        for (int i = 0; i < n; ++i) {
          cJSON *th = cJSON_GetArrayItem(thresh, i);
          if (!th || !cJSON_IsNumber(th))
            continue;
          if (fr >= th->valueint)
            idx = i;
        }

        cJSON *tt = cJSON_GetArrayItem(titles, idx);
        if (tt && cJSON_IsString(tt) && tt->valuestring && tt->valuestring[0]) {
          return tt->valuestring;
        }
      }
    }
  }

  // Hard fallback (keeps server functional if config is invalid/missing)
  if (fr < 600)
    return "^3Youngling";
  if (fr < 800)
    return "^2Padawan";
  if (fr < 1000)
    return "^7Jedi";
  if (fr < 1200)
    return "^5Jedi Knight";
  if (fr < 1400)
    return "^6Jedi Master";
  if (fr < 1600)
    return "^3Council Member";
  return "^1Legend";
}

// -----------------------------------------------------------------------------
// Simple stat display / top lists
// -----------------------------------------------------------------------------
struct RankedListEntry {
  std::string key;
  std::string displayName;
  std::string rankTitle;
  int value = 0; // ELO/FR
  int level = 1;
  int wins = 0;
  int losses = 0;
  float ratio = 0.0f;
};

static std::vector<RankedListEntry>
Ranked_BuildListByModeField(const char *mode, const char *field) {
  std::vector<RankedListEntry> list;
  if (!accountsDB || !mode || !field)
    return list;

  for (cJSON *acc = accountsDB->child; acc; acc = acc->next) {
    if (!acc->string)
      continue;

    // mode bucket is stored at account.modes.<mode>
    cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
    cJSON *modeData =
        modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, mode) : NULL;
    
    int val = 1000;
    int wins = 0;
    int losses = 0;

    if (modeData) {
      cJSON *v = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
      if (v) val = v->valueint;
      cJSON *w = cJSON_GetObjectItemCaseSensitive(modeData, "wins");
      if (w) wins = w->valueint;
      cJSON *l = cJSON_GetObjectItemCaseSensitive(modeData, "losses");
      if (l) losses = l->valueint;
    }

    cJSON *disp = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
    cJSON *lvlObj = cJSON_GetObjectItemCaseSensitive(acc, "level");
    RankedListEntry e;
    e.key = acc->string;
    e.displayName = (disp && cJSON_IsString(disp) && disp->valuestring)
                        ? disp->valuestring
                        : acc->string;
    e.value = val;
    e.level = lvlObj ? lvlObj->valueint : 1;
    e.wins = wins;
    e.losses = losses;
    e.rankTitle = SV_Ranked_GetTitle(val, acc);
    
    if (losses > 0) e.ratio = (float)wins / (float)losses;
    else if (wins > 0) e.ratio = (float)wins;
    else e.ratio = 0.0f;

    list.push_back(e);
  }

  std::sort(list.begin(), list.end(),
            [](const RankedListEntry &a, const RankedListEntry &b) {
              return a.value > b.value;
            });

  return list;
}

static std::vector<RankedListEntry>
Ranked_BuildListByAccountField(const char *field) {
  std::vector<RankedListEntry> list;
  if (!accountsDB || !field)
    return list;

  for (cJSON *acc = accountsDB->child; acc; acc = acc->next) {
    if (!acc->string)
      continue;
    cJSON *valPtr = cJSON_GetObjectItemCaseSensitive(acc, field);
    int val = valPtr ? valPtr->valueint : 0;

    cJSON *disp = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
    RankedListEntry e;
    e.key = acc->string;
    e.displayName = (disp && cJSON_IsString(disp) && disp->valuestring)
                        ? disp->valuestring
                        : acc->string;
    e.value = val;
    list.push_back(e);
  }

  std::sort(list.begin(), list.end(),
            [](const RankedListEntry &a, const RankedListEntry &b) {
              return a.value > b.value;
            });

  return list;
}

// -----------------------------------------------------------------------
// Rival & Match History System
// -----------------------------------------------------------------------

void SV_Ranked_TrackRival(const char *username, const char *opponentGuid, const char *opponentName) {
    if (!accountsDB || !username || !username[0] || !opponentGuid || !opponentGuid[0]) return;

    cJSON *acc = SV_Ranked_GetAccount(username);
    if (!acc) return;

    // --- Update rivals tally ---
    cJSON *rivals = cJSON_GetObjectItemCaseSensitive(acc, "rivals");
    if (!rivals) {
        rivals = cJSON_CreateObject();
        cJSON_AddItemToObject(acc, "rivals", rivals);
    }

    cJSON *opp = cJSON_GetObjectItemCaseSensitive(rivals, opponentGuid);
    if (!opp) {
        opp = cJSON_CreateObject();
        cJSON_AddNumberToObject(opp, "count", 1);
        cJSON_AddStringToObject(opp, "name", opponentName ? opponentName : "Unknown");
        cJSON_AddItemToObject(rivals, opponentGuid, opp);
    } else {
        cJSON *countPtr = cJSON_GetObjectItemCaseSensitive(opp, "count");
        if (countPtr) cJSON_SetNumberValue(countPtr, countPtr->valueint + 1);
        cJSON *namePtr = cJSON_GetObjectItemCaseSensitive(opp, "name");
        if (namePtr && opponentName) cJSON_SetValuestring(namePtr, opponentName);
    }

    // --- Find the most frequent opponent (min 3 matches) ---
    int maxCount = 0;
    const char *bestGuid = NULL;
    const char *bestName = NULL;

    for (cJSON *child = rivals->child; child; child = child->next) {
        cJSON *cPtr = cJSON_GetObjectItemCaseSensitive(child, "count");
        cJSON *nPtr = cJSON_GetObjectItemCaseSensitive(child, "name");
        int c = cPtr ? cPtr->valueint : 0;
        if (c > maxCount) {
            maxCount = c;
            bestGuid = child->string;
            bestName = (nPtr && nPtr->valuestring) ? nPtr->valuestring : "Unknown";
        }
    }

    if (maxCount >= 3 && bestGuid) {
        cJSON *rgPtr = cJSON_GetObjectItemCaseSensitive(acc, "rival_guid");
        const char *oldRival = rgPtr ? rgPtr->valuestring : "None";
        
        if (!rgPtr) cJSON_AddStringToObject(acc, "rival_guid", bestGuid);
        else cJSON_SetValuestring(rgPtr, bestGuid);

        cJSON *rnPtr = cJSON_GetObjectItemCaseSensitive(acc, "rival_name");
        if (!rnPtr) cJSON_AddStringToObject(acc, "rival_name", bestName);
        else cJSON_SetValuestring(rnPtr, bestName);

        if (Q_stricmp(oldRival, bestGuid) != 0) {
            SV_Ranked_Log("RIVAL: %s now has a new rival: %s (GUID: %s)", username, bestName, bestGuid);
        }
    }
}

void SV_Ranked_RecordRecentDuel(const char *username, const char *opponentName, qboolean isWin, int eloChange) {
    if (!accountsDB || !username || !username[0]) return;

    cJSON *acc = SV_Ranked_GetAccount(username);
    if (!acc) return;

    cJSON *recent = cJSON_GetObjectItemCaseSensitive(acc, "recent_duels");
    if (!recent) {
        recent = cJSON_CreateArray();
        cJSON_AddItemToObject(acc, "recent_duels", recent);
    }

    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "opponent", opponentName ? opponentName : "Unknown");
    cJSON_AddStringToObject(entry, "result", isWin ? "WIN" : "LOSS");
    cJSON_AddNumberToObject(entry, "elo", eloChange);
    cJSON_InsertItemInArray(recent, 0, entry);

    // Keep only the last 5
    while (cJSON_GetArraySize(recent) > 5) {
        cJSON_DeleteItemFromArray(recent, cJSON_GetArraySize(recent) - 1);
    }

    SV_Ranked_Log("MATCH: Recorded duel for %s (Opponent: %s | Result: %s | ELO: %+d)", 
                  username, opponentName, isWin ? "WIN" : "LOSS", eloChange);
}

void SV_Ranked_ShowStats(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in to view your "
                             "stats. Type /login <user> <pass>\n\"");
    return;
  }

  // All players are real accounts now — no guest/temp path needed

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  // READ CVAR
  const char *currentMode = SV_Ranked_GetActiveMode();

  cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  cJSON *modeData =
      modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, currentMode) : NULL;

  cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
  int xp = xpPtr ? xpPtr->valueint : 0;
  int level = SV_Ranked_CalculateLevel(xp);

  cJSON *credPtr = cJSON_GetObjectItemCaseSensitive(acc, "credits");
  int credits = credPtr ? credPtr->valueint : 0;

  int elo = 1000;
  int wins = 0;
  int losses = 0;
  int kills = 0;
  int deaths = 0;
  int highestStreak = 0;

  if (modeData) {
    elo = cJSON_GetObjectItemCaseSensitive(modeData, "elo")
              ? cJSON_GetObjectItemCaseSensitive(modeData, "elo")->valueint
              : 1000;
    wins = cJSON_GetObjectItemCaseSensitive(modeData, "wins")
               ? cJSON_GetObjectItemCaseSensitive(modeData, "wins")->valueint
               : 0;
    losses =
        cJSON_GetObjectItemCaseSensitive(modeData, "losses")
            ? cJSON_GetObjectItemCaseSensitive(modeData, "losses")->valueint
            : 0;
    kills = cJSON_GetObjectItemCaseSensitive(modeData, "kills")
                ? cJSON_GetObjectItemCaseSensitive(modeData, "kills")->valueint
                : 0;
    deaths =
        cJSON_GetObjectItemCaseSensitive(modeData, "deaths")
            ? cJSON_GetObjectItemCaseSensitive(modeData, "deaths")->valueint
            : 0;
    cJSON *hs = cJSON_GetObjectItemCaseSensitive(modeData, "highest_streak");
    if (hs)
      highestStreak = hs->valueint;
  }

  int teamWins = 0;
  int teamLosses = 0;
  if (modeData && !Q_stricmp(currentMode, "open")) {
    cJSON *tw = cJSON_GetObjectItemCaseSensitive(modeData, "team_wins");
    if (tw)
      teamWins = tw->valueint;
    cJSON *tl = cJSON_GetObjectItemCaseSensitive(modeData, "team_losses");
    if (tl)
      teamLosses = tl->valueint;
  }

  cJSON *weapons =
      modeData ? cJSON_GetObjectItemCaseSensitive(modeData, "weapons") : NULL;
  const char *favWeapon = "None";
  int maxKills = 0;

  if (weapons) {
    cJSON *wep = weapons->child;
    while (wep) {
      if (wep->valueint > maxKills) {
        maxKills = wep->valueint;
        favWeapon = wep->string;
      }
      wep = wep->next;
    }
  }

  cJSON *displayNamePtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  const char *displayName = (displayNamePtr && displayNamePtr->valuestring)
                                ? displayNamePtr->valuestring
                                : cl->name;

  int triviaWins = cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins") ? cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins")->valueint : 0;
  
  // Find top rival
  const char *topRivalName = "None";
  int topRivalCount = 0;
  cJSON *rivals = cJSON_GetObjectItemCaseSensitive(acc, "rivals");
  if (rivals) {
    for (cJSON *r = rivals->child; r; r = r->next) {
      cJSON *c = cJSON_GetObjectItemCaseSensitive(r, "count");
      cJSON *n = cJSON_GetObjectItemCaseSensitive(r, "name");
      if (c && c->valueint > topRivalCount) {
        topRivalCount = c->valueint;
        topRivalName = n ? n->valuestring : "Unknown";
      }
    }
  }

  // Calculate W/L Ratio
  float wlRatio = 0.0f;
  if (!Q_stricmp(currentMode, "open")) {
    if (teamLosses > 0) wlRatio = (float)teamWins / (float)teamLosses;
    else if (teamWins > 0) wlRatio = (float)teamWins;
  } else {
    if (losses > 0) wlRatio = (float)wins / (float)losses;
    else if (wins > 0) wlRatio = (float)wins;
  }

  const char *title = SV_Ranked_GetTitle(elo, acc);
  char upperMode[32];
  Q_strncpyz(upperMode, currentMode, sizeof(upperMode));
  Q_strupr(upperMode);

  if (!Q_stricmp(currentMode, "open")) {
    SV_SendServerCommand(
        cl,
        "chat \"^7[STATS] ^5%s ^7| ^3Lv %d ^7| %s ^7| ^2FR: ^7%d | ^3W/L: "
        "^7%d/%d ^5(%.2f) ^7| ^2K/D: ^7%d/%d ^5(Console)\n\"",
        displayName, level, title, elo, teamWins, teamLosses, wlRatio, kills, deaths);
  } else {
    SV_SendServerCommand(
        cl,
        "chat \"^7[STATS] ^5%s ^7| ^3Lv %d ^7| %s ^7| ^2FR: ^7%d | ^3W/L: "
        "^7%d/%d ^5(%.2f) ^7| ^2K/D: ^7%d/%d ^5(Console)\n\"",
        displayName, level, title, elo, wins, losses, wlRatio, kills, deaths);
  }

  SV_SendServerCommand(cl, "print \"\n^2--- ^7%s^7's Stats ^2---\n\"",
                       displayName);
  SV_SendServerCommand(cl, "print \"^2MODE: ^7%s\n\"", upperMode);
  SV_SendServerCommand(cl, "print \"^3Level: ^7%d ^3(%d XP)\n\"", level, xp);
  SV_SendServerCommand(cl, "print \"^5Rank: %s ^5(^7%d Force Rating^5)\n\"",
                       title, elo);
  SV_SendServerCommand(cl, "print \"^6Credits: ^7%d\n\"", credits);
  SV_SendServerCommand(cl, "print \"^3Trivia Wins: ^7%d\n\"", triviaWins);
  SV_SendServerCommand(cl, "print \"^1Main Rival: ^7%s ^5(%d duels)\n\"", topRivalName, topRivalCount);
  SV_SendServerCommand(cl, "print \"^5Streak: ^7%d  ^5Highest Streak: ^7%d\n\"",
                       r->killStreak, highestStreak);

  if (!Q_stricmp(currentMode, "open")) {
    SV_SendServerCommand(cl, "print \"^3Wins: ^7%d  ^1Losses: ^7%d ^5(Ratio: %.2f)\n\"",
                         teamWins, teamLosses, wlRatio);
  } else {
    SV_SendServerCommand(cl, "print \"^3Wins: ^7%d  ^1Losses: ^7%d ^5(Ratio: %.2f)\n\"", wins,
                         losses, wlRatio);
  }

  SV_SendServerCommand(cl, "print \"^2Kills: ^7%d  ^1Deaths: ^7%d\n\"", kills,
                       deaths);
  if (maxKills > 0) {
    SV_SendServerCommand(cl, "print \"^6Favorite Weapon: ^7%s (%d kills)\n\"",
                         SV_Ranked_GetWeaponName(favWeapon), maxKills);
  } else {
    SV_SendServerCommand(cl, "print \"^6Favorite Weapon: ^7%s\n\"",
                         SV_Ranked_GetWeaponName(favWeapon));
  }

  // Duel-specific stats
  if (modeData && Q_stricmp(currentMode, "duel") == 0) {
    int tdt = 0, tdc = 0, sdw = 0, mw = 0;
    cJSON *jtdt = cJSON_GetObjectItemCaseSensitive(modeData, "total_duel_time");
    cJSON *jtdc = cJSON_GetObjectItemCaseSensitive(modeData, "total_duel_count");
    cJSON *jsd  = cJSON_GetObjectItemCaseSensitive(modeData, "speed_demon_wins");
    cJSON *jmw  = cJSON_GetObjectItemCaseSensitive(modeData, "marathon_wins");
    if (jtdt) tdt = jtdt->valueint;
    if (jtdc) tdc = jtdc->valueint;
    if (jsd)  sdw = jsd->valueint;
    if (jmw)  mw  = jmw->valueint;

    if (tdc > 0) {
      SV_SendServerCommand(cl, "print \"^3Avg Duel Time: ^7%d seconds\n\"", tdt / tdc);
    }
    if (sdw > 0) {
      SV_SendServerCommand(cl, "print \"^3Speed Demon: ^7%d wins\n\"", sdw);
    }
    if (mw > 0) {
      SV_SendServerCommand(cl, "print \"^5Marathon: ^7%d wins\n\"", mw);
    }

    struct { const char *field; const char *label; int count; } modStats[] = {
      {"mod_saber_wins",      "Saber",       0},
      {"mod_melee_wins",      "Melee",       0},
      {"mod_force_dark_wins", "Force Dark",  0},
      {"mod_suicide_wins",    "Forfeit",     0},
      {"mod_other_wins",      "Other",       0},
    };
    int favModCount = 0;
    const char *favModLabel = "None";
    for (int i = 0; i < 5; i++) {
      cJSON *mc = cJSON_GetObjectItemCaseSensitive(modeData, modStats[i].field);
      if (mc) modStats[i].count = mc->valueint;
      if (modStats[i].count > favModCount) {
        favModCount = modStats[i].count;
        favModLabel = modStats[i].label;
      }
    }
    if (favModCount > 0) {
      SV_SendServerCommand(cl, "print \"^6Favorite Win Style: ^7%s (%d)\n\"", favModLabel, favModCount);
    }
  }

  SV_SendServerCommand(cl, "print \"^2----------------------\n\n\"");
}

/*
==================
SV_Ranked_ShowStatsTarget
==================
*/
void SV_Ranked_ShowStatsTarget(client_t *cl, const char *targetName) {
  int targetId = SV_Ranked_FindPlayerByNameOrId(targetName);

  if (targetId == -2) {
    SV_SendServerCommand(cl, "chat \"^3Multiple players match that name. Use "
                             "Client ID or a more specific name.\n\"");
    return;
  }

  rankedMatchState_t *t = NULL;
  cJSON *acc = NULL;

  if (targetId >= 0) {
    t = &sv_rankedPlayers[targetId];
    if (t->loggedIn) {
      acc = SV_Ranked_GetAccount(t->username);
    }
  }

  // If not currently in game, search DB by exact lowercased name directly
  if (!acc) {
    acc = SV_Ranked_GetAccount(targetName);
  }

  if (!acc) {
    SV_SendServerCommand(
        cl, "chat \"^1Player '^7%s^1' not found in database.\"", targetName);
    return;
  }

  const char *currentMode = SV_Ranked_GetActiveMode();
  cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  cJSON *modeData =
      modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, currentMode) : NULL;

  cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
  int xp = xpPtr ? xpPtr->valueint : 0;
  int level = SV_Ranked_CalculateLevel(xp);

  int elo = 1000;
  int wins = 0, losses = 0, kills = 0, deaths = 0, highestStreak = 0;

  if (modeData) {
    elo = cJSON_GetObjectItemCaseSensitive(modeData, "elo")
              ? cJSON_GetObjectItemCaseSensitive(modeData, "elo")->valueint
              : 1000;
    wins = cJSON_GetObjectItemCaseSensitive(modeData, "wins")
               ? cJSON_GetObjectItemCaseSensitive(modeData, "wins")->valueint
               : 0;
    losses =
        cJSON_GetObjectItemCaseSensitive(modeData, "losses")
            ? cJSON_GetObjectItemCaseSensitive(modeData, "losses")->valueint
            : 0;
    kills = cJSON_GetObjectItemCaseSensitive(modeData, "kills")
                ? cJSON_GetObjectItemCaseSensitive(modeData, "kills")->valueint
                : 0;
    deaths =
        cJSON_GetObjectItemCaseSensitive(modeData, "deaths")
            ? cJSON_GetObjectItemCaseSensitive(modeData, "deaths")->valueint
            : 0;
    cJSON *hs = cJSON_GetObjectItemCaseSensitive(modeData, "highest_streak");
    if (hs)
      highestStreak = hs->valueint;
  }

  int teamWins = 0, teamLosses = 0;
  if (modeData && !Q_stricmp(currentMode, "open")) {
    cJSON *tw = cJSON_GetObjectItemCaseSensitive(modeData, "team_wins");
    if (tw)
      teamWins = tw->valueint;
    cJSON *tl = cJSON_GetObjectItemCaseSensitive(modeData, "team_losses");
    if (tl)
      teamLosses = tl->valueint;
  }

  cJSON *weapons =
      modeData ? cJSON_GetObjectItemCaseSensitive(modeData, "weapons") : NULL;
  const char *favWeapon = "None";
  int maxKills = 0;
  if (weapons) {
    cJSON *wep = weapons->child;
    while (wep) {
      if (wep->valueint > maxKills) {
        maxKills = wep->valueint;
        favWeapon = wep->string;
      }
      wep = wep->next;
    }
  }

  cJSON *displayNamePtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  cJSON *usernamePtr = cJSON_GetObjectItemCaseSensitive(acc, "username");
  const char *displayName =
      displayNamePtr ? displayNamePtr->valuestring
                     : (usernamePtr ? usernamePtr->valuestring : targetName);

  int triviaWins = cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins") ? cJSON_GetObjectItemCaseSensitive(acc, "trivia_wins")->valueint : 0;
  
  // Find top rival
  const char *topRivalName = "None";
  int topRivalCount = 0;
  cJSON *rivals = cJSON_GetObjectItemCaseSensitive(acc, "rivals");
  if (rivals) {
    for (cJSON *r = rivals->child; r; r = r->next) {
      cJSON *c = cJSON_GetObjectItemCaseSensitive(r, "count");
      cJSON *n = cJSON_GetObjectItemCaseSensitive(r, "name");
      if (c && c->valueint > topRivalCount) {
        topRivalCount = c->valueint;
        topRivalName = n ? n->valuestring : "Unknown";
      }
    }
  }

  // Calculate W/L Ratio
  float wlRatio = 0.0f;
  if (!Q_stricmp(currentMode, "open")) {
    if (teamLosses > 0) wlRatio = (float)teamWins / (float)teamLosses;
    else if (teamWins > 0) wlRatio = (float)teamWins;
  } else {
    if (losses > 0) wlRatio = (float)wins / (float)losses;
    else if (wins > 0) wlRatio = (float)wins;
  }

  const char *title = SV_Ranked_GetTitle(elo, acc);
  char upperMode[32];
  Q_strncpyz(upperMode, currentMode, sizeof(upperMode));
  Q_strupr(upperMode);

  if (!Q_stricmp(currentMode, "open")) {
    SV_SendServerCommand(
        cl,
        "chat \"^7[STATS] ^5%s ^7| ^3Lv %d ^7| %s ^7| ^2FR: ^7%d | ^3W/L: "
        "^7%d/%d ^5(%.2f) ^7| ^2K/D: ^7%d/%d ^5(Console)\n\"",
        displayName, level, title, elo, teamWins, teamLosses, wlRatio, kills, deaths);
  } else {
    SV_SendServerCommand(
        cl,
        "chat \"^7[STATS] ^5%s ^7| ^3Lv %d ^7| %s ^7| ^2FR: ^7%d | ^3W/L: "
        "^7%d/%d ^5(%.2f) ^7| ^2K/D: ^7%d/%d ^5(Console)\n\"",
        displayName, level, title, elo, wins, losses, wlRatio, kills, deaths);
  }

  SV_SendServerCommand(cl, "print \"\n^2--- ^7%s^7's Stats ^2---\n\"",
                       displayName);
  SV_SendServerCommand(cl, "print \"^2MODE: ^7%s\n\"", upperMode);
  SV_SendServerCommand(cl, "print \"^3Level: ^7%d ^3(%d XP)\n\"", level, xp);
  SV_SendServerCommand(cl, "print \"^5Rank: %s ^5(^7%d Force Rating^5)\n\"",
                       title, elo);
  SV_SendServerCommand(cl, "print \"^3Trivia Wins: ^7%d\n\"", triviaWins);
  SV_SendServerCommand(cl, "print \"^1Main Rival: ^7%s ^5(%d duels)\n\"", topRivalName, topRivalCount);

  if (!Q_stricmp(currentMode, "open")) {
    SV_SendServerCommand(cl, "print \"^3Wins: ^7%d  ^1Losses: ^7%d ^5(Ratio: %.2f)\n\"",
                         teamWins, teamLosses, wlRatio);
  } else {
    SV_SendServerCommand(cl, "print \"^3Wins: ^7%d  ^1Losses: ^7%d ^5(Ratio: %.2f)\n\"", wins,
                         losses, wlRatio);
  }

  SV_SendServerCommand(cl, "print \"^2Kills: ^7%d  ^1Deaths: ^7%d\n\"", kills,
                       deaths);
  if (maxKills > 0) {
    SV_SendServerCommand(cl, "print \"^6Favorite Weapon: ^7%s (%d kills)\n\"",
                         SV_Ranked_GetWeaponName(favWeapon), maxKills);
  } else {
    SV_SendServerCommand(cl, "print \"^6Favorite Weapon: ^7%s\n\"",
                         SV_Ranked_GetWeaponName(favWeapon));
  }
  // Duel-specific stats
  if (modeData && Q_stricmp(currentMode, "duel") == 0) {
    int tdt = 0, tdc = 0, sdw = 0, mw = 0;
    cJSON *jtdt = cJSON_GetObjectItemCaseSensitive(modeData, "total_duel_time");
    cJSON *jtdc = cJSON_GetObjectItemCaseSensitive(modeData, "total_duel_count");
    cJSON *jsd  = cJSON_GetObjectItemCaseSensitive(modeData, "speed_demon_wins");
    cJSON *jmw  = cJSON_GetObjectItemCaseSensitive(modeData, "marathon_wins");
    if (jtdt) tdt = jtdt->valueint;
    if (jtdc) tdc = jtdc->valueint;
    if (jsd)  sdw = jsd->valueint;
    if (jmw)  mw  = jmw->valueint;

    if (tdc > 0) {
      SV_SendServerCommand(cl, "print \"^3Avg Duel Time: ^7%d seconds\n\"", tdt / tdc);
    }
    if (sdw > 0) {
      SV_SendServerCommand(cl, "print \"^3Speed Demon: ^7%d wins\n\"", sdw);
    }
    if (mw > 0) {
      SV_SendServerCommand(cl, "print \"^5Marathon: ^7%d wins\n\"", mw);
    }

    struct { const char *field; const char *label; int count; } modStats[] = {
      {"mod_saber_wins",      "Saber",       0},
      {"mod_melee_wins",      "Melee",       0},
      {"mod_force_dark_wins", "Force Dark",  0},
      {"mod_suicide_wins",    "Forfeit",     0},
      {"mod_other_wins",      "Other",       0},
    };
    int favModCount = 0;
    const char *favModLabel = "None";
    for (int i = 0; i < 5; i++) {
      cJSON *mc = cJSON_GetObjectItemCaseSensitive(modeData, modStats[i].field);
      if (mc) modStats[i].count = mc->valueint;
      if (modStats[i].count > favModCount) {
        favModCount = modStats[i].count;
        favModLabel = modStats[i].label;
      }
    }
    if (favModCount > 0) {
      SV_SendServerCommand(cl, "print \"^6Favorite Win Style: ^7%s (%d)\n\"", favModLabel, favModCount);
    }
  }

  SV_SendServerCommand(cl, "print \"^2----------------------\n\n\"");
}

/*
==================
SV_Ranked_ShowTop
==================
*/
void SV_Ranked_ShowTop(client_t *cl) {
  if (!cl)
    return;
  const char *mode = SV_Ranked_GetActiveMode();
  auto list = Ranked_BuildListByModeField(mode, "elo");
  if (list.empty()) {
    SV_SendServerCommand(cl, "chat \"^3No accounts in DB yet.\"");
    return;
  }

  SV_SendServerCommand(cl, "print \"\n^5--- ^7TOP 10 PLAYERS (^3%s^7) ^5---\n\"",
                       mode);
  SV_SendServerCommand(cl, "print \"^2#  Name                             FR     W/L      Ratio   Rank\n\"");
  SV_SendServerCommand(cl, "print \"^2-- ---------------------------- ------ -------- ------- -----------\n\"");

  // Send UI Leaderboard Sync command to client popup overlay
  SV_SendServerCommand(cl, "top_clear");

  const int max = (int)std::min<size_t>(10, list.size());
  for (int i = 0; i < max; ++i) {
    char name[32];
    Q_strncpyz(name, list[i].displayName.c_str(), sizeof(name));
    
    // Calculate the rendered length (excluding color codes)
    int renderedLen = 0;
    for (int j = 0; name[j] != '\0'; j++) {
      if (name[j] == '^' && name[j + 1] != '\0') {
        j++;
      } else {
        renderedLen++;
      }
    }
    
    // Pad to 28 characters
    int padSpaces = 28 - renderedLen;
    if (padSpaces < 1) {
      padSpaces = 1;
    }
    
    char paddedName[128];
    Q_strncpyz(paddedName, name, sizeof(paddedName));
    int nameLen = strlen(paddedName);
    for (int j = 0; j < padSpaces && nameLen < (int)sizeof(paddedName) - 1; j++) {
      paddedName[nameLen++] = ' ';
    }
    paddedName[nameLen] = '\0';
    
    SV_SendServerCommand(cl, va("print \"^3%2d. ^7%s ^5%5d  ^2%3d^7/^1%3d  ^3%6.2f  ^7%s\n\"", 
                         i + 1, paddedName, list[i].value, list[i].wins, list[i].losses, list[i].ratio, list[i].rankTitle.c_str()));

    SV_SendServerCommand(cl, va("top_entry %d %d %d \"%s\" \"%s\"",
                         i + 1, list[i].value, list[i].level, list[i].rankTitle.c_str(), list[i].displayName.c_str()));
  }
  SV_SendServerCommand(cl, "print \"^5--------------------------------------------------------------------\n\n\"");
  SV_SendServerCommand(cl, "top_open");
}

void SV_Ranked_ShowRank(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl,
                         "chat \"^3You must be logged in to view your Rank.\"");
    return;
  }
  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  const char *currentMode = SV_Ranked_GetActiveMode();
  cJSON *modesObj = cJSON_GetObjectItemCaseSensitive(acc, "modes");
  cJSON *modeData =
      modesObj ? cJSON_GetObjectItemCaseSensitive(modesObj, currentMode) : NULL;

  int elo = 1000;
  if (modeData) {
    cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(modeData, "elo");
    if (eloPtr)
      elo = eloPtr->valueint;
  } else if (!Q_stricmp(currentMode, "open")) {
    cJSON *eloPtr = cJSON_GetObjectItemCaseSensitive(acc, "elo");
    if (eloPtr)
      elo = eloPtr->valueint;
  }

  cJSON *displayNamePtr = cJSON_GetObjectItemCaseSensitive(acc, "displayName");
  const char *displayName = (displayNamePtr && displayNamePtr->valuestring)
                                ? displayNamePtr->valuestring
                                : cl->name;

  cJSON *xpPtr = cJSON_GetObjectItemCaseSensitive(acc, "xp");
  int xp = xpPtr ? xpPtr->valueint : 0;
  int level = SV_Ranked_CalculateLevel(xp);
  const char *title = SV_Ranked_GetTitle(elo, acc);

  SV_SendServerCommand(
      NULL, "chat \"^5[RANK] ^7%s ^7| ^3Lv %d ^7| %s ^7| ^5%d FR ^7(%s)\"",
      displayName, level, title, elo, currentMode);
}

void SV_Ranked_ShowRankThresholds(client_t *cl) {
  if (!cl)
    return;

  extern cJSON *rankedConfig;

  SV_SendServerCommand(cl, "print \"\n^5====== RANK THRESHOLDS ======\n\"");

  if (rankedConfig) {
    cJSON *titles = cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_titles");
    cJSON *thresh = cJSON_GetObjectItemCaseSensitive(rankedConfig, "rank_thresholds");
    if (titles && thresh && cJSON_IsArray(titles) && cJSON_IsArray(thresh)) {
      const int tCount = cJSON_GetArraySize(titles);
      const int thCount = cJSON_GetArraySize(thresh);
      const int n = (tCount < thCount) ? tCount : thCount;

      for (int i = 0; i < n; ++i) {
        cJSON *title = cJSON_GetArrayItem(titles, i);
        cJSON *th = cJSON_GetArrayItem(thresh, i);
        if (title && cJSON_IsString(title) && title->valuestring && th && cJSON_IsNumber(th)) {
          SV_SendServerCommand(cl, va("print \"  %-20s ^5- ^2%d FR+\n\"", title->valuestring, th->valueint));
        }
      }
      SV_SendServerCommand(cl, "print \"^5=============================\n\n\"");
      return;
    }
  }

  // Fallback if config is missing or invalid
  SV_SendServerCommand(cl, "print \"  ^3Youngling            ^5- ^20 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^2Padawan              ^5- ^2600 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^7Jedi                 ^5- ^2800 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^5Jedi Knight          ^5- ^21000 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^6Jedi Master          ^5- ^21200 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^3Council Member       ^5- ^21400 FR+\n\"");
  SV_SendServerCommand(cl, "print \"  ^1Legend               ^5- ^21600 FR+\n\"");
  SV_SendServerCommand(cl, "print \"^5=============================\n\n\"");
}

void SV_Ranked_ShowTopCredits(client_t *cl) {
  if (!cl)
    return;
  auto list = Ranked_BuildListByAccountField("credits");
  if (list.empty()) {
    SV_SendServerCommand(cl, "chat \"^3No accounts in DB yet.\"");
    return;
  }

  SV_SendServerCommand(cl, "print \"\n^2--- ^7Top Credits ^2---\n\"");
  const int max = (int)std::min<size_t>(10, list.size());
  for (int i = 0; i < max; ++i) {
    SV_SendServerCommand(cl, "print \"^3%2d^7) %s ^2- ^7%d\n\"", i + 1,
                         list[i].displayName.c_str(), list[i].value);
  }
  SV_SendServerCommand(cl, "print \"\n\"");
}

void SV_Ranked_ShowTopPotato(client_t *cl) {
  if (!cl)
    return;
  auto list = Ranked_BuildListByAccountField("max_potato_ticks");
  if (list.empty()) {
    SV_SendServerCommand(cl, "chat \"^3No accounts in DB yet.\"");
    return;
  }

  SV_SendServerCommand(cl,
                       "print \"\n^2--- ^7Top Hot Potato (ticks) ^2---\n\"");
  const int max = (int)std::min<size_t>(10, list.size());
  for (int i = 0; i < max; ++i) {
    SV_SendServerCommand(cl, "print \"^3%2d^7) %s ^2- ^7%d\n\"", i + 1,
                         list[i].displayName.c_str(), list[i].value);
  }
  SV_SendServerCommand(cl, "print \"\n\"");
}

void SV_Ranked_ShowCredits(client_t *cl) {
  if (!cl)
    return;
  const int clientNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *cr = acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int credits = cr ? cr->valueint : 0;
  SV_SendServerCommand(cl, "chat \"^2Credits: ^7%d\"", credits);
}

// -----------------------------------------------------------------------------
// Bounty + cosmetic DB actions (minimal implementations)
// -----------------------------------------------------------------------------
void SV_Ranked_SetBounty(client_t *cl, const char *targetName, int amount) {
  if (!cl || !targetName || !targetName[0])
    return;
  if (amount <= 0) {
    SV_SendServerCommand(cl, "chat \"^1Bounty amount must be > 0.\"");
    return;
  }

  const int cNum = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[cNum];
  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3You must be logged in.\"");
    return;
  }

  int tClient = SV_Ranked_FindPlayerByNameOrId(targetName);
  if (tClient < 0) {
    SV_SendServerCommand(cl, "chat \"^1Player not found.\"");
    return;
  }

  // Deduct credits from caller
  cJSON *acc = SV_Ranked_GetAccount(r->username);
  cJSON *cr = acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int credits = cr ? cr->valueint : 0;
  if (credits < amount) {
    SV_SendServerCommand(cl, "chat \"^1Not enough credits.\"");
    return;
  }
  UpdateAccountCredits(r->username, -amount);

  // Apply bounty to target's session state (volatile; claimed on kill/duel end)
  sv_rankedPlayers[tClient].bountyValue += amount;

  SV_SendServerCommand(
      NULL, "chat \"^1WANTED:^7 %s ^7now has a ^5%d^7 credit bounty!\"",
      svs.clients[tClient].name, sv_rankedPlayers[tClient].bountyValue);
  SV_Ranked_SaveAccounts();
}

void SV_Ranked_ShowBountyList(client_t *cl) {
  if (!cl)
    return;
  SV_SendServerCommand(cl,
                       "print \"\n^2--- ^7Active Bounties (online) ^2---\n\"");
  int shown = 0;
  for (int i = 0; i < sv_maxclients->integer; ++i) {
    if (svs.clients[i].state && sv_rankedPlayers[i].bountyValue > 0) {
      SV_SendServerCommand(cl, "print \"^7%s ^2- ^5%d credits\n\"",
                           svs.clients[i].name,
                           sv_rankedPlayers[i].bountyValue);
      shown++;
    }
  }
  if (!shown) {
    SV_SendServerCommand(cl, "print \"^3No active bounties.\n\"");
  }
  SV_SendServerCommand(cl, "print \"\n\"");
}

// ============================================================
//  SHOP ITEM DEFINITIONS
//  Must be declared before ShowShop which iterates them.
// ============================================================

// Shop items relocated to top of file

static const shopItem_t *FindShopItem(const char *key) {
  for (int i = 0; sv_shopItems[i].key != NULL; i++) {
    if (!Q_stricmp(sv_shopItems[i].key, key))
      return &sv_shopItems[i];
  }
  return NULL;
}

// ===========================================================================
//  DAILY QUEST SYSTEM
//  3 quests per day (2 open + 1 duel). Resets 24h after all 3 are done.
// ===========================================================================

typedef struct {
  const char *id;
  const char *desc;
  const char *mode;    // "open" or "duel"
  const char *statKey; // stat progressed via SV_Ranked_ProgressQuest
  int goal;
  int reward_cr;
  int reward_fr;
} sv_questDef_t;

static const sv_questDef_t sv_questPool[] = {
    // ---- OPEN MODE ----
    {"o_kill10", "Get 10 kills", "open", "kills", 10, 100, 10},
    {"o_kill25", "Get 25 kills", "open", "kills", 25, 200, 20},
    {"o_kill50", "Get 50 kills", "open", "kills", 50, 350, 35},
    {"o_saber20", "Get 20 saber kills", "open", "saber_kills", 20, 150, 15},
    {"o_fb1", "Draw First Blood", "open", "first_bloods", 1, 75, 5},
    {"o_fb3", "Draw First Blood 3x", "open", "first_bloods", 3, 150, 15},
    {"o_dom1", "Dominate a player", "open", "dominations", 1, 100, 10},
    {"o_dom3", "Dominate 3 times", "open", "dominations", 3, 250, 25},
    {"o_double3", "Get 3 Double Kills", "open", "double_kills", 3, 75, 10},
    {"o_triple1", "Get a Triple Kill", "open", "triple_kills", 1, 100, 15},
    {"o_melee3", "Get 3 melee kills", "open", "melee_kills", 3, 100, 10},
    {"o_bomb3", "Get 3 bomb kills", "open", "bomb_kills", 3, 75, 10},
    // ---- DUEL MODE ----
    {"d_win1", "Win 1 duel", "duel", "duel_wins", 1, 100, 10},
    {"d_win3", "Win 3 duels", "duel", "duel_wins", 3, 250, 25},
    {"d_win5", "Win 5 duels", "duel", "duel_wins", 5, 500, 50},
    {"d_bounty1", "Claim a bounty in a duel", "duel", "d_bounties", 1, 200, 20},
    {"d_rankup1", "Get promoted via dueling", "duel", "d_rankups", 1, 300, 30},
    {NULL, NULL, NULL, NULL, 0, 0, 0}};

#define SV_QUEST_DAILY_COUNT 3

static const sv_questDef_t *SV_Quest_Find(const char *id) {
  for (int i = 0; sv_questPool[i].id != NULL; i++) {
    if (!Q_stricmp(sv_questPool[i].id, id))
      return &sv_questPool[i];
  }
  return NULL;
}

/*
==================
SV_Ranked_CheckAndRefreshDailyQuests
Assigns 3 new daily quests on first login or after 24h cooldown expires.
Cooldown starts when the last quest is completed.
==================
*/
void SV_Ranked_CheckAndRefreshDailyQuests(const char *username, client_t *cl) {
  cJSON *acc = SV_Ranked_GetAccount(username);
  if (!acc)
    return;

  const char *activeMode = SV_Ranked_GetActiveMode();
  char qKey[64];
  snprintf(qKey, sizeof(qKey), "daily_quests_%s", activeMode);

  cJSON *dq = cJSON_GetObjectItemCaseSensitive(acc, qKey);
  time_t now = time(NULL);

  if (dq) {
    // Count completed quests
    cJSON *questsArr = cJSON_GetObjectItemCaseSensitive(dq, "quests");
    int done = 0, total = questsArr ? cJSON_GetArraySize(questsArr) : 0;
    if (questsArr && total > 0) {
      cJSON *q;
      cJSON_ArrayForEach(q, questsArr) {
        cJSON *d = cJSON_GetObjectItemCaseSensitive(q, "done");
        if (d && cJSON_IsTrue(d))
          done++;
      }
    }
    if (done < SV_QUEST_DAILY_COUNT)
      return; // still active

    // All done: check 24h cooldown
    cJSON *lrtPtr = cJSON_GetObjectItemCaseSensitive(dq, "last_refresh_time");
    time_t lastRefresh = lrtPtr ? (time_t)lrtPtr->valuedouble : 0;
    if ((now - lastRefresh) < 86400)
      return;

    cJSON_DeleteItemFromObject(acc, qKey);
  }

  // Build index lists for the active mode quests
  int activeIdx[30], ac = 0;
  for (int i = 0; sv_questPool[i].id != NULL; i++) {
    if (!Q_stricmp(sv_questPool[i].mode, activeMode) && ac < 30) {
      activeIdx[ac++] = i;
    }
  }
  // Fisher-Yates shuffle
  for (int i = ac - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int t = activeIdx[i];
    activeIdx[i] = activeIdx[j];
    activeIdx[j] = t;
  }

  // Select 3 quests
  int selected[SV_QUEST_DAILY_COUNT], sc = 0;
  for (int i = 0; sc < SV_QUEST_DAILY_COUNT && i < ac; i++) {
    selected[sc++] = activeIdx[i];
  }

  cJSON *newDq = cJSON_CreateObject();
  cJSON_AddNumberToObject(newDq, "last_refresh_time", 0.0);
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < sc; i++) {
    const sv_questDef_t *qd = &sv_questPool[selected[i]];
    cJSON *qObj = cJSON_CreateObject();
    cJSON_AddStringToObject(qObj, "id", qd->id);
    cJSON_AddStringToObject(qObj, "stat", qd->statKey);
    cJSON_AddNumberToObject(qObj, "goal", qd->goal);
    cJSON_AddNumberToObject(qObj, "reward_cr", qd->reward_cr);
    cJSON_AddNumberToObject(qObj, "reward_fr", qd->reward_fr);
    cJSON_AddNumberToObject(qObj, "progress", 0);
    cJSON_AddFalseToObject(qObj, "done");
    cJSON_AddItemToArray(arr, qObj);
  }
  cJSON_AddItemToObject(newDq, "quests", arr);
  cJSON_AddItemToObject(acc, qKey, newDq);
  SV_Ranked_SaveAccounts();

  if (cl) {
    SV_SendServerCommand(cl, "print \"\n^5[DAILY QUESTS] ^73 new quests "
                             "assigned! Type ^3!quests ^7to view them.\n\"");
  }
}

/*
==================
SV_Ranked_ProgressQuest
Increments all active daily quests whose statKey matches. Completes and
rewards any quest that reaches its goal.
==================
*/
void SV_Ranked_ProgressQuest(const char *username, const char *statKey,
                             int amount, client_t *cl) {
  cJSON *acc = SV_Ranked_GetAccount(username);
  if (!acc)
    return;

  char qKey[64];
  snprintf(qKey, sizeof(qKey), "daily_quests_%s", SV_Ranked_GetActiveMode());

  cJSON *dq = cJSON_GetObjectItemCaseSensitive(acc, qKey);
  if (!dq)
    return;
  cJSON *questsArr = cJSON_GetObjectItemCaseSensitive(dq, "quests");
  if (!questsArr)
    return;

  qboolean changed = qfalse;
  cJSON *q;
  cJSON_ArrayForEach(q, questsArr) {
    cJSON *doneF = cJSON_GetObjectItemCaseSensitive(q, "done");
    if (doneF && cJSON_IsTrue(doneF))
      continue;

    cJSON *statF = cJSON_GetObjectItemCaseSensitive(q, "stat");
    if (!statF || !cJSON_IsString(statF))
      continue;
    if (Q_stricmp(statF->valuestring, statKey) != 0)
      continue;

    cJSON *progF = cJSON_GetObjectItemCaseSensitive(q, "progress");
    cJSON *goalF = cJSON_GetObjectItemCaseSensitive(q, "goal");
    if (!progF || !goalF)
      continue;

    int newProg = progF->valueint + amount;
    cJSON_SetNumberValue(progF, newProg);
    changed = qtrue;

    if (newProg >= goalF->valueint) {
      cJSON_ReplaceItemInObject(q, "done", cJSON_CreateTrue());

      int cr = 0, fr = 0;
      cJSON *crF = cJSON_GetObjectItemCaseSensitive(q, "reward_cr");
      cJSON *frF = cJSON_GetObjectItemCaseSensitive(q, "reward_fr");
      if (crF)
        cr = crF->valueint;
      if (frF)
        fr = frF->valueint;

      const char *desc = "Quest";
      cJSON *idF = cJSON_GetObjectItemCaseSensitive(q, "id");
      if (idF && cJSON_IsString(idF)) {
        const sv_questDef_t *qd = SV_Quest_Find(idF->valuestring);
        if (qd)
          desc = qd->desc;
      }

      UpdateAccountCredits(username, cr);
      // Note: FR reward is displayed only; ELO is earned organically through
      // ranked play

      if (cl) {
        SV_SendServerCommand(
            cl, "print \"^2QUEST COMPLETE! ^7%s ^5(+%d CR, +%d FR)\n\"", desc,
            cr, fr);
        SV_SendServerCommand(cl, "cp \"^3QUEST DONE!\n^7%s\"", desc);
      }

      // Check if all done → start 24h cooldown
      int allDone = 1;
      cJSON *q2;
      cJSON_ArrayForEach(q2, questsArr) {
        cJSON *d2 = cJSON_GetObjectItemCaseSensitive(q2, "done");
        if (!d2 || !cJSON_IsTrue(d2)) {
          allDone = 0;
          break;
        }
      }
      if (allDone) {
        cJSON *lrtF = cJSON_GetObjectItemCaseSensitive(dq, "last_refresh_time");
        if (lrtF)
          cJSON_SetNumberValue(lrtF, (double)time(NULL));
        else
          cJSON_AddNumberToObject(dq, "last_refresh_time", (double)time(NULL));
        if (cl)
          SV_SendServerCommand(cl, "print \"^5All daily quests complete! ^7New "
                                   "quests in ^324 hours^7.\n\"");
      }
    }
  }
  if (changed)
    SV_Ranked_SaveAccounts();
}

/*
==================
SV_Ranked_ShowQuests
==================
*/
void SV_Ranked_ShowQuests(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(
        cl, "print \"^3Login first to view your daily quests.\n\"");
    return;
  }

  // Ensure quests are assigned
  SV_Ranked_CheckAndRefreshDailyQuests(r->username, cl);
  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  char qKey[64];
  snprintf(qKey, sizeof(qKey), "daily_quests_%s", SV_Ranked_GetActiveMode());
  cJSON *dq = cJSON_GetObjectItemCaseSensitive(acc, qKey);
  SV_SendServerCommand(cl, "print \"\n^5========= DAILY QUESTS =========\n\"");

  if (!dq) {
    SV_SendServerCommand(cl, "print \"^7No quests available right now.\n\"");
    SV_SendServerCommand(cl,
                         "print \"^5=================================\n\n\"");
    return;
  }

  cJSON *questsArr = cJSON_GetObjectItemCaseSensitive(dq, "quests");
  int totalDone = 0, qnum = 1;
  cJSON *q;
  cJSON_ArrayForEach(q, questsArr) {
    cJSON *doneF = cJSON_GetObjectItemCaseSensitive(q, "done");
    cJSON *idF = cJSON_GetObjectItemCaseSensitive(q, "id");
    cJSON *progF = cJSON_GetObjectItemCaseSensitive(q, "progress");
    cJSON *goalF = cJSON_GetObjectItemCaseSensitive(q, "goal");
    cJSON *crF = cJSON_GetObjectItemCaseSensitive(q, "reward_cr");
    cJSON *frF = cJSON_GetObjectItemCaseSensitive(q, "reward_fr");

    qboolean done = (doneF && cJSON_IsTrue(doneF)) ? qtrue : qfalse;
    if (done)
      totalDone++;

    int prog = progF ? progF->valueint : 0;
    int goal = goalF ? goalF->valueint : 1;
    int cr = crF ? crF->valueint : 0;
    int fr = frF ? frF->valueint : 0;
    const char *desc = "Unknown Quest";
    const char *mode = "?";
    if (idF && cJSON_IsString(idF)) {
      const sv_questDef_t *qd = SV_Quest_Find(idF->valuestring);
      if (qd) {
        desc = qd->desc;
        mode = qd->mode;
      }
    }

    if (done) {
      SV_SendServerCommand(cl, "print \"^3%d. ^2[DONE] ^7%s ^6[%s]\n\"", qnum,
                           desc, mode);
    } else {
      SV_SendServerCommand(cl,
                           "print \"^3%d. ^7%s  ^5[^3%d^7/^3%d^5]  ^7+^5%d cr "
                           "^7+^5%d fr  ^6[%s]\n\"",
                           qnum, desc, prog, goal, cr, fr, mode);
    }
    qnum++;
  }

  if (totalDone >= SV_QUEST_DAILY_COUNT) {
    cJSON *lrtF = cJSON_GetObjectItemCaseSensitive(dq, "last_refresh_time");
    time_t lastRefresh = lrtF ? (time_t)lrtF->valuedouble : 0;
    long secsLeft = 86400 - (long)(time(NULL) - lastRefresh);
    if (secsLeft < 0)
      secsLeft = 0;
    SV_SendServerCommand(cl,
                         "print \"^5All done! ^7New quests in ^3%dh %dm^7.\n\"",
                         (int)(secsLeft / 3600), (int)((secsLeft % 3600) / 60));
  }

  SV_SendServerCommand(cl, "print \"^5=================================\n\n\"");
}

// ===========================================================================
//  ACHIEVEMENT SYSTEM
// ===========================================================================

typedef struct {
  const char *id;
  const char *name;
  int reward_cr;
} sv_achDef_t;

static const sv_achDef_t sv_achPool[] = {
    // ---- Combat ----
    {"first_kill", "Draw First Blood", 50},
    {"kill_10", "Warlord I", 50},
    {"kill_100", "Warlord II", 100},
    {"kill_1000", "Warlord III", 250},
    {"streak_5", "On a Roll", 50},
    {"streak_10", "Unstoppable", 100},
    {"streak_25", "God of War", 250},
    {"melee_50", "Brawler", 100},
    {"bomb_20", "Bomberman", 100},
    {"domination_5", "Tyrant", 100},
    // ---- Duel ----
    {"duel_win_1", "Challenger", 50},
    {"duel_win_10", "Duelist", 100},
    {"duel_win_50", "Master Duelist", 250},
    {"duel_win_100", "Grand Champion", 500},
    {"rank_up", "Rising Star", 50},
    {"high_elo", "Elite", 250},
    {"bounty_claim", "Bounty Hunter", 100},
    // ---- Economy ----
    {"credits_1000", "Coin Collector", 50},
    {"credits_10000", "Moneybags", 150},
    {"quest_10", "Quest Hunter", 150},
    // ---- Milestones ----
    {"level_5", "Initiate", 50},
    {"level_10", "Apprentice", 100},
    {"level_25", "Veteran", 250},
    {"win_msg_owner", "Showoff", 50},
    {NULL, NULL, 0}};

static const sv_achDef_t *SV_Ach_Find(const char *id) {
  for (int i = 0; sv_achPool[i].id != NULL; i++) {
    if (!Q_stricmp(sv_achPool[i].id, id))
      return &sv_achPool[i];
  }
  return NULL;
}

/*
==================
SV_Ranked_GrantAchievement
Grants an achievement to a player if they don't already have it.
Notifies the player privately and rewards credits.
==================
*/
void SV_Ranked_GrantAchievement(const char *username, const char *achId,
                                client_t *cl) {
  if (!username || !achId)
    return;

  cJSON *acc = SV_Ranked_GetAccount(username);
  if (!acc)
    return;

  // Check / create achievements array — guard against legacy corruption
  cJSON *achArr = cJSON_GetObjectItemCaseSensitive(acc, "achievements");
  if (achArr && !cJSON_IsArray(achArr)) {
    // Field exists but is the wrong type (string, number, etc.) — delete and recreate
    Com_Printf("[RANKED] WARNING: 'achievements' field for %s is not an array (type %d) — resetting.\n",
               username, achArr->type);
    cJSON_DeleteItemFromObject(acc, "achievements");
    achArr = NULL;
  }
  if (!achArr) {
    achArr = cJSON_CreateArray();
    cJSON_AddItemToObject(acc, "achievements", achArr);
  }

  // Already have it?
  cJSON *item;
  cJSON_ArrayForEach(item, achArr) {
    if (cJSON_IsString(item) && !Q_stricmp(item->valuestring, achId))
      return; // already unlocked
  }

  // Grant it
  cJSON_AddItemToArray(achArr, cJSON_CreateString(achId));

  const sv_achDef_t *def = SV_Ach_Find(achId);
  const char *displayName = def ? def->name : achId;
  int reward = def ? def->reward_cr : 50;

  UpdateAccountCredits(username, reward);
  SV_Ranked_SaveAccounts();

  if (cl) {
    SV_SendServerCommand(
        cl, "chat \"^5[ACHIEVEMENT] ^3%s ^7(+^5%d CR^7)\"",
        displayName, reward);
    SV_SendServerCommand(cl, "cp \"^5ACHIEVEMENT UNLOCKED!\n^3%s\"",
                         displayName);
  }

  Com_Printf("[RANKED] Achievement '%s' granted to %s (+%d CR)\n", achId,
             username, reward);
}

/*
==================
SV_Ranked_CheckKillAchievements
Called after every kill — checks kill-count and streak milestones.
==================
*/
void SV_Ranked_CheckKillAchievements(const char *username, int totalKills,
                                     int streak, int meleeKills, int bombKills,
                                     int dominations, client_t *cl) {
  if (!username)
    return;

  // First kill
  if (totalKills == 1)
    SV_Ranked_GrantAchievement(username, "first_kill", cl);

  // Kill milestones
  if (totalKills >= 10)
    SV_Ranked_GrantAchievement(username, "kill_10", cl);
  if (totalKills >= 100)
    SV_Ranked_GrantAchievement(username, "kill_100", cl);
  if (totalKills >= 1000)
    SV_Ranked_GrantAchievement(username, "kill_1000", cl);

  // Streak milestones
  if (streak >= 5)
    SV_Ranked_GrantAchievement(username, "streak_5", cl);
  if (streak >= 10)
    SV_Ranked_GrantAchievement(username, "streak_10", cl);
  if (streak >= 25)
    SV_Ranked_GrantAchievement(username, "streak_25", cl);

  // Melee kills
  if (meleeKills >= 50)
    SV_Ranked_GrantAchievement(username, "melee_50", cl);

  // Bomb kills
  if (bombKills >= 20)
    SV_Ranked_GrantAchievement(username, "bomb_20", cl);

  // Dominations
  if (dominations >= 5)
    SV_Ranked_GrantAchievement(username, "domination_5", cl);
}

/*
==================
SV_Ranked_CheckDuelAchievements
Called after a duel ends — checks win/ELO milestones.
==================
*/
void SV_Ranked_CheckDuelAchievements(const char *username, int duelWins,
                                     int elo, client_t *cl) {
  if (!username)
    return;

  if (duelWins >= 1)
    SV_Ranked_GrantAchievement(username, "duel_win_1", cl);
  if (duelWins >= 10)
    SV_Ranked_GrantAchievement(username, "duel_win_10", cl);
  if (duelWins >= 50)
    SV_Ranked_GrantAchievement(username, "duel_win_50", cl);
  if (duelWins >= 100)
    SV_Ranked_GrantAchievement(username, "duel_win_100", cl);

  if (elo >= 1500)
    SV_Ranked_GrantAchievement(username, "high_elo", cl);
}

/*
==================
SV_Ranked_CheckLevelAchievements
==================
*/
void SV_Ranked_CheckLevelAchievements(const char *username, int newLevel,
                                      client_t *cl) {
  if (!username)
    return;
  if (newLevel >= 10)
    SV_Ranked_GrantAchievement(username, "level_10", cl);
  if (newLevel >= 25)
    SV_Ranked_GrantAchievement(username, "level_25", cl);
  if (newLevel >= 50)
    SV_Ranked_GrantAchievement(username, "level_50", cl);
  if (newLevel >= 100)
    SV_Ranked_GrantAchievement(username, "level_100", cl);
}

/*
==================
SV_Ranked_CheckStreakAchievements
==================
*/
void SV_Ranked_CheckStreakAchievements(const char *username, int streak,
                                       client_t *cl) {
  if (!username)
    return;
  if (streak >= 5)
    SV_Ranked_GrantAchievement(username, "streak_5", cl);
  if (streak >= 10)
    SV_Ranked_GrantAchievement(username, "streak_10", cl);
  if (streak >= 25)
    SV_Ranked_GrantAchievement(username, "streak_25", cl);
}

/*
==================
SV_Ranked_CheckEconomyAchievements
==================
*/
void SV_Ranked_CheckEconomyAchievements(const char *username, client_t *cl) {
  if (!username)
    return;
  cJSON *acc = SV_Ranked_GetAccount(username);
  if (!acc)
    return;
  cJSON *cr = cJSON_GetObjectItemCaseSensitive(acc, "credits");
  int credits = cr ? cr->valueint : 0;

  if (credits >= 1000)
    SV_Ranked_GrantAchievement(username, "rich_1000", cl);
  if (credits >= 10000)
    SV_Ranked_GrantAchievement(username, "rich_10000", cl);
}

void SV_Ranked_ShowAchievements(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first to view achievements.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc) return;

  cJSON *achArr = cJSON_GetObjectItemCaseSensitive(acc, "achievements");
  SV_SendServerCommand(cl, "print \"\n^5=== YOUR ACHIEVEMENTS ===\n\"");

  if (!achArr || !cJSON_IsArray(achArr) || cJSON_GetArraySize(achArr) == 0) {
    SV_SendServerCommand(cl, "print \"^7  (none yet - keep playing!)\n\"");
    SV_SendServerCommand(cl, "print \"^5=========================\n\n\"");
    return;
  }

  int total = 0;
  cJSON *item;
  cJSON_ArrayForEach(item, achArr) {
    if (!cJSON_IsString(item)) continue;
    const char *id = item->valuestring;
    const sv_achDef_t *def = SV_Ach_Find(id);
    const char *name = def ? def->name : id;
    
    SV_SendServerCommand(cl, va("print \"^3 * ^7%s\n\"", name));
    total++;
  }

  SV_SendServerCommand(cl, va("print \"^5Total: ^2%d ^5achievements\n\"", total));
  SV_SendServerCommand(cl, "print \"^5=========================\n\n\"");
}

/*
==================
SV_Ranked_ShowShop
==================
*/
void SV_Ranked_ShowShop(client_t *cl) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  cJSON *acc = r->loggedIn ? SV_Ranked_GetAccount(r->username) : NULL;
  cJSON *credPtr =
      acc ? cJSON_GetObjectItemCaseSensitive(acc, "credits") : NULL;
  int credits = credPtr ? credPtr->valueint : 0;

  SV_SendServerCommand(cl, "print \"\n^5========= RANKED SHOP =========\n\"");
  if (r->loggedIn) {
    SV_SendServerCommand(cl, "print \"^7Your Credits: ^5%d\n\"", credits);
  }
  SV_SendServerCommand(cl, "print \"^5Item Key          Description      "
                           "                 Price  Sell\n\"");
  SV_SendServerCommand(
      cl, "print "
          "\"^5-------------------------------------------------------\n\"");
  for (int i = 0; sv_shopItems[i].key != NULL; i++) {
    int price = SV_Ranked_GetShopPrice(sv_shopItems[i].key, sv_shopItems[i].price);
    int sellBack = SV_Ranked_GetShopSellBack(sv_shopItems[i].key, sv_shopItems[i].sellBack);
    SV_SendServerCommand(cl,
                         "print \"^3%-17s ^7%s ^5%d cr^7 (sell: ^3%d^7)\n\"",
                         sv_shopItems[i].key, sv_shopItems[i].display,
                         price, sellBack);
  }
  SV_SendServerCommand(cl, "print \"\n^5Commands: ^7!buy <key>  !sell "
                           "<key>  !use <key>  !inventory\n\"");
  SV_SendServerCommand(cl, "print \"^5===============================\n\n\"");
}

// ============================================================
//  SHOP - BUY / SELL / USE
//  Items are stored in JSON: "inventory": { "item_name": qty }
// ============================================================

/*
==================
SV_Ranked_ShopBuy
==================
*/
void SV_Ranked_ShopBuy(client_t *cl, const char *itemName) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first to buy items.\"");
    return;
  }

  const shopItem_t *item = FindShopItem(itemName);
  if (!item) {
    SV_SendServerCommand(cl,
                         "chat \"^1Unknown item. Type ^3!shop^1 to browse.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  cJSON *credPtr = cJSON_GetObjectItemCaseSensitive(acc, "credits");
  int credits = credPtr ? credPtr->valueint : 0;

  int price = SV_Ranked_GetShopPrice(item->key, item->price);

  if (credits < price) {
    SV_SendServerCommand(
        cl, "chat \"^1Not enough credits. Need ^5%d^1, have ^5%d^1.\"",
        price, credits);
    return;
  }

  // Deduct cost
  UpdateAccountCredits(r->username, -price);

  // Add to inventory
  cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
  if (!inv) {
    inv = cJSON_AddObjectToObject(acc, "inventory");
  }
  cJSON *owned = cJSON_GetObjectItemCaseSensitive(inv, item->key);
  if (owned) {
    cJSON_SetNumberValue(owned, owned->valueint + 1);
  } else {
    cJSON_AddNumberToObject(inv, item->key, 1);
  }

  SV_Ranked_SaveAccounts();
  SV_SendServerCommand(cl, "chat \"^2Purchased ^5%s ^7for ^5%d Credits^7!\"",
                       item->display, price);

  // Give immediate weapon if it is a weapon (key starts with "wp_")
  if (!Q_stricmpn(item->key, "wp_", 3)) {
    SV_Ranked_GiveWeapon(cl, item->key, qtrue);
  }
}

/*
==================
SV_Ranked_ShopSell
==================
*/
void SV_Ranked_ShopSell(client_t *cl, const char *itemName) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first to sell items.\"");
    return;
  }

  const shopItem_t *item = FindShopItem(itemName);
  if (!item) {
    SV_SendServerCommand(cl, "chat \"^1Unknown item.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
  cJSON *owned = inv ? cJSON_GetObjectItemCaseSensitive(inv, item->key) : NULL;

  if (!owned || owned->valueint <= 0) {
    SV_SendServerCommand(cl, "chat \"^1You don't own any ^5%s^1.\"",
                         item->display);
    return;
  }

  int sellBack = SV_Ranked_GetShopSellBack(item->key, item->sellBack);

  cJSON_SetNumberValue(owned, owned->valueint - 1);
  UpdateAccountCredits(r->username, sellBack);
  SV_Ranked_SaveAccounts();
  SV_SendServerCommand(cl, "chat \"^2Sold ^5%s^7 for ^5%d Credits^7.\"",
                       item->display, sellBack);
}

/*
==================
SV_Ranked_ShopUse
==================
*/
void SV_Ranked_ShopUse(client_t *cl, const char *itemName) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first to use items.\"");
    return;
  }

  const shopItem_t *item = FindShopItem(itemName);
  if (!item) {
    SV_SendServerCommand(cl, "chat \"^1Unknown item.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
  cJSON *owned = inv ? cJSON_GetObjectItemCaseSensitive(inv, item->key) : NULL;

  if (!owned || owned->valueint <= 0) {
    SV_SendServerCommand(cl, "chat \"^1You don't own any ^5%s^1.\"",
                         item->display);
    return;
  }

  // Consume one item
  cJSON_SetNumberValue(owned, owned->valueint - 1);
  SV_Ranked_SaveAccounts();

  // Apply effect
  if (!Q_stricmp(item->key, "xp_boost")) {
    r->activeXpBoost = 1;
    SV_SendServerCommand(cl,
                         "chat \"^2XP Boost active! ^7+50%% XP this round.\"");
  } else if (!Q_stricmp(item->key, "cr_boost")) {
    r->activeCrBoost = 1;
    SV_SendServerCommand(
        cl, "chat \"^2Credit Boost active! ^7+50%% credits this round.\"");
  } else if (!Q_stricmp(item->key, "lucky_charm")) {
    r->activeLuckyCharm = 1;
    SV_SendServerCommand(
        cl, "chat \"^5Lucky Charm activated! ^7+10%% roll luck.\"");
  } else if (!Q_stricmp(item->key, "yoda_scroll")) {
    static const char *yodaQuotes[] = {
        "Do or do not. There is no try.",
        "Fear is the path to the dark side.",
        "The greatest teacher, failure is.",
        "Patience you must have, my young Padawan.",
        "Size matters not. Look at me. Judge me by my size, do you?",
        "You must unlearn what you have learned.",
    };
    int qi = rand() % 6;
    SV_SendServerCommand(cl, "chat \"^2[Master Yoda] ^7%s\"", yodaQuotes[qi]);
  } else if (!Q_stricmp(item->key, "jedi_holocron")) {
    static const char *jediSecrets[] = {
        "The Force is not just energy; it is life itself.",
        "True power lies in balance, not dominance.",
        "The Jedi Code is a guide, not a rulebook.",
        "The Force binds all living things, even across galaxies.",
        "The ancient Jedi once used the Force to heal entire planets.",
    };
    int qi = rand() % 5;
    SV_SendServerCommand(cl, "chat \"^6[Jedi Holocron] ^7%s\"",
                         jediSecrets[qi]);
  } else if (!Q_stricmp(item->key, "sith_holocron")) {
    static const char *sithSecrets[] = {
        "Power is the only truth. Everything else is weakness.",
        "Peace is a lie. There is only passion.",
        "The dark side offers strength beyond any Jedi's comprehension.",
        "The Rule of Two ensures the Sith's eternal survival.",
        "Mercy is a disease. Purge it from yourself.",
    };
    int qi = rand() % 5;
    SV_SendServerCommand(cl, "chat \"^1[Sith Holocron] ^7%s\"",
                         sithSecrets[qi]);
  } else if (!Q_stricmp(item->key, "jedi_manual")) {
    static const char *trainingTips[] = {
        "You can block all 7 zones with perfect timing.",
        "Force pull before a swing disrupts your opponent's rhythm.",
        "Speed varies between saber stances — choose wisely.",
        "Force Seeing reveals cloaked opponents before they strike.",
        "A jump-attack combo can break even the strongest defenses.",
    };
    int qi = rand() % 5;
    SV_SendServerCommand(cl, "chat \"^3[Jedi Manual] ^7%s\"", trainingTips[qi]);
  } else if (!Q_stricmp(item->key, "jedaii_secret")) {
    SV_SendServerCommand(
        cl, "chat \"^5[Jedaii Secret] ^7Congratulations on spending 1 "
            "million credits. The real secret? There is no secret. ^1XD\"");
  } else if (!Q_stricmp(item->key, "elo_boost")) {
    r->activeEloBoost = 1;
    SV_SendServerCommand(cl, "chat \"^5Elo Boost activated! ^7+15%% FR "
                             "for your next duel win.\"");
  } else if (!Q_stricmpn(item->key, "wp_", 3)) {
    SV_Ranked_GiveWeapon(cl, item->key, qtrue);
  } else {
    SV_SendServerCommand(cl, "chat \"^3Item used.\"");
  }
}

/*
==================
SV_Ranked_Cmd_SetWinMsg
==================
*/
void SV_Ranked_Cmd_SetWinMsg(client_t *cl, const char *msg) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  if (!SV_Ranked_IsAdmin(cl)) {
    cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
    cJSON *owned =
        inv ? cJSON_GetObjectItemCaseSensitive(inv, "win_msg") : NULL;
    if (!owned || owned->valueint <= 0) {
      SV_SendServerCommand(cl, "chat \"^1You must buy ^5Custom Win Msg "
                               "^1from the shop first!\"");
      return;
    }
  }

  cJSON *winMsg = cJSON_GetObjectItemCaseSensitive(acc, "winMsg");
  if (winMsg) {
    cJSON_ReplaceItemInObject(acc, "winMsg", cJSON_CreateString(msg));
  } else {
    cJSON_AddStringToObject(acc, "winMsg", msg);
  }

  SV_Ranked_SaveAccounts();
  SV_SendServerCommand(cl, "chat \"^2Your custom win message has been set!\"");
  SV_Ranked_GrantAchievement(r->username, "win_msg_owner", cl);
}

/*
==================
SV_Ranked_Cmd_SetWinSnd
==================
*/
void SV_Ranked_Cmd_SetWinSnd(client_t *cl, const char *snd) {
  int clientNum = cl - svs.clients;
  rankedMatchState_t *r = &sv_rankedPlayers[clientNum];

  if (!r->loggedIn) {
    SV_SendServerCommand(cl, "chat \"^3Login first.\"");
    return;
  }

  cJSON *acc = SV_Ranked_GetAccount(r->username);
  if (!acc)
    return;

  if (!SV_Ranked_IsAdmin(cl)) {
    cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
    cJSON *owned =
        inv ? cJSON_GetObjectItemCaseSensitive(inv, "win_snd") : NULL;
    if (!owned || owned->valueint <= 0) {
      SV_SendServerCommand(cl, "chat \"^1You must buy ^5Custom Win Snd "
                               "^1from the shop first!\"");
      return;
    }
  }

  cJSON *winSnd = cJSON_GetObjectItemCaseSensitive(acc, "winSnd");
  if (winSnd) {
    cJSON_ReplaceItemInObject(acc, "winSnd", cJSON_CreateString(snd));
  } else {
    cJSON_AddStringToObject(acc, "winSnd", snd);
  }

  SV_Ranked_SaveAccounts();
  SV_SendServerCommand(cl, "chat \"^2Your custom win sound has been set!\"");
}

/*
==================
SV_Ranked_Cmd_AdminGiveItem
==================
*/
void SV_Ranked_Cmd_AdminGiveItem(client_t *cl, const char *target,
                                 const char *itemKey, int amount) {
  if (!SV_Ranked_IsAdmin(cl)) {
    SV_SendServerCommand(cl, "chat \"^1Admin only command.\"");
    return;
  }

  const shopItem_t *item = FindShopItem(itemKey);
  if (!item) {
    SV_SendServerCommand(cl, "chat \"^1Unknown shop item: ^3%s\"", itemKey);
    return;
  }

  // Resolve target: try online players by name first, then fall back to
  // exact offline username lookup so both !giveitem Liki key 1
  // and !giveitem ranked_0001 key 1 work correctly.
  cJSON *acc = NULL;
  int targetClient = SV_Ranked_FindPlayerByNameOrId(target);
  if (targetClient >= 0) {
    // Online player — get account via their logged-in username
    rankedMatchState_t *rTarget = &sv_rankedPlayers[targetClient];
    if (rTarget->loggedIn) {
      acc = SV_Ranked_GetAccount(rTarget->username);
    }
  }
  if (!acc) {
    // Fallback: offline account keyed by exact username / display-name string
    acc = SV_Ranked_GetAccount(target);
  }
  if (!acc) {
    SV_SendServerCommand(cl, "chat \"^1Target not found: ^3%s ^7(must be online or exact username)\"",
                         target);
    return;
  }

  cJSON *inv = cJSON_GetObjectItemCaseSensitive(acc, "inventory");
  if (!inv) {
    inv = cJSON_AddObjectToObject(acc, "inventory");
  }

  cJSON *owned = cJSON_GetObjectItemCaseSensitive(inv, itemKey);
  if (owned) {
    cJSON_SetNumberValue(owned, owned->valueint + amount);
  } else {
    cJSON_AddNumberToObject(inv, itemKey, amount);
  }

  SV_Ranked_SaveAccounts();
  SV_SendServerCommand(cl, "chat \"^2Gave ^5%d %s ^2to ^7%s^2.\"", amount,
                       item->display, target);
}
// -----------------------------------------------------------------------------
// Server console maintenance commands (registered in SV_AddOperatorCommands)
// -----------------------------------------------------------------------------
void SV_RankedResetPass_f(void) {
  if (Cmd_Argc() < 2) {
    Com_Printf("Usage: ranked_resetpass <username> [newpass]\n");
    return;
  }

  const char *userArg = Cmd_Argv(1);
  const char *newPassArg = (Cmd_Argc() >= 3) ? Cmd_Argv(2) : NULL;

  if (!accountsDB) {
    SV_Ranked_LoadAccounts();
  }

  char key[MAX_AUTH_STRING];
  Ranked_SafeUserKey(userArg, key, sizeof(key));
  cJSON *acc = SV_Ranked_GetAccount(key);
  if (!acc) {
    Com_Printf("[RANKED] No account found for '%s'\n", key);
    return;
  }

  char generated[MAX_AUTH_STRING];
  if (!newPassArg || !newPassArg[0]) {
    SV_Ranked_GenerateRandomPassword(generated, sizeof(generated));
    newPassArg = generated;
  }

  cJSON *pw = cJSON_GetObjectItemCaseSensitive(acc, "password");
  if (pw) {
    cJSON_ReplaceItemInObject(acc, "password", cJSON_CreateString(newPassArg));
  } else {
    cJSON_AddStringToObject(acc, "password", newPassArg);
  }

  SV_Ranked_SaveAccounts();
  Com_Printf("[RANKED] Password reset for '%s'. New password: %s\n", key,
             newPassArg);
}

void SV_RankedClearAccounts_f(void) {
  if (accountsDB) {
    cJSON_Delete(accountsDB);
    accountsDB = NULL;
  }
  accountsDB = cJSON_CreateObject();
  SV_Ranked_SaveAccounts();
  Com_Printf("[RANKED] Accounts database wiped.\n");
}
