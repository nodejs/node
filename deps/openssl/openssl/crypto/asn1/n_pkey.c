/* crypto/asn1/n_pkey.c */
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
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#include <openssl/objects.h>
#include <openssl/asn1t.h>
#include <openssl/asn1_mac.h>
#include <openssl/evp.h>
#include <openssl/x509.h>


#ifndef OPENSSL_NO_RC4

typedef struct netscape_pkey_st
	{
	long version;
	X509_ALGOR *algor;
	ASN1_OCTET_STRING *private_key;
	} NETSCAPE_PKEY;

typedef struct netscape_encrypted_pkey_st
	{
	ASN1_OCTET_STRING *os;
	/* This is the same structure as DigestInfo so use it:
	 * although this isn't really anything to do with
	 * digests.
	 */
	X509_SIG *enckey;
	} NETSCAPE_ENCRYPTED_PKEY;


ASN1_BROKEN_SEQUENCE(NETSCAPE_ENCRYPTED_PKEY) = {
	ASN1_SIMPLE(NETSCAPE_ENCRYPTED_PKEY, os, ASN1_OCTET_STRING),
	ASN1_SIMPLE(NETSCAPE_ENCRYPTED_PKEY, enckey, X509_SIG)
} ASN1_BROKEN_SEQUENCE_END(NETSCAPE_ENCRYPTED_PKEY)

DECLARE_ASN1_FUNCTIONS_const(NETSCAPE_ENCRYPTED_PKEY)
DECLARE_ASN1_ENCODE_FUNCTIONS_const(NETSCAPE_ENCRYPTED_PKEY,NETSCAPE_ENCRYPTED_PKEY)
IMPLEMENT_ASN1_FUNCTIONS_const(NETSCAPE_ENCRYPTED_PKEY)

