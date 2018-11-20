/* ssl/ssl_ciph.c */
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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>
#include <openssl/objects.h>
#ifndef OPENSSL_NO_COMP
# include <openssl/comp.h>
#endif
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif
#include "ssl_locl.h"

#define SSL_ENC_DES_IDX         0
#define SSL_ENC_3DES_IDX        1
#define SSL_ENC_RC4_IDX         2
#define SSL_ENC_RC2_IDX         3
#define SSL_ENC_IDEA_IDX        4
#define SSL_ENC_NULL_IDX        5
#define SSL_ENC_AES128_IDX      6
#define SSL_ENC_AES256_IDX      7
#define SSL_ENC_CAMELLIA128_IDX 8
#define SSL_ENC_CAMELLIA256_IDX 9
#define SSL_ENC_GOST89_IDX      10
#define SSL_ENC_SEED_IDX        11
#define SSL_ENC_AES128GCM_IDX   12
#define SSL_ENC_AES256GCM_IDX   13
#define SSL_ENC_NUM_IDX         14

static const EVP_CIPHER *ssl_cipher_methods[SSL_ENC_NUM_IDX] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL
};

#define SSL_COMP_NULL_IDX       0
#define SSL_COMP_ZLIB_IDX       1
#define SSL_COMP_NUM_IDX        2

static STACK_OF(SSL_COMP) *ssl_comp_methods = NULL;

#define SSL_MD_MD5_IDX  0
#define SSL_MD_SHA1_IDX 1
#define SSL_MD_GOST94_IDX 2
#define SSL_MD_GOST89MAC_IDX 3
#define SSL_MD_SHA256_IDX 4
#define SSL_MD_SHA384_IDX 5
/*
 * Constant SSL_MAX_DIGEST equal to size of digests array should be defined
 * in the ssl_locl.h
 */
#define SSL_MD_NUM_IDX  SSL_MAX_DIGEST
static const EVP_MD *ssl_digest_methods[SSL_MD_NUM_IDX] = {
    NULL, NULL, NULL, NULL, NULL, NULL
};

/*
 * PKEY_TYPE for GOST89MAC is known in advance, but, because implementation
 * is engine-provided, we'll fill it only if corresponding EVP_PKEY_METHOD is
 * found
 */
static int ssl_mac_pkey_id[SSL_MD_NUM_IDX] = {
    EVP_PKEY_HMAC, EVP_PKEY_HMAC, EVP_PKEY_HMAC, NID_undef,
    EVP_PKEY_HMAC, EVP_PKEY_HMAC
};

static int ssl_mac_secret_size[SSL_MD_NUM_IDX] = {
    0, 0, 0, 0, 0, 0
};

static int ssl_handshake_digest_flag[SSL_MD_NUM_IDX] = {
    SSL_HANDSHAKE_MAC_MD5, SSL_HANDSHAKE_MAC_SHA,
    SSL_HANDSHAKE_MAC_GOST94, 0, SSL_HANDSHAKE_MAC_SHA256,
    SSL_HANDSHAKE_MAC_SHA384
};

#define CIPHER_ADD      1
#define CIPHER_KILL     2
#define CIPHER_DEL      3
#define CIPHER_ORD      4
#define CIPHER_SPECIAL  5

typedef struct cipher_order_st {
    const SSL_CIPHER *cipher;
    int active;
    int dead;
    struct cipher_order_st *next, *prev;
} CIPHER_ORDER;

static const SSL_CIPHER cipher_aliases[] = {
    /* "ALL" doesn't include eNULL (must be specifically enabled) */
    {0, SSL_TXT_ALL, 0, 0, 0, ~SSL_eNULL, 0, 0, 0, 0, 0, 0},
    /* "COMPLEMENTOFALL" */
    {0, SSL_TXT_CMPALL, 0, 0, 0, SSL_eNULL, 0, 0, 0, 0, 0, 0},

    /*
     * "COMPLEMENTOFDEFAULT" (does *not* include ciphersuites not found in
     * ALL!)
     */
    {0, SSL_TXT_CMPDEF, 0, 0, SSL_aNULL, ~SSL_eNULL, 0, ~SSL_SSLV2,
     SSL_EXP_MASK, 0, 0, 0},

    /*
     * key exchange aliases (some of those using only a single bit here
     * combine multiple key exchange algs according to the RFCs, e.g. kEDH
     * combines DHE_DSS and DHE_RSA)
     */
    {0, SSL_TXT_kRSA, 0, SSL_kRSA, 0, 0, 0, 0, 0, 0, 0, 0},

    {0, SSL_TXT_kDHr, 0, SSL_kDHr, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kDHd, 0, SSL_kDHd, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kDH, 0, SSL_kDHr | SSL_kDHd, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kEDH, 0, SSL_kEDH, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kDHE, 0, SSL_kEDH, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_DH, 0, SSL_kDHr | SSL_kDHd | SSL_kEDH, 0, 0, 0, 0, 0, 0, 0,
     0},

    {0, SSL_TXT_kKRB5, 0, SSL_kKRB5, 0, 0, 0, 0, 0, 0, 0, 0},

    {0, SSL_TXT_kECDHr, 0, SSL_kECDHr, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kECDHe, 0, SSL_kECDHe, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kECDH, 0, SSL_kECDHr | SSL_kECDHe, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kEECDH, 0, SSL_kEECDH, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kECDHE, 0, SSL_kEECDH, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_ECDH, 0, SSL_kECDHr | SSL_kECDHe | SSL_kEECDH, 0, 0, 0, 0, 0,
     0, 0, 0},

    {0, SSL_TXT_kPSK, 0, SSL_kPSK, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kSRP, 0, SSL_kSRP, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_kGOST, 0, SSL_kGOST, 0, 0, 0, 0, 0, 0, 0, 0},

    /* server authentication aliases */
    {0, SSL_TXT_aRSA, 0, 0, SSL_aRSA, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aDSS, 0, 0, SSL_aDSS, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_DSS, 0, 0, SSL_aDSS, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aKRB5, 0, 0, SSL_aKRB5, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aNULL, 0, 0, SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    /* no such ciphersuites supported! */
    {0, SSL_TXT_aDH, 0, 0, SSL_aDH, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aECDH, 0, 0, SSL_aECDH, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aECDSA, 0, 0, SSL_aECDSA, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_ECDSA, 0, 0, SSL_aECDSA, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aPSK, 0, 0, SSL_aPSK, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aGOST94, 0, 0, SSL_aGOST94, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aGOST01, 0, 0, SSL_aGOST01, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aGOST, 0, 0, SSL_aGOST94 | SSL_aGOST01, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_aSRP, 0, 0, SSL_aSRP, 0, 0, 0, 0, 0, 0, 0},

    /* aliases combining key exchange and server authentication */
    {0, SSL_TXT_EDH, 0, SSL_kEDH, ~SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_DHE, 0, SSL_kEDH, ~SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_EECDH, 0, SSL_kEECDH, ~SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_ECDHE, 0, SSL_kEECDH, ~SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_NULL, 0, 0, 0, SSL_eNULL, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_KRB5, 0, SSL_kKRB5, SSL_aKRB5, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_RSA, 0, SSL_kRSA, SSL_aRSA, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_ADH, 0, SSL_kEDH, SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_AECDH, 0, SSL_kEECDH, SSL_aNULL, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_PSK, 0, SSL_kPSK, SSL_aPSK, 0, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SRP, 0, SSL_kSRP, 0, 0, 0, 0, 0, 0, 0, 0},

    /* symmetric encryption aliases */
    {0, SSL_TXT_DES, 0, 0, 0, SSL_DES, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_3DES, 0, 0, 0, SSL_3DES, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_RC4, 0, 0, 0, SSL_RC4, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_RC2, 0, 0, 0, SSL_RC2, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_IDEA, 0, 0, 0, SSL_IDEA, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SEED, 0, 0, 0, SSL_SEED, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_eNULL, 0, 0, 0, SSL_eNULL, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_AES128, 0, 0, 0, SSL_AES128 | SSL_AES128GCM, 0, 0, 0, 0, 0,
     0},
    {0, SSL_TXT_AES256, 0, 0, 0, SSL_AES256 | SSL_AES256GCM, 0, 0, 0, 0, 0,
     0},
    {0, SSL_TXT_AES, 0, 0, 0, SSL_AES, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_AES_GCM, 0, 0, 0, SSL_AES128GCM | SSL_AES256GCM, 0, 0, 0, 0,
     0, 0},
    {0, SSL_TXT_CAMELLIA128, 0, 0, 0, SSL_CAMELLIA128, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_CAMELLIA256, 0, 0, 0, SSL_CAMELLIA256, 0, 0, 0, 0, 0, 0},
    {0, SSL_TXT_CAMELLIA, 0, 0, 0, SSL_CAMELLIA128 | SSL_CAMELLIA256, 0, 0, 0,
     0, 0, 0},

    /* MAC aliases */
    {0, SSL_TXT_MD5, 0, 0, 0, 0, SSL_MD5, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SHA1, 0, 0, 0, 0, SSL_SHA1, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SHA, 0, 0, 0, 0, SSL_SHA1, 0, 0, 0, 0, 0},
    {0, SSL_TXT_GOST94, 0, 0, 0, 0, SSL_GOST94, 0, 0, 0, 0, 0},
    {0, SSL_TXT_GOST89MAC, 0, 0, 0, 0, SSL_GOST89MAC, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SHA256, 0, 0, 0, 0, SSL_SHA256, 0, 0, 0, 0, 0},
    {0, SSL_TXT_SHA384, 0, 0, 0, 0, SSL_SHA384, 0, 0, 0, 0, 0},

    /* protocol version aliases */
    {0, SSL_TXT_SSLV2, 0, 0, 0, 0, 0, SSL_SSLV2, 0, 0, 0, 0},
    {0, SSL_TXT_SSLV3, 0, 0, 0, 0, 0, SSL_SSLV3, 0, 0, 0, 0},
    {0, SSL_TXT_TLSV1, 0, 0, 0, 0, 0, SSL_TLSV1, 0, 0, 0, 0},
    {0, SSL_TXT_TLSV1_2, 0, 0, 0, 0, 0, SSL_TLSV1_2, 0, 0, 0, 0},

    /* export flag */
    {0, SSL_TXT_EXP, 0, 0, 0, 0, 0, 0, SSL_EXPORT, 0, 0, 0},
    {0, SSL_TXT_EXPORT, 0, 0, 0, 0, 0, 0, SSL_EXPORT, 0, 0, 0},

    /* strength classes */
    {0, SSL_TXT_EXP40, 0, 0, 0, 0, 0, 0, SSL_EXP40, 0, 0, 0},
    {0, SSL_TXT_EXP56, 0, 0, 0, 0, 0, 0, SSL_EXP56, 0, 0, 0},
    {0, SSL_TXT_LOW, 0, 0, 0, 0, 0, 0, SSL_LOW, 0, 0, 0},
    {0, SSL_TXT_MEDIUM, 0, 0, 0, 0, 0, 0, SSL_MEDIUM, 0, 0, 0},
    {0, SSL_TXT_HIGH, 0, 0, 0, 0, 0, 0, SSL_HIGH, 0, 0, 0},
    /* FIPS 140-2 approved ciphersuite */
    {0, SSL_TXT_FIPS, 0, 0, 0, ~SSL_eNULL, 0, 0, SSL_FIPS, 0, 0, 0},
    /* "DHE-" aliases to "EDH-" labels (for forward compatibility) */
    {0, SSL3_TXT_DHE_DSS_DES_40_CBC_SHA, 0,
     SSL_kDHE, SSL_aDSS, SSL_DES, SSL_SHA1, SSL_SSLV3, SSL_EXPORT | SSL_EXP40,
     0, 0, 0,},
    {0, SSL3_TXT_DHE_DSS_DES_64_CBC_SHA, 0,
     SSL_kDHE, SSL_aDSS, SSL_DES, SSL_SHA1, SSL_SSLV3, SSL_NOT_EXP | SSL_LOW,
     0, 0, 0,},
    {0, SSL3_TXT_DHE_DSS_DES_192_CBC3_SHA, 0,
     SSL_kDHE, SSL_aDSS, SSL_3DES, SSL_SHA1, SSL_SSLV3,
     SSL_NOT_EXP | SSL_HIGH | SSL_FIPS, 0, 0, 0,},
    {0, SSL3_TXT_DHE_RSA_DES_40_CBC_SHA, 0,
     SSL_kDHE, SSL_aRSA, SSL_DES, SSL_SHA1, SSL_SSLV3, SSL_EXPORT | SSL_EXP40,
     0, 0, 0,},
    {0, SSL3_TXT_DHE_RSA_DES_64_CBC_SHA, 0,
     SSL_kDHE, SSL_aRSA, SSL_DES, SSL_SHA1, SSL_SSLV3, SSL_NOT_EXP | SSL_LOW,
     0, 0, 0,},
    {0, SSL3_TXT_DHE_RSA_DES_192_CBC3_SHA, 0,
     SSL_kDHE, SSL_aRSA, SSL_3DES, SSL_SHA1, SSL_SSLV3,
     SSL_NOT_EXP | SSL_HIGH | SSL_FIPS, 0, 0, 0,},
};

/*
 * Search for public key algorithm with given name and return its pkey_id if
 * it is available. Otherwise return 0
 */
#ifdef OPENSSL_NO_ENGINE

static int get_optional_pkey_id(const char *pkey_name)
{
    const EVP_PKEY_ASN1_METHOD *ameth;
    int pkey_id = 0;
    ameth = EVP_PKEY_asn1_find_str(NULL, pkey_name, -1);
    if (ameth) {
        EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, ameth);
    }
    return pkey_id;
}

