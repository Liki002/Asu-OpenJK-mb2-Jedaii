// sv_ranked_central_accounts.cpp
//
// Account flows through the central account service (see scratch/central_accounts_design.md).
// Only active when sv_ranked_central_enable is 1; with it off nothing here runs and accounts.json is used as before.
//
// Approach: accountsDB stays the in-memory cache the rest of the ranked code already uses, but in central mode it
// only holds the accounts of players that are checked out to this server. On connect the account is fetched from the
// service into the cache and then the existing login code runs unchanged. Changes are pushed back as whole-account
// snapshots, and the account is released on disconnect / logout.
//
// If the service is unreachable the player stays on a temporary account (not persisted) and the fetch is retried.

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "server.h"
#include "sv_ranked_db.h"
#include "sv_ranked_central_core.h"
#include "sv_ranked_central.h"

extern cJSON *accountsDB;

namespace {

const int MAXC = 64;
const int PUSH_MIN_INTERVAL_MS = 5000;
const int TOUCH_INTERVAL_MS = 60000;
const int RETRY_INTERVAL_MS = 5000;

unsigned s_serial[MAXC];              // bumped on every connect of a slot; stale answers are ignored
std::string s_lastPush[MAXC];         // JSON last pushed for the account in this slot
int s_lastPushTime[MAXC];
bool s_wantAccount[MAXC];             // connected, not logged in yet: fetch (or retry fetching) the account
int s_retryAt[MAXC];
bool s_inFlight[MAXC];                // a get/register for this slot is on the wire: do not start another
int s_inFlightSince[MAXC];
bool s_notifiedTemp[MAXC];
bool s_inLegacyCall = false;
int s_lastTick = 0;
int s_lastTouch = 0;

struct ReleaseJob {
  std::string username;
  std::string json;
};
std::vector<ReleaseJob> s_releaseQueue;

struct SlotCtx {
  int n;
  unsigned serial;
  char guid[64];
  char username[MAX_AUTH_STRING];
  char password[MAX_AUTH_STRING];
};

bool SlotValid(const SlotCtx *c) {
  return c->n >= 0 && c->n < MAXC && c->n < sv_maxclients->integer && svs.clients[c->n].state >= CS_CONNECTED &&
         s_serial[c->n] == c->serial;
}

const char *PlayerGuid(client_t *cl) {
  const char *guid = Info_ValueForKey(cl->userinfo, "ja_guid");
  if (!guid || !guid[0])
    guid = Info_ValueForKey(cl->userinfo, "cl_guid");
  return guid;
}

// A copy of the account without any password material (never sent to the service in data).
cJSON *CopyForSend(cJSON *acc) {
  cJSON *copy = cJSON_Duplicate(acc, 1);
  if (copy) {
    cJSON_DeleteItemFromObject(copy, "password");
    cJSON_DeleteItemFromObject(copy, "password_hash");
  }
  return copy;
}

std::string Render(cJSON *acc) {
  cJSON *copy = CopyForSend(acc);
  std::string out;
  if (copy) {
    char *txt = cJSON_PrintUnformatted(copy);
    if (txt) { out = txt; free(txt); }
    cJSON_Delete(copy);
  }
  return out;
}

const char *ErrorOf(cJSON *res) {
  cJSON *e = res ? cJSON_GetObjectItemCaseSensitive(res, "error") : NULL;
  return (e && cJSON_IsString(e) && e->valuestring) ? e->valuestring : "";
}

void SendRelease(const std::string &username, const std::string &json);

void ReleaseReply(void *ctx, int ok, cJSON *res) {
  (void)ctx; (void)ok; (void)res;   // best effort; the checkout also expires on its own
}

// Puts a release on the wire, or queues it for the next tick if the service is not reachable right now.
void SendRelease(const std::string &username, const std::string &json) {
  if (CentralCore_IsReady()) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "username", username.c_str());
    if (!json.empty()) {
      cJSON *data = cJSON_Parse(json.c_str());
      if (data) cJSON_AddItemToObject(f, "data", data);
    }
    if (CentralCore_Request("release", f, ReleaseReply, NULL))
      return;
  }
  if (s_releaseQueue.size() < 256) {
    ReleaseJob j;
    j.username = username;
    j.json = json;
    s_releaseQueue.push_back(j);
  } else {
    SV_Ranked_Log("CENTRAL: release queue full, dropping final save of '%s'", username.c_str());
  }
}

