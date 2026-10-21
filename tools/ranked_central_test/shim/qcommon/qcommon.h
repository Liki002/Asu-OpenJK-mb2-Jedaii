// Minimal stand-in for qcommon/qcommon.h so the engine's own huffman.cpp can be compiled into the test client.
#pragma once
#include <string.h>
#include <stdlib.h>
typedef unsigned char byte;
typedef struct { byte *data; int maxsize; int cursize; } msg_t;
#define Com_Memset memset
#define Com_Memcpy memcpy
#define HMAX 256
#define NYT HMAX
#define INTERNAL_NODE (HMAX+1)
typedef struct nodetype {
	struct nodetype *left, *right, *parent;
	struct nodetype *next, *prev;
	struct nodetype **head;
	int weight;
	int symbol;
} node_t;
typedef struct huff_s {
	int blocNode;
	int blocPtrs;
	node_t *tree;
	node_t *lhead;
	node_t *ltail;
	node_t *loc[HMAX+1];
	node_t **freelist;
	node_t nodeList[768];
	node_t *nodePtrs[768];
} huff_t;
typedef struct huffman_s { huff_t compressor; huff_t decompressor; } huffman_t;
void Huff_Compress(msg_t *buf, int offset);
void Huff_putBit(int bit, byte *fout, int *offset);
void Huff_offsetTransmit(huff_t *huff, int ch, byte *fout, int *offset, int maxoffset);
void Huff_addRef(huff_t *huff, byte ch);
void Huff_Init(huffman_t *huff);
int Huff_getBit(byte *fin, int *offset);
void Huff_offsetReceive(node_t *node, int *ch, byte *fin, int *offset, int maxoffset);