#else

static int get_optional_pkey_id(const char *pkey_name)
{
    const EVP_PKEY_ASN1_METHOD *ameth;
    ENGINE *tmpeng = NULL;
    int pkey_id = 0;
    ameth = EVP_PKEY_asn1_find_str(&tmpeng, pkey_name, -1);
    if (ameth) {
        EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, ameth);
    }
    if (tmpeng)
        ENGINE_finish(tmpeng);
    return pkey_id;
}

#endif

void ssl_load_ciphers(void)
{
    ssl_cipher_methods[SSL_ENC_DES_IDX] = EVP_get_cipherbyname(SN_des_cbc);
    ssl_cipher_methods[SSL_ENC_3DES_IDX] =
        EVP_get_cipherbyname(SN_des_ede3_cbc);
    ssl_cipher_methods[SSL_ENC_RC4_IDX] = EVP_get_cipherbyname(SN_rc4);
    ssl_cipher_methods[SSL_ENC_RC2_IDX] = EVP_get_cipherbyname(SN_rc2_cbc);
#ifndef OPENSSL_NO_IDEA
    ssl_cipher_methods[SSL_ENC_IDEA_IDX] = EVP_get_cipherbyname(SN_idea_cbc);
#else
    ssl_cipher_methods[SSL_ENC_IDEA_IDX] = NULL;
#endif
    ssl_cipher_methods[SSL_ENC_AES128_IDX] =
        EVP_get_cipherbyname(SN_aes_128_cbc);
    ssl_cipher_methods[SSL_ENC_AES256_IDX] =
        EVP_get_cipherbyname(SN_aes_256_cbc);
    ssl_cipher_methods[SSL_ENC_CAMELLIA128_IDX] =
        EVP_get_cipherbyname(SN_camellia_128_cbc);
    ssl_cipher_methods[SSL_ENC_CAMELLIA256_IDX] =
        EVP_get_cipherbyname(SN_camellia_256_cbc);
    ssl_cipher_methods[SSL_ENC_GOST89_IDX] =
        EVP_get_cipherbyname(SN_gost89_cnt);
    ssl_cipher_methods[SSL_ENC_SEED_IDX] = EVP_get_cipherbyname(SN_seed_cbc);

    ssl_cipher_methods[SSL_ENC_AES128GCM_IDX] =
        EVP_get_cipherbyname(SN_aes_128_gcm);
    ssl_cipher_methods[SSL_ENC_AES256GCM_IDX] =
        EVP_get_cipherbyname(SN_aes_256_gcm);

    ssl_digest_methods[SSL_MD_MD5_IDX] = EVP_get_digestbyname(SN_md5);
    ssl_mac_secret_size[SSL_MD_MD5_IDX] =
        EVP_MD_size(ssl_digest_methods[SSL_MD_MD5_IDX]);
    OPENSSL_assert(ssl_mac_secret_size[SSL_MD_MD5_IDX] >= 0);
    ssl_digest_methods[SSL_MD_SHA1_IDX] = EVP_get_digestbyname(SN_sha1);
    ssl_mac_secret_size[SSL_MD_SHA1_IDX] =
        EVP_MD_size(ssl_digest_methods[SSL_MD_SHA1_IDX]);
    OPENSSL_assert(ssl_mac_secret_size[SSL_MD_SHA1_IDX] >= 0);
    ssl_digest_methods[SSL_MD_GOST94_IDX] =
        EVP_get_digestbyname(SN_id_GostR3411_94);
    if (ssl_digest_methods[SSL_MD_GOST94_IDX]) {
        ssl_mac_secret_size[SSL_MD_GOST94_IDX] =
            EVP_MD_size(ssl_digest_methods[SSL_MD_GOST94_IDX]);
        OPENSSL_assert(ssl_mac_secret_size[SSL_MD_GOST94_IDX] >= 0);
    }
    ssl_digest_methods[SSL_MD_GOST89MAC_IDX] =
        EVP_get_digestbyname(SN_id_Gost28147_89_MAC);
    ssl_mac_pkey_id[SSL_MD_GOST89MAC_IDX] = get_optional_pkey_id("gost-mac");
    if (ssl_mac_pkey_id[SSL_MD_GOST89MAC_IDX]) {
        ssl_mac_secret_size[SSL_MD_GOST89MAC_IDX] = 32;
    }

    ssl_digest_methods[SSL_MD_SHA256_IDX] = EVP_get_digestbyname(SN_sha256);
    ssl_mac_secret_size[SSL_MD_SHA256_IDX] =
        EVP_MD_size(ssl_digest_methods[SSL_MD_SHA256_IDX]);
    ssl_digest_methods[SSL_MD_SHA384_IDX] = EVP_get_digestbyname(SN_sha384);
    ssl_mac_secret_size[SSL_MD_SHA384_IDX] =
        EVP_MD_size(ssl_digest_methods[SSL_MD_SHA384_IDX]);
}

