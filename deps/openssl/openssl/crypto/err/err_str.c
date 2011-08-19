/* crypto/err/err_str.c */
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
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
#include <stdarg.h>
#include <string.h>
#include "cryptlib.h"
#include <openssl/lhash.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#ifndef OPENSSL_NO_ERR
static ERR_STRING_DATA ERR_str_libraries[]=
	{
{ERR_PACK(ERR_LIB_NONE,0,0)		,"unknown library"},
{ERR_PACK(ERR_LIB_SYS,0,0)		,"system library"},
{ERR_PACK(ERR_LIB_BN,0,0)		,"bignum routines"},
{ERR_PACK(ERR_LIB_RSA,0,0)		,"rsa routines"},
{ERR_PACK(ERR_LIB_DH,0,0)		,"Diffie-Hellman routines"},
{ERR_PACK(ERR_LIB_EVP,0,0)		,"digital envelope routines"},
{ERR_PACK(ERR_LIB_BUF,0,0)		,"memory buffer routines"},
{ERR_PACK(ERR_LIB_OBJ,0,0)		,"object identifier routines"},
{ERR_PACK(ERR_LIB_PEM,0,0)		,"PEM routines"},
{ERR_PACK(ERR_LIB_DSA,0,0)		,"dsa routines"},
{ERR_PACK(ERR_LIB_X509,0,0)		,"x509 certificate routines"},
{ERR_PACK(ERR_LIB_ASN1,0,0)		,"asn1 encoding routines"},
{ERR_PACK(ERR_LIB_CONF,0,0)		,"configuration file routines"},
{ERR_PACK(ERR_LIB_CRYPTO,0,0)		,"common libcrypto routines"},
{ERR_PACK(ERR_LIB_EC,0,0)		,"elliptic curve routines"},
{ERR_PACK(ERR_LIB_SSL,0,0)		,"SSL routines"},
{ERR_PACK(ERR_LIB_BIO,0,0)		,"BIO routines"},
{ERR_PACK(ERR_LIB_PKCS7,0,0)		,"PKCS7 routines"},
{ERR_PACK(ERR_LIB_X509V3,0,0)		,"X509 V3 routines"},
{ERR_PACK(ERR_LIB_PKCS12,0,0)		,"PKCS12 routines"},
{ERR_PACK(ERR_LIB_RAND,0,0)		,"random number generator"},
{ERR_PACK(ERR_LIB_DSO,0,0)		,"DSO support routines"},
{ERR_PACK(ERR_LIB_ENGINE,0,0)		,"engine routines"},
{ERR_PACK(ERR_LIB_OCSP,0,0)		,"OCSP routines"},
{ERR_PACK(ERR_LIB_FIPS,0,0)		,"FIPS routines"},
{ERR_PACK(ERR_LIB_CMS,0,0)		,"CMS routines"},
{ERR_PACK(ERR_LIB_JPAKE,0,0)		,"JPAKE routines"},
{0,NULL},
	};

static ERR_STRING_DATA ERR_str_functs[]=
	{
	{ERR_PACK(0,SYS_F_FOPEN,0),     	"fopen"},
	{ERR_PACK(0,SYS_F_CONNECT,0),		"connect"},
	{ERR_PACK(0,SYS_F_GETSERVBYNAME,0),	"getservbyname"},
	{ERR_PACK(0,SYS_F_SOCKET,0),		"socket"}, 
	{ERR_PACK(0,SYS_F_IOCTLSOCKET,0),	"ioctlsocket"},
	{ERR_PACK(0,SYS_F_BIND,0),		"bind"},
	{ERR_PACK(0,SYS_F_LISTEN,0),		"listen"},
	{ERR_PACK(0,SYS_F_ACCEPT,0),		"accept"},
#ifdef OPENSSL_SYS_WINDOWS
	{ERR_PACK(0,SYS_F_WSASTARTUP,0),	"WSAstartup"},
#endif
	{ERR_PACK(0,SYS_F_OPENDIR,0),		"opendir"},
	{ERR_PACK(0,SYS_F_FREAD,0),		"fread"},
	{0,NULL},
	};

