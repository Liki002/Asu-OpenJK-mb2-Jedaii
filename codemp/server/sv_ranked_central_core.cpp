// sv_ranked_central_core.cpp
// See sv_ranked_central_core.h.

#include "sv_ranked_central_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

// -----------------------------------------------------------------------------
// SHA-256 and HMAC-SHA256
// -----------------------------------------------------------------------------
namespace {

struct Sha256 {
  uint32_t h[8];
  uint64_t total;
  unsigned char buf[64];
  size_t n;
};

const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void Sha256Init(Sha256 *s) {
  s->h[0] = 0x6a09e667; s->h[1] = 0xbb67ae85; s->h[2] = 0x3c6ef372; s->h[3] = 0xa54ff53a;
  s->h[4] = 0x510e527f; s->h[5] = 0x9b05688c; s->h[6] = 0x1f83d9ab; s->h[7] = 0x5be0cd19;
  s->total = 0;
  s->n = 0;
}

void Sha256Block(Sha256 *s, const unsigned char *p) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) | ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
  }
  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
  for (int i = 0; i < 64; ++i) {
    uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = h + S1 + ch + K256[i] + w[i];
    uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = S0 + maj;
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
  s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

void Sha256Update(Sha256 *s, const unsigned char *data, size_t len) {
  s->total += len;
  while (len > 0) {
    size_t take = 64 - s->n;
    if (take > len) take = len;
    memcpy(s->buf + s->n, data, take);
    s->n += take; data += take; len -= take;
    if (s->n == 64) {
      Sha256Block(s, s->buf);
      s->n = 0;
    }
  }
}

void Sha256Final(Sha256 *s, unsigned char out[32]) {
  uint64_t bits = s->total * 8;
  unsigned char pad = 0x80;
  Sha256Update(s, &pad, 1);
  unsigned char zero = 0;
  while (s->n != 56) Sha256Update(s, &zero, 1);
  unsigned char lenBytes[8];
  for (int i = 0; i < 8; ++i) lenBytes[i] = (unsigned char)(bits >> (56 - 8 * i));
  Sha256Update(s, lenBytes, 8);
  for (int i = 0; i < 8; ++i) {
    out[4 * i] = (unsigned char)(s->h[i] >> 24);
    out[4 * i + 1] = (unsigned char)(s->h[i] >> 16);
    out[4 * i + 2] = (unsigned char)(s->h[i] >> 8);
    out[4 * i + 3] = (unsigned char)(s->h[i]);
  }
}

void ToHex(const unsigned char *in, size_t n, char *out) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    out[2 * i] = hex[in[i] >> 4];
    out[2 * i + 1] = hex[in[i] & 15];
  }
  out[2 * n] = '\0';
}

void Sha256Raw(const unsigned char *data, size_t len, unsigned char out[32]) {
  Sha256 s;
  Sha256Init(&s);
  Sha256Update(&s, data, len);
  Sha256Final(&s, out);
}

}  // namespace

void CentralCore_Sha256Hex(const unsigned char *data, size_t len, char out[65]) {
  unsigned char d[32];
  Sha256Raw(data, len, d);
  ToHex(d, 32, out);
}

void CentralCore_HmacSha256Hex(const char *key, const char *msg, char out[65]) {
  unsigned char k[64];
  memset(k, 0, sizeof(k));
  size_t keyLen = strlen(key);
  if (keyLen > 64) {
    Sha256Raw((const unsigned char *)key, keyLen, k);   // 32 bytes, rest stays zero
  } else {
    memcpy(k, key, keyLen);
  }
  unsigned char ipad[64], opad[64];
  for (int i = 0; i < 64; ++i) {
    ipad[i] = k[i] ^ 0x36;
    opad[i] = k[i] ^ 0x5c;
  }
  unsigned char inner[32];
  Sha256 s;
  Sha256Init(&s);
  Sha256Update(&s, ipad, 64);
  Sha256Update(&s, (const unsigned char *)msg, strlen(msg));
  Sha256Final(&s, inner);

  unsigned char outer[32];
  Sha256Init(&s);
  Sha256Update(&s, opad, 64);
  Sha256Update(&s, inner, 32);
  Sha256Final(&s, outer);
  ToHex(outer, 32, out);
}

