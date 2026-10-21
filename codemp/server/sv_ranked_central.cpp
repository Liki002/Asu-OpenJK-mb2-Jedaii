// sv_ranked_central.cpp
// Engine glue for the central account service client (sv_ranked_central_core.*).
//
// Cvars (all off / empty by default, nothing changes unless sv_ranked_central_enable is 1):
//   sv_ranked_central_enable  0/1
//   sv_ranked_central_host    numeric IPv4 of the account service (default: the EU VPS)
//   sv_ranked_central_port    default 29095
//   sv_ranked_central_secret  this server's secret (the service has one per server id)
// The server id is sv_ranked_serverid (eu / us / hq / bg).

#include <stdint.h>

#include "server.h"
#include "sv_ranked_central_core.h"
#include "sv_ranked_central.h"

static cvar_t *sv_ranked_central_enable = NULL;
static cvar_t *sv_ranked_central_host = NULL;
static cvar_t *sv_ranked_central_port = NULL;
static cvar_t *sv_ranked_central_secret = NULL;

static unsigned int Ranked_CentralClock(void) { return (unsigned int)Sys_Milliseconds(); }

static void Ranked_CentralLog(const char *msg) { Com_Printf("[RANKED-CENTRAL] %s\n", msg); }

void SV_Ranked_CentralInit(void) {
  sv_ranked_central_enable = Cvar_Get("sv_ranked_central_enable", "0", CVAR_ARCHIVE);
  sv_ranked_central_host = Cvar_Get("sv_ranked_central_host", "51.195.219.239", CVAR_ARCHIVE);
  sv_ranked_central_port = Cvar_Get("sv_ranked_central_port", "29095", CVAR_ARCHIVE);
  sv_ranked_central_secret = Cvar_Get("sv_ranked_central_secret", "", CVAR_ARCHIVE);
}

void SV_Ranked_CentralShutdown(void) { CentralCore_Shutdown(); }

// Called once per server frame.
void SV_Ranked_CentralFrame(void) {
  if (!sv_ranked_central_enable)
    return;

  static int s_lastConfigSync = 0;
  int now = Sys_Milliseconds();
  if (now - s_lastConfigSync > 1000 || now < s_lastConfigSync) {
    s_lastConfigSync = now;
    CentralCore_Configure(sv_ranked_central_host->string, sv_ranked_central_port->integer,
                          Cvar_VariableString("sv_ranked_serverid"), sv_ranked_central_secret->string,
                          Ranked_CentralClock, Ranked_CentralLog);
    CentralCore_Enable(sv_ranked_central_enable->integer);
  }
  CentralCore_Frame();
}

// True when the central service is the account store right now (enabled and connected).
qboolean SV_Ranked_CentralActive(void) {
  return (sv_ranked_central_enable && sv_ranked_central_enable->integer && CentralCore_IsReady()) ? qtrue : qfalse;
}

// ---- console commands (rcon / console) ----

void SV_RankedCentralStatus_f(void) {
  Com_Printf("[RANKED-CENTRAL] enabled=%d host=%s port=%d server=%s secret=%s status: %s\n",
             sv_ranked_central_enable ? sv_ranked_central_enable->integer : 0,
             sv_ranked_central_host ? sv_ranked_central_host->string : "",
             sv_ranked_central_port ? sv_ranked_central_port->integer : 0,
             Cvar_VariableString("sv_ranked_serverid"),
             (sv_ranked_central_secret && sv_ranked_central_secret->string[0]) ? "set" : "NOT SET",
             CentralCore_Status());
}

static void Ranked_CentralPingReply(void *ctx, int ok, cJSON *res) {
  int sentAt = (int)(intptr_t)ctx;
  (void)res;
  if (ok)
    Com_Printf("[RANKED-CENTRAL] pong in %d ms\n", Sys_Milliseconds() - sentAt);
  else
    Com_Printf("[RANKED-CENTRAL] ping failed (timeout or error)\n");
}

void SV_RankedCentralPing_f(void) {
  int id = CentralCore_Request("ping", NULL, Ranked_CentralPingReply, (void *)(intptr_t)Sys_Milliseconds());
  if (!id)
    Com_Printf("[RANKED-CENTRAL] not ready (%s)\n", CentralCore_Status());
}
