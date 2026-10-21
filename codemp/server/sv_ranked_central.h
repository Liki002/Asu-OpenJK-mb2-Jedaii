// sv_ranked_central.h: engine glue for the central account service client.
#ifndef SV_RANKED_CENTRAL_H
#define SV_RANKED_CENTRAL_H

#include "qcommon/q_shared.h"

void SV_Ranked_CentralInit(void);
void SV_Ranked_CentralShutdown(void);
void SV_Ranked_CentralFrame(void);
qboolean SV_Ranked_CentralActive(void);    // enabled and connected
qboolean SV_Ranked_CentralEnabled(void);   // enabled (accounts come from the service, never from accounts.json)

// sv_ranked_central_accounts.cpp: account flows in central mode
struct client_s;
void SV_Ranked_Central_Tick(void);
qboolean SV_Ranked_Central_InLegacyCall(void);
void SV_Ranked_Central_OnClientConnect(struct client_s *cl);
void SV_Ranked_Central_Login(struct client_s *cl, const char *username, const char *password);
void SV_Ranked_Central_ChangePassword(struct client_s *cl, const char *newPassword);
void SV_Ranked_Central_ReleasePlayer(int clientNum);
void SV_Ranked_Central_FlushAccounts(qboolean force);
void SV_Ranked_Central_Drain(void);

// leaderboards / offline lookups / rename
struct cJSON;
enum { CENTRAL_TOP_ELO = 0, CENTRAL_TOP_CREDITS, CENTRAL_TOP_POTATO, CENTRAL_TOP_WANTED };
int SV_Ranked_Central_TopRows(struct client_s *cl, int kind, const char *mode, const char *field, struct cJSON **rowsOut);
int SV_Ranked_Central_Lookup(struct client_s *cl, const char *name, struct cJSON **out);
void SV_Ranked_Central_Rename(struct client_s *cl, const char *newName);

void SV_RankedCentralStatus_f(void);
void SV_RankedCentralPing_f(void);

#endif