// -----------------------------------------------------------------------------
// Connection state machine
// -----------------------------------------------------------------------------
namespace {

enum cstate_t { CS_OFF, CS_DISCONNECTED, CS_CONNECTING, CS_AWAIT_CHALLENGE, CS_AWAIT_READY, CS_READY };

const int MAX_PENDING = 64;
const unsigned CONNECT_TIMEOUT_MS = 5000;
const unsigned HANDSHAKE_TIMEOUT_MS = 8000;
const unsigned REQUEST_TIMEOUT_MS = 8000;
const unsigned BACKOFF_MIN_MS = 2000;
const unsigned BACKOFF_MAX_MS = 30000;
const size_t MAX_INBUF = 1024 * 1024;

struct Pending {
  int id;
  centralCallback_t cb;
  void *ctx;
  unsigned sentAt;
  bool used;
};

struct State {
  bool enabled;
  cstate_t state;
  char host[64];
  int port;
  char serverId[16];
  char secret[128];
  centralClock_t clock;
  centralLog_t log;
  int fd;
  unsigned stateSince;
  unsigned nextAttempt;
  unsigned backoff;
  int nextId;
  Pending pending[MAX_PENDING];
  std::string inbuf;
  std::string outbuf;
  char status[96];
};

State S;
bool s_inited = false;

void EnsureInit() {
  if (s_inited) return;
  s_inited = true;
  memset(&S.pending, 0, sizeof(S.pending));
  S.enabled = false;
  S.state = CS_OFF;
  S.fd = -1;
  S.backoff = BACKOFF_MIN_MS;
  S.nextId = 1;
  S.port = 0;
  S.host[0] = S.serverId[0] = S.secret[0] = S.status[0] = '\0';
  S.clock = NULL;
  S.log = NULL;
}

void Logf(const char *fmt, ...) {
  if (!S.log) return;
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  msg[sizeof(msg) - 1] = '\0';
  S.log(msg);
}

unsigned Now() { return S.clock ? S.clock() : 0; }

int PendingCount() {
  int n = 0;
  for (int i = 0; i < MAX_PENDING; ++i) if (S.pending[i].used) ++n;
  return n;
}

#ifndef _WIN32

void CloseSocket() {
  if (S.fd >= 0) {
    close(S.fd);
    S.fd = -1;
  }
}

void FailAllPending() {
  // copy first: callbacks may issue new requests
  Pending copy[MAX_PENDING];
  memcpy(copy, S.pending, sizeof(copy));
  memset(S.pending, 0, sizeof(S.pending));
  for (int i = 0; i < MAX_PENDING; ++i) {
    if (copy[i].used && copy[i].cb) copy[i].cb(copy[i].ctx, 0, NULL);
  }
}

void Disconnect(const char *reason) {
  bool wasActive = (S.state != CS_DISCONNECTED && S.state != CS_OFF);
  CloseSocket();
  S.inbuf.clear();
  S.outbuf.clear();
  if (wasActive) Logf("disconnected (%s)", reason);
  S.state = S.enabled ? CS_DISCONNECTED : CS_OFF;
  S.nextAttempt = Now() + S.backoff;
  if (S.backoff < BACKOFF_MAX_MS) {
    S.backoff *= 2;
    if (S.backoff > BACKOFF_MAX_MS) S.backoff = BACKOFF_MAX_MS;
  }
  FailAllPending();
}

void StartConnect() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) { S.nextAttempt = Now() + S.backoff; return; }
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&one, sizeof(one));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)S.port);
  if (inet_pton(AF_INET, S.host, &addr.sin_addr) != 1) {
    close(fd);
    Logf("host '%s' is not a numeric IPv4 address", S.host);
    S.nextAttempt = Now() + BACKOFF_MAX_MS;
    return;
  }
  int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (r < 0 && errno != EINPROGRESS) {
    close(fd);
    S.nextAttempt = Now() + S.backoff;
    if (S.backoff < BACKOFF_MAX_MS) S.backoff *= 2;
    return;
  }
  S.fd = fd;
  S.state = CS_CONNECTING;
  S.stateSince = Now();
}

// Writes as much of outbuf as the socket accepts. Returns false on a fatal error.
bool Flush() {
  while (!S.outbuf.empty()) {
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif
    ssize_t n = send(S.fd, S.outbuf.data(), S.outbuf.size(), flags);
    if (n > 0) {
      S.outbuf.erase(0, (size_t)n);
    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      return true;
    } else {
      return false;
    }
  }
  return true;
}

void SendLine(const char *line) {
  S.outbuf.append(line);
  S.outbuf.push_back('\n');
}