#ifndef OPENSSL_NO_COMP

static int sk_comp_cmp(const SSL_COMP *const *a, const SSL_COMP *const *b)
{
    return ((*a)->id - (*b)->id);
}

static void load_builtin_compressions(void)
{
    int got_write_lock = 0;

    CRYPTO_r_lock(CRYPTO_LOCK_SSL);
    if (ssl_comp_methods == NULL) {
        CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
        CRYPTO_w_lock(CRYPTO_LOCK_SSL);
        got_write_lock = 1;

        if (ssl_comp_methods == NULL) {
            SSL_COMP *comp = NULL;

            MemCheck_off();
            ssl_comp_methods = sk_SSL_COMP_new(sk_comp_cmp);
            if (ssl_comp_methods != NULL) {
                comp = (SSL_COMP *)OPENSSL_malloc(sizeof(SSL_COMP));
                if (comp != NULL) {
                    comp->method = COMP_zlib();
                    if (comp->method && comp->method->type == NID_undef)
                        OPENSSL_free(comp);
                    else {
                        comp->id = SSL_COMP_ZLIB_IDX;
                        comp->name = comp->method->name;
                        sk_SSL_COMP_push(ssl_comp_methods, comp);
                    }
                }
                sk_SSL_COMP_sort(ssl_comp_methods);
            }
            MemCheck_on();
        }
    }

    if (got_write_lock)
        CRYPTO_w_unlock(CRYPTO_LOCK_SSL);
    else
        CRYPTO_r_unlock(CRYPTO_LOCK_SSL);
}
#endif

int ssl_cipher_get_evp(const SSL_SESSION *s, const EVP_CIPHER **enc,
                       const EVP_MD **md, int *mac_pkey_type,
                       int *mac_secret_size, SSL_COMP **comp)
{
    int i;
    const SSL_CIPHER *c;

    c = s->cipher;
    if (c == NULL)
        return (0);
    if (comp != NULL) {
        SSL_COMP ctmp;
#ifndef OPENSSL_NO_COMP
        load_builtin_compressions();
#endif

        *comp = NULL;
        ctmp.id = s->compress_meth;
        if (ssl_comp_methods != NULL) {
            i = sk_SSL_COMP_find(ssl_comp_methods, &ctmp);
            if (i >= 0)
                *comp = sk_SSL_COMP_value(ssl_comp_methods, i);
            else
                *comp = NULL;
        }
    }

    if ((enc == NULL) || (md == NULL))
        return (0);

    switch (c->algorithm_enc) {
    case SSL_DES:
        i = SSL_ENC_DES_IDX;
        break;
    case SSL_3DES:
        i = SSL_ENC_3DES_IDX;
        break;
    case SSL_RC4:
        i = SSL_ENC_RC4_IDX;
        break;
    case SSL_RC2:
        i = SSL_ENC_RC2_IDX;
        break;
    case SSL_IDEA:
        i = SSL_ENC_IDEA_IDX;
        break;
    case SSL_eNULL:
        i = SSL_ENC_NULL_IDX;
        break;
    case SSL_AES128:
        i = SSL_ENC_AES128_IDX;
        break;
    case SSL_AES256:
        i = SSL_ENC_AES256_IDX;
        break;
    case SSL_CAMELLIA128:
        i = SSL_ENC_CAMELLIA128_IDX;
        break;
    case SSL_CAMELLIA256:
        i = SSL_ENC_CAMELLIA256_IDX;
        break;
    case SSL_eGOST2814789CNT:
        i = SSL_ENC_GOST89_IDX;
        break;
    case SSL_SEED:
        i = SSL_ENC_SEED_IDX;
        break;
    case SSL_AES128GCM:
        i = SSL_ENC_AES128GCM_IDX;
        break;
    case SSL_AES256GCM:
        i = SSL_ENC_AES256GCM_IDX;
        break;
    default:
        i = -1;
        break;
    }

    if ((i < 0) || (i >= SSL_ENC_NUM_IDX))
        *enc = NULL;
    else {
        if (i == SSL_ENC_NULL_IDX)
            *enc = EVP_enc_null();
        else
            *enc = ssl_cipher_methods[i];
    }

    switch (c->algorithm_mac) {
    case SSL_MD5:
        i = SSL_MD_MD5_IDX;
        break;
    case SSL_SHA1:
        i = SSL_MD_SHA1_IDX;
        break;
    case SSL_SHA256:
        i = SSL_MD_SHA256_IDX;
        break;
    case SSL_SHA384:
        i = SSL_MD_SHA384_IDX;
        break;
    case SSL_GOST94:
        i = SSL_MD_GOST94_IDX;
        break;
    case SSL_GOST89MAC:
        i = SSL_MD_GOST89MAC_IDX;
        break;
    default:
        i = -1;
        break;
    }
    if ((i < 0) || (i >= SSL_MD_NUM_IDX)) {
        *md = NULL;
        if (mac_pkey_type != NULL)
            *mac_pkey_type = NID_undef;
        if (mac_secret_size != NULL)
            *mac_secret_size = 0;
        if (c->algorithm_mac == SSL_AEAD)
            mac_pkey_type = NULL;
    } else {
        *md = ssl_digest_methods[i];
        if (mac_pkey_type != NULL)
            *mac_pkey_type = ssl_mac_pkey_id[i];
        if (mac_secret_size != NULL)
            *mac_secret_size = ssl_mac_secret_size[i];
    }

    if ((*enc != NULL) &&
        (*md != NULL || (EVP_CIPHER_flags(*enc) & EVP_CIPH_FLAG_AEAD_CIPHER))
        && (!mac_pkey_type || *mac_pkey_type != NID_undef)) {
        const EVP_CIPHER *evp;

        if (s->ssl_version >> 8 != TLS1_VERSION_MAJOR ||
            s->ssl_version < TLS1_VERSION)
            return 1;

#ifdef OPENSSL_FIPS
        if (FIPS_mode())
            return 1;
#endif

        if (c->algorithm_enc == SSL_RC4 &&
            c->algorithm_mac == SSL_MD5 &&
            (evp = EVP_get_cipherbyname("RC4-HMAC-MD5")))
            *enc = evp, *md = NULL;
        else if (c->algorithm_enc == SSL_AES128 &&
                 c->algorithm_mac == SSL_SHA1 &&
                 (evp = EVP_get_cipherbyname("AES-128-CBC-HMAC-SHA1")))
            *enc = evp, *md = NULL;
        else if (c->algorithm_enc == SSL_AES256 &&
                 c->algorithm_mac == SSL_SHA1 &&
                 (evp = EVP_get_cipherbyname("AES-256-CBC-HMAC-SHA1")))
            *enc = evp, *md = NULL;
        else if (c->algorithm_enc == SSL_AES128 &&
                 c->algorithm_mac == SSL_SHA256 &&
                 (evp = EVP_get_cipherbyname("AES-128-CBC-HMAC-SHA256")))
            *enc = evp, *md = NULL;
        else if (c->algorithm_enc == SSL_AES256 &&
                 c->algorithm_mac == SSL_SHA256 &&
                 (evp = EVP_get_cipherbyname("AES-256-CBC-HMAC-SHA256")))
            *enc = evp, *md = NULL;
        return (1);
    } else
        return (0);
}

int ssl_get_handshake_digest(int idx, long *mask, const EVP_MD **md)
{
    if (idx < 0 || idx >= SSL_MD_NUM_IDX) {
        return 0;
    }
    *mask = ssl_handshake_digest_flag[idx];
    if (*mask)
        *md = ssl_digest_methods[idx];
    else
        *md = NULL;
    return 1;
}

#define ITEM_SEP(a) \
        (((a) == ':') || ((a) == ' ') || ((a) == ';') || ((a) == ','))

static void ll_append_tail(CIPHER_ORDER **head, CIPHER_ORDER *curr,
                           CIPHER_ORDER **tail)
{
    if (curr == *tail)
        return;
    if (curr == *head)
        *head = curr->next;
    if (curr->prev != NULL)
        curr->prev->next = curr->next;
    if (curr->next != NULL)
        curr->next->prev = curr->prev;
    (*tail)->next = curr;
    curr->prev = *tail;
    curr->next = NULL;
    *tail = curr;
}

static void ll_append_head(CIPHER_ORDER **head, CIPHER_ORDER *curr,
                           CIPHER_ORDER **tail)
{
    if (curr == *head)
        return;
    if (curr == *tail)
        *tail = curr->prev;
    if (curr->next != NULL)
        curr->next->prev = curr->prev;
    if (curr->prev != NULL)
        curr->prev->next = curr->next;
    (*head)->prev = curr;
    curr->next = *head;
    curr->prev = NULL;
    *head = curr;
}