// Puts the account into the cache (replacing any older copy) so the existing login code finds it.
void InstallAccount(const char *username, cJSON *account, const char *guid) {
  if (!accountsDB)
    accountsDB = cJSON_CreateObject();
  cJSON_DeleteItemFromObject(accountsDB, username);
  cJSON *copy = cJSON_Duplicate(account, 1);
  if (!copy)
    return;
  if (guid && guid[0]) {
    cJSON_DeleteItemFromObject(copy, "engine_guid");
    cJSON_AddStringToObject(copy, "engine_guid", guid);
  }
  cJSON_DeleteItemFromObject(copy, "password");
  cJSON_DeleteItemFromObject(copy, "password_hash");
  cJSON_AddItemToObject(accountsDB, username, copy);
}

void TellTemporary(int n, const char *why) {
  if (s_notifiedTemp[n])
    return;
  s_notifiedTemp[n] = true;
  SV_SendServerCommand(&svs.clients[n],
                       "print \"^5[RANKED] ^3%s You are on a temporary account for now (progress is not saved); "
                       "retrying automatically.\n\"", why);
}

void RequestAccount(int n);

// ---- connect: get / register -------------------------------------------------------------------------

void OnRegister(void *p, int ok, cJSON *res) {
  SlotCtx *c = (SlotCtx *)p;
  cJSON *username = res ? cJSON_GetObjectItemCaseSensitive(res, "username") : NULL;
  cJSON *password = res ? cJSON_GetObjectItemCaseSensitive(res, "password") : NULL;
  cJSON *account = res ? cJSON_GetObjectItemCaseSensitive(res, "account") : NULL;
  bool good = ok && username && cJSON_IsString(username) && account && cJSON_IsObject(account);

  if (!SlotValid(c)) {
    if (good) SendRelease(username->valuestring, "");   // the player left meanwhile: free the new account again
    delete c;
    return;
  }
  s_inFlight[c->n] = false;
  if (sv_rankedPlayers[c->n].loggedIn) {
    // already logged in (a duplicate request got here second): never replace the live account
    if (good && Q_stricmp(sv_rankedPlayers[c->n].username, username->valuestring))
      SendRelease(username->valuestring, "");
    delete c;
    return;
  }
  if (!good) {
    const char *why = ErrorOf(res);
    bool raced = !strcmp(why, "guid_bound") || !strcmp(why, "busy");   // someone else just created it: fetch again
    s_wantAccount[c->n] = true;
    s_retryAt[c->n] = Sys_Milliseconds() + (raced ? 300 : RETRY_INTERVAL_MS);
    if (!raced) TellTemporary(c->n, "Could not create your account.");
    delete c;
    return;
  }

  client_t *cl = &svs.clients[c->n];
  InstallAccount(username->valuestring, account, c->guid);
  s_wantAccount[c->n] = false;
  s_inLegacyCall = true;
  SV_Ranked_AutoRegisterByGUID(cl);   // finds the cached account by GUID and runs the normal login
  s_inLegacyCall = false;

  if (password && cJSON_IsString(password)) {
    SV_SendServerCommand(cl, "print \"^5[RANKED] ^2New ranked account created!\n\"");
    SV_SendServerCommand(cl, "print \"^5[RANKED] ^7User: ^2%s  ^7Pass: ^2%s\n\"", username->valuestring, password->valuestring);
    SV_SendServerCommand(cl, "print \"^5[RANKED] ^3Save these! Use ^7/changepassword <newpass> ^3to set your own.\n\"");
  }
  cJSON *acc = SV_Ranked_GetAccount(username->valuestring);
  if (acc) s_lastPush[c->n] = Render(acc);
  s_lastPushTime[c->n] = Sys_Milliseconds();
  delete c;
}

