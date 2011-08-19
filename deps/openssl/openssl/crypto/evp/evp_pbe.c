/* evp_pbe.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
#include <openssl/x509.h>

/* Password based encryption (PBE) functions */

static STACK *pbe_algs;

/* Setup a cipher context from a PBE algorithm */

typedef struct {
int pbe_nid;
const EVP_CIPHER *cipher;
const EVP_MD *md;
EVP_PBE_KEYGEN *keygen;
} EVP_PBE_CTL;

int EVP_PBE_CipherInit(ASN1_OBJECT *pbe_obj, const char *pass, int passlen,
	     ASN1_TYPE *param, EVP_CIPHER_CTX *ctx, int en_de)
{

	EVP_PBE_CTL *pbetmp, pbelu;
	int i;
	pbelu.pbe_nid = OBJ_obj2nid(pbe_obj);
	if (pbelu.pbe_nid != NID_undef) i = sk_find(pbe_algs, (char *)&pbelu);
	else i = -1;

	if (i == -1) {
		char obj_tmp[80];
		EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_UNKNOWN_PBE_ALGORITHM);
		if (!pbe_obj) BUF_strlcpy (obj_tmp, "NULL", sizeof obj_tmp);
		else i2t_ASN1_OBJECT(obj_tmp, sizeof obj_tmp, pbe_obj);
		ERR_add_error_data(2, "TYPE=", obj_tmp);
		return 0;
	}
	if(!pass) passlen = 0;
	else if (passlen == -1) passlen = strlen(pass);
	pbetmp = (EVP_PBE_CTL *)sk_value (pbe_algs, i);
	i = (*pbetmp->keygen)(ctx, pass, passlen, param, pbetmp->cipher,
						 pbetmp->md, en_de);
	if (!i) {
		EVPerr(EVP_F_EVP_PBE_CIPHERINIT,EVP_R_KEYGEN_FAILURE);
		return 0;
	}
	return 1;	
}

static int pbe_cmp(const char * const *a, const char * const *b)
{
	const EVP_PBE_CTL * const *pbe1 = (const EVP_PBE_CTL * const *) a,
			* const *pbe2 = (const EVP_PBE_CTL * const *)b;
	return ((*pbe1)->pbe_nid - (*pbe2)->pbe_nid);
}

/* Add a PBE algorithm */

int EVP_PBE_alg_add(int nid, const EVP_CIPHER *cipher, const EVP_MD *md,
	     EVP_PBE_KEYGEN *keygen)
{
	EVP_PBE_CTL *pbe_tmp;
	if (!pbe_algs) pbe_algs = sk_new(pbe_cmp);
	if (!(pbe_tmp = (EVP_PBE_CTL*) OPENSSL_malloc (sizeof(EVP_PBE_CTL)))) {
		EVPerr(EVP_F_EVP_PBE_ALG_ADD,ERR_R_MALLOC_FAILURE);
		return 0;
	}
	pbe_tmp->pbe_nid = nid;
	pbe_tmp->cipher = cipher;
	pbe_tmp->md = md;
	pbe_tmp->keygen = keygen;
	sk_push (pbe_algs, (char *)pbe_tmp);
	return 1;
}

void EVP_PBE_cleanup(void)
{
	sk_pop_free(pbe_algs, OPENSSL_freeFunc);
	pbe_algs = NULL;
}
