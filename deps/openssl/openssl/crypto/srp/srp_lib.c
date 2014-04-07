/* crypto/srp/srp_lib.c */
/* Written by Christophe Renou (christophe.renou@edelweb.fr) with 
 * the precious help of Peter Sylvester (peter.sylvester@edelweb.fr) 
 * for the EdelKey project and contributed to the OpenSSL project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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
 *
 */
#ifndef OPENSSL_NO_SRP
#include "cryptlib.h"
#include "srp_lcl.h"
#include <openssl/srp.h>
#include <openssl/evp.h>

#if (BN_BYTES == 8)
# if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
#  define bn_pack4(a1,a2,a3,a4) ((a1##UI64<<48)|(a2##UI64<<32)|(a3##UI64<<16)|a4##UI64)
# elif defined(__arch64__)
#  define bn_pack4(a1,a2,a3,a4) ((a1##UL<<48)|(a2##UL<<32)|(a3##UL<<16)|a4##UL)
# else
#  define bn_pack4(a1,a2,a3,a4) ((a1##ULL<<48)|(a2##ULL<<32)|(a3##ULL<<16)|a4##ULL)
# endif
#elif (BN_BYTES == 4)
# define bn_pack4(a1,a2,a3,a4)  ((a3##UL<<16)|a4##UL), ((a1##UL<<16)|a2##UL)
#else
# error "unsupported BN_BYTES"
#endif


#include "srp_grps.h"

static BIGNUM *srp_Calc_k(BIGNUM *N, BIGNUM *g)
	{
	/* k = SHA1(N | PAD(g)) -- tls-srp draft 8 */

	unsigned char digest[SHA_DIGEST_LENGTH];
	unsigned char *tmp;
	EVP_MD_CTX ctxt;
	int longg ;
	int longN = BN_num_bytes(N);

	if ((tmp = OPENSSL_malloc(longN)) == NULL)
		return NULL;
	BN_bn2bin(N,tmp) ;

	EVP_MD_CTX_init(&ctxt);
	EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
	EVP_DigestUpdate(&ctxt, tmp, longN);

	memset(tmp, 0, longN);
	longg = BN_bn2bin(g,tmp) ;
        /* use the zeros behind to pad on left */
	EVP_DigestUpdate(&ctxt, tmp + longg, longN-longg);
	EVP_DigestUpdate(&ctxt, tmp, longg);
	OPENSSL_free(tmp);

	EVP_DigestFinal_ex(&ctxt, digest, NULL);
	EVP_MD_CTX_cleanup(&ctxt);
	return BN_bin2bn(digest, sizeof(digest), NULL);	
	}

BIGNUM *SRP_Calc_u(BIGNUM *A, BIGNUM *B, BIGNUM *N)
	{
	/* k = SHA1(PAD(A) || PAD(B) ) -- tls-srp draft 8 */

	BIGNUM *u;	
	unsigned char cu[SHA_DIGEST_LENGTH];
	unsigned char *cAB;
	EVP_MD_CTX ctxt;
	int longN;  
	if ((A == NULL) ||(B == NULL) || (N == NULL))
		return NULL;

	longN= BN_num_bytes(N);

	if ((cAB = OPENSSL_malloc(2*longN)) == NULL) 
		return NULL;

	memset(cAB, 0, longN);

	EVP_MD_CTX_init(&ctxt);
	EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
	EVP_DigestUpdate(&ctxt, cAB + BN_bn2bin(A,cAB+longN), longN);
	EVP_DigestUpdate(&ctxt, cAB + BN_bn2bin(B,cAB+longN), longN);
	OPENSSL_free(cAB);
	EVP_DigestFinal_ex(&ctxt, cu, NULL);
	EVP_MD_CTX_cleanup(&ctxt);

	if (!(u = BN_bin2bn(cu, sizeof(cu), NULL)))
		return NULL;
	if (!BN_is_zero(u))
		return u;
	BN_free(u);
	return NULL;
}

BIGNUM *SRP_Calc_server_key(BIGNUM *A, BIGNUM *v, BIGNUM *u, BIGNUM *b, BIGNUM *N)
	{
	BIGNUM *tmp = NULL, *S = NULL;
	BN_CTX *bn_ctx; 
	
	if (u == NULL || A == NULL || v == NULL || b == NULL || N == NULL)
		return NULL; 

	if ((bn_ctx = BN_CTX_new()) == NULL ||
		(tmp = BN_new()) == NULL ||
		(S = BN_new()) == NULL )
		goto err;

	/* S = (A*v**u) ** b */ 

	if (!BN_mod_exp(tmp,v,u,N,bn_ctx))
		goto err;
	if (!BN_mod_mul(tmp,A,tmp,N,bn_ctx))
		goto err;
	if (!BN_mod_exp(S,tmp,b,N,bn_ctx))
		goto err;
err:
	BN_CTX_free(bn_ctx);
	BN_clear_free(tmp);
	return S;
	}

BIGNUM *SRP_Calc_B(BIGNUM *b, BIGNUM *N, BIGNUM *g, BIGNUM *v)
	{
	BIGNUM  *kv = NULL, *gb = NULL;
	BIGNUM *B = NULL, *k = NULL;
	BN_CTX *bn_ctx;

	if (b == NULL || N == NULL || g == NULL || v == NULL ||
		(bn_ctx = BN_CTX_new()) == NULL)
		return NULL; 

	if ( (kv = BN_new()) == NULL ||
		(gb = BN_new()) == NULL ||
		(B = BN_new())== NULL)
		goto err;

	/* B = g**b + k*v */

	if (!BN_mod_exp(gb,g,b,N,bn_ctx) ||
	   !(k = srp_Calc_k(N,g)) ||
	   !BN_mod_mul(kv,v,k,N,bn_ctx) || 
	   !BN_mod_add(B,gb,kv,N,bn_ctx))
		{
		BN_free(B);
		B = NULL;
		}
err:
	BN_CTX_free(bn_ctx);
	BN_clear_free(kv);
	BN_clear_free(gb);
	BN_free(k); 
	return B;
	}