void OnGet(void *p, int ok, cJSON *res) {
  SlotCtx *c = (SlotCtx *)p;
  cJSON *account = res ? cJSON_GetObjectItemCaseSensitive(res, "account") : NULL;
  cJSON *username = res ? cJSON_GetObjectItemCaseSensitive(res, "username") : NULL;
  const char *err = ErrorOf(res);

  if (!SlotValid(c)) {
    if (ok && username && cJSON_IsString(username)) SendRelease(username->valuestring, "");
    delete c;
    return;
  }
  int n = c->n;
  s_inFlight[n] = false;

  if (sv_rankedPlayers[n].loggedIn) {
    // already logged in: ignore a late duplicate answer (it could carry an older copy of the account)
    if (ok && username && cJSON_IsString(username) && Q_stricmp(sv_rankedPlayers[n].username, username->valuestring))
      SendRelease(username->valuestring, "");
    delete c;
    return;
  }

  if (ok && account && cJSON_IsObject(account) && username && cJSON_IsString(username)) {
    client_t *cl = &svs.clients[n];
    InstallAccount(username->valuestring, account, c->guid);
    s_wantAccount[n] = false;
    s_inLegacyCall = true;
    SV_Ranked_AutoRegisterByGUID(cl);
    s_inLegacyCall = false;
    cJSON *acc = SV_Ranked_GetAccount(username->valuestring);
    if (acc) s_lastPush[n] = Render(acc);
    s_lastPushTime[n] = Sys_Milliseconds();
    delete c;
    return;
  }
  if (ok) {
    // this GUID has no account yet: create one
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "guid", c->guid);
    cJSON_AddStringToObject(f, "displayName", svs.clients[n].name);
    SlotCtx *next = new SlotCtx(*c);
    if (!CentralCore_Request("register", f, OnRegister, next)) {
      delete next;
      s_retryAt[n] = Sys_Milliseconds() + RETRY_INTERVAL_MS;
    } else {
      s_inFlight[n] = true;
      s_inFlightSince[n] = Sys_Milliseconds();
    }
    delete c;
    return;
  }
  if (!strcmp(err, "active_elsewhere")) {
    cJSON *holder = res ? cJSON_GetObjectItemCaseSensitive(res, "holder") : NULL;
    s_wantAccount[n] = true;
    s_retryAt[n] = Sys_Milliseconds() + RETRY_INTERVAL_MS * 3;
    if (!s_notifiedTemp[n]) {
      s_notifiedTemp[n] = true;
      SV_SendServerCommand(&svs.clients[n], "print \"^5[RANKED] ^3Your account is active on another server (%s). "
                           "You are on a temporary account until it is free.\n\"",
                           (holder && cJSON_IsString(holder)) ? holder->valuestring : "?");
    }
  } else {
    s_wantAccount[n] = true;
    s_retryAt[n] = Sys_Milliseconds() + RETRY_INTERVAL_MS;
    TellTemporary(n, "The account service is not reachable.");
  }
  delete c;
}

void RequestAccount(int n) {
  client_t *cl = &svs.clients[n];
  const char *guid = PlayerGuid(cl);
  if (!guid || !guid[0])
    return;                                   // legacy code already told the player to /login manually
  if (s_inFlight[n])
    return;                                   // the answer to the previous request is still on its way
  if (!CentralCore_IsReady()) {
    s_retryAt[n] = Sys_Milliseconds() + RETRY_INTERVAL_MS;
    TellTemporary(n, "The account service is not reachable.");
    return;
  }
  SlotCtx *c = new SlotCtx();
  memset(c, 0, sizeof(*c));
  c->n = n;
  c->serial = s_serial[n];
  Q_strncpyz(c->guid, guid, sizeof(c->guid));
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "guid", guid);
  if (!CentralCore_Request("get", f, OnGet, c)) {
    delete c;
    s_retryAt[n] = Sys_Milliseconds() + RETRY_INTERVAL_MS;
  } else {
    s_inFlight[n] = true;
    s_inFlightSince[n] = Sys_Milliseconds();
  }
}

// ---- manual login ------------------------------------------------------------------------------------

void OnLogin(void *p, int ok, cJSON *res) {
  SlotCtx *c = (SlotCtx *)p;
  cJSON *account = res ? cJSON_GetObjectItemCaseSensitive(res, "account") : NULL;
  cJSON *username = res ? cJSON_GetObjectItemCaseSensitive(res, "username") : NULL;
  const char *err = ErrorOf(res);

  if (!SlotValid(c)) {
    if (ok && username && cJSON_IsString(username)) SendRelease(username->valuestring, "");
    delete c;
    return;
  }
  client_t *cl = &svs.clients[c->n];

  if (ok && account && cJSON_IsObject(account) && username && cJSON_IsString(username)) {
    rankedMatchState_t *r = &sv_rankedPlayers[c->n];
    // switching accounts: free the one this slot was using (final snapshot first)
    if (r->loggedIn && r->username[0] && Q_stricmp(r->username, username->valuestring))
      SV_Ranked_Central_ReleasePlayer(c->n);

    InstallAccount(username->valuestring, account, NULL);
    cJSON *acc = SV_Ranked_GetAccount(username->valuestring);
    // the legacy login compares against a "password" field: give it the one just verified by the service
    if (acc) cJSON_AddStringToObject(acc, "password", c->password);
    s_wantAccount[c->n] = false;
    s_inLegacyCall = true;
    SV_Ranked_LoginOrRegister(cl, username->valuestring, c->password);
    s_inLegacyCall = false;
    acc = SV_Ranked_GetAccount(username->valuestring);
    if (acc) {
      cJSON_DeleteItemFromObject(acc, "password");
      cJSON_DeleteItemFromObject(acc, "password_hash");
      s_lastPush[c->n] = Render(acc);
    }
    s_lastPushTime[c->n] = Sys_Milliseconds();
  } else if (!strcmp(err, "rate_limited")) {
    SV_SendServerCommand(cl, "chat \"^1Too many attempts. Try again in a few minutes.\"");
  } else if (!strcmp(err, "active_elsewhere")) {
    cJSON *holder = res ? cJSON_GetObjectItemCaseSensitive(res, "holder") : NULL;
    SV_SendServerCommand(cl, "chat \"^3That account is active on another server (%s).\"",
                         (holder && cJSON_IsString(holder)) ? holder->valuestring : "?");
  } else if (!strcmp(err, "bad_credentials")) {
    SV_SendServerCommand(cl, "chat \"^1Incorrect username or password.\"");
  } else {
    SV_SendServerCommand(cl, "chat \"^1The account service is not reachable. Try again in a moment.\"");
  }
  delete c;
}

