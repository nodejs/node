/* crypto/engine/eng_rsax.c */
/* Copyright (c) 2010-2010 Intel Corp.
 *   Author: Vinodh.Gopal@intel.com
 *           Jim Guilford
 *           Erdinc.Ozturk@intel.com
 *           Maxim.Perminov@intel.com
 *           Ying.Huang@intel.com
 *
 * More information about algorithm used can be found at:
 *   http://www.cse.buffalo.edu/srds2009/escs2009_submission_Gopal.pdf
 */
/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 */

#include <openssl/opensslconf.h>

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/engine.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#include <openssl/bn.h>
#include <openssl/err.h>

/* RSAX is available **ONLY* on x86_64 CPUs */
#undef COMPILE_RSAX

#if (defined(__x86_64) || defined(__x86_64__) || \
     defined(_M_AMD64) || defined (_M_X64)) && !defined(OPENSSL_NO_ASM)
#define COMPILE_RSAX
static ENGINE *ENGINE_rsax (void);
#endif

void ENGINE_load_rsax (void)
	{
/* On non-x86 CPUs it just returns. */
#ifdef COMPILE_RSAX
	ENGINE *toadd = ENGINE_rsax();
	if(!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
#endif
	}

#ifdef COMPILE_RSAX
#define E_RSAX_LIB_NAME "rsax engine"

static int e_rsax_destroy(ENGINE *e);
static int e_rsax_init(ENGINE *e);
static int e_rsax_finish(ENGINE *e);
static int e_rsax_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void));

#ifndef OPENSSL_NO_RSA
/* RSA stuff */
static int e_rsax_rsa_mod_exp(BIGNUM *r, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);
static int e_rsax_rsa_finish(RSA *r);
#endif

static const ENGINE_CMD_DEFN e_rsax_cmd_defns[] = {
	{0, NULL, NULL, 0}
	};

#ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD e_rsax_rsa =
	{
	"Intel RSA-X method",
	NULL,
	NULL,
	NULL,
	NULL,
	e_rsax_rsa_mod_exp,
	NULL,
	NULL,
	e_rsax_rsa_finish,
	RSA_FLAG_CACHE_PUBLIC|RSA_FLAG_CACHE_PRIVATE,
	NULL,
	NULL,
	NULL
	};
#endif

/* Constants used when creating the ENGINE */
static const char *engine_e_rsax_id = "rsax";
static const char *engine_e_rsax_name = "RSAX engine support";

/* This internal function is used by ENGINE_rsax() */
static int bind_helper(ENGINE *e)
	{
#ifndef OPENSSL_NO_RSA
	const RSA_METHOD *meth1;
#endif
	if(!ENGINE_set_id(e, engine_e_rsax_id) ||
			!ENGINE_set_name(e, engine_e_rsax_name) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_RSA(e, &e_rsax_rsa) ||
#endif
			!ENGINE_set_destroy_function(e, e_rsax_destroy) ||
			!ENGINE_set_init_function(e, e_rsax_init) ||
			!ENGINE_set_finish_function(e, e_rsax_finish) ||
			!ENGINE_set_ctrl_function(e, e_rsax_ctrl) ||
			!ENGINE_set_cmd_defns(e, e_rsax_cmd_defns))
		return 0;

#ifndef OPENSSL_NO_RSA
	meth1 = RSA_PKCS1_SSLeay();
	e_rsax_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
	e_rsax_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
	e_rsax_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
	e_rsax_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
	e_rsax_rsa.bn_mod_exp = meth1->bn_mod_exp;
#endif
	return 1;
	}

static ENGINE *ENGINE_rsax(void)
	{
	ENGINE *ret = ENGINE_new();
	if(!ret)
		return NULL;
	if(!bind_helper(ret))
		{
		ENGINE_free(ret);
		return NULL;
		}
	return ret;
	}

#ifndef OPENSSL_NO_RSA
/* Used to attach our own key-data to an RSA structure */
static int rsax_ex_data_idx = -1;
#endif

static int e_rsax_destroy(ENGINE *e)
	{
	return 1;
	}

