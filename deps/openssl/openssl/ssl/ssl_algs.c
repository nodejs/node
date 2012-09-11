/* ssl/ssl_algs.c */
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
#include <openssl/objects.h>
#include <openssl/lhash.h>
#include "ssl_locl.h"

int SSL_library_init(void)
	{

#ifndef OPENSSL_NO_DES
	EVP_add_cipher(EVP_des_cbc());
	EVP_add_cipher(EVP_des_ede3_cbc());
#endif
#ifndef OPENSSL_NO_IDEA
	EVP_add_cipher(EVP_idea_cbc());
#endif
#ifndef OPENSSL_NO_RC4
	EVP_add_cipher(EVP_rc4());
#if !defined(OPENSSL_NO_MD5) && (defined(__x86_64) || defined(__x86_64__))
	EVP_add_cipher(EVP_rc4_hmac_md5());
#endif
#endif  
#ifndef OPENSSL_NO_RC2
	EVP_add_cipher(EVP_rc2_cbc());
	/* Not actually used for SSL/TLS but this makes PKCS#12 work
	 * if an application only calls SSL_library_init().
	 */
	EVP_add_cipher(EVP_rc2_40_cbc());
#endif
#ifndef OPENSSL_NO_AES
	EVP_add_cipher(EVP_aes_128_cbc());
	EVP_add_cipher(EVP_aes_192_cbc());
	EVP_add_cipher(EVP_aes_256_cbc());
	EVP_add_cipher(EVP_aes_128_gcm());
	EVP_add_cipher(EVP_aes_256_gcm());
#if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_SHA1)
	EVP_add_cipher(EVP_aes_128_cbc_hmac_sha1());
	EVP_add_cipher(EVP_aes_256_cbc_hmac_sha1());
#endif
#endif
#ifndef OPENSSL_NO_CAMELLIA
	EVP_add_cipher(EVP_camellia_128_cbc());
	EVP_add_cipher(EVP_camellia_256_cbc());
#endif

#ifndef OPENSSL_NO_SEED
	EVP_add_cipher(EVP_seed_cbc());
#endif
  
#ifndef OPENSSL_NO_MD5
	EVP_add_digest(EVP_md5());
	EVP_add_digest_alias(SN_md5,"ssl2-md5");
	EVP_add_digest_alias(SN_md5,"ssl3-md5");
#endif
#ifndef OPENSSL_NO_SHA
	EVP_add_digest(EVP_sha1()); /* RSA with sha1 */
	EVP_add_digest_alias(SN_sha1,"ssl3-sha1");
	EVP_add_digest_alias(SN_sha1WithRSAEncryption,SN_sha1WithRSA);
#endif
#ifndef OPENSSL_NO_SHA256
	EVP_add_digest(EVP_sha224());
	EVP_add_digest(EVP_sha256());
#endif
#ifndef OPENSSL_NO_SHA512
	EVP_add_digest(EVP_sha384());
	EVP_add_digest(EVP_sha512());
#endif
#if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_DSA)
	EVP_add_digest(EVP_dss1()); /* DSA with sha1 */
	EVP_add_digest_alias(SN_dsaWithSHA1,SN_dsaWithSHA1_2);
	EVP_add_digest_alias(SN_dsaWithSHA1,"DSS1");
	EVP_add_digest_alias(SN_dsaWithSHA1,"dss1");
#endif
#ifndef OPENSSL_NO_ECDSA
	EVP_add_digest(EVP_ecdsa());
#endif
	/* If you want support for phased out ciphers, add the following */
#if 0
	EVP_add_digest(EVP_sha());
	EVP_add_digest(EVP_dss());
#endif
#ifndef OPENSSL_NO_COMP
	/* This will initialise the built-in compression algorithms.
	   The value returned is a STACK_OF(SSL_COMP), but that can
	   be discarded safely */
	(void)SSL_COMP_get_compression_methods();
#endif
	/* initialize cipher/digest methods table */
	ssl_load_ciphers();
	return(1);
	}