void OnPutReply(void *p, int ok, cJSON *res) {
  SlotCtx *c = (SlotCtx *)p;
  if (!ok && c->n >= 0 && c->n < MAXC && s_serial[c->n] == c->serial) {
    s_lastPush[c->n].clear();                         // resend on the next flush
    if (!strcmp(ErrorOf(res), "not_holder")) {
      // another server took this account over (we were unreachable for too long): stop saving it here
      rankedMatchState_t *r = &sv_rankedPlayers[c->n];
      SV_Ranked_Log("CENTRAL: lost the checkout of '%s'", c->username);
      if (r->loggedIn && !Q_stricmp(r->username, c->username)) {
        r->loggedIn = qfalse;
        r->isTemp = qtrue;
        cJSON_DeleteItemFromObject(accountsDB, c->username);
        r->username[0] = '\0';
        s_wantAccount[c->n] = true;
        s_retryAt[c->n] = Sys_Milliseconds() + RETRY_INTERVAL_MS * 3;
        SV_SendServerCommand(&svs.clients[c->n], "print \"^5[RANKED] ^3Your account was taken over by another server. "
                             "You are on a temporary account now.\n\"");
      }
    }
  }
  delete c;
}

void OnPassReply(void *p, int ok, cJSON *res) {
  SlotCtx *c = (SlotCtx *)p;
  (void)res;
  if (SlotValid(c)) {
    SV_SendServerCommand(&svs.clients[c->n], ok ? "chat \"^2Password successfully changed.\""
                                                : "chat \"^1Could not change the password right now. Try again.\"");
  }
  delete c;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------
// Public API (used by sv_ranked_db.cpp / sv_ranked_cmds.cpp)
// ---------------------------------------------------------------------------------------------------------

qboolean SV_Ranked_Central_InLegacyCall(void) { return s_inLegacyCall ? qtrue : qfalse; }

// Called by SV_Ranked_AutoRegisterByGUID in central mode instead of the file based lookup.
void SV_Ranked_Central_OnClientConnect(client_t *cl) {
  int n = (int)(cl - svs.clients);
  if (n < 0 || n >= MAXC)
    return;
  s_serial[n]++;
  s_lastPush[n].clear();
  s_lastPushTime[n] = 0;
  s_notifiedTemp[n] = false;
  s_wantAccount[n] = true;
  s_retryAt[n] = 0;
  s_inFlight[n] = false;
  RequestAccount(n);
}

// Manual /login in central mode.
void SV_Ranked_Central_Login(client_t *cl, const char *username, const char *password) {
  int n = (int)(cl - svs.clients);
  if (n < 0 || n >= MAXC)
    return;
  if (!CentralCore_IsReady()) {
    SV_SendServerCommand(cl, "chat \"^1The account service is not reachable. Try again in a moment.\"");
    return;
  }
  SlotCtx *c = new SlotCtx();
  memset(c, 0, sizeof(*c));
  c->n = n;
  c->serial = s_serial[n];
  Q_strncpyz(c->guid, PlayerGuid(cl), sizeof(c->guid));
  Q_strncpyz(c->username, username, sizeof(c->username));
  Q_strlwr(c->username);
  Q_strncpyz(c->password, password, sizeof(c->password));
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "guid", c->guid);
  cJSON_AddStringToObject(f, "username", c->username);
  cJSON_AddStringToObject(f, "password", password);
  if (!CentralCore_Request("login", f, OnLogin, c)) {
    delete c;
    SV_SendServerCommand(cl, "chat \"^1The account service is not reachable. Try again in a moment.\"");
  }
}

