// sv_ranked_central.h: engine glue for the central account service client.
#ifndef SV_RANKED_CENTRAL_H
#define SV_RANKED_CENTRAL_H

#include "qcommon/q_shared.h"

void SV_Ranked_CentralInit(void);
void SV_Ranked_CentralShutdown(void);
void SV_Ranked_CentralFrame(void);
qboolean SV_Ranked_CentralActive(void);

void SV_RankedCentralStatus_f(void);
void SV_RankedCentralPing_f(void);

#endif
