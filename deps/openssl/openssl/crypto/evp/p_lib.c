/* crypto/evp/p_lib.c */
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
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/asn1_mac.h>
#include <openssl/x509.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif

static void EVP_PKEY_free_it(EVP_PKEY *x);

int EVP_PKEY_bits(EVP_PKEY *pkey)
	{
	if (0)
		return 0;
#ifndef OPENSSL_NO_RSA
	else if (pkey->type == EVP_PKEY_RSA)
		return(BN_num_bits(pkey->pkey.rsa->n));
#endif
#ifndef OPENSSL_NO_DSA
	else if (pkey->type == EVP_PKEY_DSA)
		return(BN_num_bits(pkey->pkey.dsa->p));
#endif
#ifndef OPENSSL_NO_EC
	else if (pkey->type == EVP_PKEY_EC)
		{
		BIGNUM *order = BN_new();
		const EC_GROUP *group;
		int ret;

		if (!order)
			{
			ERR_clear_error();
			return 0;
			}
		group = EC_KEY_get0_group(pkey->pkey.ec);
		if (!EC_GROUP_get_order(group, order, NULL))
			{
			ERR_clear_error();
			return 0;
			}

		ret = BN_num_bits(order);
		BN_free(order);
		return ret;
		}
#endif
	return(0);
	}

int EVP_PKEY_size(EVP_PKEY *pkey)
	{
	if (pkey == NULL)
		return(0);
#ifndef OPENSSL_NO_RSA
	if (pkey->type == EVP_PKEY_RSA)
		return(RSA_size(pkey->pkey.rsa));
	else
#endif
#ifndef OPENSSL_NO_DSA
		if (pkey->type == EVP_PKEY_DSA)
		return(DSA_size(pkey->pkey.dsa));
#endif
#ifndef OPENSSL_NO_ECDSA
		if (pkey->type == EVP_PKEY_EC)
		return(ECDSA_size(pkey->pkey.ec));
#endif

	return(0);
	}

int EVP_PKEY_save_parameters(EVP_PKEY *pkey, int mode)
	{
#ifndef OPENSSL_NO_DSA
	if (pkey->type == EVP_PKEY_DSA)
		{
		int ret=pkey->save_parameters;

		if (mode >= 0)
			pkey->save_parameters=mode;
		return(ret);
		}
#endif
#ifndef OPENSSL_NO_EC
	if (pkey->type == EVP_PKEY_EC)
		{
		int ret = pkey->save_parameters;

		if (mode >= 0)
			pkey->save_parameters = mode;
		return(ret);
		}
#endif
	return(0);
	}

int EVP_PKEY_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from)
	{
	if (to->type != from->type)
		{
		EVPerr(EVP_F_EVP_PKEY_COPY_PARAMETERS,EVP_R_DIFFERENT_KEY_TYPES);
		goto err;
		}

	if (EVP_PKEY_missing_parameters(from))
		{
		EVPerr(EVP_F_EVP_PKEY_COPY_PARAMETERS,EVP_R_MISSING_PARAMETERS);
		goto err;
		}
#ifndef OPENSSL_NO_DSA
	if (to->type == EVP_PKEY_DSA)
		{
		BIGNUM *a;

		if ((a=BN_dup(from->pkey.dsa->p)) == NULL) goto err;
		if (to->pkey.dsa->p != NULL) BN_free(to->pkey.dsa->p);
		to->pkey.dsa->p=a;

		if ((a=BN_dup(from->pkey.dsa->q)) == NULL) goto err;
		if (to->pkey.dsa->q != NULL) BN_free(to->pkey.dsa->q);
		to->pkey.dsa->q=a;

		if ((a=BN_dup(from->pkey.dsa->g)) == NULL) goto err;
		if (to->pkey.dsa->g != NULL) BN_free(to->pkey.dsa->g);
		to->pkey.dsa->g=a;
		}
#endif
#ifndef OPENSSL_NO_EC
	if (to->type == EVP_PKEY_EC)
		{
		EC_GROUP *group = EC_GROUP_dup(EC_KEY_get0_group(from->pkey.ec));
		if (group == NULL)
			goto err;
		if (EC_KEY_set_group(to->pkey.ec, group) == 0)
			goto err;
		EC_GROUP_free(group);
		}
#endif
	return(1);
err:
	return(0);
	}

