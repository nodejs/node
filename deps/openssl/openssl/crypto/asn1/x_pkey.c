/* crypto/asn1/x_pkey.c */
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
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/asn1_mac.h>
#include <openssl/x509.h>

/* need to implement */
int i2d_X509_PKEY(X509_PKEY *a, unsigned char **pp)
	{
	return(0);
	}

X509_PKEY *d2i_X509_PKEY(X509_PKEY **a, const unsigned char **pp, long length)
	{
	int i;
	M_ASN1_D2I_vars(a,X509_PKEY *,X509_PKEY_new);

	M_ASN1_D2I_Init();
	M_ASN1_D2I_start_sequence();
	M_ASN1_D2I_get_x(X509_ALGOR,ret->enc_algor,d2i_X509_ALGOR);
	M_ASN1_D2I_get_x(ASN1_OCTET_STRING,ret->enc_pkey,d2i_ASN1_OCTET_STRING);

	ret->cipher.cipher=EVP_get_cipherbyname(
		OBJ_nid2ln(OBJ_obj2nid(ret->enc_algor->algorithm)));
	if (ret->cipher.cipher == NULL)
		{
		c.error=ASN1_R_UNSUPPORTED_CIPHER;
		c.line=__LINE__;
		goto err;
		}
	if (ret->enc_algor->parameter->type == V_ASN1_OCTET_STRING) 
		{
		i=ret->enc_algor->parameter->value.octet_string->length;
		if (i > EVP_MAX_IV_LENGTH)
			{
			c.error=ASN1_R_IV_TOO_LARGE;
			c.line=__LINE__;
			goto err;
			}
		memcpy(ret->cipher.iv,
			ret->enc_algor->parameter->value.octet_string->data,i);
		}
	else
		memset(ret->cipher.iv,0,EVP_MAX_IV_LENGTH);
	M_ASN1_D2I_Finish(a,X509_PKEY_free,ASN1_F_D2I_X509_PKEY);
	}

X509_PKEY *X509_PKEY_new(void)
	{
	X509_PKEY *ret=NULL;
	ASN1_CTX c;

	M_ASN1_New_Malloc(ret,X509_PKEY);
	ret->version=0;
	M_ASN1_New(ret->enc_algor,X509_ALGOR_new);
	M_ASN1_New(ret->enc_pkey,M_ASN1_OCTET_STRING_new);
	ret->dec_pkey=NULL;
	ret->key_length=0;
	ret->key_data=NULL;
	ret->key_free=0;
	ret->cipher.cipher=NULL;
	memset(ret->cipher.iv,0,EVP_MAX_IV_LENGTH);
	ret->references=1;
	return(ret);
	M_ASN1_New_Error(ASN1_F_X509_PKEY_NEW);
	}

void X509_PKEY_free(X509_PKEY *x)
	{
	int i;

	if (x == NULL) return;

	i=CRYPTO_add(&x->references,-1,CRYPTO_LOCK_X509_PKEY);
#ifdef REF_PRINT
	REF_PRINT("X509_PKEY",x);
#endif
	if (i > 0) return;
#ifdef REF_CHECK
	if (i < 0)
		{
		fprintf(stderr,"X509_PKEY_free, bad reference count\n");
		abort();
		}
#endif

	if (x->enc_algor != NULL) X509_ALGOR_free(x->enc_algor);
	if (x->enc_pkey != NULL) M_ASN1_OCTET_STRING_free(x->enc_pkey);
	if (x->dec_pkey != NULL)EVP_PKEY_free(x->dec_pkey);
	if ((x->key_data != NULL) && (x->key_free)) OPENSSL_free(x->key_data);
	OPENSSL_free(x);
	}
