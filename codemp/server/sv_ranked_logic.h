#ifndef SV_RANKED_LOGIC_H
#define SV_RANKED_LOGIC_H

#include "qcommon/q_shared.h"
#include "server.h"
#include "sv_ranked_db.h"

// Initialize/Shutdown the Ranked system
void SV_Ranked_Logic_Init(void);
void SV_Ranked_Logic_Shutdown(void);

// Kill hooking
void SV_Ranked_ProcessKill(int killerId, int victimId, int mod, const char *weaponStr,
                           const char *victimNameOverride);

// Round ending
void SV_Ranked_ProcessRoundEnd(int winnerTeam);

// Name Updating
void SV_Ranked_UpdateDisplayName(int clientNum, const char* newName);

// Duel hooks
void SV_Ranked_DuelStart(int p1, int p2);
void SV_Ranked_DuelEnd(int winner, int loser, int isTie, int isDisconnect, int mod);

void SV_Ranked_DuelStop(int p1, int p2);
void SV_Ranked_DuelDisconnectCheck(int clientNum);
void SV_Ranked_ProcessPrivateDuel(int d1, int d2);

// Open Scoreboard Hook
void SV_Ranked_ProcessScoreboard(const char* text);

// Admin Database Commands
void SV_RankedResetPass_f(void);
void SV_RankedClearAccounts_f(void);

// Commands
int SV_Ranked_FindPlayerByNameOrId(const char *identifier);
void SV_Ranked_SetAdmin(int targetClient, int adminId);
qboolean SV_Ranked_ProcessCommand(client_t *cl, const char *chatText);

// Hot Potato
extern qboolean sv_hotPotatoActive;
extern qboolean sv_hotPotatoEnabled;
void SV_Ranked_StartHotPotato(void);
void SV_Ranked_StopHotPotato(qboolean disableCompletely);
void SV_Ranked_Logic_Frame(void);
void SV_Ranked_ProcessRoundStart(void);
void SV_Ranked_HotPotatoDisconnect(int clientNum);
void SV_Ranked_HotPotatoHandleKill(int killerId, int victimId);
void SV_Ranked_Vote_Frame(void);
void SV_Ranked_MapChange(void);

#endif // SV_RANKED_LOGIC_H