/* (de)initialisation functions. */
static int e_rsax_init(ENGINE *e)
	{
#ifndef OPENSSL_NO_RSA
	if (rsax_ex_data_idx == -1)
		rsax_ex_data_idx = RSA_get_ex_new_index(0,
			NULL,
			NULL, NULL, NULL);
#endif
	if (rsax_ex_data_idx  == -1)
		return 0;
	return 1;
	}

static int e_rsax_finish(ENGINE *e)
	{
	return 1;
	}

static int e_rsax_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void))
	{
	int to_return = 1;

	switch(cmd)
		{
	/* The command isn't understood by this engine */
	default:
		to_return = 0;
		break;
		}

	return to_return;
	}


#ifndef OPENSSL_NO_RSA

#ifdef _WIN32
typedef unsigned __int64 UINT64;
#else
typedef unsigned long long UINT64;
#endif
typedef unsigned short UINT16;

/* Table t is interleaved in the following manner:
 * The order in memory is t[0][0], t[0][1], ..., t[0][7], t[1][0], ...
 * A particular 512-bit value is stored in t[][index] rather than the more
 * normal t[index][]; i.e. the qwords of a particular entry in t are not
 * adjacent in memory
 */

/* Init BIGNUM b from the interleaved UINT64 array */
static int interleaved_array_to_bn_512(BIGNUM* b, UINT64 *array);

/* Extract array elements from BIGNUM b
 * To set the whole array from b, call with n=8
 */
static int bn_extract_to_array_512(const BIGNUM* b, unsigned int n, UINT64 *array);

struct mod_ctx_512 {
    UINT64 t[8][8];
    UINT64 m[8];
    UINT64 m1[8]; /* 2^278 % m */
    UINT64 m2[8]; /* 2^640 % m */
    UINT64 k1[2]; /* (- 1/m) % 2^128 */
};

static int mod_exp_pre_compute_data_512(UINT64 *m, struct mod_ctx_512 *data);

void mod_exp_512(UINT64 *result, /* 512 bits, 8 qwords */
		 UINT64 *g,      /* 512 bits, 8 qwords */
		 UINT64 *exp,    /* 512 bits, 8 qwords */
		 struct mod_ctx_512 *data);

typedef struct st_e_rsax_mod_ctx
{
  UINT64 type;
  union {
    struct mod_ctx_512 b512;
  } ctx;

} E_RSAX_MOD_CTX;

static E_RSAX_MOD_CTX *e_rsax_get_ctx(RSA *rsa, int idx, BIGNUM* m)
{
	E_RSAX_MOD_CTX *hptr;

        if (idx < 0 || idx > 2)
           return NULL;

	hptr = RSA_get_ex_data(rsa, rsax_ex_data_idx);
	if (!hptr) {
	    hptr = OPENSSL_malloc(3*sizeof(E_RSAX_MOD_CTX));
	    if (!hptr) return NULL;
            hptr[2].type = hptr[1].type= hptr[0].type = 0;
	    RSA_set_ex_data(rsa, rsax_ex_data_idx, hptr);
        }

        if (hptr[idx].type == (UINT64)BN_num_bits(m))
            return hptr+idx;

        if (BN_num_bits(m) == 512) {
  	    UINT64 _m[8];
	    bn_extract_to_array_512(m, 8, _m);
	    memset( &hptr[idx].ctx.b512, 0, sizeof(struct mod_ctx_512));
	    mod_exp_pre_compute_data_512(_m, &hptr[idx].ctx.b512);
	}

        hptr[idx].type = BN_num_bits(m);
	return hptr+idx;
}

static int e_rsax_rsa_finish(RSA *rsa)
	{
	E_RSAX_MOD_CTX *hptr = RSA_get_ex_data(rsa, rsax_ex_data_idx);
	if(hptr)
		{
		OPENSSL_free(hptr);
		RSA_set_ex_data(rsa, rsax_ex_data_idx, NULL);
		}
	if (rsa->_method_mod_n)
		BN_MONT_CTX_free(rsa->_method_mod_n);
	if (rsa->_method_mod_p)
		BN_MONT_CTX_free(rsa->_method_mod_p);
	if (rsa->_method_mod_q)
		BN_MONT_CTX_free(rsa->_method_mod_q);
	return 1;
	}


