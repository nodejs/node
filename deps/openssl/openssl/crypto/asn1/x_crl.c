/* crypto/asn1/x_crl.c */
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

static int X509_REVOKED_cmp(const X509_REVOKED * const *a,
				const X509_REVOKED * const *b);

ASN1_SEQUENCE(X509_REVOKED) = {
	ASN1_SIMPLE(X509_REVOKED,serialNumber, ASN1_INTEGER),
	ASN1_SIMPLE(X509_REVOKED,revocationDate, ASN1_TIME),
	ASN1_SEQUENCE_OF_OPT(X509_REVOKED,extensions, X509_EXTENSION)
} ASN1_SEQUENCE_END(X509_REVOKED)

/* The X509_CRL_INFO structure needs a bit of customisation.
 * Since we cache the original encoding the signature wont be affected by
 * reordering of the revoked field.
 */
static int crl_inf_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	X509_CRL_INFO *a = (X509_CRL_INFO *)*pval;

	if(!a || !a->revoked) return 1;
	switch(operation) {
		/* Just set cmp function here. We don't sort because that
		 * would affect the output of X509_CRL_print().
		 */
		case ASN1_OP_D2I_POST:
		(void)sk_X509_REVOKED_set_cmp_func(a->revoked,X509_REVOKED_cmp);
		break;
	}
	return 1;
}


ASN1_SEQUENCE_enc(X509_CRL_INFO, enc, crl_inf_cb) = {
	ASN1_OPT(X509_CRL_INFO, version, ASN1_INTEGER),
	ASN1_SIMPLE(X509_CRL_INFO, sig_alg, X509_ALGOR),
	ASN1_SIMPLE(X509_CRL_INFO, issuer, X509_NAME),
	ASN1_SIMPLE(X509_CRL_INFO, lastUpdate, ASN1_TIME),
	ASN1_OPT(X509_CRL_INFO, nextUpdate, ASN1_TIME),
	ASN1_SEQUENCE_OF_OPT(X509_CRL_INFO, revoked, X509_REVOKED),
	ASN1_EXP_SEQUENCE_OF_OPT(X509_CRL_INFO, extensions, X509_EXTENSION, 0)
} ASN1_SEQUENCE_END_enc(X509_CRL_INFO, X509_CRL_INFO)

ASN1_SEQUENCE_ref(X509_CRL, 0, CRYPTO_LOCK_X509_CRL) = {
	ASN1_SIMPLE(X509_CRL, crl, X509_CRL_INFO),
	ASN1_SIMPLE(X509_CRL, sig_alg, X509_ALGOR),
	ASN1_SIMPLE(X509_CRL, signature, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END_ref(X509_CRL, X509_CRL)

IMPLEMENT_ASN1_FUNCTIONS(X509_REVOKED)
IMPLEMENT_ASN1_FUNCTIONS(X509_CRL_INFO)
IMPLEMENT_ASN1_FUNCTIONS(X509_CRL)
IMPLEMENT_ASN1_DUP_FUNCTION(X509_CRL)

static int X509_REVOKED_cmp(const X509_REVOKED * const *a,
			const X509_REVOKED * const *b)
	{
	return(ASN1_STRING_cmp(
		(ASN1_STRING *)(*a)->serialNumber,
		(ASN1_STRING *)(*b)->serialNumber));
	}

int X509_CRL_add0_revoked(X509_CRL *crl, X509_REVOKED *rev)
{
	X509_CRL_INFO *inf;
	inf = crl->crl;
	if(!inf->revoked)
		inf->revoked = sk_X509_REVOKED_new(X509_REVOKED_cmp);
	if(!inf->revoked || !sk_X509_REVOKED_push(inf->revoked, rev)) {
		ASN1err(ASN1_F_X509_CRL_ADD0_REVOKED, ERR_R_MALLOC_FAILURE);
		return 0;
	}
	inf->enc.modified = 1;
	return 1;
}

IMPLEMENT_STACK_OF(X509_REVOKED)
IMPLEMENT_ASN1_SET_OF(X509_REVOKED)
IMPLEMENT_STACK_OF(X509_CRL)
IMPLEMENT_ASN1_SET_OF(X509_CRL)
