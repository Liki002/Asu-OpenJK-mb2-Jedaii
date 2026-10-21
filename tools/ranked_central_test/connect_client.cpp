// connect_client.cpp: minimal MBII/OpenJK protocol 26 client for testing the server side ranked hooks.
// Does the connect handshake, then the two netchan packets that take the slot to CS_ACTIVE
// (SV_ClientEnterWorld -> SV_Ranked_ClientConnect), and stays quiet afterwards.
//
// build: g++ -std=c++11 -I shim connect_client.cpp ../../codemp/qcommon/huffman.cpp -o connect_client
// usage: ./connect_client <host> <port> <ja_guid> <name> <rcon_password> [hold_seconds]
// The server needs sv_pure 0 (no "cp" pak checksum is sent).
#include "qcommon/qcommon.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>

#include "msg_hdata.inc"   // int msg_hData[256] (the engine's static message Huffman table)

int oldsize = 0;
static huffman_t H;

struct Writer {
  byte d[2048];
  int bit;
  Writer() : bit(0) { memset(d, 0, sizeof(d)); }
  void bits(int value, int n) {   // same layout as MSG_WriteBits (non-OOB)
    value &= (int)(0xffffffffu >> (32 - n));
    if (n & 7) {
      int nb = n & 7;
      for (int i = 0; i < nb; i++) { Huff_putBit(value & 1, d, &bit); value >>= 1; }
      n -= nb;
    }
    for (int i = 0; i < n; i += 8) { Huff_offsetTransmit(&H.compressor, value & 0xff, d, &bit, (int)sizeof(d) << 3); value >>= 8; }
  }
  int size() const { return (bit >> 3) + 1; }
};

static int g_sock;
static sockaddr_in g_addr;

static std::string Rcon(const char *pw, const std::string &cmd) {
  std::string p = std::string("\xff\xff\xff\xff") + "rcon " + pw + " " + cmd;
  sendto(g_sock, p.data(), p.size(), 0, (sockaddr *)&g_addr, sizeof(g_addr));
  char buf[4096];
  int n = recv(g_sock, buf, sizeof(buf) - 1, 0);
  if (n <= 4) return "";
  buf[n] = 0;
  return buf + 4;
}

// Sends one client netchan packet: [seq][qport][payload], payload bytes from offset 12 xor-encoded like the engine does.
static void SendNetchan(int seq, int qport, Writer &w, int challenge, int serverId, int msgAck) {
  byte pkt[2100];
  int n = w.size();
  memcpy(pkt, &seq, 4);
  pkt[4] = qport & 0xff; pkt[5] = (qport >> 8) & 0xff;
  memcpy(pkt + 6, w.d, n);
  byte key = (byte)(challenge ^ serverId ^ msgAck);   // the acked server command string is empty this early
  for (int i = 6 + 12; i < 6 + n; i++) pkt[i] ^= key;
  sendto(g_sock, pkt, 6 + n, 0, (sockaddr *)&g_addr, sizeof(g_addr));
}

