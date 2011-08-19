/* evp_pkey.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2002 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include "cryptlib.h"
#include <openssl/x509.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
#include <openssl/dsa.h>
#endif
#include <openssl/bn.h>

#ifndef OPENSSL_NO_DSA
static int dsa_pkey2pkcs8(PKCS8_PRIV_KEY_INFO *p8inf, EVP_PKEY *pkey);
#endif
#ifndef OPENSSL_NO_EC
static int eckey_pkey2pkcs8(PKCS8_PRIV_KEY_INFO *p8inf, EVP_PKEY *pkey);
#endif

/* Extract a private key from a PKCS8 structure */

EVP_PKEY *EVP_PKCS82PKEY(PKCS8_PRIV_KEY_INFO *p8)
{
	EVP_PKEY *pkey = NULL;
#ifndef OPENSSL_NO_RSA
	RSA *rsa = NULL;
#endif
#ifndef OPENSSL_NO_DSA
	DSA *dsa = NULL;
	ASN1_TYPE *t1, *t2;
	ASN1_INTEGER *privkey;
	STACK_OF(ASN1_TYPE) *ndsa = NULL;
#endif
#ifndef OPENSSL_NO_EC
	EC_KEY *eckey = NULL;
	const unsigned char *p_tmp;
#endif
#if !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_EC)
	ASN1_TYPE    *param = NULL;	
	BN_CTX *ctx = NULL;
	int plen;
#endif
	X509_ALGOR *a;
	const unsigned char *p;
	const unsigned char *cp;
	int pkeylen;
	int  nid;
	char obj_tmp[80];

	if(p8->pkey->type == V_ASN1_OCTET_STRING) {
		p8->broken = PKCS8_OK;
		p = p8->pkey->value.octet_string->data;
		pkeylen = p8->pkey->value.octet_string->length;
	} else {
		p8->broken = PKCS8_NO_OCTET;
		p = p8->pkey->value.sequence->data;
		pkeylen = p8->pkey->value.sequence->length;
	}
	if (!(pkey = EVP_PKEY_new())) {
		EVPerr(EVP_F_EVP_PKCS82PKEY,ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	a = p8->pkeyalg;
	nid = OBJ_obj2nid(a->algorithm);
	switch(nid)
	{
#ifndef OPENSSL_NO_RSA
		case NID_rsaEncryption:
		cp = p;
		if (!(rsa = d2i_RSAPrivateKey (NULL,&cp, pkeylen))) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			return NULL;
		}
		EVP_PKEY_assign_RSA (pkey, rsa);
		break;
#endif
#ifndef OPENSSL_NO_DSA
		case NID_dsa:
		/* PKCS#8 DSA is weird: you just get a private key integer
	         * and parameters in the AlgorithmIdentifier the pubkey must
		 * be recalculated.
		 */
	
		/* Check for broken DSA PKCS#8, UGH! */
		if(*p == (V_ASN1_SEQUENCE|V_ASN1_CONSTRUCTED)) {
		    if(!(ndsa = ASN1_seq_unpack_ASN1_TYPE(p, pkeylen, 
							  d2i_ASN1_TYPE,
							  ASN1_TYPE_free))) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		    }
		    if(sk_ASN1_TYPE_num(ndsa) != 2 ) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		    }
		    /* Handle Two broken types:
		     * SEQUENCE {parameters, priv_key}
		     * SEQUENCE {pub_key, priv_key}
		     */

		    t1 = sk_ASN1_TYPE_value(ndsa, 0);
		    t2 = sk_ASN1_TYPE_value(ndsa, 1);
		    if(t1->type == V_ASN1_SEQUENCE) {
			p8->broken = PKCS8_EMBEDDED_PARAM;
			param = t1;
		    } else if(a->parameter->type == V_ASN1_SEQUENCE) {
			p8->broken = PKCS8_NS_DB;
			param = a->parameter;
		    } else {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		    }

		    if(t2->type != V_ASN1_INTEGER) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		    }
		    privkey = t2->value.integer;
		} else {
			if (!(privkey=d2i_ASN1_INTEGER (NULL, &p, pkeylen))) {
				EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
				goto dsaerr;
			}
			param = p8->pkeyalg->parameter;
		}
		if (!param || (param->type != V_ASN1_SEQUENCE)) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		}
		cp = p = param->value.sequence->data;
		plen = param->value.sequence->length;
		if (!(dsa = d2i_DSAparams (NULL, &cp, plen))) {
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto dsaerr;
		}
		/* We have parameters now set private key */
		if (!(dsa->priv_key = ASN1_INTEGER_to_BN(privkey, NULL))) {
			EVPerr(EVP_F_EVP_PKCS82PKEY,EVP_R_BN_DECODE_ERROR);
			goto dsaerr;
		}
		/* Calculate public key (ouch!) */
		if (!(dsa->pub_key = BN_new())) {
			EVPerr(EVP_F_EVP_PKCS82PKEY,ERR_R_MALLOC_FAILURE);
			goto dsaerr;
		}
		if (!(ctx = BN_CTX_new())) {
			EVPerr(EVP_F_EVP_PKCS82PKEY,ERR_R_MALLOC_FAILURE);
			goto dsaerr;
		}
			
		if (!BN_mod_exp(dsa->pub_key, dsa->g,
						 dsa->priv_key, dsa->p, ctx)) {
			
			EVPerr(EVP_F_EVP_PKCS82PKEY,EVP_R_BN_PUBKEY_ERROR);
			goto dsaerr;
		}

		EVP_PKEY_assign_DSA(pkey, dsa);
		BN_CTX_free (ctx);
		if(ndsa) sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
		else ASN1_INTEGER_free(privkey);
		break;
		dsaerr:
		BN_CTX_free (ctx);
		sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
		DSA_free(dsa);
		EVP_PKEY_free(pkey);
		return NULL;
		break;