BIGNUM *SRP_Calc_x(BIGNUM *s, const char *user, const char *pass)
	{
	unsigned char dig[SHA_DIGEST_LENGTH];
	EVP_MD_CTX ctxt;
	unsigned char *cs;

	if ((s == NULL) ||
		(user == NULL) ||
		(pass == NULL))
		return NULL;

	if ((cs = OPENSSL_malloc(BN_num_bytes(s))) == NULL)
		return NULL;

	EVP_MD_CTX_init(&ctxt);
	EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
	EVP_DigestUpdate(&ctxt, user, strlen(user));
	EVP_DigestUpdate(&ctxt, ":", 1);
	EVP_DigestUpdate(&ctxt, pass, strlen(pass));
	EVP_DigestFinal_ex(&ctxt, dig, NULL);

	EVP_DigestInit_ex(&ctxt, EVP_sha1(), NULL);
	BN_bn2bin(s,cs);
	EVP_DigestUpdate(&ctxt, cs, BN_num_bytes(s));
	OPENSSL_free(cs);
	EVP_DigestUpdate(&ctxt, dig, sizeof(dig));
	EVP_DigestFinal_ex(&ctxt, dig, NULL);
	EVP_MD_CTX_cleanup(&ctxt);

	return BN_bin2bn(dig, sizeof(dig), NULL);
	}

BIGNUM *SRP_Calc_A(BIGNUM *a, BIGNUM *N, BIGNUM *g)
	{
	BN_CTX *bn_ctx; 
	BIGNUM * A = NULL;

	if (a == NULL || N == NULL || g == NULL ||
		(bn_ctx = BN_CTX_new()) == NULL) 
		return NULL;

	if ((A = BN_new()) != NULL &&
	   !BN_mod_exp(A,g,a,N,bn_ctx))
		{
		BN_free(A);
		A = NULL;
		}
	BN_CTX_free(bn_ctx);
	return A;
	}


BIGNUM *SRP_Calc_client_key(BIGNUM *N, BIGNUM *B, BIGNUM *g, BIGNUM *x, BIGNUM *a, BIGNUM *u)
	{
	BIGNUM *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL , *k = NULL, *K = NULL;
	BN_CTX *bn_ctx;

	if (u == NULL || B == NULL || N == NULL || g == NULL || x == NULL || a == NULL ||
		(bn_ctx = BN_CTX_new()) == NULL)
		return NULL; 

	if ((tmp = BN_new()) == NULL ||
		(tmp2 = BN_new())== NULL ||
		(tmp3 = BN_new())== NULL ||
		(K = BN_new()) == NULL)
		goto err;
	
	if (!BN_mod_exp(tmp,g,x,N,bn_ctx))
		goto err;
	if (!(k = srp_Calc_k(N,g)))
		goto err;
	if (!BN_mod_mul(tmp2,tmp,k,N,bn_ctx))
		goto err;
	if (!BN_mod_sub(tmp,B,tmp2,N,bn_ctx))
		goto err;

	if (!BN_mod_mul(tmp3,u,x,N,bn_ctx))
		goto err;
	if (!BN_mod_add(tmp2,a,tmp3,N,bn_ctx))
		goto err;
	if (!BN_mod_exp(K,tmp,tmp2,N,bn_ctx))
		goto err;

err :
	BN_CTX_free(bn_ctx);
	BN_clear_free(tmp);
	BN_clear_free(tmp2);
	BN_clear_free(tmp3);
	BN_free(k);
	return K;	
	}

int SRP_Verify_B_mod_N(BIGNUM *B, BIGNUM *N)
	{
	BIGNUM *r;
	BN_CTX *bn_ctx; 
	int ret = 0;

	if (B == NULL || N == NULL ||
		(bn_ctx = BN_CTX_new()) == NULL)
		return 0;

	if ((r = BN_new()) == NULL)
		goto err;
	/* Checks if B % N == 0 */
	if (!BN_nnmod(r,B,N,bn_ctx))
		goto err;
	ret = !BN_is_zero(r);
err:
	BN_CTX_free(bn_ctx);
	BN_free(r);
	return ret;
	}

int SRP_Verify_A_mod_N(BIGNUM *A, BIGNUM *N)
	{
	/* Checks if A % N == 0 */
	return SRP_Verify_B_mod_N(A,N) ;
	}


/* Check if G and N are kwown parameters. 
   The values have been generated from the ietf-tls-srp draft version 8
*/
char *SRP_check_known_gN_param(BIGNUM *g, BIGNUM *N)
	{
	size_t i;
	if ((g == NULL) || (N == NULL))
		return 0;

	srp_bn_print(g);
	srp_bn_print(N);

	for(i = 0; i < KNOWN_GN_NUMBER; i++)
		{
		if (BN_cmp(knowngN[i].g, g) == 0 && BN_cmp(knowngN[i].N, N) == 0) 
			return knowngN[i].id;
		}
	return NULL;
	}

SRP_gN *SRP_get_default_gN(const char *id)
	{
	size_t i;

	if (id == NULL) 
		return knowngN;
	for(i = 0; i < KNOWN_GN_NUMBER; i++)
		{
		if (strcmp(knowngN[i].id, id)==0)
			return knowngN + i;
		}
	return NULL;
	}
#endif
