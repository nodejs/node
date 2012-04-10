/* evp_pbe.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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
#include "cryptlib.h"
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

/* Password based encryption (PBE) functions */

DECLARE_STACK_OF(EVP_PBE_CTL)
static STACK_OF(EVP_PBE_CTL) *pbe_algs;

/* Setup a cipher context from a PBE algorithm */

typedef struct
	{
	int pbe_type;
	int pbe_nid;
	int cipher_nid;
	int md_nid;
	EVP_PBE_KEYGEN *keygen;
	} EVP_PBE_CTL;

static const EVP_PBE_CTL builtin_pbe[] = 
	{
	{EVP_PBE_TYPE_OUTER, NID_pbeWithMD2AndDES_CBC,
			NID_des_cbc, NID_md2, PKCS5_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbeWithMD5AndDES_CBC,
			NID_des_cbc, NID_md5, PKCS5_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbeWithSHA1AndRC2_CBC,
			NID_rc2_64_cbc, NID_sha1, PKCS5_PBE_keyivgen},

	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And128BitRC4,
			NID_rc4, NID_sha1, PKCS12_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And40BitRC4,
			NID_rc4_40, NID_sha1, PKCS12_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
		 	NID_des_ede3_cbc, NID_sha1, PKCS12_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And2_Key_TripleDES_CBC, 
			NID_des_ede_cbc, NID_sha1, PKCS12_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And128BitRC2_CBC,
			NID_rc2_cbc, NID_sha1, PKCS12_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbe_WithSHA1And40BitRC2_CBC,
			NID_rc2_40_cbc, NID_sha1, PKCS12_PBE_keyivgen},

#ifndef OPENSSL_NO_HMAC
	{EVP_PBE_TYPE_OUTER, NID_pbes2, -1, -1, PKCS5_v2_PBE_keyivgen},
#endif
	{EVP_PBE_TYPE_OUTER, NID_pbeWithMD2AndRC2_CBC,
			NID_rc2_64_cbc, NID_md2, PKCS5_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbeWithMD5AndRC2_CBC,
			NID_rc2_64_cbc, NID_md5, PKCS5_PBE_keyivgen},
	{EVP_PBE_TYPE_OUTER, NID_pbeWithSHA1AndDES_CBC,
			NID_des_cbc, NID_sha1, PKCS5_PBE_keyivgen},


	{EVP_PBE_TYPE_PRF, NID_hmacWithSHA1, -1, NID_sha1, 0},
	{EVP_PBE_TYPE_PRF, NID_hmacWithMD5, -1, NID_md5, 0},
	{EVP_PBE_TYPE_PRF, NID_hmacWithSHA224, -1, NID_sha224, 0},
	{EVP_PBE_TYPE_PRF, NID_hmacWithSHA256, -1, NID_sha256, 0},
	{EVP_PBE_TYPE_PRF, NID_hmacWithSHA384, -1, NID_sha384, 0},
	{EVP_PBE_TYPE_PRF, NID_hmacWithSHA512, -1, NID_sha512, 0},
	{EVP_PBE_TYPE_PRF, NID_id_HMACGostR3411_94, -1, NID_id_GostR3411_94, 0},
	};

#ifdef TEST
int main(int argc, char **argv)
	{
	int i, nid_md, nid_cipher;
	EVP_PBE_CTL *tpbe, *tpbe2;
	/*OpenSSL_add_all_algorithms();*/

	for (i = 0; i < sizeof(builtin_pbe)/sizeof(EVP_PBE_CTL); i++)
		{
		tpbe = builtin_pbe + i;
		fprintf(stderr, "%d %d %s ", tpbe->pbe_type, tpbe->pbe_nid,
						OBJ_nid2sn(tpbe->pbe_nid));
		if (EVP_PBE_find(tpbe->pbe_type, tpbe->pbe_nid,
					&nid_cipher ,&nid_md,0))
			fprintf(stderr, "Found %s %s\n",
					OBJ_nid2sn(nid_cipher),
					OBJ_nid2sn(nid_md));
		else
			fprintf(stderr, "Find ERROR!!\n");
		}

	return 0;
	}
#endif
		