static int e_rsax_bn_mod_exp(BIGNUM *r, const BIGNUM *g, const BIGNUM *e,
                    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont, E_RSAX_MOD_CTX* rsax_mod_ctx )
{
	if (rsax_mod_ctx && BN_get_flags(e, BN_FLG_CONSTTIME) != 0) {
	   if (BN_num_bits(m) == 512) {
  		UINT64 _r[8];
   	  	UINT64 _g[8];
		UINT64 _e[8];

		/* Init the arrays from the BIGNUMs */
		bn_extract_to_array_512(g, 8, _g);
		bn_extract_to_array_512(e, 8, _e);

		mod_exp_512(_r, _g, _e, &rsax_mod_ctx->ctx.b512);
		/* Return the result in the BIGNUM */
		interleaved_array_to_bn_512(r, _r);
                return 1;
           }
        }

	return BN_mod_exp_mont(r, g, e, m, ctx, in_mont);
}

/* Declares for the Intel CIAP 512-bit / CRT / 1024 bit RSA modular
 * exponentiation routine precalculations and a structure to hold the
 * necessary values.  These files are meant to live in crypto/rsa/ in
 * the target openssl.
 */

/*
 * Local method: extracts a piece from a BIGNUM, to fit it into
 * an array. Call with n=8 to extract an entire 512-bit BIGNUM
 */
static int bn_extract_to_array_512(const BIGNUM* b, unsigned int n, UINT64 *array)
{
	int i;
	UINT64 tmp;
	unsigned char bn_buff[64];
	memset(bn_buff, 0, 64);
	if (BN_num_bytes(b) > 64) {
		printf ("Can't support this byte size\n");
		return 0; }
	if (BN_num_bytes(b)!=0) {
		if (!BN_bn2bin(b, bn_buff+(64-BN_num_bytes(b)))) {
			printf ("Error's in bn2bin\n");
			/* We have to error, here */
			return 0; } }
	while (n-- > 0) {
		array[n] = 0;
		for (i=7; i>=0; i--) {
			tmp = bn_buff[63-(n*8+i)];
			array[n] |= tmp << (8*i); } }
	return 1;
}

/* Init a 512-bit BIGNUM from the UINT64*_ (8 * 64) interleaved array */
static int interleaved_array_to_bn_512(BIGNUM* b, UINT64 *array)
{
	unsigned char tmp[64];
	int n=8;
	int i;
	while (n-- > 0) {
		for (i = 7; i>=0; i--) {
			tmp[63-(n*8+i)] = (unsigned char)(array[n]>>(8*i)); } }
	BN_bin2bn(tmp, 64, b);
        return 0;
}


