#ifndef __SV_SPIN_TELEMETRY_H__
#define __SV_SPIN_TELEMETRY_H__

#include "server.h"

// Initialize Spin Telemetry Subsystem
void SV_SpinTelemetry_Init(void);

// Call when a client enters !spin in chat or console
void SV_SpinTelemetry_OnSpinCommand(client_t *cl);

// Manually trigger telemetry observation window for a specific event (e.g. !burn, !speed, !givegun)
void SV_SpinTelemetry_Trigger(int clientNum, const char *reason);

// Call whenever the Game VM sends a server command via SV_GameSendServerCommand
void SV_SpinTelemetry_OnGameServerCommand(int clientNum, const char *text);

// Call every server frame to detect and log playerState diffs
void SV_SpinTelemetry_Frame(void);

// Check if any client is currently in active telemetry observation window
qboolean SV_SpinTelemetry_IsActive(void);

#endif // __SV_SPIN_TELEMETRY_H__
