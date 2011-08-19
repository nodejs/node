/* ====================================================================
 * Copyright (c) 2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * This is a FIPS approved AES PRNG based on ANSI X9.31 A.2.4.
 */

#include "e_os.h"

/* If we don't define _XOPEN_SOURCE_EXTENDED, struct timeval won't
   be defined and gettimeofday() won't be declared with strict compilers
   like DEC C in ANSI C mode.  */
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/fips_rand.h>
#ifndef OPENSSL_SYS_WIN32
#include <sys/time.h>
#endif
#include <assert.h>
#ifndef OPENSSL_SYS_WIN32
# ifdef OPENSSL_UNISTD
#  include OPENSSL_UNISTD
# else
#  include <unistd.h>
# endif
#endif
#include <string.h>
#include <openssl/fips.h>
#include "fips_locl.h"

#ifdef OPENSSL_FIPS

void *OPENSSL_stderr(void);

#define AES_BLOCK_LENGTH	16


/* AES FIPS PRNG implementation */

typedef struct 
	{
	int seeded;
	int keyed;
	int test_mode;
	int second;
	int error;
	unsigned long counter;
	AES_KEY ks;
	int vpos;
	/* Temporary storage for key if it equals seed length */
	unsigned char tmp_key[AES_BLOCK_LENGTH];
	unsigned char V[AES_BLOCK_LENGTH];
	unsigned char DT[AES_BLOCK_LENGTH];
	unsigned char last[AES_BLOCK_LENGTH];
	} FIPS_PRNG_CTX;

static FIPS_PRNG_CTX sctx;

static int fips_prng_fail = 0;

void FIPS_rng_stick(void)
	{
	fips_prng_fail = 1;
	}

static void fips_rand_prng_reset(FIPS_PRNG_CTX *ctx)
	{
	ctx->seeded = 0;
	ctx->keyed = 0;
	ctx->test_mode = 0;
	ctx->counter = 0;
	ctx->second = 0;
	ctx->error = 0;
	ctx->vpos = 0;
	OPENSSL_cleanse(ctx->V, AES_BLOCK_LENGTH);
	OPENSSL_cleanse(&ctx->ks, sizeof(AES_KEY));
	}
	

static int fips_set_prng_key(FIPS_PRNG_CTX *ctx,
			const unsigned char *key, FIPS_RAND_SIZE_T keylen)
	{
	FIPS_selftest_check();
	if (keylen != 16 && keylen != 24 && keylen != 32)
		{
		/* error: invalid key size */
		return 0;
		}
	AES_set_encrypt_key(key, keylen << 3, &ctx->ks);
	if (keylen == 16)
		{
		memcpy(ctx->tmp_key, key, 16);
		ctx->keyed = 2;
		}
	else
		ctx->keyed = 1;
	ctx->seeded = 0;
	ctx->second = 0;
	return 1;
	}

static int fips_set_prng_seed(FIPS_PRNG_CTX *ctx,
			const unsigned char *seed, FIPS_RAND_SIZE_T seedlen)
	{
	int i;
	if (!ctx->keyed)
		return 0;
	/* In test mode seed is just supplied data */
	if (ctx->test_mode)
		{
		if (seedlen != AES_BLOCK_LENGTH)
			return 0;
		memcpy(ctx->V, seed, AES_BLOCK_LENGTH);
		ctx->seeded = 1;
		return 1;
		}
	/* Outside test mode XOR supplied data with existing seed */
	for (i = 0; i < seedlen; i++)
		{
		ctx->V[ctx->vpos++] ^= seed[i];
		if (ctx->vpos == AES_BLOCK_LENGTH)
			{
			ctx->vpos = 0;
			/* Special case if first seed and key length equals
 			 * block size check key and seed do not match.
 			 */ 
			if (ctx->keyed == 2)
				{
				if (!memcmp(ctx->tmp_key, ctx->V, 16))
					{
					RANDerr(RAND_F_FIPS_SET_PRNG_SEED,
						RAND_R_PRNG_SEED_MUST_NOT_MATCH_KEY);
					return 0;
					}
				OPENSSL_cleanse(ctx->tmp_key, 16);
				ctx->keyed = 1;
				}
			ctx->seeded = 1;
			}
		}
	return 1;
	}