/* The main 512bit precompute call */
static int mod_exp_pre_compute_data_512(UINT64 *m, struct mod_ctx_512 *data)
 {
    BIGNUM two_768, two_640, two_128, two_512, tmp, _m, tmp2;

    /* We need a BN_CTX for the modulo functions */
    BN_CTX* ctx;
    /* Some tmps */
    UINT64 _t[8];
    int i, j, ret = 0;

    /* Init _m with m */
    BN_init(&_m);
    interleaved_array_to_bn_512(&_m, m);
    memset(_t, 0, 64);

    /* Inits */
    BN_init(&two_768);
    BN_init(&two_640);
    BN_init(&two_128);
    BN_init(&two_512);
    BN_init(&tmp);
    BN_init(&tmp2);

    /* Create our context */
    if ((ctx=BN_CTX_new()) == NULL) { goto err; }
	BN_CTX_start(ctx);

    /*
     * For production, if you care, these only need to be set once,
     * and may be made constants.
     */
    BN_lshift(&two_768, BN_value_one(), 768);
    BN_lshift(&two_640, BN_value_one(), 640);
    BN_lshift(&two_128, BN_value_one(), 128);
    BN_lshift(&two_512, BN_value_one(), 512);

    if (0 == (m[7] & 0x8000000000000000)) {
        exit(1);
    }
    if (0 == (m[0] & 0x1)) { /* Odd modulus required for Mont */
        exit(1);
    }

    /* Precompute m1 */
    BN_mod(&tmp, &two_768, &_m, ctx);
    if (!bn_extract_to_array_512(&tmp, 8, &data->m1[0])) {
	    goto err; }

    /* Precompute m2 */
    BN_mod(&tmp, &two_640, &_m, ctx);
    if (!bn_extract_to_array_512(&tmp, 8, &data->m2[0])) {
	    goto err;
    }

    /*
     * Precompute k1, a 128b number = ((-1)* m-1 ) mod 2128; k1 should
     * be non-negative.
     */
    BN_mod_inverse(&tmp, &_m, &two_128, ctx);
    if (!BN_is_zero(&tmp)) { BN_sub(&tmp, &two_128, &tmp); }
    if (!bn_extract_to_array_512(&tmp, 2, &data->k1[0])) {
	    goto err; }

    /* Precompute t */
    for (i=0; i<8; i++) {
        BN_zero(&tmp);
        if (i & 1) { BN_add(&tmp, &two_512, &tmp); }
        if (i & 2) { BN_add(&tmp, &two_512, &tmp); }
        if (i & 4) { BN_add(&tmp, &two_640, &tmp); }

        BN_nnmod(&tmp2, &tmp, &_m, ctx);
        if (!bn_extract_to_array_512(&tmp2, 8, _t)) {
	        goto err; }
        for (j=0; j<8; j++) data->t[j][i] = _t[j]; }

    /* Precompute m */
    for (i=0; i<8; i++) {
        data->m[i] = m[i]; }

    ret = 1;

err:
    /* Cleanup */
	if (ctx != NULL) {
		BN_CTX_end(ctx); BN_CTX_free(ctx); }
    BN_free(&two_768);
    BN_free(&two_640);
    BN_free(&two_128);
    BN_free(&two_512);
    BN_free(&tmp);
    BN_free(&tmp2);
    BN_free(&_m);

    return ret;
}