int EVP_PBE_CipherInit(ASN1_OBJECT *pbe_obj, const char *pass, int passlen,
		       ASN1_TYPE *param, EVP_CIPHER_CTX *ctx, int en_de)
	{
	const EVP_CIPHER *cipher;
	const EVP_MD *md;
	int cipher_nid, md_nid;
	EVP_PBE_KEYGEN *keygen;

	if (!EVP_PBE_find(EVP_PBE_TYPE_OUTER, OBJ_obj2nid(pbe_obj),
					&cipher_nid, &md_nid, &keygen))
		{
		char obj_tmp[80];
		EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_UNKNOWN_PBE_ALGORITHM);
		if (!pbe_obj) BUF_strlcpy (obj_tmp, "NULL", sizeof obj_tmp);
		else i2t_ASN1_OBJECT(obj_tmp, sizeof obj_tmp, pbe_obj);
		ERR_add_error_data(2, "TYPE=", obj_tmp);
		return 0;
		}

	if(!pass)
		passlen = 0;
	else if (passlen == -1)
		passlen = strlen(pass);

	if (cipher_nid == -1)
		cipher = NULL;
	else
		{
		cipher = EVP_get_cipherbynid(cipher_nid);
		if (!cipher)
			{
			EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_UNKNOWN_CIPHER);
			return 0;
			}
		}

	if (md_nid == -1)
		md = NULL;
	else
		{
		md = EVP_get_digestbynid(md_nid);
		if (!md)
			{
			EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_UNKNOWN_DIGEST);
			return 0;
			}
		}

	if (!keygen(ctx, pass, passlen, param, cipher, md, en_de))
		{
		EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_KEYGEN_FAILURE);
		return 0;
		}
	return 1;	
}

DECLARE_OBJ_BSEARCH_CMP_FN(EVP_PBE_CTL, EVP_PBE_CTL, pbe2);

static int pbe2_cmp(const EVP_PBE_CTL *pbe1, const EVP_PBE_CTL *pbe2)
	{
	int ret = pbe1->pbe_type - pbe2->pbe_type;
	if (ret)
		return ret;
	else
		return pbe1->pbe_nid - pbe2->pbe_nid;
	}

IMPLEMENT_OBJ_BSEARCH_CMP_FN(EVP_PBE_CTL, EVP_PBE_CTL, pbe2);

static int pbe_cmp(const EVP_PBE_CTL * const *a, const EVP_PBE_CTL * const *b)
	{
	int ret = (*a)->pbe_type - (*b)->pbe_type;
	if (ret)
		return ret;
	else
		return (*a)->pbe_nid - (*b)->pbe_nid;
	}

/* Add a PBE algorithm */

int EVP_PBE_alg_add_type(int pbe_type, int pbe_nid, int cipher_nid, int md_nid,
			 EVP_PBE_KEYGEN *keygen)
	{
	EVP_PBE_CTL *pbe_tmp;
	if (!pbe_algs)
		pbe_algs = sk_EVP_PBE_CTL_new(pbe_cmp);
	if (!(pbe_tmp = (EVP_PBE_CTL*) OPENSSL_malloc (sizeof(EVP_PBE_CTL))))
		{
		EVPerr(EVP_F_EVP_PBE_ALG_ADD_TYPE,ERR_R_MALLOC_FAILURE);
		return 0;
		}
	pbe_tmp->pbe_type = pbe_type;
	pbe_tmp->pbe_nid = pbe_nid;
	pbe_tmp->cipher_nid = cipher_nid;
	pbe_tmp->md_nid = md_nid;
	pbe_tmp->keygen = keygen;


	sk_EVP_PBE_CTL_push (pbe_algs, pbe_tmp);
	return 1;
	}

int EVP_PBE_alg_add(int nid, const EVP_CIPHER *cipher, const EVP_MD *md,
		    EVP_PBE_KEYGEN *keygen)
	{
	int cipher_nid, md_nid;
	if (cipher)
		cipher_nid = EVP_CIPHER_type(cipher);
	else
		cipher_nid = -1;
	if (md)
		md_nid = EVP_MD_type(md);
	else
		md_nid = -1;

	return EVP_PBE_alg_add_type(EVP_PBE_TYPE_OUTER, nid,
					cipher_nid, md_nid, keygen);
	}

int EVP_PBE_find(int type, int pbe_nid,
		 int *pcnid, int *pmnid, EVP_PBE_KEYGEN **pkeygen)
	{
	EVP_PBE_CTL *pbetmp = NULL, pbelu;
	int i;
	if (pbe_nid == NID_undef)
		return 0;

	pbelu.pbe_type = type;
	pbelu.pbe_nid = pbe_nid;

	if (pbe_algs)
		{
		i = sk_EVP_PBE_CTL_find(pbe_algs, &pbelu);
		if (i != -1)
			pbetmp = sk_EVP_PBE_CTL_value (pbe_algs, i);
		}
	if (pbetmp == NULL)
		{
		pbetmp = OBJ_bsearch_pbe2(&pbelu, builtin_pbe,
				     sizeof(builtin_pbe)/sizeof(EVP_PBE_CTL));
		}
	if (pbetmp == NULL)
		return 0;
	if (pcnid)
		*pcnid = pbetmp->cipher_nid;
	if (pmnid)
		*pmnid = pbetmp->md_nid;
	if (pkeygen)
		*pkeygen = pbetmp->keygen;
	return 1;
	}

static void free_evp_pbe_ctl(EVP_PBE_CTL *pbe)
	 {
	 OPENSSL_freeFunc(pbe);
	 }

void EVP_PBE_cleanup(void)
	{
	sk_EVP_PBE_CTL_pop_free(pbe_algs, free_evp_pbe_ctl);
	pbe_algs = NULL;
	}