int EVP_PKEY_missing_parameters(const EVP_PKEY *pkey)
	{
#ifndef OPENSSL_NO_DSA
	if (pkey->type == EVP_PKEY_DSA)
		{
		DSA *dsa;

		dsa=pkey->pkey.dsa;
		if ((dsa->p == NULL) || (dsa->q == NULL) || (dsa->g == NULL))
			return(1);
		}
#endif
#ifndef OPENSSL_NO_EC
	if (pkey->type == EVP_PKEY_EC)
		{
		if (EC_KEY_get0_group(pkey->pkey.ec) == NULL)
			return(1);
		}
#endif

	return(0);
	}

int EVP_PKEY_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b)
	{
#ifndef OPENSSL_NO_DSA
	if ((a->type == EVP_PKEY_DSA) && (b->type == EVP_PKEY_DSA))
		{
		if (	BN_cmp(a->pkey.dsa->p,b->pkey.dsa->p) ||
			BN_cmp(a->pkey.dsa->q,b->pkey.dsa->q) ||
			BN_cmp(a->pkey.dsa->g,b->pkey.dsa->g))
			return(0);
		else
			return(1);
		}
#endif
#ifndef OPENSSL_NO_EC
	if (a->type == EVP_PKEY_EC && b->type == EVP_PKEY_EC)
		{
		const EC_GROUP *group_a = EC_KEY_get0_group(a->pkey.ec),
		               *group_b = EC_KEY_get0_group(b->pkey.ec);
		if (EC_GROUP_cmp(group_a, group_b, NULL))
			return 0;
		else
			return 1;
		}
#endif
	return(-1);
	}

int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
	{
	if (a->type != b->type)
		return -1;

	if (EVP_PKEY_cmp_parameters(a, b) == 0)
		return 0;

	switch (a->type)
		{
#ifndef OPENSSL_NO_RSA
	case EVP_PKEY_RSA:
		if (BN_cmp(b->pkey.rsa->n,a->pkey.rsa->n) != 0
			|| BN_cmp(b->pkey.rsa->e,a->pkey.rsa->e) != 0)
			return 0;
		break;
#endif
#ifndef OPENSSL_NO_DSA
	case EVP_PKEY_DSA:
		if (BN_cmp(b->pkey.dsa->pub_key,a->pkey.dsa->pub_key) != 0)
			return 0;
		break;
#endif
#ifndef OPENSSL_NO_EC
	case EVP_PKEY_EC:
		{
		int  r;
		const EC_GROUP *group = EC_KEY_get0_group(b->pkey.ec);
		const EC_POINT *pa = EC_KEY_get0_public_key(a->pkey.ec),
		               *pb = EC_KEY_get0_public_key(b->pkey.ec);
		r = EC_POINT_cmp(group, pa, pb, NULL);
		if (r != 0)
			{
			if (r == 1)
				return 0;
			else
				return -2;
			}
		}
 		break;
#endif
#ifndef OPENSSL_NO_DH
	case EVP_PKEY_DH:
		return -2;
#endif
	default:
		return -2;
		}

	return 1;
	}

EVP_PKEY *EVP_PKEY_new(void)
	{
	EVP_PKEY *ret;

	ret=(EVP_PKEY *)OPENSSL_malloc(sizeof(EVP_PKEY));
	if (ret == NULL)
		{
		EVPerr(EVP_F_EVP_PKEY_NEW,ERR_R_MALLOC_FAILURE);
		return(NULL);
		}
	ret->type=EVP_PKEY_NONE;
	ret->references=1;
	ret->pkey.ptr=NULL;
	ret->attributes=NULL;
	ret->save_parameters=1;
	return(ret);
	}

int EVP_PKEY_assign(EVP_PKEY *pkey, int type, char *key)
	{
	if (pkey == NULL) return(0);
	if (pkey->pkey.ptr != NULL)
		EVP_PKEY_free_it(pkey);
	pkey->type=EVP_PKEY_type(type);
	pkey->save_type=type;
	pkey->pkey.ptr=key;
	return(key != NULL);
	}

#ifndef OPENSSL_NO_RSA
int EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key)
{
	int ret = EVP_PKEY_assign_RSA(pkey, key);
	if(ret)
		RSA_up_ref(key);
	return ret;
}

RSA *EVP_PKEY_get1_RSA(EVP_PKEY *pkey)
	{
	if(pkey->type != EVP_PKEY_RSA) {
		EVPerr(EVP_F_EVP_PKEY_GET1_RSA, EVP_R_EXPECTING_AN_RSA_KEY);
		return NULL;
	}
	RSA_up_ref(pkey->pkey.rsa);
	return pkey->pkey.rsa;
}
#endif