static int e_rsax_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
	{
	BIGNUM *r1,*m1,*vrfy;
	BIGNUM local_dmp1,local_dmq1,local_c,local_r1;
	BIGNUM *dmp1,*dmq1,*c,*pr1;
	int ret=0;

	BN_CTX_start(ctx);
	r1 = BN_CTX_get(ctx);
	m1 = BN_CTX_get(ctx);
	vrfy = BN_CTX_get(ctx);

	{
		BIGNUM local_p, local_q;
		BIGNUM *p = NULL, *q = NULL;
		int error = 0;

		/* Make sure BN_mod_inverse in Montgomery
		 * intialization uses the BN_FLG_CONSTTIME flag
		 * (unless RSA_FLAG_NO_CONSTTIME is set)
		 */
		if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
			{
			BN_init(&local_p);
			p = &local_p;
			BN_with_flags(p, rsa->p, BN_FLG_CONSTTIME);

			BN_init(&local_q);
			q = &local_q;
			BN_with_flags(q, rsa->q, BN_FLG_CONSTTIME);
			}
		else
			{
			p = rsa->p;
			q = rsa->q;
			}

		if (rsa->flags & RSA_FLAG_CACHE_PRIVATE)
			{
			if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_p, CRYPTO_LOCK_RSA, p, ctx))
				error = 1;
			if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_q, CRYPTO_LOCK_RSA, q, ctx))
				error = 1;
			}

		/* clean up */
		if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
			{
			BN_free(&local_p);
			BN_free(&local_q);
			}
		if ( error )
			goto err;
	}

	if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
		if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
			goto err;

	/* compute I mod q */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
		{
		c = &local_c;
		BN_with_flags(c, I, BN_FLG_CONSTTIME);
		if (!BN_mod(r1,c,rsa->q,ctx)) goto err;
		}
	else
		{
		if (!BN_mod(r1,I,rsa->q,ctx)) goto err;
		}

	/* compute r1^dmq1 mod q */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
		{
		dmq1 = &local_dmq1;
		BN_with_flags(dmq1, rsa->dmq1, BN_FLG_CONSTTIME);
		}
	else
		dmq1 = rsa->dmq1;

	if (!e_rsax_bn_mod_exp(m1,r1,dmq1,rsa->q,ctx,
		rsa->_method_mod_q, e_rsax_get_ctx(rsa, 0, rsa->q) )) goto err;

	/* compute I mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
		{
		c = &local_c;
		BN_with_flags(c, I, BN_FLG_CONSTTIME);
		if (!BN_mod(r1,c,rsa->p,ctx)) goto err;
		}
	else
		{
		if (!BN_mod(r1,I,rsa->p,ctx)) goto err;
		}

	/* compute r1^dmp1 mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
		{
		dmp1 = &local_dmp1;
		BN_with_flags(dmp1, rsa->dmp1, BN_FLG_CONSTTIME);
		}
	else
		dmp1 = rsa->dmp1;

	if (!e_rsax_bn_mod_exp(r0,r1,dmp1,rsa->p,ctx,
		rsa->_method_mod_p, e_rsax_get_ctx(rsa, 1, rsa->p) )) goto err;

	if (!BN_sub(r0,r0,m1)) goto err;
	/* This will help stop the size of r0 increasing, which does
	 * affect the multiply if it optimised for a power of 2 size */
	if (BN_is_negative(r0))
		if (!BN_add(r0,r0,rsa->p)) goto err;

	if (!BN_mul(r1,r0,rsa->iqmp,ctx)) goto err;

	/* Turn BN_FLG_CONSTTIME flag on before division operation */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
		{
		pr1 = &local_r1;
		BN_with_flags(pr1, r1, BN_FLG_CONSTTIME);
		}
	else
		pr1 = r1;
	if (!BN_mod(r0,pr1,rsa->p,ctx)) goto err;

	/* If p < q it is occasionally possible for the correction of
         * adding 'p' if r0 is negative above to leave the result still
	 * negative. This can break the private key operations: the following
	 * second correction should *always* correct this rare occurrence.
	 * This will *never* happen with OpenSSL generated keys because
         * they ensure p > q [steve]
         */
	if (BN_is_negative(r0))
		if (!BN_add(r0,r0,rsa->p)) goto err;
	if (!BN_mul(r1,r0,rsa->q,ctx)) goto err;
	if (!BN_add(r0,r1,m1)) goto err;

	if (rsa->e && rsa->n)
		{
		if (!e_rsax_bn_mod_exp(vrfy,r0,rsa->e,rsa->n,ctx,rsa->_method_mod_n, e_rsax_get_ctx(rsa, 2, rsa->n) ))
                    goto err;

		/* If 'I' was greater than (or equal to) rsa->n, the operation
		 * will be equivalent to using 'I mod n'. However, the result of
		 * the verify will *always* be less than 'n' so we don't check
		 * for absolute equality, just congruency. */
		if (!BN_sub(vrfy, vrfy, I)) goto err;
		if (!BN_mod(vrfy, vrfy, rsa->n, ctx)) goto err;
		if (BN_is_negative(vrfy))
			if (!BN_add(vrfy, vrfy, rsa->n)) goto err;
		if (!BN_is_zero(vrfy))
			{
			/* 'I' and 'vrfy' aren't congruent mod n. Don't leak
			 * miscalculated CRT output, just do a raw (slower)
			 * mod_exp and return that instead. */

			BIGNUM local_d;
			BIGNUM *d = NULL;

			if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME))
				{
				d = &local_d;
				BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
				}
			else
				d = rsa->d;
			if (!e_rsax_bn_mod_exp(r0,I,d,rsa->n,ctx,
						   rsa->_method_mod_n, e_rsax_get_ctx(rsa, 2, rsa->n) )) goto err;
			}
		}
	ret=1;

err:
	BN_CTX_end(ctx);

	return ret;
	}
#endif /* !OPENSSL_NO_RSA */
#endif /* !COMPILE_RSAX */
