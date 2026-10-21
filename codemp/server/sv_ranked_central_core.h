// sv_ranked_central_core.h
//
// Client for the Jedaii central account service (see scratch/central_accounts_design.md).
// Engine-independent: plain sockets + cJSON, so it can be unit tested outside the engine.
//
// Protocol: TCP, one JSON object per line. The service sends {"t":"challenge","nonce":..}; we answer
// {"t":"hello","server":ID,"mac":HMAC-SHA256(secret, nonce + ":" + ID)}; after {"t":"ready"} requests
// {"t":"req","id":N,"op":...} are answered by {"t":"res","id":N,"ok":..,...}.
//
// Everything is non-blocking and driven by CentralCore_Frame() once per server frame.
// POSIX sockets only: on Windows the client is a stub that never becomes ready (the dedicated server
// is run on Linux).

#ifndef SV_RANKED_CENTRAL_CORE_H
#define SV_RANKED_CENTRAL_CORE_H

#include <stddef.h>
#include "cJSON.h"

// ok: 1 if the service answered with "ok":true; 0 for an error answer, a timeout or a lost connection.
// response: the parsed answer (valid only during the call, do not free), or NULL on timeout/disconnect.
typedef void (*centralCallback_t)(void *ctx, int ok, cJSON *response);

typedef unsigned int (*centralClock_t)(void);   // milliseconds
typedef void (*centralLog_t)(const char *msg);

// SHA-256 / HMAC-SHA256, lowercase hex (out must hold 65 bytes)
void CentralCore_Sha256Hex(const unsigned char *data, size_t len, char out[65]);
void CentralCore_HmacSha256Hex(const char *key, const char *msg, char out[65]);

// host must be a numeric IPv4 address (no blocking DNS lookups in the server frame)
void CentralCore_Configure(const char *host, int port, const char *serverId, const char *secret,
                           centralClock_t clock, centralLog_t log);
void CentralCore_Enable(int enabled);
void CentralCore_Frame(void);
int CentralCore_IsReady(void);
void CentralCore_Shutdown(void);

// Sends a request. 'fields' (a cJSON object with the op's parameters, may be NULL) is consumed.
// Returns the request id (> 0), or 0 if the client is not ready / the queue is full (caller falls back).
int CentralCore_Request(const char *op, cJSON *fields, centralCallback_t cb, void *ctx);

const char *CentralCore_Status(void);   // short human readable state, e.g. "ready (3 pending)"

#endif