void HandleLine(const std::string &line) {
  cJSON *msg = cJSON_Parse(line.c_str());
  if (!msg) return;
  cJSON *t = cJSON_GetObjectItemCaseSensitive(msg, "t");
  const char *type = (t && cJSON_IsString(t) && t->valuestring) ? t->valuestring : "";

  if (S.state == CS_AWAIT_CHALLENGE && !strcmp(type, "challenge")) {
    cJSON *nonce = cJSON_GetObjectItemCaseSensitive(msg, "nonce");
    if (nonce && cJSON_IsString(nonce) && nonce->valuestring) {
      char data[160];
      snprintf(data, sizeof(data), "%s:%s", nonce->valuestring, S.serverId);
      char mac[65];
      CentralCore_HmacSha256Hex(S.secret, data, mac);
      cJSON *hello = cJSON_CreateObject();
      cJSON_AddStringToObject(hello, "t", "hello");
      cJSON_AddStringToObject(hello, "server", S.serverId);
      cJSON_AddStringToObject(hello, "mac", mac);
      char *txt = cJSON_PrintUnformatted(hello);
      if (txt) { SendLine(txt); free(txt); }
      cJSON_Delete(hello);
      S.state = CS_AWAIT_READY;
      S.stateSince = Now();
    }
  } else if (S.state == CS_AWAIT_READY && !strcmp(type, "ready")) {
    S.state = CS_READY;
    S.stateSince = Now();
    S.backoff = BACKOFF_MIN_MS;
    Logf("connected to the account service as '%s'", S.serverId);
  } else if (S.state == CS_READY && !strcmp(type, "res")) {
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(msg, "id");
    int id = (idItem && cJSON_IsNumber(idItem)) ? idItem->valueint : 0;
    for (int i = 0; i < MAX_PENDING; ++i) {
      if (S.pending[i].used && S.pending[i].id == id) {
        Pending p = S.pending[i];
        S.pending[i].used = false;
        cJSON *ok = cJSON_GetObjectItemCaseSensitive(msg, "ok");
        if (p.cb) p.cb(p.ctx, (ok && cJSON_IsTrue(ok)) ? 1 : 0, msg);
        break;
      }
    }
  }
  cJSON_Delete(msg);
}

void ReadAvailable() {
  char tmp[4096];
  for (;;) {
    ssize_t n = recv(S.fd, tmp, sizeof(tmp), 0);
    if (n > 0) {
      S.inbuf.append(tmp, (size_t)n);
      if (S.inbuf.size() > MAX_INBUF) { Disconnect("input too large"); return; }
    } else if (n == 0) {
      Disconnect("closed by the service");
      return;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    } else if (errno == EINTR) {
      continue;
    } else {
      Disconnect("read error");
      return;
    }
  }
  size_t pos;
  while (S.fd >= 0 && (pos = S.inbuf.find('\n')) != std::string::npos) {
    std::string line = S.inbuf.substr(0, pos);
    S.inbuf.erase(0, pos + 1);
    HandleLine(line);
  }
}

#endif  // !_WIN32

}  // namespace

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void CentralCore_Configure(const char *host, int port, const char *serverId, const char *secret,
                           centralClock_t clock, centralLog_t log) {
  EnsureInit();
  bool changed = strcmp(S.host, host ? host : "") || S.port != port || strcmp(S.serverId, serverId ? serverId : "") ||
                 strcmp(S.secret, secret ? secret : "");
  snprintf(S.host, sizeof(S.host), "%s", host ? host : "");
  S.port = port;
  snprintf(S.serverId, sizeof(S.serverId), "%s", serverId ? serverId : "");
  snprintf(S.secret, sizeof(S.secret), "%s", secret ? secret : "");
  S.clock = clock;
  S.log = log;
#ifndef _WIN32
  if (changed && S.state != CS_OFF && S.state != CS_DISCONNECTED) Disconnect("configuration changed");
  if (changed) S.backoff = BACKOFF_MIN_MS;
#else
  (void)changed;
#endif
}

void CentralCore_Enable(int enabled) {
  EnsureInit();
  bool on = enabled != 0;
  if (on == S.enabled) return;
  S.enabled = on;
#ifndef _WIN32
  if (on) {
    S.state = CS_DISCONNECTED;
    S.nextAttempt = Now();
  } else {
    Disconnect("disabled");
    S.state = CS_OFF;
  }
#endif
}

