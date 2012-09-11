/* crypto/evp/c_allc.c */
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
#include <openssl/pkcs12.h>
#include <openssl/objects.h>

void OpenSSL_add_all_ciphers(void)
	{

#ifndef OPENSSL_NO_DES
	EVP_add_cipher(EVP_des_cfb());
	EVP_add_cipher(EVP_des_cfb1());
	EVP_add_cipher(EVP_des_cfb8());
	EVP_add_cipher(EVP_des_ede_cfb());
	EVP_add_cipher(EVP_des_ede3_cfb());
	EVP_add_cipher(EVP_des_ede3_cfb1());
	EVP_add_cipher(EVP_des_ede3_cfb8());

	EVP_add_cipher(EVP_des_ofb());
	EVP_add_cipher(EVP_des_ede_ofb());
	EVP_add_cipher(EVP_des_ede3_ofb());

	EVP_add_cipher(EVP_desx_cbc());
	EVP_add_cipher_alias(SN_desx_cbc,"DESX");
	EVP_add_cipher_alias(SN_desx_cbc,"desx");

	EVP_add_cipher(EVP_des_cbc());
	EVP_add_cipher_alias(SN_des_cbc,"DES");
	EVP_add_cipher_alias(SN_des_cbc,"des");
	EVP_add_cipher(EVP_des_ede_cbc());
	EVP_add_cipher(EVP_des_ede3_cbc());
	EVP_add_cipher_alias(SN_des_ede3_cbc,"DES3");
	EVP_add_cipher_alias(SN_des_ede3_cbc,"des3");

	EVP_add_cipher(EVP_des_ecb());
	EVP_add_cipher(EVP_des_ede());
	EVP_add_cipher(EVP_des_ede3());
#endif

#ifndef OPENSSL_NO_RC4
	EVP_add_cipher(EVP_rc4());
	EVP_add_cipher(EVP_rc4_40());
#ifndef OPENSSL_NO_MD5
	EVP_add_cipher(EVP_rc4_hmac_md5());
#endif
#endif

#ifndef OPENSSL_NO_IDEA
	EVP_add_cipher(EVP_idea_ecb());
	EVP_add_cipher(EVP_idea_cfb());
	EVP_add_cipher(EVP_idea_ofb());
	EVP_add_cipher(EVP_idea_cbc());
	EVP_add_cipher_alias(SN_idea_cbc,"IDEA");
	EVP_add_cipher_alias(SN_idea_cbc,"idea");
#endif

#ifndef OPENSSL_NO_SEED
	EVP_add_cipher(EVP_seed_ecb());
	EVP_add_cipher(EVP_seed_cfb());
	EVP_add_cipher(EVP_seed_ofb());
	EVP_add_cipher(EVP_seed_cbc());
	EVP_add_cipher_alias(SN_seed_cbc,"SEED");
	EVP_add_cipher_alias(SN_seed_cbc,"seed");
#endif

#ifndef OPENSSL_NO_RC2
	EVP_add_cipher(EVP_rc2_ecb());
	EVP_add_cipher(EVP_rc2_cfb());
	EVP_add_cipher(EVP_rc2_ofb());
	EVP_add_cipher(EVP_rc2_cbc());
	EVP_add_cipher(EVP_rc2_40_cbc());
	EVP_add_cipher(EVP_rc2_64_cbc());
	EVP_add_cipher_alias(SN_rc2_cbc,"RC2");
	EVP_add_cipher_alias(SN_rc2_cbc,"rc2");
#endif

#ifndef OPENSSL_NO_BF
	EVP_add_cipher(EVP_bf_ecb());
	EVP_add_cipher(EVP_bf_cfb());
	EVP_add_cipher(EVP_bf_ofb());
	EVP_add_cipher(EVP_bf_cbc());
	EVP_add_cipher_alias(SN_bf_cbc,"BF");
	EVP_add_cipher_alias(SN_bf_cbc,"bf");
	EVP_add_cipher_alias(SN_bf_cbc,"blowfish");
#endif

#ifndef OPENSSL_NO_CAST
	EVP_add_cipher(EVP_cast5_ecb());
	EVP_add_cipher(EVP_cast5_cfb());
	EVP_add_cipher(EVP_cast5_ofb());
	EVP_add_cipher(EVP_cast5_cbc());
	EVP_add_cipher_alias(SN_cast5_cbc,"CAST");
	EVP_add_cipher_alias(SN_cast5_cbc,"cast");
	EVP_add_cipher_alias(SN_cast5_cbc,"CAST-cbc");
	EVP_add_cipher_alias(SN_cast5_cbc,"cast-cbc");
#endif

#ifndef OPENSSL_NO_RC5
	EVP_add_cipher(EVP_rc5_32_12_16_ecb());
	EVP_add_cipher(EVP_rc5_32_12_16_cfb());
	EVP_add_cipher(EVP_rc5_32_12_16_ofb());
	EVP_add_cipher(EVP_rc5_32_12_16_cbc());
	EVP_add_cipher_alias(SN_rc5_cbc,"rc5");
	EVP_add_cipher_alias(SN_rc5_cbc,"RC5");
#endif

#ifndef OPENSSL_NO_AES
	EVP_add_cipher(EVP_aes_128_ecb());
	EVP_add_cipher(EVP_aes_128_cbc());
	EVP_add_cipher(EVP_aes_128_cfb());
	EVP_add_cipher(EVP_aes_128_cfb1());
	EVP_add_cipher(EVP_aes_128_cfb8());
	EVP_add_cipher(EVP_aes_128_ofb());
	EVP_add_cipher(EVP_aes_128_ctr());
	EVP_add_cipher(EVP_aes_128_gcm());
	EVP_add_cipher(EVP_aes_128_xts());
	EVP_add_cipher_alias(SN_aes_128_cbc,"AES128");
	EVP_add_cipher_alias(SN_aes_128_cbc,"aes128");
	EVP_add_cipher(EVP_aes_192_ecb());
	EVP_add_cipher(EVP_aes_192_cbc());
	EVP_add_cipher(EVP_aes_192_cfb());
	EVP_add_cipher(EVP_aes_192_cfb1());
	EVP_add_cipher(EVP_aes_192_cfb8());
	EVP_add_cipher(EVP_aes_192_ofb());
	EVP_add_cipher(EVP_aes_192_ctr());
	EVP_add_cipher(EVP_aes_192_gcm());
	EVP_add_cipher_alias(SN_aes_192_cbc,"AES192");
	EVP_add_cipher_alias(SN_aes_192_cbc,"aes192");
	EVP_add_cipher(EVP_aes_256_ecb());
	EVP_add_cipher(EVP_aes_256_cbc());
	EVP_add_cipher(EVP_aes_256_cfb());
	EVP_add_cipher(EVP_aes_256_cfb1());
	EVP_add_cipher(EVP_aes_256_cfb8());
	EVP_add_cipher(EVP_aes_256_ofb());
	EVP_add_cipher(EVP_aes_256_ctr());
	EVP_add_cipher(EVP_aes_256_gcm());
	EVP_add_cipher(EVP_aes_256_xts());
	EVP_add_cipher_alias(SN_aes_256_cbc,"AES256");
	EVP_add_cipher_alias(SN_aes_256_cbc,"aes256");
#if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_SHA1)
	EVP_add_cipher(EVP_aes_128_cbc_hmac_sha1());
	EVP_add_cipher(EVP_aes_256_cbc_hmac_sha1());
#endif
#endif

#ifndef OPENSSL_NO_CAMELLIA
	EVP_add_cipher(EVP_camellia_128_ecb());
	EVP_add_cipher(EVP_camellia_128_cbc());
	EVP_add_cipher(EVP_camellia_128_cfb());
	EVP_add_cipher(EVP_camellia_128_cfb1());
	EVP_add_cipher(EVP_camellia_128_cfb8());
	EVP_add_cipher(EVP_camellia_128_ofb());
	EVP_add_cipher_alias(SN_camellia_128_cbc,"CAMELLIA128");
	EVP_add_cipher_alias(SN_camellia_128_cbc,"camellia128");
	EVP_add_cipher(EVP_camellia_192_ecb());
	EVP_add_cipher(EVP_camellia_192_cbc());
	EVP_add_cipher(EVP_camellia_192_cfb());
	EVP_add_cipher(EVP_camellia_192_cfb1());
	EVP_add_cipher(EVP_camellia_192_cfb8());
	EVP_add_cipher(EVP_camellia_192_ofb());
	EVP_add_cipher_alias(SN_camellia_192_cbc,"CAMELLIA192");
	EVP_add_cipher_alias(SN_camellia_192_cbc,"camellia192");
	EVP_add_cipher(EVP_camellia_256_ecb());
	EVP_add_cipher(EVP_camellia_256_cbc());
	EVP_add_cipher(EVP_camellia_256_cfb());
	EVP_add_cipher(EVP_camellia_256_cfb1());
	EVP_add_cipher(EVP_camellia_256_cfb8());
	EVP_add_cipher(EVP_camellia_256_ofb());
	EVP_add_cipher_alias(SN_camellia_256_cbc,"CAMELLIA256");
	EVP_add_cipher_alias(SN_camellia_256_cbc,"camellia256");
#endif
	}
