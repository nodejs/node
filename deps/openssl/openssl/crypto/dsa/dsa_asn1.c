/* dsa_asn1.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
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
#include <openssl/dsa.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/rand.h>

/* Override the default new methods */
static int sig_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
								void *exarg)
{
	if(operation == ASN1_OP_NEW_PRE) {
		DSA_SIG *sig;
		sig = OPENSSL_malloc(sizeof(DSA_SIG));
		if (!sig)
			{
			DSAerr(DSA_F_SIG_CB, ERR_R_MALLOC_FAILURE);
			return 0;
			}
		sig->r = NULL;
		sig->s = NULL;
		*pval = (ASN1_VALUE *)sig;
		return 2;
	}
	return 1;
}

ASN1_SEQUENCE_cb(DSA_SIG, sig_cb) = {
	ASN1_SIMPLE(DSA_SIG, r, CBIGNUM),
	ASN1_SIMPLE(DSA_SIG, s, CBIGNUM)
} ASN1_SEQUENCE_END_cb(DSA_SIG, DSA_SIG)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA_SIG, DSA_SIG, DSA_SIG)

/* Override the default free and new methods */
static int dsa_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
							void *exarg)
{
	if(operation == ASN1_OP_NEW_PRE) {
		*pval = (ASN1_VALUE *)DSA_new();
		if(*pval) return 2;
		return 0;
	} else if(operation == ASN1_OP_FREE_PRE) {
		DSA_free((DSA *)*pval);
		*pval = NULL;
		return 2;
	}
	return 1;
}

ASN1_SEQUENCE_cb(DSAPrivateKey, dsa_cb) = {
	ASN1_SIMPLE(DSA, version, LONG),
	ASN1_SIMPLE(DSA, p, BIGNUM),
	ASN1_SIMPLE(DSA, q, BIGNUM),
	ASN1_SIMPLE(DSA, g, BIGNUM),
	ASN1_SIMPLE(DSA, pub_key, BIGNUM),
	ASN1_SIMPLE(DSA, priv_key, BIGNUM)
} ASN1_SEQUENCE_END_cb(DSA, DSAPrivateKey)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAPrivateKey, DSAPrivateKey)

ASN1_SEQUENCE_cb(DSAparams, dsa_cb) = {
	ASN1_SIMPLE(DSA, p, BIGNUM),
	ASN1_SIMPLE(DSA, q, BIGNUM),
	ASN1_SIMPLE(DSA, g, BIGNUM),
} ASN1_SEQUENCE_END_cb(DSA, DSAparams)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAparams, DSAparams)

/* DSA public key is a bit trickier... its effectively a CHOICE type
 * decided by a field called write_params which can either write out
 * just the public key as an INTEGER or the parameters and public key
 * in a SEQUENCE
 */

ASN1_SEQUENCE(dsa_pub_internal) = {
	ASN1_SIMPLE(DSA, pub_key, BIGNUM),
	ASN1_SIMPLE(DSA, p, BIGNUM),
	ASN1_SIMPLE(DSA, q, BIGNUM),
	ASN1_SIMPLE(DSA, g, BIGNUM)
} ASN1_SEQUENCE_END_name(DSA, dsa_pub_internal)

ASN1_CHOICE_cb(DSAPublicKey, dsa_cb) = {
	ASN1_SIMPLE(DSA, pub_key, BIGNUM),
	ASN1_EX_COMBINE(0, 0, dsa_pub_internal)
} ASN1_CHOICE_END_cb(DSA, DSAPublicKey, write_params)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAPublicKey, DSAPublicKey)

DSA *DSAparams_dup(DSA *dsa)
	{
	return ASN1_item_dup(ASN1_ITEM_rptr(DSAparams), dsa);
	}

int DSA_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
	     unsigned int *siglen, DSA *dsa)
	{
	DSA_SIG *s;
	RAND_seed(dgst, dlen);
	s=DSA_do_sign(dgst,dlen,dsa);
	if (s == NULL)
		{
		*siglen=0;
		return(0);
		}
	*siglen=i2d_DSA_SIG(s,&sig);
	DSA_SIG_free(s);
	return(1);
	}

/* data has already been hashed (probably with SHA or SHA-1). */
/* returns
 *      1: correct signature
 *      0: incorrect signature
 *     -1: error
 */
int DSA_verify(int type, const unsigned char *dgst, int dgst_len,
	     const unsigned char *sigbuf, int siglen, DSA *dsa)
	{
	DSA_SIG *s;
	int ret=-1;

	s = DSA_SIG_new();
	if (s == NULL) return(ret);
	if (d2i_DSA_SIG(&s,&sigbuf,siglen) == NULL) goto err;
	ret=DSA_do_verify(dgst,dgst_len,s,dsa);
err:
	DSA_SIG_free(s);
	return(ret);
	}
