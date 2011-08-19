/* crypto/asn1/a_hdr.c */
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
#include <openssl/asn1_mac.h>
#include <openssl/asn1.h>

int i2d_ASN1_HEADER(ASN1_HEADER *a, unsigned char **pp)
	{
	M_ASN1_I2D_vars(a);

	M_ASN1_I2D_len(a->header,	i2d_ASN1_OCTET_STRING);
	M_ASN1_I2D_len(a->data,		a->meth->i2d);

	M_ASN1_I2D_seq_total();

	M_ASN1_I2D_put(a->header,	i2d_ASN1_OCTET_STRING);
	M_ASN1_I2D_put(a->data,		a->meth->i2d);

	M_ASN1_I2D_finish();
	}

ASN1_HEADER *d2i_ASN1_HEADER(ASN1_HEADER **a, const unsigned char **pp,
	     long length)
	{
	M_ASN1_D2I_vars(a,ASN1_HEADER *,ASN1_HEADER_new);

	M_ASN1_D2I_Init();
        M_ASN1_D2I_start_sequence();
        M_ASN1_D2I_get_x(ASN1_OCTET_STRING,ret->header,d2i_ASN1_OCTET_STRING);
	if (ret->meth != NULL)
		{
		M_ASN1_D2I_get_x(void,ret->data,ret->meth->d2i);
		}
	else
		{
		if (a != NULL) (*a)=ret;
		return(ret);
		}
        M_ASN1_D2I_Finish(a,ASN1_HEADER_free,ASN1_F_D2I_ASN1_HEADER);
	}

ASN1_HEADER *ASN1_HEADER_new(void)
	{
	ASN1_HEADER *ret=NULL;
	ASN1_CTX c;

	M_ASN1_New_Malloc(ret,ASN1_HEADER);
	M_ASN1_New(ret->header,M_ASN1_OCTET_STRING_new);
	ret->meth=NULL;
	ret->data=NULL;
	return(ret);
        M_ASN1_New_Error(ASN1_F_ASN1_HEADER_NEW);
	}

void ASN1_HEADER_free(ASN1_HEADER *a)
	{
	if (a == NULL) return;
	M_ASN1_OCTET_STRING_free(a->header);
	if (a->meth != NULL)
		a->meth->destroy(a->data);
	OPENSSL_free(a);
	}
