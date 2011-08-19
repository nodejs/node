/* crypto/asn1/x_pubkey.c */
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
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif

/* Minor tweak to operation: free up EVP_PKEY */
static int pubkey_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
	{
	if (operation == ASN1_OP_FREE_POST)
		{
		X509_PUBKEY *pubkey = (X509_PUBKEY *)*pval;
		EVP_PKEY_free(pubkey->pkey);
		}
	return 1;
	}

ASN1_SEQUENCE_cb(X509_PUBKEY, pubkey_cb) = {
	ASN1_SIMPLE(X509_PUBKEY, algor, X509_ALGOR),
	ASN1_SIMPLE(X509_PUBKEY, public_key, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END_cb(X509_PUBKEY, X509_PUBKEY)

IMPLEMENT_ASN1_FUNCTIONS(X509_PUBKEY)

int X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey)
	{
	X509_PUBKEY *pk=NULL;
	X509_ALGOR *a;
	ASN1_OBJECT *o;
	unsigned char *s,*p = NULL;
	int i;

	if (x == NULL) return(0);

	if ((pk=X509_PUBKEY_new()) == NULL) goto err;
	a=pk->algor;

	/* set the algorithm id */
	if ((o=OBJ_nid2obj(pkey->type)) == NULL) goto err;
	ASN1_OBJECT_free(a->algorithm);
	a->algorithm=o;

	/* Set the parameter list */
	if (!pkey->save_parameters || (pkey->type == EVP_PKEY_RSA))
		{
		if ((a->parameter == NULL) ||
			(a->parameter->type != V_ASN1_NULL))
			{
			ASN1_TYPE_free(a->parameter);
			if (!(a->parameter=ASN1_TYPE_new()))
				{
				X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
				goto err;
				}
			a->parameter->type=V_ASN1_NULL;
			}
		}
#ifndef OPENSSL_NO_DSA
	else if (pkey->type == EVP_PKEY_DSA)
		{
		unsigned char *pp;
		DSA *dsa;
		
		dsa=pkey->pkey.dsa;
		dsa->write_params=0;
		ASN1_TYPE_free(a->parameter);
		if ((i=i2d_DSAparams(dsa,NULL)) <= 0)
			goto err;
		if (!(p=(unsigned char *)OPENSSL_malloc(i)))
			{
			X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		pp=p;
		i2d_DSAparams(dsa,&pp);
		if (!(a->parameter=ASN1_TYPE_new()))
			{
			OPENSSL_free(p);
			X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		a->parameter->type=V_ASN1_SEQUENCE;
		if (!(a->parameter->value.sequence=ASN1_STRING_new()))
			{
			OPENSSL_free(p);
			X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		if (!ASN1_STRING_set(a->parameter->value.sequence,p,i))
			{
			OPENSSL_free(p);
			X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		OPENSSL_free(p);
		}
#endif
#ifndef OPENSSL_NO_EC
	else if (pkey->type == EVP_PKEY_EC)
		{
		int nid=0;
		unsigned char *pp;
		EC_KEY *ec_key;
		const EC_GROUP *group;
		
		ec_key = pkey->pkey.ec;
		ASN1_TYPE_free(a->parameter);

		if ((a->parameter = ASN1_TYPE_new()) == NULL)
			{
			X509err(X509_F_X509_PUBKEY_SET, ERR_R_ASN1_LIB);
			goto err;
			}

		group = EC_KEY_get0_group(ec_key);
		if (EC_GROUP_get_asn1_flag(group)
                     && (nid = EC_GROUP_get_curve_name(group)))
			{
			/* just set the OID */
			a->parameter->type = V_ASN1_OBJECT;
			a->parameter->value.object = OBJ_nid2obj(nid);
			}
		else /* explicit parameters */
			{
			if ((i = i2d_ECParameters(ec_key, NULL)) == 0)
				{
				X509err(X509_F_X509_PUBKEY_SET, ERR_R_EC_LIB);
				goto err;
				}
			if ((p = (unsigned char *) OPENSSL_malloc(i)) == NULL)
				{
				X509err(X509_F_X509_PUBKEY_SET, ERR_R_MALLOC_FAILURE);
				goto err;
				}	
			pp = p;
			if (!i2d_ECParameters(ec_key, &pp))
				{
				X509err(X509_F_X509_PUBKEY_SET, ERR_R_EC_LIB);
				OPENSSL_free(p);
				goto err;
				}
			a->parameter->type = V_ASN1_SEQUENCE;
			if ((a->parameter->value.sequence = ASN1_STRING_new()) == NULL)
				{
				X509err(X509_F_X509_PUBKEY_SET, ERR_R_ASN1_LIB);
				OPENSSL_free(p);
				goto err;
				}
			ASN1_STRING_set(a->parameter->value.sequence, p, i);
			OPENSSL_free(p);
			}
		}
#endif
	else if (1)
		{
		X509err(X509_F_X509_PUBKEY_SET,X509_R_UNSUPPORTED_ALGORITHM);
		goto err;
		}

	if ((i=i2d_PublicKey(pkey,NULL)) <= 0) goto err;
	if ((s=(unsigned char *)OPENSSL_malloc(i+1)) == NULL)
		{
		X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	p=s;
	i2d_PublicKey(pkey,&p);
	if (!M_ASN1_BIT_STRING_set(pk->public_key,s,i))
		{
		X509err(X509_F_X509_PUBKEY_SET,ERR_R_MALLOC_FAILURE);
		goto err;
		}
  	/* Set number of unused bits to zero */
	pk->public_key->flags&= ~(ASN1_STRING_FLAG_BITS_LEFT|0x07);
	pk->public_key->flags|=ASN1_STRING_FLAG_BITS_LEFT;

	OPENSSL_free(s);

#if 0
	CRYPTO_add(&pkey->references,1,CRYPTO_LOCK_EVP_PKEY);
	pk->pkey=pkey;
#endif

	if (*x != NULL)
		X509_PUBKEY_free(*x);

	*x=pk;

	return 1;
err:
	if (pk != NULL) X509_PUBKEY_free(pk);
	return 0;
	}

EVP_PKEY *X509_PUBKEY_get(X509_PUBKEY *key)
	{
	EVP_PKEY *ret=NULL;
	long j;
	int type;
	const unsigned char *p;
#if !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_ECDSA)
	const unsigned char *cp;
	X509_ALGOR *a;
#endif

	if (key == NULL) goto err;

	if (key->pkey != NULL)
		{
		CRYPTO_add(&key->pkey->references, 1, CRYPTO_LOCK_EVP_PKEY);
		return(key->pkey);
		}

	if (key->public_key == NULL) goto err;

	type=OBJ_obj2nid(key->algor->algorithm);
	if ((ret = EVP_PKEY_new()) == NULL)
		{
		X509err(X509_F_X509_PUBKEY_GET, ERR_R_MALLOC_FAILURE);
		goto err;
		}
	ret->type = EVP_PKEY_type(type);

	/* the parameters must be extracted before the public key (ECDSA!) */
	
#if !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_ECDSA)
	a=key->algor;
#endif

	if (0)
		;
#ifndef OPENSSL_NO_DSA
	else if (ret->type == EVP_PKEY_DSA)
		{
		if (a->parameter && (a->parameter->type == V_ASN1_SEQUENCE))
			{
			if ((ret->pkey.dsa = DSA_new()) == NULL)
				{
				X509err(X509_F_X509_PUBKEY_GET, ERR_R_MALLOC_FAILURE);
				goto err;
				}
			ret->pkey.dsa->write_params=0;
			cp=p=a->parameter->value.sequence->data;
			j=a->parameter->value.sequence->length;
			if (!d2i_DSAparams(&ret->pkey.dsa, &cp, (long)j))
				goto err;
			}
		ret->save_parameters=1;
		}
#endif
#ifndef OPENSSL_NO_EC
	else if (ret->type == EVP_PKEY_EC)
		{
		if (a->parameter && (a->parameter->type == V_ASN1_SEQUENCE))
			{
			/* type == V_ASN1_SEQUENCE => we have explicit parameters
                         * (e.g. parameters in the X9_62_EC_PARAMETERS-structure )
			 */
			if ((ret->pkey.ec= EC_KEY_new()) == NULL)
				{
				X509err(X509_F_X509_PUBKEY_GET, 
					ERR_R_MALLOC_FAILURE);
				goto err;
				}
			cp = p = a->parameter->value.sequence->data;
			j = a->parameter->value.sequence->length;
			if (!d2i_ECParameters(&ret->pkey.ec, &cp, (long)j))
				{
				X509err(X509_F_X509_PUBKEY_GET, ERR_R_EC_LIB);
				goto err;
				}
			}
		else if (a->parameter && (a->parameter->type == V_ASN1_OBJECT))
			{
			/* type == V_ASN1_OBJECT => the parameters are given
			 * by an asn1 OID
			 */
			EC_KEY   *ec_key;
			EC_GROUP *group;

			if (ret->pkey.ec == NULL)
				ret->pkey.ec = EC_KEY_new();
			ec_key = ret->pkey.ec;
			if (ec_key == NULL)
				goto err;
			group = EC_GROUP_new_by_curve_name(OBJ_obj2nid(a->parameter->value.object));
			if (group == NULL)
				goto err;
			EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);
			if (EC_KEY_set_group(ec_key, group) == 0)
				goto err;
			EC_GROUP_free(group);
			}
			/* the case implicitlyCA is currently not implemented */
		ret->save_parameters = 1;
		}
#endif

	p=key->public_key->data;
        j=key->public_key->length;
        if (!d2i_PublicKey(type, &ret, &p, (long)j))
		{
		X509err(X509_F_X509_PUBKEY_GET, X509_R_ERR_ASN1_LIB);
		goto err;
		}

	key->pkey = ret;
	CRYPTO_add(&ret->references, 1, CRYPTO_LOCK_EVP_PKEY);
	return(ret);
err:
	if (ret != NULL)
		EVP_PKEY_free(ret);
	return(NULL);
	}

/* Now two pseudo ASN1 routines that take an EVP_PKEY structure
 * and encode or decode as X509_PUBKEY
 */

EVP_PKEY *d2i_PUBKEY(EVP_PKEY **a, const unsigned char **pp,
	     long length)
	{
	X509_PUBKEY *xpk;
	EVP_PKEY *pktmp;
	xpk = d2i_X509_PUBKEY(NULL, pp, length);
	if(!xpk) return NULL;
	pktmp = X509_PUBKEY_get(xpk);
	X509_PUBKEY_free(xpk);
	if(!pktmp) return NULL;
	if(a)
		{
		EVP_PKEY_free(*a);
		*a = pktmp;
		}
	return pktmp;
	}

int i2d_PUBKEY(EVP_PKEY *a, unsigned char **pp)
	{
	X509_PUBKEY *xpk=NULL;
	int ret;
	if(!a) return 0;
	if(!X509_PUBKEY_set(&xpk, a)) return 0;
	ret = i2d_X509_PUBKEY(xpk, pp);
	X509_PUBKEY_free(xpk);
	return ret;
	}

/* The following are equivalents but which return RSA and DSA
 * keys
 */
#ifndef OPENSSL_NO_RSA
RSA *d2i_RSA_PUBKEY(RSA **a, const unsigned char **pp,
	     long length)
	{
	EVP_PKEY *pkey;
	RSA *key;
	const unsigned char *q;
	q = *pp;
	pkey = d2i_PUBKEY(NULL, &q, length);
	if (!pkey) return NULL;
	key = EVP_PKEY_get1_RSA(pkey);
	EVP_PKEY_free(pkey);
	if (!key) return NULL;
	*pp = q;
	if (a)
		{
		RSA_free(*a);
		*a = key;
		}
	return key;
	}

int i2d_RSA_PUBKEY(RSA *a, unsigned char **pp)
	{
	EVP_PKEY *pktmp;
	int ret;
	if (!a) return 0;
	pktmp = EVP_PKEY_new();
	if (!pktmp)
		{
		ASN1err(ASN1_F_I2D_RSA_PUBKEY, ERR_R_MALLOC_FAILURE);
		return 0;
		}
	EVP_PKEY_set1_RSA(pktmp, a);
	ret = i2d_PUBKEY(pktmp, pp);
	EVP_PKEY_free(pktmp);
	return ret;
	}
#endif

#ifndef OPENSSL_NO_DSA
DSA *d2i_DSA_PUBKEY(DSA **a, const unsigned char **pp,
	     long length)
	{
	EVP_PKEY *pkey;
	DSA *key;
	const unsigned char *q;
	q = *pp;
	pkey = d2i_PUBKEY(NULL, &q, length);
	if (!pkey) return NULL;
	key = EVP_PKEY_get1_DSA(pkey);
	EVP_PKEY_free(pkey);
	if (!key) return NULL;
	*pp = q;
	if (a)
		{
		DSA_free(*a);
		*a = key;
		}
	return key;
	}

int i2d_DSA_PUBKEY(DSA *a, unsigned char **pp)
	{
	EVP_PKEY *pktmp;
	int ret;
	if(!a) return 0;
	pktmp = EVP_PKEY_new();
	if(!pktmp)
		{
		ASN1err(ASN1_F_I2D_DSA_PUBKEY, ERR_R_MALLOC_FAILURE);
		return 0;
		}
	EVP_PKEY_set1_DSA(pktmp, a);
	ret = i2d_PUBKEY(pktmp, pp);
	EVP_PKEY_free(pktmp);
	return ret;
	}
#endif

#ifndef OPENSSL_NO_EC
EC_KEY *d2i_EC_PUBKEY(EC_KEY **a, const unsigned char **pp, long length)
	{
	EVP_PKEY *pkey;
	EC_KEY *key;
	const unsigned char *q;
	q = *pp;
	pkey = d2i_PUBKEY(NULL, &q, length);
	if (!pkey) return(NULL);
	key = EVP_PKEY_get1_EC_KEY(pkey);
	EVP_PKEY_free(pkey);
	if (!key)  return(NULL);
	*pp = q;
	if (a)
		{
		EC_KEY_free(*a);
		*a = key;
		}
	return(key);
	}

int i2d_EC_PUBKEY(EC_KEY *a, unsigned char **pp)
	{
	EVP_PKEY *pktmp;
	int ret;
	if (!a)	return(0);
	if ((pktmp = EVP_PKEY_new()) == NULL)
		{
		ASN1err(ASN1_F_I2D_EC_PUBKEY, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	EVP_PKEY_set1_EC_KEY(pktmp, a);
	ret = i2d_PUBKEY(pktmp, pp);
	EVP_PKEY_free(pktmp);
	return(ret);
	}
#endif