// /changepassword <new> in central mode: the service stores only a hash.
void SV_Ranked_Central_ChangePassword(client_t *cl, const char *newPassword) {
  int n = (int)(cl - svs.clients);
  rankedMatchState_t *r = &sv_rankedPlayers[n];
  if (n < 0 || n >= MAXC || !r->loggedIn || !r->username[0])
    return;
  SlotCtx *c = new SlotCtx();
  memset(c, 0, sizeof(*c));
  c->n = n;
  c->serial = s_serial[n];
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "username", r->username);
  cJSON_AddStringToObject(f, "newPassword", newPassword);
  if (!CentralCore_Request("setpass", f, OnPassReply, c)) {
    delete c;
    SV_SendServerCommand(cl, "chat \"^1Could not change the password right now. Try again.\"");
  }
}

// Frees the account of a slot (disconnect / logout / switching accounts): final snapshot + release.
void SV_Ranked_Central_ReleasePlayer(int n) {
  if (n < 0 || n >= MAXC)
    return;
  s_wantAccount[n] = false;
  s_inFlight[n] = false;
  s_serial[n]++;                                // answers to requests of the player who left are ignored
  rankedMatchState_t *r = &sv_rankedPlayers[n];
  if (!r->loggedIn || !r->username[0])
    return;

  char username[MAX_AUTH_STRING];
  Q_strncpyz(username, r->username, sizeof(username));

  // another slot on this server still using the same account: keep it checked out
  for (int i = 0; i < sv_maxclients->integer && i < MAXC; ++i) {
    if (i != n && svs.clients[i].state >= CS_CONNECTED && sv_rankedPlayers[i].loggedIn &&
        !Q_stricmp(sv_rankedPlayers[i].username, username))
      return;
  }

  cJSON *acc = SV_Ranked_GetAccount(username);
  std::string json = acc ? Render(acc) : std::string();
  SendRelease(username, json);
  if (accountsDB)
    cJSON_DeleteItemFromObject(accountsDB, username);
  s_lastPush[n].clear();
}

// Pushes changed accounts of logged-in players (whole snapshots, rate limited per player).
void SV_Ranked_Central_FlushAccounts(qboolean force) {
  if (!SV_Ranked_CentralActive() || !accountsDB || s_inLegacyCall)
    return;
  int now = Sys_Milliseconds();
  for (int n = 0; n < sv_maxclients->integer && n < MAXC; ++n) {
    rankedMatchState_t *r = &sv_rankedPlayers[n];
    if (svs.clients[n].state < CS_CONNECTED || !r->loggedIn || r->isTemp || !r->username[0])
      continue;
    cJSON *acc = SV_Ranked_GetAccount(r->username);
    if (!acc)
      continue;

    // a password change made by the legacy command ends up here as a plain "password" field: send it as setpass
    cJSON *pw = cJSON_GetObjectItemCaseSensitive(acc, "password");
    if (pw && cJSON_IsString(pw) && pw->valuestring && pw->valuestring[0]) {
      SV_Ranked_Central_ChangePassword(&svs.clients[n], pw->valuestring);
    }
    cJSON_DeleteItemFromObject(acc, "password");
    cJSON_DeleteItemFromObject(acc, "password_hash");

    std::string json = Render(acc);
    if (json.empty() || json == s_lastPush[n])
      continue;
    if (!force && now - s_lastPushTime[n] < PUSH_MIN_INTERVAL_MS)
      continue;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "username", r->username);
    cJSON *data = CopyForSend(acc);
    if (!data) { cJSON_Delete(f); continue; }
    cJSON_AddItemToObject(f, "data", data);
    SlotCtx *c = new SlotCtx();
    memset(c, 0, sizeof(*c));
    c->n = n;
    c->serial = s_serial[n];
    Q_strncpyz(c->username, r->username, sizeof(c->username));
    if (CentralCore_Request("put", f, OnPutReply, c)) {
      s_lastPush[n] = json;
      s_lastPushTime[n] = now;
    } else {
      delete c;
    }
  }
}

// Server shutdown: release everyone still logged in and give the packets a moment to leave.
void SV_Ranked_Central_Drain(void) {
  for (int n = 0; n < sv_maxclients->integer && n < MAXC; ++n) {
    if (svs.clients[n].state >= CS_CONNECTED)
      SV_Ranked_Central_ReleasePlayer(n);
  }
  int end = Sys_Milliseconds() + 300;
  while (Sys_Milliseconds() < end) {
    CentralCore_Frame();
    SV_Ranked_Central_Tick();
    if (s_releaseQueue.empty())
      break;
  }
  for (int i = 0; i < 20; ++i)
    CentralCore_Frame();
}