#ifndef OPENSSL_NO_DSA
int EVP_PKEY_set1_DSA(EVP_PKEY *pkey, DSA *key)
{
	int ret = EVP_PKEY_assign_DSA(pkey, key);
	if(ret)
		DSA_up_ref(key);
	return ret;
}

DSA *EVP_PKEY_get1_DSA(EVP_PKEY *pkey)
	{
	if(pkey->type != EVP_PKEY_DSA) {
		EVPerr(EVP_F_EVP_PKEY_GET1_DSA, EVP_R_EXPECTING_A_DSA_KEY);
		return NULL;
	}
	DSA_up_ref(pkey->pkey.dsa);
	return pkey->pkey.dsa;
}
#endif

#ifndef OPENSSL_NO_EC

int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey, EC_KEY *key)
{
	int ret = EVP_PKEY_assign_EC_KEY(pkey,key);
	if (ret)
		EC_KEY_up_ref(key);
	return ret;
}

EC_KEY *EVP_PKEY_get1_EC_KEY(EVP_PKEY *pkey)
{
	if (pkey->type != EVP_PKEY_EC)
	{
		EVPerr(EVP_F_EVP_PKEY_GET1_EC_KEY, EVP_R_EXPECTING_A_EC_KEY);
		return NULL;
	}
	EC_KEY_up_ref(pkey->pkey.ec);
	return pkey->pkey.ec;
}
#endif


#ifndef OPENSSL_NO_DH

int EVP_PKEY_set1_DH(EVP_PKEY *pkey, DH *key)
{
	int ret = EVP_PKEY_assign_DH(pkey, key);
	if(ret)
		DH_up_ref(key);
	return ret;
}

DH *EVP_PKEY_get1_DH(EVP_PKEY *pkey)
	{
	if(pkey->type != EVP_PKEY_DH) {
		EVPerr(EVP_F_EVP_PKEY_GET1_DH, EVP_R_EXPECTING_A_DH_KEY);
		return NULL;
	}
	DH_up_ref(pkey->pkey.dh);
	return pkey->pkey.dh;
}
#endif

int EVP_PKEY_type(int type)
	{
	switch (type)
		{
	case EVP_PKEY_RSA:
	case EVP_PKEY_RSA2:
		return(EVP_PKEY_RSA);
	case EVP_PKEY_DSA:
	case EVP_PKEY_DSA1:
	case EVP_PKEY_DSA2:
	case EVP_PKEY_DSA3:
	case EVP_PKEY_DSA4:
		return(EVP_PKEY_DSA);
	case EVP_PKEY_DH:
		return(EVP_PKEY_DH);
	case EVP_PKEY_EC:
		return(EVP_PKEY_EC);
	default:
		return(NID_undef);
		}
	}

void EVP_PKEY_free(EVP_PKEY *x)
	{
	int i;

	if (x == NULL) return;

	i=CRYPTO_add(&x->references,-1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",x);
#endif
	if (i > 0) return;
#ifdef REF_CHECK
	if (i < 0)
		{
		fprintf(stderr,"EVP_PKEY_free, bad reference count\n");
		abort();
		}
#endif
	EVP_PKEY_free_it(x);
	if (x->attributes)
		sk_X509_ATTRIBUTE_pop_free(x->attributes, X509_ATTRIBUTE_free);
	OPENSSL_free(x);
	}

static void EVP_PKEY_free_it(EVP_PKEY *x)
	{
	switch (x->type)
		{
#ifndef OPENSSL_NO_RSA
	case EVP_PKEY_RSA:
	case EVP_PKEY_RSA2:
		RSA_free(x->pkey.rsa);
		break;
#endif
#ifndef OPENSSL_NO_DSA
	case EVP_PKEY_DSA:
	case EVP_PKEY_DSA2:
	case EVP_PKEY_DSA3:
	case EVP_PKEY_DSA4:
		DSA_free(x->pkey.dsa);
		break;
#endif
#ifndef OPENSSL_NO_EC
	case EVP_PKEY_EC:
		EC_KEY_free(x->pkey.ec);
		break;
#endif
#ifndef OPENSSL_NO_DH
	case EVP_PKEY_DH:
		DH_free(x->pkey.dh);
		break;
#endif
		}
	}