#endif
#ifndef OPENSSL_NO_EC
		case NID_X9_62_id_ecPublicKey:
		p_tmp = p;
		/* extract the ec parameters */
		param = p8->pkeyalg->parameter;

		if (!param || ((param->type != V_ASN1_SEQUENCE) &&
		    (param->type != V_ASN1_OBJECT)))
		{
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto ecerr;
		}

		if (param->type == V_ASN1_SEQUENCE)
		{
			cp = p = param->value.sequence->data;
			plen = param->value.sequence->length;

			if (!(eckey = d2i_ECParameters(NULL, &cp, plen)))
			{
				EVPerr(EVP_F_EVP_PKCS82PKEY,
					EVP_R_DECODE_ERROR);
				goto ecerr;
			}
		}
		else
		{
			EC_GROUP *group;
			cp = p = param->value.object->data;
			plen = param->value.object->length;

			/* type == V_ASN1_OBJECT => the parameters are given
			 * by an asn1 OID
			 */
			if ((eckey = EC_KEY_new()) == NULL)
			{
				EVPerr(EVP_F_EVP_PKCS82PKEY,
					ERR_R_MALLOC_FAILURE);
				goto ecerr;
			}
			group = EC_GROUP_new_by_curve_name(OBJ_obj2nid(a->parameter->value.object));
			if (group == NULL)
				goto ecerr;
			EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);
			if (EC_KEY_set_group(eckey, group) == 0)
				goto ecerr;
			EC_GROUP_free(group);
		}

		/* We have parameters now set private key */
		if (!d2i_ECPrivateKey(&eckey, &p_tmp, pkeylen))
		{
			EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_DECODE_ERROR);
			goto ecerr;
		}

		/* calculate public key (if necessary) */
		if (EC_KEY_get0_public_key(eckey) == NULL)
		{
			const BIGNUM *priv_key;
			const EC_GROUP *group;
			EC_POINT *pub_key;
			/* the public key was not included in the SEC1 private
			 * key => calculate the public key */
			group   = EC_KEY_get0_group(eckey);
			pub_key = EC_POINT_new(group);
			if (pub_key == NULL)
			{
				EVPerr(EVP_F_EVP_PKCS82PKEY, ERR_R_EC_LIB);
				goto ecerr;
			}
			if (!EC_POINT_copy(pub_key, EC_GROUP_get0_generator(group)))
			{
				EC_POINT_free(pub_key);
				EVPerr(EVP_F_EVP_PKCS82PKEY, ERR_R_EC_LIB);
				goto ecerr;
			}
			priv_key = EC_KEY_get0_private_key(eckey);
			if (!EC_POINT_mul(group, pub_key, priv_key, NULL, NULL, ctx))
			{
				EC_POINT_free(pub_key);
				EVPerr(EVP_F_EVP_PKCS82PKEY, ERR_R_EC_LIB);
				goto ecerr;
			}
			if (EC_KEY_set_public_key(eckey, pub_key) == 0)
			{
				EC_POINT_free(pub_key);
				EVPerr(EVP_F_EVP_PKCS82PKEY, ERR_R_EC_LIB);
				goto ecerr;
			}
			EC_POINT_free(pub_key);
		}

		EVP_PKEY_assign_EC_KEY(pkey, eckey);
		if (ctx)
			BN_CTX_free(ctx);
		break;