static int fips_set_test_mode(FIPS_PRNG_CTX *ctx)
	{
	if (ctx->keyed)
		{
		RANDerr(RAND_F_FIPS_SET_TEST_MODE,RAND_R_PRNG_KEYED);
		return 0;
		}
	ctx->test_mode = 1;
	return 1;
	}

int FIPS_rand_test_mode(void)
	{
	return fips_set_test_mode(&sctx);
	}

int FIPS_rand_set_dt(unsigned char *dt)
	{
	if (!sctx.test_mode)
		{
		RANDerr(RAND_F_FIPS_RAND_SET_DT,RAND_R_NOT_IN_TEST_MODE);
		return 0;
		}
	memcpy(sctx.DT, dt, AES_BLOCK_LENGTH);
	return 1;
	}

static void fips_get_dt(FIPS_PRNG_CTX *ctx)
    {
#ifdef OPENSSL_SYS_WIN32
	FILETIME ft;
#else
	struct timeval tv;
#endif
	unsigned char *buf = ctx->DT;

#ifndef GETPID_IS_MEANINGLESS
	unsigned long pid;
#endif

#ifdef OPENSSL_SYS_WIN32
	GetSystemTimeAsFileTime(&ft);
	buf[0] = (unsigned char) (ft.dwHighDateTime & 0xff);
	buf[1] = (unsigned char) ((ft.dwHighDateTime >> 8) & 0xff);
	buf[2] = (unsigned char) ((ft.dwHighDateTime >> 16) & 0xff);
	buf[3] = (unsigned char) ((ft.dwHighDateTime >> 24) & 0xff);
	buf[4] = (unsigned char) (ft.dwLowDateTime & 0xff);
	buf[5] = (unsigned char) ((ft.dwLowDateTime >> 8) & 0xff);
	buf[6] = (unsigned char) ((ft.dwLowDateTime >> 16) & 0xff);
	buf[7] = (unsigned char) ((ft.dwLowDateTime >> 24) & 0xff);
#else
	gettimeofday(&tv,NULL);
	buf[0] = (unsigned char) (tv.tv_sec & 0xff);
	buf[1] = (unsigned char) ((tv.tv_sec >> 8) & 0xff);
	buf[2] = (unsigned char) ((tv.tv_sec >> 16) & 0xff);
	buf[3] = (unsigned char) ((tv.tv_sec >> 24) & 0xff);
	buf[4] = (unsigned char) (tv.tv_usec & 0xff);
	buf[5] = (unsigned char) ((tv.tv_usec >> 8) & 0xff);
	buf[6] = (unsigned char) ((tv.tv_usec >> 16) & 0xff);
	buf[7] = (unsigned char) ((tv.tv_usec >> 24) & 0xff);
#endif
	buf[8] = (unsigned char) (ctx->counter & 0xff);
	buf[9] = (unsigned char) ((ctx->counter >> 8) & 0xff);
	buf[10] = (unsigned char) ((ctx->counter >> 16) & 0xff);
	buf[11] = (unsigned char) ((ctx->counter >> 24) & 0xff);

	ctx->counter++;


#ifndef GETPID_IS_MEANINGLESS
	pid=(unsigned long)getpid();
	buf[12] = (unsigned char) (pid & 0xff);
	buf[13] = (unsigned char) ((pid >> 8) & 0xff);
	buf[14] = (unsigned char) ((pid >> 16) & 0xff);
	buf[15] = (unsigned char) ((pid >> 24) & 0xff);
#endif
    }

