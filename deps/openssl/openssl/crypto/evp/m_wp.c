/* crypto/evp/m_wp.c */

#include <stdio.h>
#include "cryptlib.h"

#ifndef OPENSSL_NO_WHIRLPOOL

#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/whrlpool.h>

static int init(EVP_MD_CTX *ctx)
	{ return WHIRLPOOL_Init(ctx->md_data); }

static int update(EVP_MD_CTX *ctx,const void *data,size_t count)
	{ return WHIRLPOOL_Update(ctx->md_data,data,count); }

static int final(EVP_MD_CTX *ctx,unsigned char *md)
	{ return WHIRLPOOL_Final(md,ctx->md_data); }

static const EVP_MD whirlpool_md=
	{
	NID_whirlpool,
	0,
	WHIRLPOOL_DIGEST_LENGTH,
	0,
	init,
	update,
	final,
	NULL,
	NULL,
	EVP_PKEY_NULL_method,
	WHIRLPOOL_BBLOCK/8,
	sizeof(EVP_MD *)+sizeof(WHIRLPOOL_CTX),
	};

const EVP_MD *EVP_whirlpool(void)
	{
	return(&whirlpool_md);
	}
#endif