static void ssl_cipher_get_disabled(unsigned long *mkey, unsigned long *auth,
                                    unsigned long *enc, unsigned long *mac,
                                    unsigned long *ssl)
{
    *mkey = 0;
    *auth = 0;
    *enc = 0;
    *mac = 0;
    *ssl = 0;

#ifdef OPENSSL_NO_RSA
    *mkey |= SSL_kRSA;
    *auth |= SSL_aRSA;
#endif
#ifdef OPENSSL_NO_DSA
    *auth |= SSL_aDSS;
#endif
#ifdef OPENSSL_NO_DH
    *mkey |= SSL_kDHr | SSL_kDHd | SSL_kEDH;
    *auth |= SSL_aDH;
#endif
#ifdef OPENSSL_NO_KRB5
    *mkey |= SSL_kKRB5;
    *auth |= SSL_aKRB5;
#endif
#ifdef OPENSSL_NO_ECDSA
    *auth |= SSL_aECDSA;
#endif
#ifdef OPENSSL_NO_ECDH
    *mkey |= SSL_kECDHe | SSL_kECDHr;
    *auth |= SSL_aECDH;
#endif
#ifdef OPENSSL_NO_PSK
    *mkey |= SSL_kPSK;
    *auth |= SSL_aPSK;
#endif
#ifdef OPENSSL_NO_SRP
    *mkey |= SSL_kSRP;
#endif
    /*
     * Check for presence of GOST 34.10 algorithms, and if they do not
     * present, disable appropriate auth and key exchange
     */
    if (!get_optional_pkey_id("gost94")) {
        *auth |= SSL_aGOST94;
    }
    if (!get_optional_pkey_id("gost2001")) {
        *auth |= SSL_aGOST01;
    }
    /*
     * Disable GOST key exchange if no GOST signature algs are available *
     */
    if ((*auth & (SSL_aGOST94 | SSL_aGOST01)) == (SSL_aGOST94 | SSL_aGOST01)) {
        *mkey |= SSL_kGOST;
    }
#ifdef SSL_FORBID_ENULL
    *enc |= SSL_eNULL;
#endif

    *enc |= (ssl_cipher_methods[SSL_ENC_DES_IDX] == NULL) ? SSL_DES : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_3DES_IDX] == NULL) ? SSL_3DES : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_RC4_IDX] == NULL) ? SSL_RC4 : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_RC2_IDX] == NULL) ? SSL_RC2 : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_IDEA_IDX] == NULL) ? SSL_IDEA : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_AES128_IDX] == NULL) ? SSL_AES128 : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_AES256_IDX] == NULL) ? SSL_AES256 : 0;
    *enc |=
        (ssl_cipher_methods[SSL_ENC_AES128GCM_IDX] ==
         NULL) ? SSL_AES128GCM : 0;
    *enc |=
        (ssl_cipher_methods[SSL_ENC_AES256GCM_IDX] ==
         NULL) ? SSL_AES256GCM : 0;
    *enc |=
        (ssl_cipher_methods[SSL_ENC_CAMELLIA128_IDX] ==
         NULL) ? SSL_CAMELLIA128 : 0;
    *enc |=
        (ssl_cipher_methods[SSL_ENC_CAMELLIA256_IDX] ==
         NULL) ? SSL_CAMELLIA256 : 0;
    *enc |=
        (ssl_cipher_methods[SSL_ENC_GOST89_IDX] ==
         NULL) ? SSL_eGOST2814789CNT : 0;
    *enc |= (ssl_cipher_methods[SSL_ENC_SEED_IDX] == NULL) ? SSL_SEED : 0;

    *mac |= (ssl_digest_methods[SSL_MD_MD5_IDX] == NULL) ? SSL_MD5 : 0;
    *mac |= (ssl_digest_methods[SSL_MD_SHA1_IDX] == NULL) ? SSL_SHA1 : 0;
    *mac |= (ssl_digest_methods[SSL_MD_SHA256_IDX] == NULL) ? SSL_SHA256 : 0;
    *mac |= (ssl_digest_methods[SSL_MD_SHA384_IDX] == NULL) ? SSL_SHA384 : 0;
    *mac |= (ssl_digest_methods[SSL_MD_GOST94_IDX] == NULL) ? SSL_GOST94 : 0;
    *mac |= (ssl_digest_methods[SSL_MD_GOST89MAC_IDX] == NULL
             || ssl_mac_pkey_id[SSL_MD_GOST89MAC_IDX] ==
             NID_undef) ? SSL_GOST89MAC : 0;

}

static void ssl_cipher_collect_ciphers(const SSL_METHOD *ssl_method,
                                       int num_of_ciphers,
                                       unsigned long disabled_mkey,
                                       unsigned long disabled_auth,
                                       unsigned long disabled_enc,
                                       unsigned long disabled_mac,
                                       unsigned long disabled_ssl,
                                       CIPHER_ORDER *co_list,
                                       CIPHER_ORDER **head_p,
                                       CIPHER_ORDER **tail_p)
{
    int i, co_list_num;
    const SSL_CIPHER *c;

    /*
     * We have num_of_ciphers descriptions compiled in, depending on the
     * method selected (SSLv2 and/or SSLv3, TLSv1 etc).
     * These will later be sorted in a linked list with at most num
     * entries.
     */

    /* Get the initial list of ciphers */
    co_list_num = 0;            /* actual count of ciphers */
    for (i = 0; i < num_of_ciphers; i++) {
        c = ssl_method->get_cipher(i);
        /* drop those that use any of that is not available */
        if ((c != NULL) && c->valid &&
#ifdef OPENSSL_FIPS
            (!FIPS_mode() || (c->algo_strength & SSL_FIPS)) &&
#endif
            !(c->algorithm_mkey & disabled_mkey) &&
            !(c->algorithm_auth & disabled_auth) &&
            !(c->algorithm_enc & disabled_enc) &&
            !(c->algorithm_mac & disabled_mac) &&
            !(c->algorithm_ssl & disabled_ssl)) {
            co_list[co_list_num].cipher = c;
            co_list[co_list_num].next = NULL;
            co_list[co_list_num].prev = NULL;
            co_list[co_list_num].active = 0;
            co_list_num++;
#ifdef KSSL_DEBUG
            fprintf(stderr, "\t%d: %s %lx %lx %lx\n", i, c->name, c->id,
                    c->algorithm_mkey, c->algorithm_auth);
#endif                          /* KSSL_DEBUG */
            /*
             * if (!sk_push(ca_list,(char *)c)) goto err;
             */
        }
    }

    /*
     * Prepare linked list from list entries
     */
    if (co_list_num > 0) {
        co_list[0].prev = NULL;

        if (co_list_num > 1) {
            co_list[0].next = &co_list[1];

            for (i = 1; i < co_list_num - 1; i++) {
                co_list[i].prev = &co_list[i - 1];
                co_list[i].next = &co_list[i + 1];
            }

            co_list[co_list_num - 1].prev = &co_list[co_list_num - 2];
        }

        co_list[co_list_num - 1].next = NULL;

        *head_p = &co_list[0];
        *tail_p = &co_list[co_list_num - 1];
    }
}

static void ssl_cipher_collect_aliases(const SSL_CIPHER **ca_list,
                                       int num_of_group_aliases,
                                       unsigned long disabled_mkey,
                                       unsigned long disabled_auth,
                                       unsigned long disabled_enc,
                                       unsigned long disabled_mac,
                                       unsigned long disabled_ssl,
                                       CIPHER_ORDER *head)
{
    CIPHER_ORDER *ciph_curr;
    const SSL_CIPHER **ca_curr;
    int i;
    unsigned long mask_mkey = ~disabled_mkey;
    unsigned long mask_auth = ~disabled_auth;
    unsigned long mask_enc = ~disabled_enc;
    unsigned long mask_mac = ~disabled_mac;
    unsigned long mask_ssl = ~disabled_ssl;

    /*
     * First, add the real ciphers as already collected
     */
    ciph_curr = head;
    ca_curr = ca_list;
    while (ciph_curr != NULL) {
        *ca_curr = ciph_curr->cipher;
        ca_curr++;
        ciph_curr = ciph_curr->next;
    }

    /*
     * Now we add the available ones from the cipher_aliases[] table.
     * They represent either one or more algorithms, some of which
     * in any affected category must be supported (set in enabled_mask),
     * or represent a cipher strength value (will be added in any case because algorithms=0).
     */
    for (i = 0; i < num_of_group_aliases; i++) {
        unsigned long algorithm_mkey = cipher_aliases[i].algorithm_mkey;
        unsigned long algorithm_auth = cipher_aliases[i].algorithm_auth;
        unsigned long algorithm_enc = cipher_aliases[i].algorithm_enc;
        unsigned long algorithm_mac = cipher_aliases[i].algorithm_mac;
        unsigned long algorithm_ssl = cipher_aliases[i].algorithm_ssl;

        if (algorithm_mkey)
            if ((algorithm_mkey & mask_mkey) == 0)
                continue;

        if (algorithm_auth)
            if ((algorithm_auth & mask_auth) == 0)
                continue;

        if (algorithm_enc)
            if ((algorithm_enc & mask_enc) == 0)
                continue;

        if (algorithm_mac)
            if ((algorithm_mac & mask_mac) == 0)
                continue;

        if (algorithm_ssl)
            if ((algorithm_ssl & mask_ssl) == 0)
                continue;

        *ca_curr = (SSL_CIPHER *)(cipher_aliases + i);
        ca_curr++;
    }

    *ca_curr = NULL;            /* end of list */
}