ASN1_SEQUENCE(NETSCAPE_PKEY) = {
	ASN1_SIMPLE(NETSCAPE_PKEY, version, LONG),
	ASN1_SIMPLE(NETSCAPE_PKEY, algor, X509_ALGOR),
	ASN1_SIMPLE(NETSCAPE_PKEY, private_key, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(NETSCAPE_PKEY)

DECLARE_ASN1_FUNCTIONS_const(NETSCAPE_PKEY)
DECLARE_ASN1_ENCODE_FUNCTIONS_const(NETSCAPE_PKEY,NETSCAPE_PKEY)
IMPLEMENT_ASN1_FUNCTIONS_const(NETSCAPE_PKEY)

static RSA *d2i_RSA_NET_2(RSA **a, ASN1_OCTET_STRING *os,
			  int (*cb)(char *buf, int len, const char *prompt,
				    int verify),
			  int sgckey);

int i2d_Netscape_RSA(const RSA *a, unsigned char **pp,
		     int (*cb)(char *buf, int len, const char *prompt,
			       int verify))
{
	return i2d_RSA_NET(a, pp, cb, 0);
}

int i2d_RSA_NET(const RSA *a, unsigned char **pp,
		int (*cb)(char *buf, int len, const char *prompt, int verify),
		int sgckey)
	{
	int i, j, ret = 0;
	int rsalen, pkeylen, olen;
	NETSCAPE_PKEY *pkey = NULL;
	NETSCAPE_ENCRYPTED_PKEY *enckey = NULL;
	unsigned char buf[256],*zz;
	unsigned char key[EVP_MAX_KEY_LENGTH];
	EVP_CIPHER_CTX ctx;

	if (a == NULL) return(0);

	if ((pkey=NETSCAPE_PKEY_new()) == NULL) goto err;
	if ((enckey=NETSCAPE_ENCRYPTED_PKEY_new()) == NULL) goto err;
	pkey->version = 0;

	pkey->algor->algorithm=OBJ_nid2obj(NID_rsaEncryption);
	if ((pkey->algor->parameter=ASN1_TYPE_new()) == NULL) goto err;
	pkey->algor->parameter->type=V_ASN1_NULL;

	rsalen = i2d_RSAPrivateKey(a, NULL);

	/* Fake some octet strings just for the initial length
	 * calculation.
 	 */

	pkey->private_key->length=rsalen;

	pkeylen=i2d_NETSCAPE_PKEY(pkey,NULL);

	enckey->enckey->digest->length = pkeylen;

	enckey->os->length = 11;	/* "private-key" */

	enckey->enckey->algor->algorithm=OBJ_nid2obj(NID_rc4);
	if ((enckey->enckey->algor->parameter=ASN1_TYPE_new()) == NULL) goto err;
	enckey->enckey->algor->parameter->type=V_ASN1_NULL;

	if (pp == NULL)
		{
		olen = i2d_NETSCAPE_ENCRYPTED_PKEY(enckey, NULL);
		NETSCAPE_PKEY_free(pkey);
		NETSCAPE_ENCRYPTED_PKEY_free(enckey);
		return olen;
		}


	/* Since its RC4 encrypted length is actual length */
	if ((zz=(unsigned char *)OPENSSL_malloc(rsalen)) == NULL)
		{
		ASN1err(ASN1_F_I2D_RSA_NET,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	pkey->private_key->data = zz;
	/* Write out private key encoding */
	i2d_RSAPrivateKey(a,&zz);

	if ((zz=OPENSSL_malloc(pkeylen)) == NULL)
		{
		ASN1err(ASN1_F_I2D_RSA_NET,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (!ASN1_STRING_set(enckey->os, "private-key", -1)) 
		{
		ASN1err(ASN1_F_I2D_RSA_NET,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	enckey->enckey->digest->data = zz;
	i2d_NETSCAPE_PKEY(pkey,&zz);

	/* Wipe the private key encoding */
	OPENSSL_cleanse(pkey->private_key->data, rsalen);
		
	if (cb == NULL)
		cb=EVP_read_pw_string;
	i=cb((char *)buf,256,"Enter Private Key password:",1);
	if (i != 0)
		{
		ASN1err(ASN1_F_I2D_RSA_NET,ASN1_R_BAD_PASSWORD_READ);
		goto err;
		}
	i = strlen((char *)buf);
	/* If the key is used for SGC the algorithm is modified a little. */
	if(sgckey) {
		EVP_Digest(buf, i, buf, NULL, EVP_md5(), NULL);
		memcpy(buf + 16, "SGCKEYSALT", 10);
		i = 26;
	}

	EVP_BytesToKey(EVP_rc4(),EVP_md5(),NULL,buf,i,1,key,NULL);
	OPENSSL_cleanse(buf,256);

	/* Encrypt private key in place */
	zz = enckey->enckey->digest->data;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx,EVP_rc4(),NULL,key,NULL);
	EVP_EncryptUpdate(&ctx,zz,&i,zz,pkeylen);
	EVP_EncryptFinal_ex(&ctx,zz + i,&j);
	EVP_CIPHER_CTX_cleanup(&ctx);

	ret = i2d_NETSCAPE_ENCRYPTED_PKEY(enckey, pp);
err:
	NETSCAPE_ENCRYPTED_PKEY_free(enckey);
	NETSCAPE_PKEY_free(pkey);
	return(ret);
	}


RSA *d2i_Netscape_RSA(RSA **a, const unsigned char **pp, long length,
		      int (*cb)(char *buf, int len, const char *prompt,
				int verify))
{
	return d2i_RSA_NET(a, pp, length, cb, 0);
}

RSA *d2i_RSA_NET(RSA **a, const unsigned char **pp, long length,
		 int (*cb)(char *buf, int len, const char *prompt, int verify),
		 int sgckey)
	{
	RSA *ret=NULL;
	const unsigned char *p, *kp;
	NETSCAPE_ENCRYPTED_PKEY *enckey = NULL;

	p = *pp;

	enckey = d2i_NETSCAPE_ENCRYPTED_PKEY(NULL, &p, length);
	if(!enckey) {
		ASN1err(ASN1_F_D2I_RSA_NET,ASN1_R_DECODING_ERROR);
		return NULL;
	}

	if ((enckey->os->length != 11) || (strncmp("private-key",
		(char *)enckey->os->data,11) != 0))
		{
		ASN1err(ASN1_F_D2I_RSA_NET,ASN1_R_PRIVATE_KEY_HEADER_MISSING);
		NETSCAPE_ENCRYPTED_PKEY_free(enckey);
		return NULL;
		}
	if (OBJ_obj2nid(enckey->enckey->algor->algorithm) != NID_rc4)
		{
		ASN1err(ASN1_F_D2I_RSA_NET,ASN1_R_UNSUPPORTED_ENCRYPTION_ALGORITHM);
		goto err;
	}
	kp = enckey->enckey->digest->data;
	if (cb == NULL)
		cb=EVP_read_pw_string;
	if ((ret=d2i_RSA_NET_2(a, enckey->enckey->digest,cb, sgckey)) == NULL) goto err;

	*pp = p;

	err:
	NETSCAPE_ENCRYPTED_PKEY_free(enckey);
	return ret;

	}

static RSA *d2i_RSA_NET_2(RSA **a, ASN1_OCTET_STRING *os,
			  int (*cb)(char *buf, int len, const char *prompt,
				    int verify), int sgckey)
	{
	NETSCAPE_PKEY *pkey=NULL;
	RSA *ret=NULL;
	int i,j;
	unsigned char buf[256];
	const unsigned char *zz;
	unsigned char key[EVP_MAX_KEY_LENGTH];
	EVP_CIPHER_CTX ctx;

	i=cb((char *)buf,256,"Enter Private Key password:",0);
	if (i != 0)
		{
		ASN1err(ASN1_F_D2I_RSA_NET_2,ASN1_R_BAD_PASSWORD_READ);
		goto err;
		}

	i = strlen((char *)buf);
	if(sgckey){
		EVP_Digest(buf, i, buf, NULL, EVP_md5(), NULL);
		memcpy(buf + 16, "SGCKEYSALT", 10);
		i = 26;
	}
		
	EVP_BytesToKey(EVP_rc4(),EVP_md5(),NULL,buf,i,1,key,NULL);
	OPENSSL_cleanse(buf,256);

	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx,EVP_rc4(),NULL, key,NULL);
	EVP_DecryptUpdate(&ctx,os->data,&i,os->data,os->length);
	EVP_DecryptFinal_ex(&ctx,&(os->data[i]),&j);
	EVP_CIPHER_CTX_cleanup(&ctx);
	os->length=i+j;

	zz=os->data;

	if ((pkey=d2i_NETSCAPE_PKEY(NULL,&zz,os->length)) == NULL)
		{
		ASN1err(ASN1_F_D2I_RSA_NET_2,ASN1_R_UNABLE_TO_DECODE_RSA_PRIVATE_KEY);
		goto err;
		}
		
	zz=pkey->private_key->data;
	if ((ret=d2i_RSAPrivateKey(a,&zz,pkey->private_key->length)) == NULL)
		{
		ASN1err(ASN1_F_D2I_RSA_NET_2,ASN1_R_UNABLE_TO_DECODE_RSA_KEY);
		goto err;
		}
err:
	NETSCAPE_PKEY_free(pkey);
	return(ret);
	}

#endif /* OPENSSL_NO_RC4 */

#else /* !OPENSSL_NO_RSA */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
