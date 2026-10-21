// central_core_test.cpp: unit/integration test of sv_ranked_central_core.cpp (Linux).
// Starts a throwaway Node account service (in-memory store) itself and talks to it with the real client.
//
// build: gcc -c cJSON.c -o cJSON.o && g++ -std=c++11 -Wall -I. central_core_test.cpp sv_ranked_central_core.cpp cJSON.o -lm -o central_core_test
// run:   ./central_core_test <botdir with accounts_core.js> <test_service.js>

#include "sv_ranked_central_core.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string>

static int passed = 0, total = 0;
static void check(bool cond, const char *msg) {
  ++total;
  if (cond) { ++passed; printf("[PASS] %s\n", msg); } else { printf("[FAIL] %s\n", msg); }
}

static unsigned Clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
static void Log(const char *m) { printf("   (client) %s\n", m); }

static void Pump(int ms) {
  unsigned end = Clock() + ms;
  while ((int)(end - Clock()) > 0) { CentralCore_Frame(); usleep(3000); }
}
static bool WaitFor(bool (*cond)(), int ms) {
  unsigned end = Clock() + ms;
  while ((int)(end - Clock()) > 0) { CentralCore_Frame(); if (cond()) return true; usleep(3000); }
  return cond();
}
static bool IsReady() { return CentralCore_IsReady() != 0; }
static bool NotReady() { return CentralCore_IsReady() == 0; }

struct Reply {
  bool got;
  int ok;
  std::string text;
  Reply() : got(false), ok(0) {}
};
static void OnReply(void *ctx, int ok, cJSON *res) {
  Reply *r = (Reply *)ctx;
  r->got = true;
  r->ok = ok;
  if (res) { char *t = cJSON_PrintUnformatted(res); if (t) { r->text = t; free(t); } } else { r->text = "(no response)"; }
}
static Reply *g_wait;
static bool Got() { return g_wait && g_wait->got; }
static void Call(Reply &r, const char *op, cJSON *fields) {
  g_wait = &r;
  int id = CentralCore_Request(op, fields, OnReply, &r);
  if (id > 0) WaitFor(Got, 3000);
}
static std::string Field(const Reply &r, const char *key) {
  cJSON *j = cJSON_Parse(r.text.c_str());
  std::string out;
  if (j) { cJSON *f = cJSON_GetObjectItemCaseSensitive(j, key); if (f && cJSON_IsString(f)) out = f->valuestring; cJSON_Delete(j); }
  return out;
}

static pid_t StartService(const char *botdir, const char *script, int port) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid == 0) {
    char p[16]; snprintf(p, sizeof(p), "%d", port);
    freopen("/dev/null", "w", stdout);
    execlp("node", "node", script, p, botdir, (char *)NULL);
    _exit(127);
  }
  usleep(700 * 1000);
  return pid;
}
static void StopService(pid_t pid) { kill(pid, SIGTERM); int st; waitpid(pid, &st, 0); }