static void ssl_cipher_apply_rule(unsigned long cipher_id,
                                  unsigned long alg_mkey,
                                  unsigned long alg_auth,
                                  unsigned long alg_enc,
                                  unsigned long alg_mac,
                                  unsigned long alg_ssl,
                                  unsigned long algo_strength, int rule,
                                  int strength_bits, CIPHER_ORDER **head_p,
                                  CIPHER_ORDER **tail_p)
{
    CIPHER_ORDER *head, *tail, *curr, *next, *last;
    const SSL_CIPHER *cp;
    int reverse = 0;

#ifdef CIPHER_DEBUG
    fprintf(stderr,
            "Applying rule %d with %08lx/%08lx/%08lx/%08lx/%08lx %08lx (%d)\n",
            rule, alg_mkey, alg_auth, alg_enc, alg_mac, alg_ssl,
            algo_strength, strength_bits);
#endif

    if (rule == CIPHER_DEL)
        reverse = 1;            /* needed to maintain sorting between
                                 * currently deleted ciphers */

    head = *head_p;
    tail = *tail_p;

    if (reverse) {
        next = tail;
        last = head;
    } else {
        next = head;
        last = tail;
    }

    curr = NULL;
    for (;;) {
        if (curr == last)
            break;

        curr = next;

        if (curr == NULL)
            break;

        next = reverse ? curr->prev : curr->next;

        cp = curr->cipher;

        /*
         * Selection criteria is either the value of strength_bits
         * or the algorithms used.
         */
        if (strength_bits >= 0) {
            if (strength_bits != cp->strength_bits)
                continue;
        } else {
#ifdef CIPHER_DEBUG
            fprintf(stderr,
                    "\nName: %s:\nAlgo = %08lx/%08lx/%08lx/%08lx/%08lx Algo_strength = %08lx\n",
                    cp->name, cp->algorithm_mkey, cp->algorithm_auth,
                    cp->algorithm_enc, cp->algorithm_mac, cp->algorithm_ssl,
                    cp->algo_strength);
#endif
#ifdef OPENSSL_SSL_DEBUG_BROKEN_PROTOCOL
            if (cipher_id && cipher_id != cp->id)
                continue;
#endif
            if (algo_strength == SSL_EXP_MASK && SSL_C_IS_EXPORT(cp))
                goto ok;
            if (alg_ssl == ~SSL_SSLV2 && cp->algorithm_ssl == SSL_SSLV2)
                goto ok;
            if (alg_mkey && !(alg_mkey & cp->algorithm_mkey))
                continue;
            if (alg_auth && !(alg_auth & cp->algorithm_auth))
                continue;
            if (alg_enc && !(alg_enc & cp->algorithm_enc))
                continue;
            if (alg_mac && !(alg_mac & cp->algorithm_mac))
                continue;
            if (alg_ssl && !(alg_ssl & cp->algorithm_ssl))
                continue;
            if ((algo_strength & SSL_EXP_MASK)
                && !(algo_strength & SSL_EXP_MASK & cp->algo_strength))
                continue;
            if ((algo_strength & SSL_STRONG_MASK)
                && !(algo_strength & SSL_STRONG_MASK & cp->algo_strength))
                continue;
        }

    ok:

#ifdef CIPHER_DEBUG
        fprintf(stderr, "Action = %d\n", rule);
#endif

        /* add the cipher if it has not been added yet. */
        if (rule == CIPHER_ADD) {
            /* reverse == 0 */
            if (!curr->active) {
                ll_append_tail(&head, curr, &tail);
                curr->active = 1;
            }
        }
        /* Move the added cipher to this location */
        else if (rule == CIPHER_ORD) {
            /* reverse == 0 */
            if (curr->active) {
                ll_append_tail(&head, curr, &tail);
            }
        } else if (rule == CIPHER_DEL) {
            /* reverse == 1 */
            if (curr->active) {
                /*
                 * most recently deleted ciphersuites get best positions for
                 * any future CIPHER_ADD (note that the CIPHER_DEL loop works
                 * in reverse to maintain the order)
                 */
                ll_append_head(&head, curr, &tail);
                curr->active = 0;
            }
        } else if (rule == CIPHER_KILL) {
            /* reverse == 0 */
            if (head == curr)
                head = curr->next;
            else
                curr->prev->next = curr->next;
            if (tail == curr)
                tail = curr->prev;
            curr->active = 0;
            if (curr->next != NULL)
                curr->next->prev = curr->prev;
            if (curr->prev != NULL)
                curr->prev->next = curr->next;
            curr->next = NULL;
            curr->prev = NULL;
        }
    }

    *head_p = head;
    *tail_p = tail;
}

static int ssl_cipher_strength_sort(CIPHER_ORDER **head_p,
                                    CIPHER_ORDER **tail_p)
{
    int max_strength_bits, i, *number_uses;
    CIPHER_ORDER *curr;

    /*
     * This routine sorts the ciphers with descending strength. The sorting
     * must keep the pre-sorted sequence, so we apply the normal sorting
     * routine as '+' movement to the end of the list.
     */
    max_strength_bits = 0;
    curr = *head_p;
    while (curr != NULL) {
        if (curr->active && (curr->cipher->strength_bits > max_strength_bits))
            max_strength_bits = curr->cipher->strength_bits;
        curr = curr->next;
    }

    number_uses = OPENSSL_malloc((max_strength_bits + 1) * sizeof(int));
    if (!number_uses) {
        SSLerr(SSL_F_SSL_CIPHER_STRENGTH_SORT, ERR_R_MALLOC_FAILURE);
        return (0);
    }
    memset(number_uses, 0, (max_strength_bits + 1) * sizeof(int));

    /*
     * Now find the strength_bits values actually used
     */
    curr = *head_p;
    while (curr != NULL) {
        if (curr->active)
            number_uses[curr->cipher->strength_bits]++;
        curr = curr->next;
    }
    /*
     * Go through the list of used strength_bits values in descending
     * order.
     */
    for (i = max_strength_bits; i >= 0; i--)
        if (number_uses[i] > 0)
            ssl_cipher_apply_rule(0, 0, 0, 0, 0, 0, 0, CIPHER_ORD, i, head_p,
                                  tail_p);

    OPENSSL_free(number_uses);
    return (1);
}

