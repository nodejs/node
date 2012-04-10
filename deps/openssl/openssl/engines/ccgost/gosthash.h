/**********************************************************************
 *                          gosthash.h                                *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *       This file is distributed under the same license as OpenSSL   *
 *                                                                    *
 *    Declaration of GOST R 34.11-94 hash functions                   *
 *       uses  and gost89.h Doesn't need OpenSSL                      *
 **********************************************************************/
#ifndef GOSTHASH_H
#define GOSTHASH_H
#include "gost89.h"
#include <stdlib.h>

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
typedef __int64 ghosthash_len;
#elif defined(__arch64__)
typedef long ghosthash_len;
#else
typedef long long ghosthash_len;
#endif

typedef struct gost_hash_ctx {
		ghosthash_len len;
		gost_ctx *cipher_ctx;
		int left;
		byte H[32];
		byte S[32];
		byte remainder[32];
} gost_hash_ctx;		


/* Initalizes gost hash ctx, including creation of gost cipher ctx */

int init_gost_hash_ctx(gost_hash_ctx *ctx, const gost_subst_block *subst_block);
void done_gost_hash_ctx(gost_hash_ctx *ctx);

/* Cleans up all fields, except cipher ctx preparing ctx for computing
 * of new hash value */
int start_hash(gost_hash_ctx *ctx);

/* Hashes block of data */
int hash_block(gost_hash_ctx *ctx, const byte *block, size_t length);

/* Finalizes computation of hash  and fills buffer (which should be at
 * least 32 bytes long) with value of computed hash. */
int finish_hash(gost_hash_ctx *ctx, byte *hashval);

#endif	