static ERR_STRING_DATA ERR_str_reasons[]=
	{
{ERR_R_SYS_LIB				,"system lib"},
{ERR_R_BN_LIB				,"BN lib"},
{ERR_R_RSA_LIB				,"RSA lib"},
{ERR_R_DH_LIB				,"DH lib"},
{ERR_R_EVP_LIB				,"EVP lib"},
{ERR_R_BUF_LIB				,"BUF lib"},
{ERR_R_OBJ_LIB				,"OBJ lib"},
{ERR_R_PEM_LIB				,"PEM lib"},
{ERR_R_DSA_LIB				,"DSA lib"},
{ERR_R_X509_LIB				,"X509 lib"},
{ERR_R_ASN1_LIB				,"ASN1 lib"},
{ERR_R_CONF_LIB				,"CONF lib"},
{ERR_R_CRYPTO_LIB			,"CRYPTO lib"},
{ERR_R_EC_LIB				,"EC lib"},
{ERR_R_SSL_LIB				,"SSL lib"},
{ERR_R_BIO_LIB				,"BIO lib"},
{ERR_R_PKCS7_LIB			,"PKCS7 lib"},
{ERR_R_X509V3_LIB			,"X509V3 lib"},
{ERR_R_PKCS12_LIB			,"PKCS12 lib"},
{ERR_R_RAND_LIB				,"RAND lib"},
{ERR_R_DSO_LIB				,"DSO lib"},
{ERR_R_ENGINE_LIB			,"ENGINE lib"},
{ERR_R_OCSP_LIB				,"OCSP lib"},

{ERR_R_NESTED_ASN1_ERROR		,"nested asn1 error"},
{ERR_R_BAD_ASN1_OBJECT_HEADER		,"bad asn1 object header"},
{ERR_R_BAD_GET_ASN1_OBJECT_CALL		,"bad get asn1 object call"},
{ERR_R_EXPECTING_AN_ASN1_SEQUENCE	,"expecting an asn1 sequence"},
{ERR_R_ASN1_LENGTH_MISMATCH		,"asn1 length mismatch"},
{ERR_R_MISSING_ASN1_EOS			,"missing asn1 eos"},

{ERR_R_FATAL                            ,"fatal"},
{ERR_R_MALLOC_FAILURE			,"malloc failure"},
{ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED	,"called a function you should not call"},
{ERR_R_PASSED_NULL_PARAMETER		,"passed a null parameter"},
{ERR_R_INTERNAL_ERROR			,"internal error"},
{ERR_R_DISABLED				,"called a function that was disabled at compile-time"},

{0,NULL},
	};
#endif

#ifndef OPENSSL_NO_ERR
#define NUM_SYS_STR_REASONS 127
#define LEN_SYS_STR_REASON 32

static ERR_STRING_DATA SYS_str_reasons[NUM_SYS_STR_REASONS + 1];
/* SYS_str_reasons is filled with copies of strerror() results at
 * initialization.
 * 'errno' values up to 127 should cover all usual errors,
 * others will be displayed numerically by ERR_error_string.
 * It is crucial that we have something for each reason code
 * that occurs in ERR_str_reasons, or bogus reason strings
 * will be returned for SYSerr, which always gets an errno
 * value and never one of those 'standard' reason codes. */

static void build_SYS_str_reasons(void)
	{
	/* OPENSSL_malloc cannot be used here, use static storage instead */
	static char strerror_tab[NUM_SYS_STR_REASONS][LEN_SYS_STR_REASON];
	int i;
	static int init = 1;

	CRYPTO_r_lock(CRYPTO_LOCK_ERR);
	if (!init)
		{
		CRYPTO_r_unlock(CRYPTO_LOCK_ERR);
		return;
		}
	
	CRYPTO_r_unlock(CRYPTO_LOCK_ERR);
	CRYPTO_w_lock(CRYPTO_LOCK_ERR);
	if (!init)
		{
		CRYPTO_w_unlock(CRYPTO_LOCK_ERR);
		return;
		}

	for (i = 1; i <= NUM_SYS_STR_REASONS; i++)
		{
		ERR_STRING_DATA *str = &SYS_str_reasons[i - 1];

		str->error = (unsigned long)i;
		if (str->string == NULL)
			{
			char (*dest)[LEN_SYS_STR_REASON] = &(strerror_tab[i - 1]);
			char *src = strerror(i);
			if (src != NULL)
				{
				strncpy(*dest, src, sizeof *dest);
				(*dest)[sizeof *dest - 1] = '\0';
				str->string = *dest;
				}
			}
		if (str->string == NULL)
			str->string = "unknown";
		}

	/* Now we still have SYS_str_reasons[NUM_SYS_STR_REASONS] = {0, NULL},
	 * as required by ERR_load_strings. */

	init = 0;
	
	CRYPTO_w_unlock(CRYPTO_LOCK_ERR);
	}
#endif

void ERR_load_ERR_strings(void)
	{
#ifndef OPENSSL_NO_ERR
	if (ERR_func_error_string(ERR_str_functs[0].error) == NULL)
		{
		ERR_load_strings(0,ERR_str_libraries);
		ERR_load_strings(0,ERR_str_reasons);
		ERR_load_strings(ERR_LIB_SYS,ERR_str_functs);
		build_SYS_str_reasons();
		ERR_load_strings(ERR_LIB_SYS,SYS_str_reasons);
		}
#endif
	}