static int fips_rand(FIPS_PRNG_CTX *ctx,
			unsigned char *out, FIPS_RAND_SIZE_T outlen)
	{
	unsigned char R[AES_BLOCK_LENGTH], I[AES_BLOCK_LENGTH];
	unsigned char tmp[AES_BLOCK_LENGTH];
	int i;
	if (ctx->error)
		{
		RANDerr(RAND_F_FIPS_RAND,RAND_R_PRNG_ERROR);
		return 0;
		}
	if (!ctx->keyed)
		{
		RANDerr(RAND_F_FIPS_RAND,RAND_R_NO_KEY_SET);
		return 0;
		}
	if (!ctx->seeded)
		{
		RANDerr(RAND_F_FIPS_RAND,RAND_R_PRNG_NOT_SEEDED);
		return 0;
		}
	for (;;)
		{
		if (!ctx->test_mode)
			fips_get_dt(ctx);
		AES_encrypt(ctx->DT, I, &ctx->ks);
		for (i = 0; i < AES_BLOCK_LENGTH; i++)
			tmp[i] = I[i] ^ ctx->V[i];
		AES_encrypt(tmp, R, &ctx->ks);
		for (i = 0; i < AES_BLOCK_LENGTH; i++)
			tmp[i] = R[i] ^ I[i];
		AES_encrypt(tmp, ctx->V, &ctx->ks);
		/* Continuous PRNG test */
		if (ctx->second)
			{
			if (fips_prng_fail)
				memcpy(ctx->last, R, AES_BLOCK_LENGTH);
			if (!memcmp(R, ctx->last, AES_BLOCK_LENGTH))
				{
	    			RANDerr(RAND_F_FIPS_RAND,RAND_R_PRNG_STUCK);
				ctx->error = 1;
				fips_set_selftest_fail();
				return 0;
				}
			}
		memcpy(ctx->last, R, AES_BLOCK_LENGTH);
		if (!ctx->second)
			{
			ctx->second = 1;
			if (!ctx->test_mode)
				continue;
			}

		if (outlen <= AES_BLOCK_LENGTH)
			{
			memcpy(out, R, outlen);
			break;
			}

		memcpy(out, R, AES_BLOCK_LENGTH);
		out += AES_BLOCK_LENGTH;
		outlen -= AES_BLOCK_LENGTH;
		}
	return 1;
	}


int FIPS_rand_set_key(const unsigned char *key, FIPS_RAND_SIZE_T keylen)
	{
	int ret;
	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	ret = fips_set_prng_key(&sctx, key, keylen);
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
	return ret;
	}

int FIPS_rand_seed(const void *seed, FIPS_RAND_SIZE_T seedlen)
	{
	int ret;
	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	ret = fips_set_prng_seed(&sctx, seed, seedlen);
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
	return ret;
	}


int FIPS_rand_bytes(unsigned char *out, FIPS_RAND_SIZE_T count)
	{
	int ret;
	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	ret = fips_rand(&sctx, out, count);
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
	return ret;
	}

int FIPS_rand_status(void)
	{
	int ret;
	CRYPTO_r_lock(CRYPTO_LOCK_RAND);
	ret = sctx.seeded;
	CRYPTO_r_unlock(CRYPTO_LOCK_RAND);
	return ret;
	}

void FIPS_rand_reset(void)
	{
	CRYPTO_w_lock(CRYPTO_LOCK_RAND);
	fips_rand_prng_reset(&sctx);
	CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
	}

static void fips_do_rand_seed(const void *seed, FIPS_RAND_SIZE_T seedlen)
	{
	FIPS_rand_seed(seed, seedlen);
	}

static void fips_do_rand_add(const void *seed, FIPS_RAND_SIZE_T seedlen,
					double add_entropy)
	{
	FIPS_rand_seed(seed, seedlen);
	}

static const RAND_METHOD rand_fips_meth=
    {
    fips_do_rand_seed,
    FIPS_rand_bytes,
    FIPS_rand_reset,
    fips_do_rand_add,
    FIPS_rand_bytes,
    FIPS_rand_status
    };

const RAND_METHOD *FIPS_rand_method(void)
{
  return &rand_fips_meth;
}

#endif
