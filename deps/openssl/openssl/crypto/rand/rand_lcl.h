/* crypto/rand/rand_lcl.h */
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
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_RAND_LCL_H
#define HEADER_RAND_LCL_H

#define ENTROPY_NEEDED 32  /* require 256 bits = 32 bytes of randomness */


#if !defined(USE_MD5_RAND) && !defined(USE_SHA1_RAND) && !defined(USE_MDC2_RAND) && !defined(USE_MD2_RAND)
#if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_SHA1)
#define USE_SHA1_RAND
#elif !defined(OPENSSL_NO_MD5)
#define USE_MD5_RAND
#elif !defined(OPENSSL_NO_MDC2) && !defined(OPENSSL_NO_DES)
#define USE_MDC2_RAND
#elif !defined(OPENSSL_NO_MD2)
#define USE_MD2_RAND
#else
#error No message digest algorithm available
#endif
#endif

#include <openssl/evp.h>
#define MD_Update(a,b,c)	EVP_DigestUpdate(a,b,c)
#define	MD_Final(a,b)		EVP_DigestFinal_ex(a,b,NULL)
#if defined(USE_MD5_RAND)
#include <openssl/md5.h>
#define MD_DIGEST_LENGTH	MD5_DIGEST_LENGTH
#define MD_Init(a)		EVP_DigestInit_ex(a,EVP_md5(), NULL)
#define	MD(a,b,c)		EVP_Digest(a,b,c,NULL,EVP_md5(), NULL)
#elif defined(USE_SHA1_RAND)
#include <openssl/sha.h>
#define MD_DIGEST_LENGTH	SHA_DIGEST_LENGTH
#define MD_Init(a)		EVP_DigestInit_ex(a,EVP_sha1(), NULL)
#define	MD(a,b,c)		EVP_Digest(a,b,c,NULL,EVP_sha1(), NULL)
#elif defined(USE_MDC2_RAND)
#include <openssl/mdc2.h>
#define MD_DIGEST_LENGTH	MDC2_DIGEST_LENGTH
#define MD_Init(a)		EVP_DigestInit_ex(a,EVP_mdc2(), NULL)
#define	MD(a,b,c)		EVP_Digest(a,b,c,NULL,EVP_mdc2(), NULL)
#elif defined(USE_MD2_RAND)
#include <openssl/md2.h>
#define MD_DIGEST_LENGTH	MD2_DIGEST_LENGTH
#define MD_Init(a)		EVP_DigestInit_ex(a,EVP_md2(), NULL)
#define	MD(a,b,c)		EVP_Digest(a,b,c,NULL,EVP_md2(), NULL)
#endif

#ifndef OPENSSL_NO_ENGINE
void int_RAND_set_callbacks(
	int (*set_rand_func)(const RAND_METHOD *meth,
						const RAND_METHOD **pmeth),
	const RAND_METHOD *(*get_rand_func)
						(const RAND_METHOD **pmeth));
int eng_RAND_set_rand_method(const RAND_METHOD *meth,
				const RAND_METHOD **pmeth);
const RAND_METHOD *eng_RAND_get_rand_method(const RAND_METHOD **pmeth);
#endif


#endif
