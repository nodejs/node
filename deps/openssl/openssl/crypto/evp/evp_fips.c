/* crypto/evp/evp_fips.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2011 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 */


#include <openssl/evp.h>

#ifdef OPENSSL_FIPS
#include <openssl/fips.h>

const EVP_CIPHER *EVP_aes_128_cbc(void)  { return FIPS_evp_aes_128_cbc(); }
const EVP_CIPHER *EVP_aes_128_ccm(void)  { return FIPS_evp_aes_128_ccm(); }
const EVP_CIPHER *EVP_aes_128_cfb1(void)  { return FIPS_evp_aes_128_cfb1(); }
const EVP_CIPHER *EVP_aes_128_cfb128(void)  { return FIPS_evp_aes_128_cfb128(); }
const EVP_CIPHER *EVP_aes_128_cfb8(void)  { return FIPS_evp_aes_128_cfb8(); }
const EVP_CIPHER *EVP_aes_128_ctr(void)  { return FIPS_evp_aes_128_ctr(); }
const EVP_CIPHER *EVP_aes_128_ecb(void)  { return FIPS_evp_aes_128_ecb(); }
const EVP_CIPHER *EVP_aes_128_gcm(void)  { return FIPS_evp_aes_128_gcm(); }
const EVP_CIPHER *EVP_aes_128_ofb(void)  { return FIPS_evp_aes_128_ofb(); }
const EVP_CIPHER *EVP_aes_128_xts(void)  { return FIPS_evp_aes_128_xts(); }
const EVP_CIPHER *EVP_aes_192_cbc(void)  { return FIPS_evp_aes_192_cbc(); }
const EVP_CIPHER *EVP_aes_192_ccm(void)  { return FIPS_evp_aes_192_ccm(); }
const EVP_CIPHER *EVP_aes_192_cfb1(void)  { return FIPS_evp_aes_192_cfb1(); }
const EVP_CIPHER *EVP_aes_192_cfb128(void)  { return FIPS_evp_aes_192_cfb128(); }
const EVP_CIPHER *EVP_aes_192_cfb8(void)  { return FIPS_evp_aes_192_cfb8(); }
const EVP_CIPHER *EVP_aes_192_ctr(void)  { return FIPS_evp_aes_192_ctr(); }
const EVP_CIPHER *EVP_aes_192_ecb(void)  { return FIPS_evp_aes_192_ecb(); }
const EVP_CIPHER *EVP_aes_192_gcm(void)  { return FIPS_evp_aes_192_gcm(); }
const EVP_CIPHER *EVP_aes_192_ofb(void)  { return FIPS_evp_aes_192_ofb(); }
const EVP_CIPHER *EVP_aes_256_cbc(void)  { return FIPS_evp_aes_256_cbc(); }
const EVP_CIPHER *EVP_aes_256_ccm(void)  { return FIPS_evp_aes_256_ccm(); }
const EVP_CIPHER *EVP_aes_256_cfb1(void)  { return FIPS_evp_aes_256_cfb1(); }
const EVP_CIPHER *EVP_aes_256_cfb128(void)  { return FIPS_evp_aes_256_cfb128(); }
const EVP_CIPHER *EVP_aes_256_cfb8(void)  { return FIPS_evp_aes_256_cfb8(); }
const EVP_CIPHER *EVP_aes_256_ctr(void)  { return FIPS_evp_aes_256_ctr(); }
const EVP_CIPHER *EVP_aes_256_ecb(void)  { return FIPS_evp_aes_256_ecb(); }
const EVP_CIPHER *EVP_aes_256_gcm(void)  { return FIPS_evp_aes_256_gcm(); }
const EVP_CIPHER *EVP_aes_256_ofb(void)  { return FIPS_evp_aes_256_ofb(); }
const EVP_CIPHER *EVP_aes_256_xts(void)  { return FIPS_evp_aes_256_xts(); }
const EVP_CIPHER *EVP_des_ede(void)  { return FIPS_evp_des_ede(); }
const EVP_CIPHER *EVP_des_ede3(void)  { return FIPS_evp_des_ede3(); }
const EVP_CIPHER *EVP_des_ede3_cbc(void)  { return FIPS_evp_des_ede3_cbc(); }
const EVP_CIPHER *EVP_des_ede3_cfb1(void)  { return FIPS_evp_des_ede3_cfb1(); }
const EVP_CIPHER *EVP_des_ede3_cfb64(void)  { return FIPS_evp_des_ede3_cfb64(); }
const EVP_CIPHER *EVP_des_ede3_cfb8(void)  { return FIPS_evp_des_ede3_cfb8(); }
const EVP_CIPHER *EVP_des_ede3_ecb(void)  { return FIPS_evp_des_ede3_ecb(); }
const EVP_CIPHER *EVP_des_ede3_ofb(void)  { return FIPS_evp_des_ede3_ofb(); }
const EVP_CIPHER *EVP_des_ede_cbc(void)  { return FIPS_evp_des_ede_cbc(); }
const EVP_CIPHER *EVP_des_ede_cfb64(void)  { return FIPS_evp_des_ede_cfb64(); }
const EVP_CIPHER *EVP_des_ede_ecb(void)  { return FIPS_evp_des_ede_ecb(); }
const EVP_CIPHER *EVP_des_ede_ofb(void)  { return FIPS_evp_des_ede_ofb(); }
const EVP_CIPHER *EVP_enc_null(void)  { return FIPS_evp_enc_null(); }

const EVP_MD *EVP_sha1(void)  { return FIPS_evp_sha1(); }
const EVP_MD *EVP_sha224(void)  { return FIPS_evp_sha224(); }
const EVP_MD *EVP_sha256(void)  { return FIPS_evp_sha256(); }
const EVP_MD *EVP_sha384(void)  { return FIPS_evp_sha384(); }
const EVP_MD *EVP_sha512(void)  { return FIPS_evp_sha512(); }

const EVP_MD *EVP_dss(void)  { return FIPS_evp_dss(); }
const EVP_MD *EVP_dss1(void)  { return FIPS_evp_dss1(); }
const EVP_MD *EVP_ecdsa(void)  { return FIPS_evp_ecdsa(); }

#endif