// Once per frame (cheap): retries, queued releases, heartbeat.
void SV_Ranked_Central_Tick(void) {
  int now = Sys_Milliseconds();
  if (now - s_lastTick < 500 && now >= s_lastTick)
    return;
  s_lastTick = now;

  if (!CentralCore_IsReady())
    return;

  while (!s_releaseQueue.empty()) {
    ReleaseJob j = s_releaseQueue.front();
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "username", j.username.c_str());
    if (!j.json.empty()) {
      cJSON *data = cJSON_Parse(j.json.c_str());
      if (data) cJSON_AddItemToObject(f, "data", data);
    }
    if (!CentralCore_Request("release", f, ReleaseReply, NULL))
      break;
    s_releaseQueue.erase(s_releaseQueue.begin());
  }

  for (int n = 0; n < sv_maxclients->integer && n < MAXC; ++n) {
    if (s_inFlight[n] && now - s_inFlightSince[n] > 15000) {   // no answer for 15 s: forget it and try again
      s_inFlight[n] = false;
      s_serial[n]++;
    }
    if (s_wantAccount[n] && svs.clients[n].state >= CS_CONNECTED && !sv_rankedPlayers[n].loggedIn && now >= s_retryAt[n])
      RequestAccount(n);
    else if (svs.clients[n].state < CS_CONNECTED)
      s_wantAccount[n] = false;
  }

  SV_Ranked_Central_FlushAccounts(qfalse);

  if (now - s_lastTouch > TOUCH_INTERVAL_MS) {
    s_lastTouch = now;
    cJSON *list = cJSON_CreateArray();
    for (int n = 0; n < sv_maxclients->integer && n < MAXC; ++n) {
      rankedMatchState_t *r = &sv_rankedPlayers[n];
      if (svs.clients[n].state >= CS_CONNECTED && r->loggedIn && !r->isTemp && r->username[0])
        cJSON_AddItemToArray(list, cJSON_CreateString(r->username));
    }
    if (cJSON_GetArraySize(list) > 0) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddItemToObject(f, "usernames", list);
      CentralCore_Request("touch", f, ReleaseReply, NULL);
    } else {
      cJSON_Delete(list);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------
// Step 3: leaderboards (cached `top`), offline lookups and rename.
// ---------------------------------------------------------------------------------------------------------

namespace {

const int TOP_CACHE_MS = 20000;
const int LOOKUP_CACHE_MS = 60000;
const int MAX_TOP_KEYS = 12;

struct TopEntry {
  std::string key;      // "mode|field"
  cJSON *rows;          // array of rows from the service
  int time;
  bool inFlight;
  TopEntry() : rows(NULL), time(0), inFlight(false) {}
};
std::vector<TopEntry> s_top;

struct Waiter {
  int n;
  unsigned serial;
  int kind;             // CENTRAL_TOP_*, or -1 for a lookup
  std::string key;      // top key, or the name typed for a lookup
  std::string lookupKey;
};
std::vector<Waiter> s_waiters;

struct LookupEntry {
  std::string name;
  cJSON *account;       // NULL = the service has no such account
  int time;
  LookupEntry() : account(NULL), time(0) {}
};
std::vector<LookupEntry> s_lookups;

struct TopCtx { std::string key; };
struct LookupCtx { std::string name; };

TopEntry *FindTop(const std::string &key) {
  for (size_t i = 0; i < s_top.size(); ++i)
    if (s_top[i].key == key) return &s_top[i];
  return NULL;
}

bool WaiterAlive(const Waiter &w) {
  return w.n >= 0 && w.n < MAXC && s_serial[w.n] == w.serial && svs.clients[w.n].state >= CS_CONNECTED;
}

void RunDisplay(const Waiter &w) {
  if (!WaiterAlive(w))
    return;
  client_t *cl = &svs.clients[w.n];
  switch (w.kind) {
    case CENTRAL_TOP_ELO: SV_Ranked_ShowTop(cl); break;
    case CENTRAL_TOP_ELO_ALL: SV_Ranked_ShowTopAll(cl); break;
    case CENTRAL_TOP_CREDITS: SV_Ranked_ShowTopCredits(cl); break;
    case CENTRAL_TOP_POTATO: SV_Ranked_ShowTopPotato(cl); break;
    case CENTRAL_TOP_WANTED: SV_Ranked_ShowWanted(cl); break;
    default: break;
  }
}

void OnTop(void *p, int ok, cJSON *res) {
  TopCtx *c = (TopCtx *)p;
  TopEntry *e = FindTop(c->key);
  cJSON *rows = res ? cJSON_GetObjectItemCaseSensitive(res, "rows") : NULL;
  if (e) {
    e->inFlight = false;
    if (ok && rows && cJSON_IsArray(rows)) {
      if (e->rows) cJSON_Delete(e->rows);
      e->rows = cJSON_Duplicate(rows, 1);
      e->time = Sys_Milliseconds();
    }
  }
  // answer everybody waiting for this list (they find it in the cache now, or are told it is unavailable)
  std::vector<Waiter> mine, rest;
  for (size_t i = 0; i < s_waiters.size(); ++i) {
    if (s_waiters[i].kind >= 0 && s_waiters[i].key == c->key) mine.push_back(s_waiters[i]);
    else rest.push_back(s_waiters[i]);
  }
  s_waiters.swap(rest);
  for (size_t i = 0; i < mine.size(); ++i) {
    if (e && e->rows) RunDisplay(mine[i]);
    else if (WaiterAlive(mine[i]))
      SV_SendServerCommand(&svs.clients[mine[i].n], "chat \"^1The leaderboard is not available right now.\"");
  }
  delete c;
}

void OnLookup(void *p, int ok, cJSON *res) {
  LookupCtx *c = (LookupCtx *)p;
  cJSON *account = (ok && res) ? cJSON_GetObjectItemCaseSensitive(res, "account") : NULL;
  if (ok) {
    LookupEntry *e = NULL;
    for (size_t i = 0; i < s_lookups.size(); ++i)
      if (s_lookups[i].name == c->name) { e = &s_lookups[i]; break; }
    if (!e) {
      s_lookups.push_back(LookupEntry());
      e = &s_lookups.back();
      e->name = c->name;
    }
    if (e->account) cJSON_Delete(e->account);
    e->account = (account && cJSON_IsObject(account)) ? cJSON_Duplicate(account, 1) : NULL;
    e->time = Sys_Milliseconds();
    if (s_lookups.size() > 64) {       // keep it small: drop the oldest
      size_t oldest = 0;
      for (size_t i = 1; i < s_lookups.size(); ++i)
        if (s_lookups[i].time < s_lookups[oldest].time) oldest = i;
      if (s_lookups[oldest].account) cJSON_Delete(s_lookups[oldest].account);
      s_lookups.erase(s_lookups.begin() + oldest);
    }
  }
  std::vector<Waiter> mine, rest;
  for (size_t i = 0; i < s_waiters.size(); ++i) {
    if (s_waiters[i].kind < 0 && s_waiters[i].lookupKey == c->name) mine.push_back(s_waiters[i]);
    else rest.push_back(s_waiters[i]);
  }
  s_waiters.swap(rest);
  for (size_t i = 0; i < mine.size(); ++i) {
    const Waiter &w = mine[i];
    if (!WaiterAlive(w)) continue;
    if (!ok) SV_SendServerCommand(&svs.clients[w.n], "chat \"^1The account service is not reachable.\"");
    else SV_Ranked_ShowStatsTarget(&svs.clients[w.n], w.key.c_str());   // answered from the cache this time
  }
  delete c;
}

struct RenameCtx {
  int n;
  unsigned serial;
  char oldName[MAX_AUTH_STRING];
  char newName[MAX_AUTH_STRING];
};

void OnRename(void *p, int ok, cJSON *res) {
  RenameCtx *c = (RenameCtx *)p;
  const char *err = ErrorOf(res);
  if (c->n < 0 || c->n >= MAXC || s_serial[c->n] != c->serial || svs.clients[c->n].state < CS_CONNECTED) {
    delete c;
    return;
  }
  client_t *cl = &svs.clients[c->n];
  rankedMatchState_t *r = &sv_rankedPlayers[c->n];
  if (ok && r->loggedIn && !Q_stricmp(r->username, c->oldName)) {
    cJSON *detached = accountsDB ? cJSON_DetachItemFromObject(accountsDB, c->oldName) : NULL;
    if (detached) {
      cJSON_DeleteItemFromObject(detached, "username");
      cJSON_AddStringToObject(detached, "username", c->newName);
      cJSON_AddItemToObject(accountsDB, c->newName, detached);
    }
    Q_strncpyz(r->username, c->newName, sizeof(r->username));
    s_lastPush[c->n].clear();
    SV_SendServerCommand(cl, "chat \"^2Username changed to ^7%s^2!\"", c->newName);
    SV_Ranked_Log("RENAME: client %d renamed '%s' to '%s'", c->n, c->oldName, c->newName);
  } else if (!strcmp(err, "exists")) {
    SV_SendServerCommand(cl, "chat \"^1Username '^7%s^1' is already taken.\"", c->newName);
  } else if (!strcmp(err, "bad_username")) {
    SV_SendServerCommand(cl, "chat \"^1Invalid username. Use only letters, numbers, _, -\"");
  } else {
    SV_SendServerCommand(cl, "chat \"^1Could not change the username right now. Try again.\"");
  }
  delete c;
}

}  // namespace

// Rows for a leaderboard. 1 = *rowsOut set (borrowed, valid until the next frame), 0 = requested, the list is shown
// again when it arrives, -1 = unavailable.
int SV_Ranked_Central_TopRows(client_t *cl, int kind, const char *mode, const char *field, cJSON **rowsOut) {
  int n = (int)(cl - svs.clients);
  *rowsOut = NULL;
  if (n < 0 || n >= MAXC)
    return -1;
  // leaderboards are per region (EU: ranked/HQ/BG, US) unless the player asked for everybody
  const char *region = (kind == CENTRAL_TOP_ELO_ALL) ? "all" : "auto";
  std::string key = std::string(mode) + "|" + field + "|" + region;
  TopEntry *e = FindTop(key);
  if (!e) {
    if ((int)s_top.size() >= MAX_TOP_KEYS) return -1;
    s_top.push_back(TopEntry());
    e = &s_top.back();
    e->key = key;
  }
  int now = Sys_Milliseconds();
  bool stale = !e->rows || now - e->time > TOP_CACHE_MS || now < e->time;
  if (stale && !e->inFlight) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "mode", mode);
    cJSON_AddStringToObject(f, "field", field);
    cJSON_AddStringToObject(f, "region", region);
    cJSON_AddNumberToObject(f, "limit", 10);
    TopCtx *c = new TopCtx();
    c->key = key;
    if (CentralCore_Request("top", f, OnTop, c)) e->inFlight = true;
    else delete c;
  }
  if (e->rows) {
    *rowsOut = e->rows;
    return 1;
  }
  if (!e->inFlight)
    return -1;
  Waiter w;
  w.n = n; w.serial = s_serial[n]; w.kind = kind; w.key = key;
  s_waiters.push_back(w);
  SV_SendServerCommand(cl, "print \"^5[RANKED] ^7Loading...\n\"");
  return 0;
}

