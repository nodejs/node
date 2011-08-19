/* crypto/asn1/a_sign.c */
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
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
#include <time.h>

#include "cryptlib.h"

#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/objects.h>
#include <openssl/buffer.h>

#ifndef NO_ASN1_OLD

int ASN1_sign(i2d_of_void *i2d, X509_ALGOR *algor1, X509_ALGOR *algor2,
	      ASN1_BIT_STRING *signature, char *data, EVP_PKEY *pkey,
	      const EVP_MD *type)
	{
	EVP_MD_CTX ctx;
	unsigned char *p,*buf_in=NULL,*buf_out=NULL;
	int i,inl=0,outl=0,outll=0;
	X509_ALGOR *a;

	EVP_MD_CTX_init(&ctx);
	for (i=0; i<2; i++)
		{
		if (i == 0)
			a=algor1;
		else
			a=algor2;
		if (a == NULL) continue;
                if (type->pkey_type == NID_dsaWithSHA1)
			{
			/* special case: RFC 2459 tells us to omit 'parameters'
			 * with id-dsa-with-sha1 */
			ASN1_TYPE_free(a->parameter);
			a->parameter = NULL;
			}
		else if ((a->parameter == NULL) || 
			(a->parameter->type != V_ASN1_NULL))
			{
			ASN1_TYPE_free(a->parameter);
			if ((a->parameter=ASN1_TYPE_new()) == NULL) goto err;
			a->parameter->type=V_ASN1_NULL;
			}
		ASN1_OBJECT_free(a->algorithm);
		a->algorithm=OBJ_nid2obj(type->pkey_type);
		if (a->algorithm == NULL)
			{
			ASN1err(ASN1_F_ASN1_SIGN,ASN1_R_UNKNOWN_OBJECT_TYPE);
			goto err;
			}
		if (a->algorithm->length == 0)
			{
			ASN1err(ASN1_F_ASN1_SIGN,ASN1_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD);
			goto err;
			}
		}
	inl=i2d(data,NULL);
	buf_in=(unsigned char *)OPENSSL_malloc((unsigned int)inl);
	outll=outl=EVP_PKEY_size(pkey);
	buf_out=(unsigned char *)OPENSSL_malloc((unsigned int)outl);
	if ((buf_in == NULL) || (buf_out == NULL))
		{
		outl=0;
		ASN1err(ASN1_F_ASN1_SIGN,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	p=buf_in;

	i2d(data,&p);
	EVP_SignInit_ex(&ctx,type, NULL);
	EVP_SignUpdate(&ctx,(unsigned char *)buf_in,inl);
	if (!EVP_SignFinal(&ctx,(unsigned char *)buf_out,
			(unsigned int *)&outl,pkey))
		{
		outl=0;
		ASN1err(ASN1_F_ASN1_SIGN,ERR_R_EVP_LIB);
		goto err;
		}
	if (signature->data != NULL) OPENSSL_free(signature->data);
	signature->data=buf_out;
	buf_out=NULL;
	signature->length=outl;
	/* In the interests of compatibility, I'll make sure that
	 * the bit string has a 'not-used bits' value of 0
	 */
	signature->flags&= ~(ASN1_STRING_FLAG_BITS_LEFT|0x07);
	signature->flags|=ASN1_STRING_FLAG_BITS_LEFT;
err:
	EVP_MD_CTX_cleanup(&ctx);
	if (buf_in != NULL)
		{ OPENSSL_cleanse((char *)buf_in,(unsigned int)inl); OPENSSL_free(buf_in); }
	if (buf_out != NULL)
		{ OPENSSL_cleanse((char *)buf_out,outll); OPENSSL_free(buf_out); }
	return(outl);
	}

#endif

int ASN1_item_sign(const ASN1_ITEM *it, X509_ALGOR *algor1, X509_ALGOR *algor2,
	     ASN1_BIT_STRING *signature, void *asn, EVP_PKEY *pkey,
	     const EVP_MD *type)
	{
	EVP_MD_CTX ctx;
	unsigned char *buf_in=NULL,*buf_out=NULL;
	int i,inl=0,outl=0,outll=0;
	X509_ALGOR *a;

	EVP_MD_CTX_init(&ctx);
	for (i=0; i<2; i++)
		{
		if (i == 0)
			a=algor1;
		else
			a=algor2;
		if (a == NULL) continue;
                if (type->pkey_type == NID_dsaWithSHA1 ||
			type->pkey_type == NID_ecdsa_with_SHA1)
			{
			/* special case: RFC 3279 tells us to omit 'parameters'
			 * with id-dsa-with-sha1 and ecdsa-with-SHA1 */
			ASN1_TYPE_free(a->parameter);
			a->parameter = NULL;
			}
		else if ((a->parameter == NULL) || 
			(a->parameter->type != V_ASN1_NULL))
			{
			ASN1_TYPE_free(a->parameter);
			if ((a->parameter=ASN1_TYPE_new()) == NULL) goto err;
			a->parameter->type=V_ASN1_NULL;
			}
		ASN1_OBJECT_free(a->algorithm);
		a->algorithm=OBJ_nid2obj(type->pkey_type);
		if (a->algorithm == NULL)
			{
			ASN1err(ASN1_F_ASN1_ITEM_SIGN,ASN1_R_UNKNOWN_OBJECT_TYPE);
			goto err;
			}
		if (a->algorithm->length == 0)
			{
			ASN1err(ASN1_F_ASN1_ITEM_SIGN,ASN1_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD);
			goto err;
			}
		}
	inl=ASN1_item_i2d(asn,&buf_in, it);
	outll=outl=EVP_PKEY_size(pkey);
	buf_out=(unsigned char *)OPENSSL_malloc((unsigned int)outl);
	if ((buf_in == NULL) || (buf_out == NULL))
		{
		outl=0;
		ASN1err(ASN1_F_ASN1_ITEM_SIGN,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (!EVP_SignInit_ex(&ctx,type, NULL))
		{
		outl=0;
		ASN1err(ASN1_F_ASN1_ITEM_SIGN,ERR_R_EVP_LIB);
		goto err;
		}
	EVP_SignUpdate(&ctx,(unsigned char *)buf_in,inl);
	if (!EVP_SignFinal(&ctx,(unsigned char *)buf_out,
			(unsigned int *)&outl,pkey))
		{
		outl=0;
		ASN1err(ASN1_F_ASN1_ITEM_SIGN,ERR_R_EVP_LIB);
		goto err;
		}
	if (signature->data != NULL) OPENSSL_free(signature->data);
	signature->data=buf_out;
	buf_out=NULL;
	signature->length=outl;
	/* In the interests of compatibility, I'll make sure that
	 * the bit string has a 'not-used bits' value of 0
	 */
	signature->flags&= ~(ASN1_STRING_FLAG_BITS_LEFT|0x07);
	signature->flags|=ASN1_STRING_FLAG_BITS_LEFT;
err:
	EVP_MD_CTX_cleanup(&ctx);
	if (buf_in != NULL)
		{ OPENSSL_cleanse((char *)buf_in,(unsigned int)inl); OPENSSL_free(buf_in); }
	if (buf_out != NULL)
		{ OPENSSL_cleanse((char *)buf_out,outll); OPENSSL_free(buf_out); }
	return(outl);
	}