static int ssl_cipher_process_rulestr(const char *rule_str,
                                      CIPHER_ORDER **head_p,
                                      CIPHER_ORDER **tail_p,
                                      const SSL_CIPHER **ca_list)
{
    unsigned long alg_mkey, alg_auth, alg_enc, alg_mac, alg_ssl,
        algo_strength;
    const char *l, *buf;
    int j, multi, found, rule, retval, ok, buflen;
    unsigned long cipher_id = 0;
    char ch;

    retval = 1;
    l = rule_str;
    for (;;) {
        ch = *l;

        if (ch == '\0')
            break;              /* done */
        if (ch == '-') {
            rule = CIPHER_DEL;
            l++;
        } else if (ch == '+') {
            rule = CIPHER_ORD;
            l++;
        } else if (ch == '!') {
            rule = CIPHER_KILL;
            l++;
        } else if (ch == '@') {
            rule = CIPHER_SPECIAL;
            l++;
        } else {
            rule = CIPHER_ADD;
        }

        if (ITEM_SEP(ch)) {
            l++;
            continue;
        }

        alg_mkey = 0;
        alg_auth = 0;
        alg_enc = 0;
        alg_mac = 0;
        alg_ssl = 0;
        algo_strength = 0;

        for (;;) {
            ch = *l;
            buf = l;
            buflen = 0;
#ifndef CHARSET_EBCDIC
            while (((ch >= 'A') && (ch <= 'Z')) ||
                   ((ch >= '0') && (ch <= '9')) ||
                   ((ch >= 'a') && (ch <= 'z')) || (ch == '-') || (ch == '.'))
#else
            while (isalnum(ch) || (ch == '-') || (ch == '.'))
#endif
            {
                ch = *(++l);
                buflen++;
            }

            if (buflen == 0) {
                /*
                 * We hit something we cannot deal with,
                 * it is no command or separator nor
                 * alphanumeric, so we call this an error.
                 */
                SSLerr(SSL_F_SSL_CIPHER_PROCESS_RULESTR,
                       SSL_R_INVALID_COMMAND);
                retval = found = 0;
                l++;
                break;
            }

            if (rule == CIPHER_SPECIAL) {
                found = 0;      /* unused -- avoid compiler warning */
                break;          /* special treatment */
            }

            /* check for multi-part specification */
            if (ch == '+') {
                multi = 1;
                l++;
            } else
                multi = 0;

            /*
             * Now search for the cipher alias in the ca_list. Be careful
             * with the strncmp, because the "buflen" limitation
             * will make the rule "ADH:SOME" and the cipher
             * "ADH-MY-CIPHER" look like a match for buflen=3.
             * So additionally check whether the cipher name found
             * has the correct length. We can save a strlen() call:
             * just checking for the '\0' at the right place is
             * sufficient, we have to strncmp() anyway. (We cannot
             * use strcmp(), because buf is not '\0' terminated.)
             */
            j = found = 0;
            cipher_id = 0;
            while (ca_list[j]) {
                if (!strncmp(buf, ca_list[j]->name, buflen) &&
                    (ca_list[j]->name[buflen] == '\0')) {
                    found = 1;
                    break;
                } else
                    j++;
            }

            if (!found)
                break;          /* ignore this entry */

            if (ca_list[j]->algorithm_mkey) {
                if (alg_mkey) {
                    alg_mkey &= ca_list[j]->algorithm_mkey;
                    if (!alg_mkey) {
                        found = 0;
                        break;
                    }
                } else
                    alg_mkey = ca_list[j]->algorithm_mkey;
            }

            if (ca_list[j]->algorithm_auth) {
                if (alg_auth) {
                    alg_auth &= ca_list[j]->algorithm_auth;
                    if (!alg_auth) {
                        found = 0;
                        break;
                    }
                } else
                    alg_auth = ca_list[j]->algorithm_auth;
            }

            if (ca_list[j]->algorithm_enc) {
                if (alg_enc) {
                    alg_enc &= ca_list[j]->algorithm_enc;
                    if (!alg_enc) {
                        found = 0;
                        break;
                    }
                } else
                    alg_enc = ca_list[j]->algorithm_enc;
            }

            if (ca_list[j]->algorithm_mac) {
                if (alg_mac) {
                    alg_mac &= ca_list[j]->algorithm_mac;
                    if (!alg_mac) {
                        found = 0;
                        break;
                    }
                } else
                    alg_mac = ca_list[j]->algorithm_mac;
            }

            if (ca_list[j]->algo_strength & SSL_EXP_MASK) {
                if (algo_strength & SSL_EXP_MASK) {
                    algo_strength &=
                        (ca_list[j]->algo_strength & SSL_EXP_MASK) |
                        ~SSL_EXP_MASK;
                    if (!(algo_strength & SSL_EXP_MASK)) {
                        found = 0;
                        break;
                    }
                } else
                    algo_strength |= ca_list[j]->algo_strength & SSL_EXP_MASK;
            }

            if (ca_list[j]->algo_strength & SSL_STRONG_MASK) {
                if (algo_strength & SSL_STRONG_MASK) {
                    algo_strength &=
                        (ca_list[j]->algo_strength & SSL_STRONG_MASK) |
                        ~SSL_STRONG_MASK;
                    if (!(algo_strength & SSL_STRONG_MASK)) {
                        found = 0;
                        break;
                    }
                } else
                    algo_strength |=
                        ca_list[j]->algo_strength & SSL_STRONG_MASK;
            }

            if (ca_list[j]->valid) {
                /*
                 * explicit ciphersuite found; its protocol version does not
                 * become part of the search pattern!
                 */

                cipher_id = ca_list[j]->id;
            } else {
                /*
                 * not an explicit ciphersuite; only in this case, the
                 * protocol version is considered part of the search pattern
                 */

                if (ca_list[j]->algorithm_ssl) {
                    if (alg_ssl) {
                        alg_ssl &= ca_list[j]->algorithm_ssl;
                        if (!alg_ssl) {
                            found = 0;
                            break;
                        }
                    } else
                        alg_ssl = ca_list[j]->algorithm_ssl;
                }
            }

            if (!multi)
                break;
        }

        /*
         * Ok, we have the rule, now apply it
         */
        if (rule == CIPHER_SPECIAL) { /* special command */
            ok = 0;
            if ((buflen == 8) && !strncmp(buf, "STRENGTH", 8))
                ok = ssl_cipher_strength_sort(head_p, tail_p);
            else
                SSLerr(SSL_F_SSL_CIPHER_PROCESS_RULESTR,
                       SSL_R_INVALID_COMMAND);
            if (ok == 0)
                retval = 0;
            /*
             * We do not support any "multi" options
             * together with "@", so throw away the
             * rest of the command, if any left, until
             * end or ':' is found.
             */
            while ((*l != '\0') && !ITEM_SEP(*l))
                l++;
        } else if (found) {
            ssl_cipher_apply_rule(cipher_id,
                                  alg_mkey, alg_auth, alg_enc, alg_mac,
                                  alg_ssl, algo_strength, rule, -1, head_p,
                                  tail_p);
        } else {
            while ((*l != '\0') && !ITEM_SEP(*l))
                l++;
        }
        if (*l == '\0')
            break;              /* done */
    }

    return (retval);
}

#ifndef OPENSSL_NO_EC
static int check_suiteb_cipher_list(const SSL_METHOD *meth, CERT *c,
                                    const char **prule_str)
{
    unsigned int suiteb_flags = 0, suiteb_comb2 = 0;
    if (!strcmp(*prule_str, "SUITEB128"))
        suiteb_flags = SSL_CERT_FLAG_SUITEB_128_LOS;
    else if (!strcmp(*prule_str, "SUITEB128ONLY"))
        suiteb_flags = SSL_CERT_FLAG_SUITEB_128_LOS_ONLY;
    else if (!strcmp(*prule_str, "SUITEB128C2")) {
        suiteb_comb2 = 1;
        suiteb_flags = SSL_CERT_FLAG_SUITEB_128_LOS;
    } else if (!strcmp(*prule_str, "SUITEB192"))
        suiteb_flags = SSL_CERT_FLAG_SUITEB_192_LOS;

    if (suiteb_flags) {
        c->cert_flags &= ~SSL_CERT_FLAG_SUITEB_128_LOS;
        c->cert_flags |= suiteb_flags;
    } else
        suiteb_flags = c->cert_flags & SSL_CERT_FLAG_SUITEB_128_LOS;

    if (!suiteb_flags)
        return 1;
    /* Check version: if TLS 1.2 ciphers allowed we can use Suite B */

    if (!(meth->ssl3_enc->enc_flags & SSL_ENC_FLAG_TLS1_2_CIPHERS)) {
        if (meth->ssl3_enc->enc_flags & SSL_ENC_FLAG_DTLS)
            SSLerr(SSL_F_CHECK_SUITEB_CIPHER_LIST,
                   SSL_R_ONLY_DTLS_1_2_ALLOWED_IN_SUITEB_MODE);
        else
            SSLerr(SSL_F_CHECK_SUITEB_CIPHER_LIST,
                   SSL_R_ONLY_TLS_1_2_ALLOWED_IN_SUITEB_MODE);
        return 0;
    }
# ifndef OPENSSL_NO_ECDH
    switch (suiteb_flags) {
    case SSL_CERT_FLAG_SUITEB_128_LOS:
        if (suiteb_comb2)
            *prule_str = "ECDHE-ECDSA-AES256-GCM-SHA384";
        else
            *prule_str =
                "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384";
        break;
    case SSL_CERT_FLAG_SUITEB_128_LOS_ONLY:
        *prule_str = "ECDHE-ECDSA-AES128-GCM-SHA256";
        break;
    case SSL_CERT_FLAG_SUITEB_192_LOS:
        *prule_str = "ECDHE-ECDSA-AES256-GCM-SHA384";
        break;
    }
    /* Set auto ECDH parameter determination */
    c->ecdh_tmp_auto = 1;
    return 1;
# else
    SSLerr(SSL_F_CHECK_SUITEB_CIPHER_LIST,
           SSL_R_ECDH_REQUIRED_FOR_SUITEB_MODE);
    return 0;
# endif
}
#endif

