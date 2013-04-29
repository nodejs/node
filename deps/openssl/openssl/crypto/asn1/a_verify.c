/* crypto/asn1/a_verify.c */
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
#include <time.h>

#include "cryptlib.h"
#include "asn1_locl.h"

#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <openssl/bn.h>
#include <openssl/x509.h>
#include <openssl/objects.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

#ifndef NO_ASN1_OLD

int ASN1_verify(i2d_of_void *i2d, X509_ALGOR *a, ASN1_BIT_STRING *signature,
		char *data, EVP_PKEY *pkey)
	{
	EVP_MD_CTX ctx;
	const EVP_MD *type;
	unsigned char *p,*buf_in=NULL;
	int ret= -1,i,inl;

	EVP_MD_CTX_init(&ctx);
	i=OBJ_obj2nid(a->algorithm);
	type=EVP_get_digestbyname(OBJ_nid2sn(i));
	if (type == NULL)
		{
		ASN1err(ASN1_F_ASN1_VERIFY,ASN1_R_UNKNOWN_MESSAGE_DIGEST_ALGORITHM);
		goto err;
		}
	
	inl=i2d(data,NULL);
	buf_in=OPENSSL_malloc((unsigned int)inl);
	if (buf_in == NULL)
		{
		ASN1err(ASN1_F_ASN1_VERIFY,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	p=buf_in;

	i2d(data,&p);
	EVP_VerifyInit_ex(&ctx,type, NULL);
	EVP_VerifyUpdate(&ctx,(unsigned char *)buf_in,inl);

	OPENSSL_cleanse(buf_in,(unsigned int)inl);
	OPENSSL_free(buf_in);

	if (EVP_VerifyFinal(&ctx,(unsigned char *)signature->data,
			(unsigned int)signature->length,pkey) <= 0)
		{
		ASN1err(ASN1_F_ASN1_VERIFY,ERR_R_EVP_LIB);
		ret=0;
		goto err;
		}
	/* we don't need to zero the 'ctx' because we just checked
	 * public information */
	/* memset(&ctx,0,sizeof(ctx)); */
	ret=1;
err:
	EVP_MD_CTX_cleanup(&ctx);
	return(ret);
	}

#endif


int ASN1_item_verify(const ASN1_ITEM *it, X509_ALGOR *a, ASN1_BIT_STRING *signature,
	     void *asn, EVP_PKEY *pkey)
	{
	EVP_MD_CTX ctx;
	const EVP_MD *type = NULL;
	unsigned char *buf_in=NULL;
	int ret= -1,inl;

	int mdnid, pknid;

	EVP_MD_CTX_init(&ctx);

	/* Convert signature OID into digest and public key OIDs */
	if (!OBJ_find_sigid_algs(OBJ_obj2nid(a->algorithm), &mdnid, &pknid))
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ASN1_R_UNKNOWN_SIGNATURE_ALGORITHM);
		goto err;
		}
	type=EVP_get_digestbynid(mdnid);
	if (type == NULL)
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ASN1_R_UNKNOWN_MESSAGE_DIGEST_ALGORITHM);
		goto err;
		}

	/* Check public key OID matches public key type */
	if (EVP_PKEY_type(pknid) != pkey->ameth->pkey_id)
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ASN1_R_WRONG_PUBLIC_KEY_TYPE);
		goto err;
		}

	if (!EVP_VerifyInit_ex(&ctx,type, NULL))
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ERR_R_EVP_LIB);
		ret=0;
		goto err;
		}

	inl = ASN1_item_i2d(asn, &buf_in, it);
	
	if (buf_in == NULL)
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	EVP_VerifyUpdate(&ctx,(unsigned char *)buf_in,inl);

	OPENSSL_cleanse(buf_in,(unsigned int)inl);
	OPENSSL_free(buf_in);

	if (EVP_VerifyFinal(&ctx,(unsigned char *)signature->data,
			(unsigned int)signature->length,pkey) <= 0)
		{
		ASN1err(ASN1_F_ASN1_ITEM_VERIFY,ERR_R_EVP_LIB);
		ret=0;
		goto err;
		}
	/* we don't need to zero the 'ctx' because we just checked
	 * public information */
	/* memset(&ctx,0,sizeof(ctx)); */
	ret=1;
err:
	EVP_MD_CTX_cleanup(&ctx);
	return(ret);
	}