int main(int argc, char **argv) {
  if (argc < 3) { printf("usage: %s <botdir> <test_service.js>\n", argv[0]); return 2; }
  const char *botdir = argv[1], *script = argv[2];

  // ---- SHA-256 / HMAC known answers ----
  char out[65];
  CentralCore_Sha256Hex((const unsigned char *)"abc", 3, out);
  check(!strcmp(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 of 'abc'");
  CentralCore_Sha256Hex((const unsigned char *)"", 0, out);
  check(!strcmp(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "SHA-256 of the empty string");
  const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CentralCore_Sha256Hex((const unsigned char *)two, strlen(two), out);
  check(!strcmp(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA-256 of the 448-bit test message (padding boundary)");
  CentralCore_HmacSha256Hex("Jefe", "what do ya want for nothing?", out);
  check(!strcmp(out, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"), "HMAC-SHA256 RFC 4231 test case 2");
  std::string bigKey(131, (char)0xaa);
  CentralCore_HmacSha256Hex(bigKey.c_str(), "Test Using Larger Than Block-Size Key - Hash Key First", out);
  check(!strcmp(out, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"), "HMAC-SHA256 RFC 4231 test case 6 (key longer than the block size)");

  // ---- client behaviour against the throwaway service ----
  const int port = 29198;
  Reply r;
  CentralCore_Configure("127.0.0.1", port, "eu", "eu-secret", Clock, Log);
  check(CentralCore_Request("ping", NULL, OnReply, &r) == 0, "a request while disabled/not ready returns 0 (caller falls back)");

  pid_t svc = StartService(botdir, script, port);
  CentralCore_Enable(1);
  check(WaitFor(IsReady, 4000), "connects and completes the HMAC handshake");
  printf("   status: %s\n", CentralCore_Status());

  r = Reply(); Call(r, "ping", NULL);
  check(r.got && r.ok && r.text.find("pong") != std::string::npos, "ping is answered");

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "guid", "CPP-GUID-1");
  cJSON_AddStringToObject(f, "displayName", "Cpp ^1Tester");
  r = Reply(); Call(r, "register", f);
  std::string user = Field(r, "username"), pass = Field(r, "password");
  check(r.ok && user.rfind("ranked_", 0) == 0 && pass.size() == 10, "register returns a username and a 10 character password");

  f = cJSON_CreateObject(); cJSON_AddStringToObject(f, "guid", "CPP-GUID-1");
  r = Reply(); Call(r, "get", f);
  check(r.ok && Field(r, "username") == user, "get by GUID returns the same account");

  f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "username", user.c_str());
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "xp", 777);
  cJSON_AddItemToObject(f, "data", data);
  r = Reply(); Call(r, "put", f);
  check(r.ok && r.text.find("\"version\":2") != std::string::npos, "put saves the account (version 2)");

  f = cJSON_CreateObject(); cJSON_AddStringToObject(f, "username", user.c_str());
  r = Reply(); Call(r, "release", f);
  check(r.ok, "release frees the account");

  f = cJSON_CreateObject(); cJSON_AddStringToObject(f, "guid", "CPP-GUID-1");
  r = Reply(); Call(r, "get", f);
  check(r.ok && r.text.find("\"xp\":777") != std::string::npos, "the saved data comes back on the next get");

  // several requests in flight at once get their own answers
  Reply a, b, c;
  g_wait = &c;
  CentralCore_Request("ping", NULL, OnReply, &a);
  cJSON *tf = cJSON_CreateObject(); cJSON_AddStringToObject(tf, "mode", "duel"); cJSON_AddStringToObject(tf, "field", "elo");
  CentralCore_Request("top", tf, OnReply, &b);
  CentralCore_Request("ping", NULL, OnReply, &c);
  WaitFor(Got, 3000);
  Pump(100);
  check(a.got && b.got && c.got && b.text.find("\"rows\"") != std::string::npos, "three requests in flight are all answered");

  // ---- server goes away: pending requests fail, then the client reconnects by itself ----
  Reply lost;
  CentralCore_Request("ping", NULL, OnReply, &lost);
  StopService(svc);
  WaitFor(NotReady, 3000);
  check(lost.got && !lost.ok && CentralCore_IsReady() == 0, "when the service dies the pending request fails and the client reports not ready");
  check(CentralCore_Request("ping", NULL, OnReply, &lost) == 0, "requests are refused (return 0) while disconnected");
  svc = StartService(botdir, script, port);
  check(WaitFor(IsReady, 12000), "reconnects by itself once the service is back (backoff)");
  r = Reply(); Call(r, "ping", NULL);
  check(r.got && r.ok, "requests work again after the reconnect");

  // ---- wrong secret never becomes ready ----
  CentralCore_Configure("127.0.0.1", port, "eu", "WRONG-secret", Clock, Log);
  Pump(300);
  check(CentralCore_IsReady() == 0, "reconfiguring drops the connection");
  Pump(3500);
  check(CentralCore_IsReady() == 0, "a wrong secret never gets past the handshake");

  CentralCore_Shutdown();
  StopService(svc);
  printf("\n%d/%d assertions passed\n", passed, total);
  return passed == total ? 0 : 1;
}