// Offline account by name for !stats. 1 = *out set (borrowed), 0 = looking it up, -1 = no such account / unavailable.
int SV_Ranked_Central_Lookup(client_t *cl, const char *name, cJSON **out) {
  int n = (int)(cl - svs.clients);
  *out = NULL;
  if (n < 0 || n >= MAXC || !name || !name[0])
    return -1;
  std::string key(name);
  for (size_t i = 0; i < key.size(); ++i) key[i] = (char)tolower((unsigned char)key[i]);
  int now = Sys_Milliseconds();
  for (size_t i = 0; i < s_lookups.size(); ++i) {
    if (s_lookups[i].name != key) continue;
    if (now - s_lookups[i].time <= LOOKUP_CACHE_MS && now >= s_lookups[i].time) {
      *out = s_lookups[i].account;
      return s_lookups[i].account ? 1 : -1;
    }
    break;
  }
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "name", key.c_str());
  LookupCtx *c = new LookupCtx();
  c->name = key;
  if (!CentralCore_Request("lookup", f, OnLookup, c)) {
    delete c;
    return -1;
  }
  Waiter w;
  w.n = n; w.serial = s_serial[n]; w.kind = -1; w.key = name; w.lookupKey = key;
  s_waiters.push_back(w);
  return 0;
}

// !changeusername in central mode: the service renames the stored account, then the cache is re-keyed.
void SV_Ranked_Central_Rename(client_t *cl, const char *newName) {
  int n = (int)(cl - svs.clients);
  if (n < 0 || n >= MAXC)
    return;
  rankedMatchState_t *r = &sv_rankedPlayers[n];
  if (!r->loggedIn || !r->username[0])
    return;
  SV_Ranked_Central_FlushAccounts(qtrue);   // make sure the service has the latest data before it moves
  RenameCtx *c = new RenameCtx();
  memset(c, 0, sizeof(*c));
  c->n = n;
  c->serial = s_serial[n];
  Q_strncpyz(c->oldName, r->username, sizeof(c->oldName));
  Q_strncpyz(c->newName, newName, sizeof(c->newName));
  Q_strlwr(c->newName);
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "username", c->oldName);
  cJSON_AddStringToObject(f, "newUsername", c->newName);
  if (!CentralCore_Request("rename", f, OnRename, c)) {
    delete c;
    SV_SendServerCommand(cl, "chat \"^1The account service is not reachable. Try again in a moment.\"");
  }
}