int main(int argc, char **argv) {
  if (argc < 6) { printf("usage: %s host port ja_guid name rcon_password [hold_seconds]\n", argv[0]); return 2; }
  int hold = argc > 6 ? atoi(argv[6]) : 0;

  Huff_Init(&H);
  for (int i = 0; i < 256; i++)
    for (int j = 0; j < msg_hData[i]; j++) { Huff_addRef(&H.compressor, (byte)i); Huff_addRef(&H.decompressor, (byte)i); }

  g_sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct timeval tv = {2, 0};
  setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  g_addr.sin_family = AF_INET; g_addr.sin_port = htons(atoi(argv[2])); inet_pton(AF_INET, argv[1], &g_addr.sin_addr);
  srand(time(NULL) ^ getpid());

  // serverId (needed in the second netchan packet) comes from the rcon-readable cvar
  std::string sid = Rcon(argv[5], "sv_serverid");
  // reply looks like: Cvar sv_serverid = "1525" (with ^ color codes around the value)
  size_t at = sid.find("sv_serverid");
  if (at != std::string::npos) at = sid.find('=', at);
  while (at != std::string::npos && at < sid.size() && !(sid[at] >= '0' && sid[at] <= '9' && sid[at - 1] != '^')) at++;
  if (at == std::string::npos || at >= sid.size()) { printf("could not read sv_serverid: %s\n", sid.c_str()); return 1; }
  int serverId = atoi(sid.c_str() + at);

  std::string out = std::string("\xff\xff\xff\xff") + "getchallenge " + std::to_string(rand());
  sendto(g_sock, out.data(), out.size(), 0, (sockaddr *)&g_addr, sizeof(g_addr));
  char buf[8192];
  int n = recv(g_sock, buf, sizeof(buf) - 1, 0);
  if (n <= 0) { printf("no challenge response\n"); return 1; }
  buf[n] = 0;
  int serverChal = 0;
  if (sscanf(buf + 4, "challengeResponse %d", &serverChal) != 1) { printf("bad challenge reply: %s\n", buf + 4); return 1; }

  int qport = 20000 + rand() % 20000;
  char info[1024];
  snprintf(info, sizeof(info), "\\challenge\\%d\\qport\\%d\\protocol\\26\\name\\%s\\rate\\25000\\snaps\\40\\ja_guid\\%s",
           serverChal, qport, argv[4], argv[3]);
  std::string cmd = std::string("connect \"") + info + "\"";
  byte pkt[8192];
  memset(pkt, 0, sizeof(pkt));
  memcpy(pkt, "\xff\xff\xff\xff", 4);
  memcpy(pkt + 4, cmd.data(), cmd.size());
  msg_t m; m.data = pkt; m.maxsize = sizeof(pkt); m.cursize = 4 + (int)cmd.size();
  Huff_Compress(&m, 12);
  sendto(g_sock, pkt, m.cursize, 0, (sockaddr *)&g_addr, sizeof(g_addr));
  n = recv(g_sock, buf, sizeof(buf) - 1, 0);
  if (n <= 0) { printf("no connect response\n"); return 1; }
  buf[n] = 0;
  if (strncmp(buf + 4, "connectResponse", 15)) { printf("server said: %s\n", buf + 4); return 1; }
  printf("connected (serverId %d)\n", serverId);

  // packet 1: wrong serverId makes the server send the gamestate (CS_CONNECTED -> CS_PRIMED)
  { Writer w; w.bits(0, 32); w.bits(0, 32); w.bits(0, 32); w.bits(5 /*clc_EOF*/, 8); SendNetchan(1, qport, w, serverChal, 0, 0); }
  usleep(600 * 1000);
  int lastSeq = 0;
  struct timeval quick = {0, 100000};
  setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &quick, sizeof(quick));
  time_t until = time(NULL) + 1;
  while (time(NULL) <= until && (n = recv(g_sock, buf, sizeof(buf), 0)) > 0) {
    if (n >= 4) { int s; memcpy(&s, buf, 4); if (s != -1) lastSeq = s & 0x7fffffff; }
  }
  printf("gamestate packets seen up to sequence %d\n", lastSeq);

  // packet 2: right serverId + one empty usercmd; the first usercmd of a primed client enters the world
  {
    Writer w;
    w.bits(serverId, 32); w.bits(lastSeq, 32); w.bits(0, 32);
    w.bits(2 /*clc_move*/, 8); w.bits(1 /*cmdCount*/, 8);
    w.bits(1, 1); w.bits(50, 8); w.bits(0, 1);   // serverTime +50, rest unchanged
    w.bits(5 /*clc_EOF*/, 8);
    SendNetchan(2, qport, w, serverChal, serverId, lastSeq);
  }
  sleep(hold);
  return 0;
}