ecerr:
		if (ctx)
			BN_CTX_free(ctx);
		if (eckey)
			EC_KEY_free(eckey);
		if (pkey)
			EVP_PKEY_free(pkey);
		return NULL;
#endif
		default:
		EVPerr(EVP_F_EVP_PKCS82PKEY, EVP_R_UNSUPPORTED_PRIVATE_KEY_ALGORITHM);
		if (!a->algorithm) BUF_strlcpy (obj_tmp, "NULL", sizeof obj_tmp);
		else i2t_ASN1_OBJECT(obj_tmp, 80, a->algorithm);
		ERR_add_error_data(2, "TYPE=", obj_tmp);
		EVP_PKEY_free (pkey);
		return NULL;
	}
	return pkey;
}

PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8(EVP_PKEY *pkey)
{
	return EVP_PKEY2PKCS8_broken(pkey, PKCS8_OK);
}

/* Turn a private key into a PKCS8 structure */

PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8_broken(EVP_PKEY *pkey, int broken)
{
	PKCS8_PRIV_KEY_INFO *p8;

	if (!(p8 = PKCS8_PRIV_KEY_INFO_new())) {	
		EVPerr(EVP_F_EVP_PKEY2PKCS8_BROKEN,ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	p8->broken = broken;
	if (!ASN1_INTEGER_set(p8->version, 0)) {
		EVPerr(EVP_F_EVP_PKEY2PKCS8_BROKEN,ERR_R_MALLOC_FAILURE);
		PKCS8_PRIV_KEY_INFO_free (p8);
		return NULL;
	}
	if (!(p8->pkeyalg->parameter = ASN1_TYPE_new ())) {
		EVPerr(EVP_F_EVP_PKEY2PKCS8_BROKEN,ERR_R_MALLOC_FAILURE);
		PKCS8_PRIV_KEY_INFO_free (p8);
		return NULL;
	}
	p8->pkey->type = V_ASN1_OCTET_STRING;
	switch (EVP_PKEY_type(pkey->type)) {
#ifndef OPENSSL_NO_RSA
		case EVP_PKEY_RSA:

		if(p8->broken == PKCS8_NO_OCTET) p8->pkey->type = V_ASN1_SEQUENCE;

		p8->pkeyalg->algorithm = OBJ_nid2obj(NID_rsaEncryption);
		p8->pkeyalg->parameter->type = V_ASN1_NULL;
		if (!ASN1_pack_string_of (EVP_PKEY,pkey, i2d_PrivateKey,
					 &p8->pkey->value.octet_string)) {
			EVPerr(EVP_F_EVP_PKEY2PKCS8_BROKEN,ERR_R_MALLOC_FAILURE);
			PKCS8_PRIV_KEY_INFO_free (p8);
			return NULL;
		}
		break;
#endif
#ifndef OPENSSL_NO_DSA
		case EVP_PKEY_DSA:
		if(!dsa_pkey2pkcs8(p8, pkey)) {
			PKCS8_PRIV_KEY_INFO_free (p8);
			return NULL;
		}

		break;
#endif
#ifndef OPENSSL_NO_EC
		case EVP_PKEY_EC:
		if (!eckey_pkey2pkcs8(p8, pkey))
		{
			PKCS8_PRIV_KEY_INFO_free(p8);
			return(NULL);
		}
		break;
#endif
		default:
		EVPerr(EVP_F_EVP_PKEY2PKCS8_BROKEN, EVP_R_UNSUPPORTED_PRIVATE_KEY_ALGORITHM);
		PKCS8_PRIV_KEY_INFO_free (p8);
		return NULL;
	}
	RAND_add(p8->pkey->value.octet_string->data,
		 p8->pkey->value.octet_string->length, 0.0);
	return p8;
}

PKCS8_PRIV_KEY_INFO *PKCS8_set_broken(PKCS8_PRIV_KEY_INFO *p8, int broken)
{
	switch (broken) {

		case PKCS8_OK:
		p8->broken = PKCS8_OK;
		return p8;
		break;

		case PKCS8_NO_OCTET:
		p8->broken = PKCS8_NO_OCTET;
		p8->pkey->type = V_ASN1_SEQUENCE;
		return p8;
		break;

		default:
		EVPerr(EVP_F_PKCS8_SET_BROKEN,EVP_R_PKCS8_UNKNOWN_BROKEN_TYPE);
		return NULL;
	}
}

#ifndef OPENSSL_NO_DSA
static int dsa_pkey2pkcs8(PKCS8_PRIV_KEY_INFO *p8, EVP_PKEY *pkey)
{
	ASN1_STRING *params = NULL;
	ASN1_INTEGER *prkey = NULL;
	ASN1_TYPE *ttmp = NULL;
	STACK_OF(ASN1_TYPE) *ndsa = NULL;
	unsigned char *p = NULL, *q;
	int len;

	p8->pkeyalg->algorithm = OBJ_nid2obj(NID_dsa);
	len = i2d_DSAparams (pkey->pkey.dsa, NULL);
	if (!(p = OPENSSL_malloc(len))) {
		EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	q = p;
	i2d_DSAparams (pkey->pkey.dsa, &q);
	if (!(params = ASN1_STRING_new())) {
		EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!ASN1_STRING_set(params, p, len)) {
		EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	OPENSSL_free(p);
	p = NULL;
	/* Get private key into integer */
	if (!(prkey = BN_to_ASN1_INTEGER (pkey->pkey.dsa->priv_key, NULL))) {
		EVPerr(EVP_F_DSA_PKEY2PKCS8,EVP_R_ENCODE_ERROR);
		goto err;
	}

	switch(p8->broken) {

		case PKCS8_OK:
		case PKCS8_NO_OCTET:

		if (!ASN1_pack_string_of(ASN1_INTEGER,prkey, i2d_ASN1_INTEGER,
					 &p8->pkey->value.octet_string)) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}

		M_ASN1_INTEGER_free (prkey);
		prkey = NULL;
		p8->pkeyalg->parameter->value.sequence = params;
		params = NULL;
		p8->pkeyalg->parameter->type = V_ASN1_SEQUENCE;

		break;

		case PKCS8_NS_DB:

		p8->pkeyalg->parameter->value.sequence = params;
		params = NULL;
		p8->pkeyalg->parameter->type = V_ASN1_SEQUENCE;
		if (!(ndsa = sk_ASN1_TYPE_new_null())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		if (!(ttmp = ASN1_TYPE_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		if (!(ttmp->value.integer =
			BN_to_ASN1_INTEGER(pkey->pkey.dsa->pub_key, NULL))) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,EVP_R_ENCODE_ERROR);
			goto err;
		}
		ttmp->type = V_ASN1_INTEGER;
		if (!sk_ASN1_TYPE_push(ndsa, ttmp)) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if (!(ttmp = ASN1_TYPE_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ttmp->value.integer = prkey;
		prkey = NULL;
		ttmp->type = V_ASN1_INTEGER;
		if (!sk_ASN1_TYPE_push(ndsa, ttmp)) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ttmp = NULL;

		if (!(p8->pkey->value.octet_string = ASN1_OCTET_STRING_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if (!ASN1_seq_pack_ASN1_TYPE(ndsa, i2d_ASN1_TYPE,
					 &p8->pkey->value.octet_string->data,
					 &p8->pkey->value.octet_string->length)) {

			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
		break;

		case PKCS8_EMBEDDED_PARAM:

		p8->pkeyalg->parameter->type = V_ASN1_NULL;
		if (!(ndsa = sk_ASN1_TYPE_new_null())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		if (!(ttmp = ASN1_TYPE_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ttmp->value.sequence = params;
		params = NULL;
		ttmp->type = V_ASN1_SEQUENCE;
		if (!sk_ASN1_TYPE_push(ndsa, ttmp)) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if (!(ttmp = ASN1_TYPE_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ttmp->value.integer = prkey;
		prkey = NULL;
		ttmp->type = V_ASN1_INTEGER;
		if (!sk_ASN1_TYPE_push(ndsa, ttmp)) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ttmp = NULL;

		if (!(p8->pkey->value.octet_string = ASN1_OCTET_STRING_new())) {
			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if (!ASN1_seq_pack_ASN1_TYPE(ndsa, i2d_ASN1_TYPE,
					 &p8->pkey->value.octet_string->data,
					 &p8->pkey->value.octet_string->length)) {

			EVPerr(EVP_F_DSA_PKEY2PKCS8,ERR_R_MALLOC_FAILURE);
			goto err;
		}
		sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
		break;
	}
	return 1;
err:
	if (p != NULL) OPENSSL_free(p);
	if (params != NULL) ASN1_STRING_free(params);
	if (prkey != NULL) M_ASN1_INTEGER_free(prkey);
	if (ttmp != NULL) ASN1_TYPE_free(ttmp);
	if (ndsa != NULL) sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
	return 0;
}
#endif

#ifndef OPENSSL_NO_EC
static int eckey_pkey2pkcs8(PKCS8_PRIV_KEY_INFO *p8, EVP_PKEY *pkey)
{
	EC_KEY		*ec_key;
	const EC_GROUP  *group;
	unsigned char	*p, *pp;
	int 		nid, i, ret = 0;
	unsigned int    tmp_flags, old_flags;

	ec_key = pkey->pkey.ec;
	if (ec_key == NULL || (group = EC_KEY_get0_group(ec_key)) == NULL) 
	{
		EVPerr(EVP_F_ECKEY_PKEY2PKCS8, EVP_R_MISSING_PARAMETERS);
		return 0;
	}

	/* set the ec parameters OID */
	if (p8->pkeyalg->algorithm)
		ASN1_OBJECT_free(p8->pkeyalg->algorithm);

	p8->pkeyalg->algorithm = OBJ_nid2obj(NID_X9_62_id_ecPublicKey);

	/* set the ec parameters */

	if (p8->pkeyalg->parameter)
	{
		ASN1_TYPE_free(p8->pkeyalg->parameter);
		p8->pkeyalg->parameter = NULL;
	}

	if ((p8->pkeyalg->parameter = ASN1_TYPE_new()) == NULL)
	{
		EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_MALLOC_FAILURE);
		return 0;
	}
	
	if (EC_GROUP_get_asn1_flag(group)
                     && (nid = EC_GROUP_get_curve_name(group)))
	{
		/* we have a 'named curve' => just set the OID */
		p8->pkeyalg->parameter->type = V_ASN1_OBJECT;
		p8->pkeyalg->parameter->value.object = OBJ_nid2obj(nid);
	}
	else	/* explicit parameters */
	{
		if ((i = i2d_ECParameters(ec_key, NULL)) == 0)
		{
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_EC_LIB);
			return 0;
		}
		if ((p = (unsigned char *) OPENSSL_malloc(i)) == NULL)
		{
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_MALLOC_FAILURE);
			return 0;
		}	
		pp = p;
		if (!i2d_ECParameters(ec_key, &pp))
		{
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_EC_LIB);
			OPENSSL_free(p);
			return 0;
		}
		p8->pkeyalg->parameter->type = V_ASN1_SEQUENCE;
		if ((p8->pkeyalg->parameter->value.sequence 
			= ASN1_STRING_new()) == NULL)
		{
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_ASN1_LIB);
			OPENSSL_free(p);
			return 0;
		}
		ASN1_STRING_set(p8->pkeyalg->parameter->value.sequence, p, i);
		OPENSSL_free(p);
	}

	/* set the private key */

	/* do not include the parameters in the SEC1 private key
	 * see PKCS#11 12.11 */
	old_flags = EC_KEY_get_enc_flags(pkey->pkey.ec);
	tmp_flags = old_flags | EC_PKEY_NO_PARAMETERS;
	EC_KEY_set_enc_flags(pkey->pkey.ec, tmp_flags);
	i = i2d_ECPrivateKey(pkey->pkey.ec, NULL);
	if (!i)
	{
		EC_KEY_set_enc_flags(pkey->pkey.ec, old_flags);
		EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_EC_LIB);
		return 0;
	}
	p = (unsigned char *) OPENSSL_malloc(i);
	if (!p)
	{
		EC_KEY_set_enc_flags(pkey->pkey.ec, old_flags);
		EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_MALLOC_FAILURE);
		return 0;
	}
	pp = p;
	if (!i2d_ECPrivateKey(pkey->pkey.ec, &pp))
	{
		EC_KEY_set_enc_flags(pkey->pkey.ec, old_flags);
		EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_EC_LIB);
		OPENSSL_free(p);
		return 0;
	}
	/* restore old encoding flags */
	EC_KEY_set_enc_flags(pkey->pkey.ec, old_flags);

	switch(p8->broken) {

		case PKCS8_OK:
		p8->pkey->value.octet_string = ASN1_OCTET_STRING_new();
		if (!p8->pkey->value.octet_string ||
		    !M_ASN1_OCTET_STRING_set(p8->pkey->value.octet_string,
		    (const void *)p, i))

		{
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8, ERR_R_MALLOC_FAILURE);
		}
		else
			ret = 1;
		break;
		case PKCS8_NO_OCTET:		/* RSA specific */
		case PKCS8_NS_DB:		/* DSA specific */
		case PKCS8_EMBEDDED_PARAM:	/* DSA specific */
		default:
			EVPerr(EVP_F_ECKEY_PKEY2PKCS8,EVP_R_ENCODE_ERROR);
	}
	OPENSSL_cleanse(p, (size_t)i);
	OPENSSL_free(p);
	return ret;
}
#endif

/* EVP_PKEY attribute functions */

int EVP_PKEY_get_attr_count(const EVP_PKEY *key)
{
	return X509at_get_attr_count(key->attributes);
}

int EVP_PKEY_get_attr_by_NID(const EVP_PKEY *key, int nid,
			  int lastpos)
{
	return X509at_get_attr_by_NID(key->attributes, nid, lastpos);
}

int EVP_PKEY_get_attr_by_OBJ(const EVP_PKEY *key, ASN1_OBJECT *obj,
			  int lastpos)
{
	return X509at_get_attr_by_OBJ(key->attributes, obj, lastpos);
}

X509_ATTRIBUTE *EVP_PKEY_get_attr(const EVP_PKEY *key, int loc)
{
	return X509at_get_attr(key->attributes, loc);
}

X509_ATTRIBUTE *EVP_PKEY_delete_attr(EVP_PKEY *key, int loc)
{
	return X509at_delete_attr(key->attributes, loc);
}

int EVP_PKEY_add1_attr(EVP_PKEY *key, X509_ATTRIBUTE *attr)
{
	if(X509at_add1_attr(&key->attributes, attr)) return 1;
	return 0;
}

int EVP_PKEY_add1_attr_by_OBJ(EVP_PKEY *key,
			const ASN1_OBJECT *obj, int type,
			const unsigned char *bytes, int len)
{
	if(X509at_add1_attr_by_OBJ(&key->attributes, obj,
				type, bytes, len)) return 1;
	return 0;
}

int EVP_PKEY_add1_attr_by_NID(EVP_PKEY *key,
			int nid, int type,
			const unsigned char *bytes, int len)
{
	if(X509at_add1_attr_by_NID(&key->attributes, nid,
				type, bytes, len)) return 1;
	return 0;
}

int EVP_PKEY_add1_attr_by_txt(EVP_PKEY *key,
			const char *attrname, int type,
			const unsigned char *bytes, int len)
{
	if(X509at_add1_attr_by_txt(&key->attributes, attrname,
				type, bytes, len)) return 1;
	return 0;
}
