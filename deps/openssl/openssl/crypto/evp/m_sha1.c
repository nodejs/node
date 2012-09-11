/* crypto/evp/m_sha1.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"

#ifndef OPENSSL_FIPS

#ifndef OPENSSL_NO_SHA

#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif


static int init(EVP_MD_CTX *ctx)
	{ return SHA1_Init(ctx->md_data); }

static int update(EVP_MD_CTX *ctx,const void *data,size_t count)
	{ return SHA1_Update(ctx->md_data,data,count); }

static int final(EVP_MD_CTX *ctx,unsigned char *md)
	{ return SHA1_Final(md,ctx->md_data); }

static const EVP_MD sha1_md=
	{
	NID_sha1,
	NID_sha1WithRSAEncryption,
	SHA_DIGEST_LENGTH,
	EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	init,
	update,
	final,
	NULL,
	NULL,
	EVP_PKEY_RSA_method,
	SHA_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA_CTX),
	};

const EVP_MD *EVP_sha1(void)
	{
	return(&sha1_md);
	}
#endif

#ifndef OPENSSL_NO_SHA256
static int init224(EVP_MD_CTX *ctx)
	{ return SHA224_Init(ctx->md_data); }
static int init256(EVP_MD_CTX *ctx)
	{ return SHA256_Init(ctx->md_data); }
/*
 * Even though there're separate SHA224_[Update|Final], we call
 * SHA256 functions even in SHA224 context. This is what happens
 * there anyway, so we can spare few CPU cycles:-)
 */
static int update256(EVP_MD_CTX *ctx,const void *data,size_t count)
	{ return SHA256_Update(ctx->md_data,data,count); }
static int final256(EVP_MD_CTX *ctx,unsigned char *md)
	{ return SHA256_Final(md,ctx->md_data); }

static const EVP_MD sha224_md=
	{
	NID_sha224,
	NID_sha224WithRSAEncryption,
	SHA224_DIGEST_LENGTH,
	EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	init224,
	update256,
	final256,
	NULL,
	NULL,
	EVP_PKEY_RSA_method,
	SHA256_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA256_CTX),
	};

const EVP_MD *EVP_sha224(void)
	{ return(&sha224_md); }

static const EVP_MD sha256_md=
	{
	NID_sha256,
	NID_sha256WithRSAEncryption,
	SHA256_DIGEST_LENGTH,
	EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	init256,
	update256,
	final256,
	NULL,
	NULL,
	EVP_PKEY_RSA_method,
	SHA256_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA256_CTX),
	};

const EVP_MD *EVP_sha256(void)
	{ return(&sha256_md); }
#endif	/* ifndef OPENSSL_NO_SHA256 */

#ifndef OPENSSL_NO_SHA512
static int init384(EVP_MD_CTX *ctx)
	{ return SHA384_Init(ctx->md_data); }
static int init512(EVP_MD_CTX *ctx)
	{ return SHA512_Init(ctx->md_data); }
/* See comment in SHA224/256 section */
static int update512(EVP_MD_CTX *ctx,const void *data,size_t count)
	{ return SHA512_Update(ctx->md_data,data,count); }
static int final512(EVP_MD_CTX *ctx,unsigned char *md)
	{ return SHA512_Final(md,ctx->md_data); }

static const EVP_MD sha384_md=
	{
	NID_sha384,
	NID_sha384WithRSAEncryption,
	SHA384_DIGEST_LENGTH,
	EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	init384,
	update512,
	final512,
	NULL,
	NULL,
	EVP_PKEY_RSA_method,
	SHA512_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA512_CTX),
	};

const EVP_MD *EVP_sha384(void)
	{ return(&sha384_md); }

static const EVP_MD sha512_md=
	{
	NID_sha512,
	NID_sha512WithRSAEncryption,
	SHA512_DIGEST_LENGTH,
	EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	init512,
	update512,
	final512,
	NULL,
	NULL,
	EVP_PKEY_RSA_method,
	SHA512_CBLOCK,
	sizeof(EVP_MD *)+sizeof(SHA512_CTX),
	};

const EVP_MD *EVP_sha512(void)
	{ return(&sha512_md); }
#endif	/* ifndef OPENSSL_NO_SHA512 */

#endif