STACK_OF(SSL_CIPHER) *ssl_create_cipher_list(const SSL_METHOD *ssl_method, STACK_OF(SSL_CIPHER)
                                             **cipher_list, STACK_OF(SSL_CIPHER)
                                             **cipher_list_by_id,
                                             const char *rule_str, CERT *c)
{
    int ok, num_of_ciphers, num_of_alias_max, num_of_group_aliases;
    unsigned long disabled_mkey, disabled_auth, disabled_enc, disabled_mac,
        disabled_ssl;
    STACK_OF(SSL_CIPHER) *cipherstack, *tmp_cipher_list;
    const char *rule_p;
    CIPHER_ORDER *co_list = NULL, *head = NULL, *tail = NULL, *curr;
    const SSL_CIPHER **ca_list = NULL;

    /*
     * Return with error if nothing to do.
     */
    if (rule_str == NULL || cipher_list == NULL || cipher_list_by_id == NULL)
        return NULL;
#ifndef OPENSSL_NO_EC
    if (!check_suiteb_cipher_list(ssl_method, c, &rule_str))
        return NULL;
#endif

    /*
     * To reduce the work to do we only want to process the compiled
     * in algorithms, so we first get the mask of disabled ciphers.
     */
    ssl_cipher_get_disabled(&disabled_mkey, &disabled_auth, &disabled_enc,
                            &disabled_mac, &disabled_ssl);

    /*
     * Now we have to collect the available ciphers from the compiled
     * in ciphers. We cannot get more than the number compiled in, so
     * it is used for allocation.
     */
    num_of_ciphers = ssl_method->num_ciphers();
#ifdef KSSL_DEBUG
    fprintf(stderr, "ssl_create_cipher_list() for %d ciphers\n",
            num_of_ciphers);
#endif                          /* KSSL_DEBUG */
    co_list =
        (CIPHER_ORDER *)OPENSSL_malloc(sizeof(CIPHER_ORDER) * num_of_ciphers);
    if (co_list == NULL) {
        SSLerr(SSL_F_SSL_CREATE_CIPHER_LIST, ERR_R_MALLOC_FAILURE);
        return (NULL);          /* Failure */
    }

    ssl_cipher_collect_ciphers(ssl_method, num_of_ciphers,
                               disabled_mkey, disabled_auth, disabled_enc,
                               disabled_mac, disabled_ssl, co_list, &head,
                               &tail);

    /* Now arrange all ciphers by preference: */

    /*
     * Everything else being equal, prefer ephemeral ECDH over other key
     * exchange mechanisms
     */
    ssl_cipher_apply_rule(0, SSL_kEECDH, 0, 0, 0, 0, 0, CIPHER_ADD, -1, &head,
                          &tail);
    ssl_cipher_apply_rule(0, SSL_kEECDH, 0, 0, 0, 0, 0, CIPHER_DEL, -1, &head,
                          &tail);

    /* AES is our preferred symmetric cipher */
    ssl_cipher_apply_rule(0, 0, 0, SSL_AES, 0, 0, 0, CIPHER_ADD, -1, &head,
                          &tail);

    /* Temporarily enable everything else for sorting */
    ssl_cipher_apply_rule(0, 0, 0, 0, 0, 0, 0, CIPHER_ADD, -1, &head, &tail);

    /* Low priority for MD5 */
    ssl_cipher_apply_rule(0, 0, 0, 0, SSL_MD5, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);

    /*
     * Move anonymous ciphers to the end.  Usually, these will remain
     * disabled. (For applications that allow them, they aren't too bad, but
     * we prefer authenticated ciphers.)
     */
    ssl_cipher_apply_rule(0, 0, SSL_aNULL, 0, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);

    /* Move ciphers without forward secrecy to the end */
    ssl_cipher_apply_rule(0, 0, SSL_aECDH, 0, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);
    /*
     * ssl_cipher_apply_rule(0, 0, SSL_aDH, 0, 0, 0, 0, CIPHER_ORD, -1,
     * &head, &tail);
     */
    ssl_cipher_apply_rule(0, SSL_kRSA, 0, 0, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);
    ssl_cipher_apply_rule(0, SSL_kPSK, 0, 0, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);
    ssl_cipher_apply_rule(0, SSL_kKRB5, 0, 0, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);

    /* RC4 is sort-of broken -- move the the end */
    ssl_cipher_apply_rule(0, 0, 0, SSL_RC4, 0, 0, 0, CIPHER_ORD, -1, &head,
                          &tail);

    /*
     * Now sort by symmetric encryption strength.  The above ordering remains
     * in force within each class
     */
    if (!ssl_cipher_strength_sort(&head, &tail)) {
        OPENSSL_free(co_list);
        return NULL;
    }

    /* Now disable everything (maintaining the ordering!) */
    ssl_cipher_apply_rule(0, 0, 0, 0, 0, 0, 0, CIPHER_DEL, -1, &head, &tail);

    /*
     * We also need cipher aliases for selecting based on the rule_str.
     * There might be two types of entries in the rule_str: 1) names
     * of ciphers themselves 2) aliases for groups of ciphers.
     * For 1) we need the available ciphers and for 2) the cipher
     * groups of cipher_aliases added together in one list (otherwise
     * we would be happy with just the cipher_aliases table).
     */
    num_of_group_aliases = sizeof(cipher_aliases) / sizeof(SSL_CIPHER);
    num_of_alias_max = num_of_ciphers + num_of_group_aliases + 1;
    ca_list = OPENSSL_malloc(sizeof(SSL_CIPHER *) * num_of_alias_max);
    if (ca_list == NULL) {
        OPENSSL_free(co_list);
        SSLerr(SSL_F_SSL_CREATE_CIPHER_LIST, ERR_R_MALLOC_FAILURE);
        return (NULL);          /* Failure */
    }
    ssl_cipher_collect_aliases(ca_list, num_of_group_aliases,
                               disabled_mkey, disabled_auth, disabled_enc,
                               disabled_mac, disabled_ssl, head);

    /*
     * If the rule_string begins with DEFAULT, apply the default rule
     * before using the (possibly available) additional rules.
     */
    ok = 1;
    rule_p = rule_str;
    if (strncmp(rule_str, "DEFAULT", 7) == 0) {
        ok = ssl_cipher_process_rulestr(SSL_DEFAULT_CIPHER_LIST,
                                        &head, &tail, ca_list);
        rule_p += 7;
        if (*rule_p == ':')
            rule_p++;
    }

    if (ok && (strlen(rule_p) > 0))
        ok = ssl_cipher_process_rulestr(rule_p, &head, &tail, ca_list);

    OPENSSL_free((void *)ca_list); /* Not needed anymore */

    if (!ok) {                  /* Rule processing failure */
        OPENSSL_free(co_list);
        return (NULL);
    }

    /*
     * Allocate new "cipherstack" for the result, return with error
     * if we cannot get one.
     */
    if ((cipherstack = sk_SSL_CIPHER_new_null()) == NULL) {
        OPENSSL_free(co_list);
        return (NULL);
    }

    /*
     * The cipher selection for the list is done. The ciphers are added
     * to the resulting precedence to the STACK_OF(SSL_CIPHER).
     */
    for (curr = head; curr != NULL; curr = curr->next) {
#ifdef OPENSSL_FIPS
        if (curr->active
            && (!FIPS_mode() || curr->cipher->algo_strength & SSL_FIPS))
#else
        if (curr->active)
#endif
        {
            sk_SSL_CIPHER_push(cipherstack, curr->cipher);
#ifdef CIPHER_DEBUG
            fprintf(stderr, "<%s>\n", curr->cipher->name);
#endif
        }
    }
    OPENSSL_free(co_list);      /* Not needed any longer */

    tmp_cipher_list = sk_SSL_CIPHER_dup(cipherstack);
    if (tmp_cipher_list == NULL) {
        sk_SSL_CIPHER_free(cipherstack);
        return NULL;
    }
    if (*cipher_list != NULL)
        sk_SSL_CIPHER_free(*cipher_list);
    *cipher_list = cipherstack;
    if (*cipher_list_by_id != NULL)
        sk_SSL_CIPHER_free(*cipher_list_by_id);
    *cipher_list_by_id = tmp_cipher_list;
    (void)sk_SSL_CIPHER_set_cmp_func(*cipher_list_by_id,
                                     ssl_cipher_ptr_id_cmp);

    sk_SSL_CIPHER_sort(*cipher_list_by_id);
    return (cipherstack);
}