void CentralCore_Frame(void) {
  EnsureInit();
#ifndef _WIN32
  if (!S.enabled || !S.clock) return;
  unsigned now = Now();

  switch (S.state) {
  case CS_OFF:
    return;
  case CS_DISCONNECTED:
    if ((int)(now - S.nextAttempt) >= 0 && S.host[0] && S.port > 0 && S.serverId[0] && S.secret[0]) StartConnect();
    return;
  case CS_CONNECTING: {
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(S.fd, &wset);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int r = select(S.fd + 1, NULL, &wset, NULL, &tv);
    if (r > 0) {
      int err = 0;
      socklen_t len = sizeof(err);
      getsockopt(S.fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
      if (err == 0) {
        S.state = CS_AWAIT_CHALLENGE;
        S.stateSince = now;
      } else {
        Disconnect("connect failed");
      }
    } else if ((unsigned)(now - S.stateSince) > CONNECT_TIMEOUT_MS) {
      Disconnect("connect timed out");
    }
    return;
  }
  default:
    break;
  }

  // AWAIT_CHALLENGE / AWAIT_READY / READY
  ReadAvailable();
  if (S.fd < 0) return;
  if (!Flush()) { Disconnect("write error"); return; }

  if ((S.state == CS_AWAIT_CHALLENGE || S.state == CS_AWAIT_READY) && (unsigned)(now - S.stateSince) > HANDSHAKE_TIMEOUT_MS) {
    Disconnect("handshake timed out (wrong secret or server id?)");
    return;
  }
  if (S.state == CS_READY) {
    bool hasTimedOut = false;
    for (int i = 0; i < MAX_PENDING; ++i) {
      if (S.pending[i].used && (unsigned)(now - S.pending[i].sentAt) > REQUEST_TIMEOUT_MS) {
        hasTimedOut = true;
        break;
      }
    }
    if (hasTimedOut) {
      Disconnect("request timed out (reconnecting fresh)");
      return;
    }
  }
#endif
}

int CentralCore_IsReady(void) {
  EnsureInit();
#ifndef _WIN32
  return S.enabled && S.state == CS_READY;
#else
  return 0;
#endif
}

int CentralCore_Request(const char *op, cJSON *fields, centralCallback_t cb, void *ctx) {
  EnsureInit();
  if (!CentralCore_IsReady() || !op) {
    if (fields) cJSON_Delete(fields);
    return 0;
  }
#ifndef _WIN32
  int slot = -1;
  for (int i = 0; i < MAX_PENDING; ++i) {
    if (!S.pending[i].used) { slot = i; break; }
  }
  if (slot < 0) {
    if (fields) cJSON_Delete(fields);
    return 0;
  }
  int id = S.nextId++;
  if (S.nextId > 2000000000) S.nextId = 1;

  cJSON *msg = cJSON_CreateObject();
  cJSON_AddStringToObject(msg, "t", "req");
  cJSON_AddNumberToObject(msg, "id", id);
  cJSON_AddStringToObject(msg, "op", op);
  if (fields) {
    for (cJSON *c = fields->child; c; c = c->next) {
      if (c->string) cJSON_AddItemToObject(msg, c->string, cJSON_Duplicate(c, 1));
    }
    cJSON_Delete(fields);
  }
  char *txt = cJSON_PrintUnformatted(msg);
  cJSON_Delete(msg);
  if (!txt) return 0;
  SendLine(txt);
  free(txt);

  S.pending[slot].used = true;
  S.pending[slot].id = id;
  S.pending[slot].cb = cb;
  S.pending[slot].ctx = ctx;
  S.pending[slot].sentAt = Now();
  if (!Flush()) Disconnect("write error");
  return id;
#else
  if (fields) cJSON_Delete(fields);
  (void)cb; (void)ctx;
  return 0;
#endif
}

void CentralCore_Shutdown(void) {
  EnsureInit();
#ifndef _WIN32
  S.enabled = false;
  Disconnect("shutdown");
  S.state = CS_OFF;
#endif
}

const char *CentralCore_Status(void) {
  EnsureInit();
  const char *name = "off";
  switch (S.state) {
  case CS_OFF: name = S.enabled ? "starting" : "disabled"; break;
  case CS_DISCONNECTED: name = "disconnected (retrying)"; break;
  case CS_CONNECTING: name = "connecting"; break;
  case CS_AWAIT_CHALLENGE: name = "handshake (waiting for challenge)"; break;
  case CS_AWAIT_READY: name = "handshake (waiting for ready)"; break;
  case CS_READY: name = "ready"; break;
  }
#ifdef _WIN32
  name = "unsupported on Windows";
#endif
  snprintf(S.status, sizeof(S.status), "%s, %d pending", name, PendingCount());
  return S.status;
}