char *SSL_CIPHER_description(const SSL_CIPHER *cipher, char *buf, int len)
{
    int is_export, pkl, kl;
    const char *ver, *exp_str;
    const char *kx, *au, *enc, *mac;
    unsigned long alg_mkey, alg_auth, alg_enc, alg_mac, alg_ssl, alg2;
#ifdef KSSL_DEBUG
    static const char *format =
        "%-23s %s Kx=%-8s Au=%-4s Enc=%-9s Mac=%-4s%s AL=%lx/%lx/%lx/%lx/%lx\n";
#else
    static const char *format =
        "%-23s %s Kx=%-8s Au=%-4s Enc=%-9s Mac=%-4s%s\n";
#endif                          /* KSSL_DEBUG */

    alg_mkey = cipher->algorithm_mkey;
    alg_auth = cipher->algorithm_auth;
    alg_enc = cipher->algorithm_enc;
    alg_mac = cipher->algorithm_mac;
    alg_ssl = cipher->algorithm_ssl;

    alg2 = cipher->algorithm2;

    is_export = SSL_C_IS_EXPORT(cipher);
    pkl = SSL_C_EXPORT_PKEYLENGTH(cipher);
    kl = SSL_C_EXPORT_KEYLENGTH(cipher);
    exp_str = is_export ? " export" : "";

    if (alg_ssl & SSL_SSLV2)
        ver = "SSLv2";
    else if (alg_ssl & SSL_SSLV3)
        ver = "SSLv3";
    else if (alg_ssl & SSL_TLSV1_2)
        ver = "TLSv1.2";
    else
        ver = "unknown";

    switch (alg_mkey) {
    case SSL_kRSA:
        kx = is_export ? (pkl == 512 ? "RSA(512)" : "RSA(1024)") : "RSA";
        break;
    case SSL_kDHr:
        kx = "DH/RSA";
        break;
    case SSL_kDHd:
        kx = "DH/DSS";
        break;
    case SSL_kKRB5:
        kx = "KRB5";
        break;
    case SSL_kEDH:
        kx = is_export ? (pkl == 512 ? "DH(512)" : "DH(1024)") : "DH";
        break;
    case SSL_kECDHr:
        kx = "ECDH/RSA";
        break;
    case SSL_kECDHe:
        kx = "ECDH/ECDSA";
        break;
    case SSL_kEECDH:
        kx = "ECDH";
        break;
    case SSL_kPSK:
        kx = "PSK";
        break;
    case SSL_kSRP:
        kx = "SRP";
        break;
    case SSL_kGOST:
        kx = "GOST";
        break;
    default:
        kx = "unknown";
    }

    switch (alg_auth) {
    case SSL_aRSA:
        au = "RSA";
        break;
    case SSL_aDSS:
        au = "DSS";
        break;
    case SSL_aDH:
        au = "DH";
        break;
    case SSL_aKRB5:
        au = "KRB5";
        break;
    case SSL_aECDH:
        au = "ECDH";
        break;
    case SSL_aNULL:
        au = "None";
        break;
    case SSL_aECDSA:
        au = "ECDSA";
        break;
    case SSL_aPSK:
        au = "PSK";
        break;
    case SSL_aSRP:
        au = "SRP";
        break;
    case SSL_aGOST94:
        au = "GOST94";
        break;
    case SSL_aGOST01:
        au = "GOST01";
        break;
    default:
        au = "unknown";
        break;
    }

    switch (alg_enc) {
    case SSL_DES:
        enc = (is_export && kl == 5) ? "DES(40)" : "DES(56)";
        break;
    case SSL_3DES:
        enc = "3DES(168)";
        break;
    case SSL_RC4:
        enc = is_export ? (kl == 5 ? "RC4(40)" : "RC4(56)")
            : ((alg2 & SSL2_CF_8_BYTE_ENC) ? "RC4(64)" : "RC4(128)");
        break;
    case SSL_RC2:
        enc = is_export ? (kl == 5 ? "RC2(40)" : "RC2(56)") : "RC2(128)";
        break;
    case SSL_IDEA:
        enc = "IDEA(128)";
        break;
    case SSL_eNULL:
        enc = "None";
        break;
    case SSL_AES128:
        enc = "AES(128)";
        break;
    case SSL_AES256:
        enc = "AES(256)";
        break;
    case SSL_AES128GCM:
        enc = "AESGCM(128)";
        break;
    case SSL_AES256GCM:
        enc = "AESGCM(256)";
        break;
    case SSL_CAMELLIA128:
        enc = "Camellia(128)";
        break;
    case SSL_CAMELLIA256:
        enc = "Camellia(256)";
        break;
    case SSL_SEED:
        enc = "SEED(128)";
        break;
    case SSL_eGOST2814789CNT:
        enc = "GOST89(256)";
        break;
    default:
        enc = "unknown";
        break;
    }

    switch (alg_mac) {
    case SSL_MD5:
        mac = "MD5";
        break;
    case SSL_SHA1:
        mac = "SHA1";
        break;
    case SSL_SHA256:
        mac = "SHA256";
        break;
    case SSL_SHA384:
        mac = "SHA384";
        break;
    case SSL_AEAD:
        mac = "AEAD";
        break;
    case SSL_GOST89MAC:
        mac = "GOST89";
        break;
    case SSL_GOST94:
        mac = "GOST94";
        break;
    default:
        mac = "unknown";
        break;
    }

    if (buf == NULL) {
        len = 128;
        buf = OPENSSL_malloc(len);
        if (buf == NULL)
            return ("OPENSSL_malloc Error");
    } else if (len < 128)
        return ("Buffer too small");

#ifdef KSSL_DEBUG
    BIO_snprintf(buf, len, format, cipher->name, ver, kx, au, enc, mac,
                 exp_str, alg_mkey, alg_auth, alg_enc, alg_mac, alg_ssl);
#else
    BIO_snprintf(buf, len, format, cipher->name, ver, kx, au, enc, mac,
                 exp_str);
#endif                          /* KSSL_DEBUG */
    return (buf);
}

char *SSL_CIPHER_get_version(const SSL_CIPHER *c)
{
    int i;

    if (c == NULL)
        return ("(NONE)");
    i = (int)(c->id >> 24L);
    if (i == 3)
        return ("TLSv1/SSLv3");
    else if (i == 2)
        return ("SSLv2");
    else
        return ("unknown");
}

/* return the actual cipher being used */
const char *SSL_CIPHER_get_name(const SSL_CIPHER *c)
{
    if (c != NULL)
        return (c->name);
    return ("(NONE)");
}

/* number of bits for symmetric cipher */
int SSL_CIPHER_get_bits(const SSL_CIPHER *c, int *alg_bits)
{
    int ret = 0;

    if (c != NULL) {
        if (alg_bits != NULL)
            *alg_bits = c->alg_bits;
        ret = c->strength_bits;
    }
    return (ret);
}

unsigned long SSL_CIPHER_get_id(const SSL_CIPHER *c)
{
    return c->id;
}

SSL_COMP *ssl3_comp_find(STACK_OF(SSL_COMP) *sk, int n)
{
    SSL_COMP *ctmp;
    int i, nn;

    if ((n == 0) || (sk == NULL))
        return (NULL);
    nn = sk_SSL_COMP_num(sk);
    for (i = 0; i < nn; i++) {
        ctmp = sk_SSL_COMP_value(sk, i);
        if (ctmp->id == n)
            return (ctmp);
    }
    return (NULL);
}

#ifdef OPENSSL_NO_COMP
void *SSL_COMP_get_compression_methods(void)
{
    return NULL;
}

int SSL_COMP_add_compression_method(int id, void *cm)
{
    return 1;
}

const char *SSL_COMP_get_name(const void *comp)
{
    return NULL;
}
#else
STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void)
{
    load_builtin_compressions();
    return (ssl_comp_methods);
}

STACK_OF(SSL_COMP) *SSL_COMP_set0_compression_methods(STACK_OF(SSL_COMP)
                                                      *meths)
{
    STACK_OF(SSL_COMP) *old_meths = ssl_comp_methods;
    ssl_comp_methods = meths;
    return old_meths;
}

static void cmeth_free(SSL_COMP *cm)
{
    OPENSSL_free(cm);
}

void SSL_COMP_free_compression_methods(void)
{
    STACK_OF(SSL_COMP) *old_meths = ssl_comp_methods;
    ssl_comp_methods = NULL;
    sk_SSL_COMP_pop_free(old_meths, cmeth_free);
}

int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm)
{
    SSL_COMP *comp;

    if (cm == NULL || cm->type == NID_undef)
        return 1;

    /*-
     * According to draft-ietf-tls-compression-04.txt, the
     * compression number ranges should be the following:
     *
     *   0 to  63:  methods defined by the IETF
     *  64 to 192:  external party methods assigned by IANA
     * 193 to 255:  reserved for private use
     */
    if (id < 193 || id > 255) {
        SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD,
               SSL_R_COMPRESSION_ID_NOT_WITHIN_PRIVATE_RANGE);
        return 0;
    }

    MemCheck_off();
    comp = (SSL_COMP *)OPENSSL_malloc(sizeof(SSL_COMP));
    comp->id = id;
    comp->method = cm;
    load_builtin_compressions();
    if (ssl_comp_methods && sk_SSL_COMP_find(ssl_comp_methods, comp) >= 0) {
        OPENSSL_free(comp);
        MemCheck_on();
        SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD,
               SSL_R_DUPLICATE_COMPRESSION_ID);
        return (1);
    } else if ((ssl_comp_methods == NULL)
               || !sk_SSL_COMP_push(ssl_comp_methods, comp)) {
        OPENSSL_free(comp);
        MemCheck_on();
        SSLerr(SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD, ERR_R_MALLOC_FAILURE);
        return (1);
    } else {
        MemCheck_on();
        return (0);
    }
}

const char *SSL_COMP_get_name(const COMP_METHOD *comp)
{
    if (comp)
        return comp->name;
    return NULL;
}
#endif
/* For a cipher return the index corresponding to the certificate type */
int ssl_cipher_get_cert_index(const SSL_CIPHER *c)
{
    unsigned long alg_k, alg_a;

    alg_k = c->algorithm_mkey;
    alg_a = c->algorithm_auth;

    if (alg_k & (SSL_kECDHr | SSL_kECDHe)) {
        /*
         * we don't need to look at SSL_kEECDH since no certificate is needed
         * for anon ECDH and for authenticated EECDH, the check for the auth
         * algorithm will set i correctly NOTE: For ECDH-RSA, we need an ECC
         * not an RSA cert but for EECDH-RSA we need an RSA cert. Placing the
         * checks for SSL_kECDH before RSA checks ensures the correct cert is
         * chosen.
         */
        return SSL_PKEY_ECC;
    } else if (alg_a & SSL_aECDSA)
        return SSL_PKEY_ECC;
    else if (alg_k & SSL_kDHr)
        return SSL_PKEY_DH_RSA;
    else if (alg_k & SSL_kDHd)
        return SSL_PKEY_DH_DSA;
    else if (alg_a & SSL_aDSS)
        return SSL_PKEY_DSA_SIGN;
    else if (alg_a & SSL_aRSA)
        return SSL_PKEY_RSA_ENC;
    else if (alg_a & SSL_aKRB5)
        /* VRS something else here? */
        return -1;
    else if (alg_a & SSL_aGOST94)
        return SSL_PKEY_GOST94;
    else if (alg_a & SSL_aGOST01)
        return SSL_PKEY_GOST01;
    return -1;
}

const SSL_CIPHER *ssl_get_cipher_by_char(SSL *ssl, const unsigned char *ptr)
{
    const SSL_CIPHER *c;
    c = ssl->method->get_cipher_by_char(ptr);
    if (c == NULL || c->valid == 0)
        return NULL;
    return c;
}

const SSL_CIPHER *SSL_CIPHER_find(SSL *ssl, const unsigned char *ptr)
{
    return ssl->method->get_cipher_by_char(ptr);
}
