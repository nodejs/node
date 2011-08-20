/* ssl/s3_lib.c */
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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
 *
 * Portions of the attached software ("Contribution") are developed by 
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */

#include <stdio.h>
#include <openssl/objects.h>
#include "ssl_locl.h"
#include "kssl_lcl.h"
#include <openssl/md5.h>
#ifndef OPENSSL_NO_DH
#include <openssl/dh.h>
#endif
#include <openssl/pq_compat.h>

const char ssl3_version_str[]="SSLv3" OPENSSL_VERSION_PTEXT;

#define SSL3_NUM_CIPHERS	(sizeof(ssl3_ciphers)/sizeof(SSL_CIPHER))

/* list of available SSLv3 ciphers (sorted by id) */
OPENSSL_GLOBAL SSL_CIPHER ssl3_ciphers[]={
/* The RSA ciphers */
/* Cipher 01 */
	{
	1,
	SSL3_TXT_RSA_NULL_MD5,
	SSL3_CK_RSA_NULL_MD5,
	SSL_kRSA|SSL_aRSA|SSL_eNULL |SSL_MD5|SSL_SSLV3,
	SSL_NOT_EXP|SSL_STRONG_NONE,
	0,
	0,
	0,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 02 */
	{
	1,
	SSL3_TXT_RSA_NULL_SHA,
	SSL3_CK_RSA_NULL_SHA,
	SSL_kRSA|SSL_aRSA|SSL_eNULL |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_STRONG_NONE|SSL_FIPS,
	0,
	0,
	0,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 03 */
	{
	1,
	SSL3_TXT_RSA_RC4_40_MD5,
	SSL3_CK_RSA_RC4_40_MD5,
	SSL_kRSA|SSL_aRSA|SSL_RC4  |SSL_MD5 |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 04 */
	{
	1,
	SSL3_TXT_RSA_RC4_128_MD5,
	SSL3_CK_RSA_RC4_128_MD5,
	SSL_kRSA|SSL_aRSA|SSL_RC4  |SSL_MD5|SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 05 */
	{
	1,
	SSL3_TXT_RSA_RC4_128_SHA,
	SSL3_CK_RSA_RC4_128_SHA,
	SSL_kRSA|SSL_aRSA|SSL_RC4  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 06 */
	{
	1,
	SSL3_TXT_RSA_RC2_40_MD5,
	SSL3_CK_RSA_RC2_40_MD5,
	SSL_kRSA|SSL_aRSA|SSL_RC2  |SSL_MD5 |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 07 */
#ifndef OPENSSL_NO_IDEA
	{
	1,
	SSL3_TXT_RSA_IDEA_128_SHA,
	SSL3_CK_RSA_IDEA_128_SHA,
	SSL_kRSA|SSL_aRSA|SSL_IDEA |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
#endif
/* Cipher 08 */
	{
	1,
	SSL3_TXT_RSA_DES_40_CBC_SHA,
	SSL3_CK_RSA_DES_40_CBC_SHA,
	SSL_kRSA|SSL_aRSA|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 09 */
	{
	1,
	SSL3_TXT_RSA_DES_64_CBC_SHA,
	SSL3_CK_RSA_DES_64_CBC_SHA,
	SSL_kRSA|SSL_aRSA|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 0A */
	{
	1,
	SSL3_TXT_RSA_DES_192_CBC3_SHA,
	SSL3_CK_RSA_DES_192_CBC3_SHA,
	SSL_kRSA|SSL_aRSA|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* The DH ciphers */
/* Cipher 0B */
	{
	0,
	SSL3_TXT_DH_DSS_DES_40_CBC_SHA,
	SSL3_CK_DH_DSS_DES_40_CBC_SHA,
	SSL_kDHd |SSL_aDH|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 0C */
	{
	0,
	SSL3_TXT_DH_DSS_DES_64_CBC_SHA,
	SSL3_CK_DH_DSS_DES_64_CBC_SHA,
	SSL_kDHd |SSL_aDH|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 0D */
	{
	0,
	SSL3_TXT_DH_DSS_DES_192_CBC3_SHA,
	SSL3_CK_DH_DSS_DES_192_CBC3_SHA,
	SSL_kDHd |SSL_aDH|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 0E */
	{
	0,
	SSL3_TXT_DH_RSA_DES_40_CBC_SHA,
	SSL3_CK_DH_RSA_DES_40_CBC_SHA,
	SSL_kDHr |SSL_aDH|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 0F */
	{
	0,
	SSL3_TXT_DH_RSA_DES_64_CBC_SHA,
	SSL3_CK_DH_RSA_DES_64_CBC_SHA,
	SSL_kDHr |SSL_aDH|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 10 */
	{
	0,
	SSL3_TXT_DH_RSA_DES_192_CBC3_SHA,
	SSL3_CK_DH_RSA_DES_192_CBC3_SHA,
	SSL_kDHr |SSL_aDH|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* The Ephemeral DH ciphers */
/* Cipher 11 */
	{
	1,
	SSL3_TXT_EDH_DSS_DES_40_CBC_SHA,
	SSL3_CK_EDH_DSS_DES_40_CBC_SHA,
	SSL_kEDH|SSL_aDSS|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 12 */
	{
	1,
	SSL3_TXT_EDH_DSS_DES_64_CBC_SHA,
	SSL3_CK_EDH_DSS_DES_64_CBC_SHA,
	SSL_kEDH|SSL_aDSS|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 13 */
	{
	1,
	SSL3_TXT_EDH_DSS_DES_192_CBC3_SHA,
	SSL3_CK_EDH_DSS_DES_192_CBC3_SHA,
	SSL_kEDH|SSL_aDSS|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 14 */
	{
	1,
	SSL3_TXT_EDH_RSA_DES_40_CBC_SHA,
	SSL3_CK_EDH_RSA_DES_40_CBC_SHA,
	SSL_kEDH|SSL_aRSA|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 15 */
	{
	1,
	SSL3_TXT_EDH_RSA_DES_64_CBC_SHA,
	SSL3_CK_EDH_RSA_DES_64_CBC_SHA,
	SSL_kEDH|SSL_aRSA|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 16 */
	{
	1,
	SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA,
	SSL3_CK_EDH_RSA_DES_192_CBC3_SHA,
	SSL_kEDH|SSL_aRSA|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 17 */
	{
	1,
	SSL3_TXT_ADH_RC4_40_MD5,
	SSL3_CK_ADH_RC4_40_MD5,
	SSL_kEDH |SSL_aNULL|SSL_RC4  |SSL_MD5 |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 18 */
	{
	1,
	SSL3_TXT_ADH_RC4_128_MD5,
	SSL3_CK_ADH_RC4_128_MD5,
	SSL_kEDH |SSL_aNULL|SSL_RC4  |SSL_MD5 |SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 19 */
	{
	1,
	SSL3_TXT_ADH_DES_40_CBC_SHA,
	SSL3_CK_ADH_DES_40_CBC_SHA,
	SSL_kEDH |SSL_aNULL|SSL_DES|SSL_SHA1|SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 1A */
	{
	1,
	SSL3_TXT_ADH_DES_64_CBC_SHA,
	SSL3_CK_ADH_DES_64_CBC_SHA,
	SSL_kEDH |SSL_aNULL|SSL_DES  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 1B */
	{
	1,
	SSL3_TXT_ADH_DES_192_CBC_SHA,
	SSL3_CK_ADH_DES_192_CBC_SHA,
	SSL_kEDH |SSL_aNULL|SSL_3DES |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Fortezza */
/* Cipher 1C */
	{
	0,
	SSL3_TXT_FZA_DMS_NULL_SHA,
	SSL3_CK_FZA_DMS_NULL_SHA,
	SSL_kFZA|SSL_aFZA |SSL_eNULL |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_STRONG_NONE,
	0,
	0,
	0,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 1D */
	{
	0,
	SSL3_TXT_FZA_DMS_FZA_SHA,
	SSL3_CK_FZA_DMS_FZA_SHA,
	SSL_kFZA|SSL_aFZA |SSL_eFZA |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_STRONG_NONE,
	0,
	0,
	0,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

#if 0
/* Cipher 1E */
	{
	0,
	SSL3_TXT_FZA_DMS_RC4_SHA,
	SSL3_CK_FZA_DMS_RC4_SHA,
	SSL_kFZA|SSL_aFZA |SSL_RC4  |SSL_SHA1|SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
#endif

#ifndef OPENSSL_NO_KRB5
/* The Kerberos ciphers */
/* Cipher 1E */
	{
	1,
	SSL3_TXT_KRB5_DES_64_CBC_SHA,
	SSL3_CK_KRB5_DES_64_CBC_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_DES|SSL_SHA1   |SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 1F */
	{
	1,
	SSL3_TXT_KRB5_DES_192_CBC3_SHA,
	SSL3_CK_KRB5_DES_192_CBC3_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_3DES|SSL_SHA1  |SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 20 */
	{
	1,
	SSL3_TXT_KRB5_RC4_128_SHA,
	SSL3_CK_KRB5_RC4_128_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC4|SSL_SHA1  |SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 21 */
	{
	1,
	SSL3_TXT_KRB5_IDEA_128_CBC_SHA,
	SSL3_CK_KRB5_IDEA_128_CBC_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_IDEA|SSL_SHA1  |SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 22 */
	{
	1,
	SSL3_TXT_KRB5_DES_64_CBC_MD5,
	SSL3_CK_KRB5_DES_64_CBC_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_DES|SSL_MD5    |SSL_SSLV3,
	SSL_NOT_EXP|SSL_LOW,
	0,
	56,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 23 */
	{
	1,
	SSL3_TXT_KRB5_DES_192_CBC3_MD5,
	SSL3_CK_KRB5_DES_192_CBC3_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_3DES|SSL_MD5   |SSL_SSLV3,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	168,
	168,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 24 */
	{
	1,
	SSL3_TXT_KRB5_RC4_128_MD5,
	SSL3_CK_KRB5_RC4_128_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC4|SSL_MD5  |SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 25 */
	{
	1,
	SSL3_TXT_KRB5_IDEA_128_CBC_MD5,
	SSL3_CK_KRB5_IDEA_128_CBC_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_IDEA|SSL_MD5  |SSL_SSLV3,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 26 */
	{
	1,
	SSL3_TXT_KRB5_DES_40_CBC_SHA,
	SSL3_CK_KRB5_DES_40_CBC_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_DES|SSL_SHA1   |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 27 */
	{
	1,
	SSL3_TXT_KRB5_RC2_40_CBC_SHA,
	SSL3_CK_KRB5_RC2_40_CBC_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC2|SSL_SHA1   |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 28 */
	{
	1,
	SSL3_TXT_KRB5_RC4_40_SHA,
	SSL3_CK_KRB5_RC4_40_SHA,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC4|SSL_SHA1   |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 29 */
	{
	1,
	SSL3_TXT_KRB5_DES_40_CBC_MD5,
	SSL3_CK_KRB5_DES_40_CBC_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_DES|SSL_MD5    |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	56,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 2A */
	{
	1,
	SSL3_TXT_KRB5_RC2_40_CBC_MD5,
	SSL3_CK_KRB5_RC2_40_CBC_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC2|SSL_MD5    |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 2B */
	{
	1,
	SSL3_TXT_KRB5_RC4_40_MD5,
	SSL3_CK_KRB5_RC4_40_MD5,
	SSL_kKRB5|SSL_aKRB5|  SSL_RC4|SSL_MD5    |SSL_SSLV3,
	SSL_EXPORT|SSL_EXP40,
	0,
	40,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
#endif	/* OPENSSL_NO_KRB5 */

/* New AES ciphersuites */
/* Cipher 2F */
	{
	1,
	TLS1_TXT_RSA_WITH_AES_128_SHA,
	TLS1_CK_RSA_WITH_AES_128_SHA,
	SSL_kRSA|SSL_aRSA|SSL_AES|SSL_SHA |SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 30 */
	{
	0,
	TLS1_TXT_DH_DSS_WITH_AES_128_SHA,
	TLS1_CK_DH_DSS_WITH_AES_128_SHA,
	SSL_kDHd|SSL_aDH|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 31 */
	{
	0,
	TLS1_TXT_DH_RSA_WITH_AES_128_SHA,
	TLS1_CK_DH_RSA_WITH_AES_128_SHA,
	SSL_kDHr|SSL_aDH|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 32 */
	{
	1,
	TLS1_TXT_DHE_DSS_WITH_AES_128_SHA,
	TLS1_CK_DHE_DSS_WITH_AES_128_SHA,
	SSL_kEDH|SSL_aDSS|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 33 */
	{
	1,
	TLS1_TXT_DHE_RSA_WITH_AES_128_SHA,
	TLS1_CK_DHE_RSA_WITH_AES_128_SHA,
	SSL_kEDH|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 34 */
	{
	1,
	TLS1_TXT_ADH_WITH_AES_128_SHA,
	TLS1_CK_ADH_WITH_AES_128_SHA,
	SSL_kEDH|SSL_aNULL|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

/* Cipher 35 */
	{
	1,
	TLS1_TXT_RSA_WITH_AES_256_SHA,
	TLS1_CK_RSA_WITH_AES_256_SHA,
	SSL_kRSA|SSL_aRSA|SSL_AES|SSL_SHA |SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 36 */
	{
	0,
	TLS1_TXT_DH_DSS_WITH_AES_256_SHA,
	TLS1_CK_DH_DSS_WITH_AES_256_SHA,
	SSL_kDHd|SSL_aDH|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 37 */
	{
	0,
	TLS1_TXT_DH_RSA_WITH_AES_256_SHA,
	TLS1_CK_DH_RSA_WITH_AES_256_SHA,
	SSL_kDHr|SSL_aDH|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 38 */
	{
	1,
	TLS1_TXT_DHE_DSS_WITH_AES_256_SHA,
	TLS1_CK_DHE_DSS_WITH_AES_256_SHA,
	SSL_kEDH|SSL_aDSS|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
/* Cipher 39 */
	{
	1,
	TLS1_TXT_DHE_RSA_WITH_AES_256_SHA,
	TLS1_CK_DHE_RSA_WITH_AES_256_SHA,
	SSL_kEDH|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},
	/* Cipher 3A */
	{
	1,
	TLS1_TXT_ADH_WITH_AES_256_SHA,
	TLS1_CK_ADH_WITH_AES_256_SHA,
	SSL_kEDH|SSL_aNULL|SSL_AES|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH|SSL_FIPS,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

#ifndef OPENSSL_NO_CAMELLIA
	/* Camellia ciphersuites from RFC4132 (128-bit portion) */

	/* Cipher 41 */
	{
	1,
	TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kRSA|SSL_aRSA|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 42 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_DSS_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_DH_DSS_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kDHd|SSL_aDH|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 43 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_RSA_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_DH_RSA_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kDHr|SSL_aDH|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 44 */
	{
	1,
	TLS1_TXT_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kEDH|SSL_aDSS|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 45 */
	{
	1,
	TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kEDH|SSL_aRSA|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 46 */
	{
	1,
	TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA,
	TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA,
	SSL_kEDH|SSL_aNULL|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
#endif /* OPENSSL_NO_CAMELLIA */

#if TLS1_ALLOW_EXPERIMENTAL_CIPHERSUITES
	/* New TLS Export CipherSuites from expired ID */
#if 0
	/* Cipher 60 */
	    {
	    1,
	    TLS1_TXT_RSA_EXPORT1024_WITH_RC4_56_MD5,
	    TLS1_CK_RSA_EXPORT1024_WITH_RC4_56_MD5,
	    SSL_kRSA|SSL_aRSA|SSL_RC4|SSL_MD5|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    128,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
	/* Cipher 61 */
	    {
	    1,
	    TLS1_TXT_RSA_EXPORT1024_WITH_RC2_CBC_56_MD5,
	    TLS1_CK_RSA_EXPORT1024_WITH_RC2_CBC_56_MD5,
	    SSL_kRSA|SSL_aRSA|SSL_RC2|SSL_MD5|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    128,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
#endif
	/* Cipher 62 */
	    {
	    1,
	    TLS1_TXT_RSA_EXPORT1024_WITH_DES_CBC_SHA,
	    TLS1_CK_RSA_EXPORT1024_WITH_DES_CBC_SHA,
	    SSL_kRSA|SSL_aRSA|SSL_DES|SSL_SHA|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    56,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
	/* Cipher 63 */
	    {
	    1,
	    TLS1_TXT_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA,
	    TLS1_CK_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA,
	    SSL_kEDH|SSL_aDSS|SSL_DES|SSL_SHA|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    56,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
	/* Cipher 64 */
	    {
	    1,
	    TLS1_TXT_RSA_EXPORT1024_WITH_RC4_56_SHA,
	    TLS1_CK_RSA_EXPORT1024_WITH_RC4_56_SHA,
	    SSL_kRSA|SSL_aRSA|SSL_RC4|SSL_SHA|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    128,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
	/* Cipher 65 */
	    {
	    1,
	    TLS1_TXT_DHE_DSS_EXPORT1024_WITH_RC4_56_SHA,
	    TLS1_CK_DHE_DSS_EXPORT1024_WITH_RC4_56_SHA,
	    SSL_kEDH|SSL_aDSS|SSL_RC4|SSL_SHA|SSL_TLSV1,
	    SSL_EXPORT|SSL_EXP56,
	    0,
	    56,
	    128,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS,
	    },
	/* Cipher 66 */
	    {
	    1,
	    TLS1_TXT_DHE_DSS_WITH_RC4_128_SHA,
	    TLS1_CK_DHE_DSS_WITH_RC4_128_SHA,
	    SSL_kEDH|SSL_aDSS|SSL_RC4|SSL_SHA|SSL_TLSV1,
	    SSL_NOT_EXP|SSL_MEDIUM,
	    0,
	    128,
	    128,
	    SSL_ALL_CIPHERS,
	    SSL_ALL_STRENGTHS
	    },
#endif

#ifndef OPENSSL_NO_CAMELLIA
	/* Camellia ciphersuites from RFC4132 (256-bit portion) */

	/* Cipher 84 */
	{
	1,
	TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kRSA|SSL_aRSA|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 85 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_DSS_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_DH_DSS_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kDHd|SSL_aDH|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 86 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_RSA_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_DH_RSA_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kDHr|SSL_aDH|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 87 */
	{
	1,
	TLS1_TXT_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kEDH|SSL_aDSS|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 88 */
	{
	1,
	TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kEDH|SSL_aRSA|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
	/* Cipher 89 */
	{
	1,
	TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA,
	TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA,
	SSL_kEDH|SSL_aNULL|SSL_CAMELLIA|SSL_SHA|SSL_TLSV1,
	SSL_NOT_EXP|SSL_HIGH,
	0,
	256,
	256,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS
	},
#endif /* OPENSSL_NO_CAMELLIA */

#ifndef OPENSSL_NO_SEED
	/* SEED ciphersuites from RFC4162 */

	/* Cipher 96 */
	{
	1,
	TLS1_TXT_RSA_WITH_SEED_SHA,
	TLS1_CK_RSA_WITH_SEED_SHA,
	SSL_kRSA|SSL_aRSA|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

	/* Cipher 97 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_DSS_WITH_SEED_SHA,
	TLS1_CK_DH_DSS_WITH_SEED_SHA,
	SSL_kDHd|SSL_aDH|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

	/* Cipher 98 */
	{
	0, /* not implemented (non-ephemeral DH) */
	TLS1_TXT_DH_RSA_WITH_SEED_SHA,
	TLS1_CK_DH_RSA_WITH_SEED_SHA,
	SSL_kDHr|SSL_aDH|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

	/* Cipher 99 */
	{
	1,
	TLS1_TXT_DHE_DSS_WITH_SEED_SHA,
	TLS1_CK_DHE_DSS_WITH_SEED_SHA,
	SSL_kEDH|SSL_aDSS|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

	/* Cipher 9A */
	{
	1,
	TLS1_TXT_DHE_RSA_WITH_SEED_SHA,
	TLS1_CK_DHE_RSA_WITH_SEED_SHA,
	SSL_kEDH|SSL_aRSA|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

	/* Cipher 9B */
	{
	1,
	TLS1_TXT_ADH_WITH_SEED_SHA,
	TLS1_CK_ADH_WITH_SEED_SHA,
	SSL_kEDH|SSL_aNULL|SSL_SEED|SSL_SHA1|SSL_TLSV1,
	SSL_NOT_EXP|SSL_MEDIUM,
	0,
	128,
	128,
	SSL_ALL_CIPHERS,
	SSL_ALL_STRENGTHS,
	},

#endif /* OPENSSL_NO_SEED */

#ifndef OPENSSL_NO_ECDH
	/* Cipher C001 */
	    {
            1,
            TLS1_TXT_ECDH_ECDSA_WITH_NULL_SHA,
            TLS1_CK_ECDH_ECDSA_WITH_NULL_SHA,
            SSL_kECDH|SSL_aECDSA|SSL_eNULL|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            0,
            0,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C002 */
	    {
            1,
            TLS1_TXT_ECDH_ECDSA_WITH_RC4_128_SHA,
            TLS1_CK_ECDH_ECDSA_WITH_RC4_128_SHA,
            SSL_kECDH|SSL_aECDSA|SSL_RC4|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C003 */
	    {
            1,
            TLS1_TXT_ECDH_ECDSA_WITH_DES_192_CBC3_SHA,
            TLS1_CK_ECDH_ECDSA_WITH_DES_192_CBC3_SHA,
            SSL_kECDH|SSL_aECDSA|SSL_3DES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            168,
            168,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C004 */
	    {
            1,
            TLS1_TXT_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
            TLS1_CK_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
            SSL_kECDH|SSL_aECDSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C005 */
	    {
            1,
            TLS1_TXT_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
            TLS1_CK_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
            SSL_kECDH|SSL_aECDSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            256,
            256,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C006 */
	    {
            1,
            TLS1_TXT_ECDHE_ECDSA_WITH_NULL_SHA,
            TLS1_CK_ECDHE_ECDSA_WITH_NULL_SHA,
            SSL_kECDHE|SSL_aECDSA|SSL_eNULL|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            0,
            0,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C007 */
	    {
            1,
            TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA,
            TLS1_CK_ECDHE_ECDSA_WITH_RC4_128_SHA,
            SSL_kECDHE|SSL_aECDSA|SSL_RC4|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C008 */
	    {
            1,
            TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
            TLS1_CK_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
            SSL_kECDHE|SSL_aECDSA|SSL_3DES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            168,
            168,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C009 */
	    {
            1,
            TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
            TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
            SSL_kECDHE|SSL_aECDSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00A */
	    {
            1,
            TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
            TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
            SSL_kECDHE|SSL_aECDSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            256,
            256,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00B */
	    {
            1,
            TLS1_TXT_ECDH_RSA_WITH_NULL_SHA,
            TLS1_CK_ECDH_RSA_WITH_NULL_SHA,
            SSL_kECDH|SSL_aRSA|SSL_eNULL|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            0,
            0,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00C */
	    {
            1,
            TLS1_TXT_ECDH_RSA_WITH_RC4_128_SHA,
            TLS1_CK_ECDH_RSA_WITH_RC4_128_SHA,
            SSL_kECDH|SSL_aRSA|SSL_RC4|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00D */
	    {
            1,
            TLS1_TXT_ECDH_RSA_WITH_DES_192_CBC3_SHA,
            TLS1_CK_ECDH_RSA_WITH_DES_192_CBC3_SHA,
            SSL_kECDH|SSL_aRSA|SSL_3DES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            168,
            168,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00E */
	    {
            1,
            TLS1_TXT_ECDH_RSA_WITH_AES_128_CBC_SHA,
            TLS1_CK_ECDH_RSA_WITH_AES_128_CBC_SHA,
            SSL_kECDH|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C00F */
	    {
            1,
            TLS1_TXT_ECDH_RSA_WITH_AES_256_CBC_SHA,
            TLS1_CK_ECDH_RSA_WITH_AES_256_CBC_SHA,
            SSL_kECDH|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            256,
            256,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C010 */
	    {
            1,
            TLS1_TXT_ECDHE_RSA_WITH_NULL_SHA,
            TLS1_CK_ECDHE_RSA_WITH_NULL_SHA,
            SSL_kECDHE|SSL_aRSA|SSL_eNULL|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            0,
            0,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C011 */
	    {
            1,
            TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA,
            TLS1_CK_ECDHE_RSA_WITH_RC4_128_SHA,
            SSL_kECDHE|SSL_aRSA|SSL_RC4|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C012 */
	    {
            1,
            TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
            TLS1_CK_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
            SSL_kECDHE|SSL_aRSA|SSL_3DES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            168,
            168,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C013 */
	    {
            1,
            TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA,
            TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA,
            SSL_kECDHE|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C014 */
	    {
            1,
            TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA,
            TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA,
            SSL_kECDHE|SSL_aRSA|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            256,
            256,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C015 */
            {
            1,
            TLS1_TXT_ECDH_anon_WITH_NULL_SHA,
            TLS1_CK_ECDH_anon_WITH_NULL_SHA,
            SSL_kECDHE|SSL_aNULL|SSL_eNULL|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            0,
            0,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
	    },

	/* Cipher C016 */
            {
            1,
            TLS1_TXT_ECDH_anon_WITH_RC4_128_SHA,
            TLS1_CK_ECDH_anon_WITH_RC4_128_SHA,
            SSL_kECDHE|SSL_aNULL|SSL_RC4|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
	    },

	/* Cipher C017 */
	    {
            1,
            TLS1_TXT_ECDH_anon_WITH_DES_192_CBC3_SHA,
            TLS1_CK_ECDH_anon_WITH_DES_192_CBC3_SHA,
            SSL_kECDHE|SSL_aNULL|SSL_3DES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            168,
            168,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C018 */
	    {
            1,
            TLS1_TXT_ECDH_anon_WITH_AES_128_CBC_SHA,
            TLS1_CK_ECDH_anon_WITH_AES_128_CBC_SHA,
            SSL_kECDHE|SSL_aNULL|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            128,
            128,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },

	/* Cipher C019 */
	    {
            1,
            TLS1_TXT_ECDH_anon_WITH_AES_256_CBC_SHA,
            TLS1_CK_ECDH_anon_WITH_AES_256_CBC_SHA,
            SSL_kECDHE|SSL_aNULL|SSL_AES|SSL_SHA|SSL_TLSV1,
            SSL_NOT_EXP|SSL_HIGH,
            0,
            256,
            256,
            SSL_ALL_CIPHERS,
            SSL_ALL_STRENGTHS,
            },
#endif	/* OPENSSL_NO_ECDH */


/* end of list */
	};

SSL3_ENC_METHOD SSLv3_enc_data={
	ssl3_enc,
	ssl3_mac,
	ssl3_setup_key_block,
	ssl3_generate_master_secret,
	ssl3_change_cipher_state,
	ssl3_final_finish_mac,
	MD5_DIGEST_LENGTH+SHA_DIGEST_LENGTH,
	ssl3_cert_verify_mac,
	SSL3_MD_CLIENT_FINISHED_CONST,4,
	SSL3_MD_SERVER_FINISHED_CONST,4,
	ssl3_alert_code,
	};

long ssl3_default_timeout(void)
	{
	/* 2 hours, the 24 hours mentioned in the SSLv3 spec
	 * is way too long for http, the cache would over fill */
	return(60*60*2);
	}

IMPLEMENT_ssl3_meth_func(sslv3_base_method,
			ssl_undefined_function,
			ssl_undefined_function,
			ssl_bad_method)

int ssl3_num_ciphers(void)
	{
	return(SSL3_NUM_CIPHERS);
	}

SSL_CIPHER *ssl3_get_cipher(unsigned int u)
	{
	if (u < SSL3_NUM_CIPHERS)
		return(&(ssl3_ciphers[SSL3_NUM_CIPHERS-1-u]));
	else
		return(NULL);
	}

int ssl3_pending(const SSL *s)
	{
	if (s->rstate == SSL_ST_READ_BODY)
		return 0;
	
	return (s->s3->rrec.type == SSL3_RT_APPLICATION_DATA) ? s->s3->rrec.length : 0;
	}

int ssl3_new(SSL *s)
	{
	SSL3_STATE *s3;

	if ((s3=OPENSSL_malloc(sizeof *s3)) == NULL) goto err;
	memset(s3,0,sizeof *s3);
	EVP_MD_CTX_init(&s3->finish_dgst1);
	EVP_MD_CTX_init(&s3->finish_dgst2);
	pq_64bit_init(&(s3->rrec.seq_num));
	pq_64bit_init(&(s3->wrec.seq_num));

	s->s3=s3;

	s->method->ssl_clear(s);
	return(1);
err:
	return(0);
	}

void ssl3_free(SSL *s)
	{
	if(s == NULL)
	    return;

	ssl3_cleanup_key_block(s);
	if (s->s3->rbuf.buf != NULL)
		OPENSSL_free(s->s3->rbuf.buf);
	if (s->s3->wbuf.buf != NULL)
		OPENSSL_free(s->s3->wbuf.buf);
	if (s->s3->rrec.comp != NULL)
		OPENSSL_free(s->s3->rrec.comp);
#ifndef OPENSSL_NO_DH
	if (s->s3->tmp.dh != NULL)
		DH_free(s->s3->tmp.dh);
#endif
#ifndef OPENSSL_NO_ECDH
	if (s->s3->tmp.ecdh != NULL)
		EC_KEY_free(s->s3->tmp.ecdh);
#endif

	if (s->s3->tmp.ca_names != NULL)
		sk_X509_NAME_pop_free(s->s3->tmp.ca_names,X509_NAME_free);
	EVP_MD_CTX_cleanup(&s->s3->finish_dgst1);
	EVP_MD_CTX_cleanup(&s->s3->finish_dgst2);
	pq_64bit_free(&(s->s3->rrec.seq_num));
	pq_64bit_free(&(s->s3->wrec.seq_num));

	if (s->s3->snap_start_client_hello.buf)
		{
		/* s->s3->snap_start_records, if set, uses the same buffer */
		OPENSSL_free(s->s3->snap_start_client_hello.buf);
		}

	OPENSSL_cleanse(s->s3,sizeof *s->s3);
	OPENSSL_free(s->s3);
	s->s3=NULL;
	}

void ssl3_clear(SSL *s)
	{
	unsigned char *rp,*wp;
	size_t rlen, wlen;

	ssl3_cleanup_key_block(s);
	if (s->s3->tmp.ca_names != NULL)
		sk_X509_NAME_pop_free(s->s3->tmp.ca_names,X509_NAME_free);

	if (s->s3->rrec.comp != NULL)
		{
		OPENSSL_free(s->s3->rrec.comp);
		s->s3->rrec.comp=NULL;
		}
#ifndef OPENSSL_NO_DH
	if (s->s3->tmp.dh != NULL)
		DH_free(s->s3->tmp.dh);
#endif
#ifndef OPENSSL_NO_ECDH
	if (s->s3->tmp.ecdh != NULL)
		EC_KEY_free(s->s3->tmp.ecdh);
#endif

	rp = s->s3->rbuf.buf;
	wp = s->s3->wbuf.buf;
	rlen = s->s3->rbuf.len;
 	wlen = s->s3->wbuf.len;

	EVP_MD_CTX_cleanup(&s->s3->finish_dgst1);
	EVP_MD_CTX_cleanup(&s->s3->finish_dgst2);

	memset(s->s3,0,sizeof *s->s3);
	s->s3->rbuf.buf = rp;
	s->s3->wbuf.buf = wp;
	s->s3->rbuf.len = rlen;
 	s->s3->wbuf.len = wlen;

	ssl_free_wbio_buffer(s);

	s->packet_length=0;
	s->s3->renegotiate=0;
	s->s3->total_renegotiations=0;
	s->s3->num_renegotiations=0;
	s->s3->in_read_app_data=0;
	s->version=SSL3_VERSION;

#ifndef OPENSSL_NO_TLSEXT
	if (s->next_proto_negotiated) {
		OPENSSL_free(s->next_proto_negotiated);
		s->next_proto_negotiated = 0;
		s->next_proto_negotiated_len = 0;
	}
#endif
	}

long ssl3_ctrl(SSL *s, int cmd, long larg, void *parg)
	{
	int ret=0;

#if !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_RSA)
	if (
#ifndef OPENSSL_NO_RSA
	    cmd == SSL_CTRL_SET_TMP_RSA ||
	    cmd == SSL_CTRL_SET_TMP_RSA_CB ||
#endif
#ifndef OPENSSL_NO_DSA
	    cmd == SSL_CTRL_SET_TMP_DH ||
	    cmd == SSL_CTRL_SET_TMP_DH_CB ||
#endif
		0)
		{
		if (!ssl_cert_inst(&s->cert))
		    	{
			SSLerr(SSL_F_SSL3_CTRL, ERR_R_MALLOC_FAILURE);
			return(0);
			}
		}
#endif

	switch (cmd)
		{
	case SSL_CTRL_GET_SESSION_REUSED:
		ret=s->hit;
		break;
	case SSL_CTRL_GET_CLIENT_CERT_REQUEST:
		break;
	case SSL_CTRL_GET_NUM_RENEGOTIATIONS:
		ret=s->s3->num_renegotiations;
		break;
	case SSL_CTRL_CLEAR_NUM_RENEGOTIATIONS:
		ret=s->s3->num_renegotiations;
		s->s3->num_renegotiations=0;
		break;
	case SSL_CTRL_GET_TOTAL_RENEGOTIATIONS:
		ret=s->s3->total_renegotiations;
		break;
	case SSL_CTRL_GET_FLAGS:
		ret=(int)(s->s3->flags);
		break;
#ifndef OPENSSL_NO_RSA
	case SSL_CTRL_NEED_TMP_RSA:
		if ((s->cert != NULL) && (s->cert->rsa_tmp == NULL) &&
		    ((s->cert->pkeys[SSL_PKEY_RSA_ENC].privatekey == NULL) ||
		     (EVP_PKEY_size(s->cert->pkeys[SSL_PKEY_RSA_ENC].privatekey) > (512/8))))
			ret = 1;
		break;
	case SSL_CTRL_SET_TMP_RSA:
		{
			RSA *rsa = (RSA *)parg;
			if (rsa == NULL)
				{
				SSLerr(SSL_F_SSL3_CTRL, ERR_R_PASSED_NULL_PARAMETER);
				return(ret);
				}
			if ((rsa = RSAPrivateKey_dup(rsa)) == NULL)
				{
				SSLerr(SSL_F_SSL3_CTRL, ERR_R_RSA_LIB);
				return(ret);
				}
			if (s->cert->rsa_tmp != NULL)
				RSA_free(s->cert->rsa_tmp);
			s->cert->rsa_tmp = rsa;
			ret = 1;
		}
		break;
	case SSL_CTRL_SET_TMP_RSA_CB:
		{
		SSLerr(SSL_F_SSL3_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(ret);
		}
		break;
#endif
#ifndef OPENSSL_NO_DH
	case SSL_CTRL_SET_TMP_DH:
		{
			DH *dh = (DH *)parg;
			if (dh == NULL)
				{
				SSLerr(SSL_F_SSL3_CTRL, ERR_R_PASSED_NULL_PARAMETER);
				return(ret);
				}
			if ((dh = DHparams_dup(dh)) == NULL)
				{
				SSLerr(SSL_F_SSL3_CTRL, ERR_R_DH_LIB);
				return(ret);
				}
			if (!(s->options & SSL_OP_SINGLE_DH_USE))
				{
				if (!DH_generate_key(dh))
					{
					DH_free(dh);
					SSLerr(SSL_F_SSL3_CTRL, ERR_R_DH_LIB);
					return(ret);
					}
				}
			if (s->cert->dh_tmp != NULL)
				DH_free(s->cert->dh_tmp);
			s->cert->dh_tmp = dh;
			ret = 1;
		}
		break;
	case SSL_CTRL_SET_TMP_DH_CB:
		{
		SSLerr(SSL_F_SSL3_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(ret);
		}
		break;
#endif
#ifndef OPENSSL_NO_ECDH
	case SSL_CTRL_SET_TMP_ECDH:
		{
		EC_KEY *ecdh = NULL;
 			
		if (parg == NULL)
			{
			SSLerr(SSL_F_SSL3_CTRL, ERR_R_PASSED_NULL_PARAMETER);
			return(ret);
			}
		if (!EC_KEY_up_ref((EC_KEY *)parg))
			{
			SSLerr(SSL_F_SSL3_CTRL,ERR_R_ECDH_LIB);
			return(ret);
			}
		ecdh = (EC_KEY *)parg;
		if (!(s->options & SSL_OP_SINGLE_ECDH_USE))
			{
			if (!EC_KEY_generate_key(ecdh))
				{
				EC_KEY_free(ecdh);
				SSLerr(SSL_F_SSL3_CTRL,ERR_R_ECDH_LIB);
				return(ret);
				}
			}
		if (s->cert->ecdh_tmp != NULL)
			EC_KEY_free(s->cert->ecdh_tmp);
		s->cert->ecdh_tmp = ecdh;
		ret = 1;
		}
		break;
	case SSL_CTRL_SET_TMP_ECDH_CB:
		{
		SSLerr(SSL_F_SSL3_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(ret);
		}
		break;
#endif /* !OPENSSL_NO_ECDH */
#ifndef OPENSSL_NO_TLSEXT
	case SSL_CTRL_SET_TLSEXT_HOSTNAME:
 		if (larg == TLSEXT_NAMETYPE_host_name)
			{
			if (s->tlsext_hostname != NULL) 
				OPENSSL_free(s->tlsext_hostname);
			s->tlsext_hostname = NULL;

			ret = 1;
			if (parg == NULL) 
				break;
			if (strlen((char *)parg) > TLSEXT_MAXLEN_host_name)
				{
				SSLerr(SSL_F_SSL3_CTRL, SSL_R_SSL3_EXT_INVALID_SERVERNAME);
				return 0;
				}
			if ((s->tlsext_hostname = BUF_strdup((char *)parg)) == NULL)
				{
				SSLerr(SSL_F_SSL3_CTRL, ERR_R_INTERNAL_ERROR);
				return 0;
				}
			}
		else
			{
			SSLerr(SSL_F_SSL3_CTRL, SSL_R_SSL3_EXT_INVALID_SERVERNAME_TYPE);
			return 0;
			}
 		break;
	case SSL_CTRL_SET_TLSEXT_DEBUG_ARG:
		s->tlsext_debug_arg=parg;
		ret = 1;
		break;
  
	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE:
		s->tlsext_status_type=larg;
		ret = 1;
		break;

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_EXTS:
		*(STACK_OF(X509_EXTENSION) **)parg = s->tlsext_ocsp_exts;
		ret = 1;
		break;

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_EXTS:
		s->tlsext_ocsp_exts = parg;
		ret = 1;
		break;

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_IDS:
		*(STACK_OF(OCSP_RESPID) **)parg = s->tlsext_ocsp_ids;
		ret = 1;
		break;

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_IDS:
		s->tlsext_ocsp_ids = parg;
		ret = 1;
		break;

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_OCSP_RESP:
		*(unsigned char **)parg = s->tlsext_ocsp_resp;
		return s->tlsext_ocsp_resplen;
		
	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_OCSP_RESP:
		if (s->tlsext_ocsp_resp)
			OPENSSL_free(s->tlsext_ocsp_resp);
		s->tlsext_ocsp_resp = parg;
		s->tlsext_ocsp_resplen = larg;
		ret = 1;
		break;

#endif /* !OPENSSL_NO_TLSEXT */
	default:
		break;
		}
	return(ret);
	}

long ssl3_callback_ctrl(SSL *s, int cmd, void (*fp)(void))
	{
	int ret=0;

#if !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_RSA)
	if (
#ifndef OPENSSL_NO_RSA
	    cmd == SSL_CTRL_SET_TMP_RSA_CB ||
#endif
#ifndef OPENSSL_NO_DSA
	    cmd == SSL_CTRL_SET_TMP_DH_CB ||
#endif
		0)
		{
		if (!ssl_cert_inst(&s->cert))
			{
			SSLerr(SSL_F_SSL3_CALLBACK_CTRL, ERR_R_MALLOC_FAILURE);
			return(0);
			}
		}
#endif

	switch (cmd)
		{
#ifndef OPENSSL_NO_RSA
	case SSL_CTRL_SET_TMP_RSA_CB:
		{
		s->cert->rsa_tmp_cb = (RSA *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_DH
	case SSL_CTRL_SET_TMP_DH_CB:
		{
		s->cert->dh_tmp_cb = (DH *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_ECDH
	case SSL_CTRL_SET_TMP_ECDH_CB:
		{
		s->cert->ecdh_tmp_cb = (EC_KEY *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_TLSEXT
	case SSL_CTRL_SET_TLSEXT_DEBUG_CB:
		s->tlsext_debug_cb=(void (*)(SSL *,int ,int,
					unsigned char *, int, void *))fp;
		break;
#endif
	default:
		break;
		}
	return(ret);
	}

long ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
	{
	CERT *cert;

	cert=ctx->cert;

	switch (cmd)
		{
#ifndef OPENSSL_NO_RSA
	case SSL_CTRL_NEED_TMP_RSA:
		if (	(cert->rsa_tmp == NULL) &&
			((cert->pkeys[SSL_PKEY_RSA_ENC].privatekey == NULL) ||
			 (EVP_PKEY_size(cert->pkeys[SSL_PKEY_RSA_ENC].privatekey) > (512/8)))
			)
			return(1);
		else
			return(0);
		/* break; */
	case SSL_CTRL_SET_TMP_RSA:
		{
		RSA *rsa;
		int i;

		rsa=(RSA *)parg;
		i=1;
		if (rsa == NULL)
			i=0;
		else
			{
			if ((rsa=RSAPrivateKey_dup(rsa)) == NULL)
				i=0;
			}
		if (!i)
			{
			SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_RSA_LIB);
			return(0);
			}
		else
			{
			if (cert->rsa_tmp != NULL)
				RSA_free(cert->rsa_tmp);
			cert->rsa_tmp=rsa;
			return(1);
			}
		}
		/* break; */
	case SSL_CTRL_SET_TMP_RSA_CB:
		{
		SSLerr(SSL_F_SSL3_CTX_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(0);
		}
		break;
#endif
#ifndef OPENSSL_NO_DH
	case SSL_CTRL_SET_TMP_DH:
		{
		DH *new=NULL,*dh;

		dh=(DH *)parg;
		if ((new=DHparams_dup(dh)) == NULL)
			{
			SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_DH_LIB);
			return 0;
			}
		if (!(ctx->options & SSL_OP_SINGLE_DH_USE))
			{
			if (!DH_generate_key(new))
				{
				SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_DH_LIB);
				DH_free(new);
				return 0;
				}
			}
		if (cert->dh_tmp != NULL)
			DH_free(cert->dh_tmp);
		cert->dh_tmp=new;
		return 1;
		}
		/*break; */
	case SSL_CTRL_SET_TMP_DH_CB:
		{
		SSLerr(SSL_F_SSL3_CTX_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(0);
		}
		break;
#endif
#ifndef OPENSSL_NO_ECDH
	case SSL_CTRL_SET_TMP_ECDH:
		{
		EC_KEY *ecdh = NULL;
 			
		if (parg == NULL)
			{
			SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_ECDH_LIB);
			return 0;
			}
		ecdh = EC_KEY_dup((EC_KEY *)parg);
		if (ecdh == NULL)
			{
			SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_EC_LIB);
			return 0;
			}
		if (!(ctx->options & SSL_OP_SINGLE_ECDH_USE))
			{
			if (!EC_KEY_generate_key(ecdh))
				{
				EC_KEY_free(ecdh);
				SSLerr(SSL_F_SSL3_CTX_CTRL,ERR_R_ECDH_LIB);
				return 0;
				}
			}

		if (cert->ecdh_tmp != NULL)
			{
			EC_KEY_free(cert->ecdh_tmp);
			}
		cert->ecdh_tmp = ecdh;
		return 1;
		}
		/* break; */
	case SSL_CTRL_SET_TMP_ECDH_CB:
		{
		SSLerr(SSL_F_SSL3_CTX_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return(0);
		}
		break;
#endif /* !OPENSSL_NO_ECDH */
#ifndef OPENSSL_NO_TLSEXT
	case SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG:
		ctx->tlsext_servername_arg=parg;
		break;
	case SSL_CTRL_SET_TLSEXT_TICKET_KEYS:
	case SSL_CTRL_GET_TLSEXT_TICKET_KEYS:
		{
		unsigned char *keys = parg;
		if (!keys)
			return 48;
		if (larg != 48)
			{
			SSLerr(SSL_F_SSL3_CTX_CTRL, SSL_R_INVALID_TICKET_KEYS_LENGTH);
			return 0;
			}
		if (cmd == SSL_CTRL_SET_TLSEXT_TICKET_KEYS)
			{
			memcpy(ctx->tlsext_tick_key_name, keys, 16);
			memcpy(ctx->tlsext_tick_hmac_key, keys + 16, 16);
			memcpy(ctx->tlsext_tick_aes_key, keys + 32, 16);
			}
		else
			{
			memcpy(keys, ctx->tlsext_tick_key_name, 16);
			memcpy(keys + 16, ctx->tlsext_tick_hmac_key, 16);
			memcpy(keys + 32, ctx->tlsext_tick_aes_key, 16);
			}
		return 1;
		}
  
	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB_ARG:
		ctx->tlsext_status_arg=parg;
		return 1;
		break;

#endif /* !OPENSSL_NO_TLSEXT */
	/* A Thawte special :-) */
	case SSL_CTRL_EXTRA_CHAIN_CERT:
		if (ctx->extra_certs == NULL)
			{
			if ((ctx->extra_certs=sk_X509_new_null()) == NULL)
				return(0);
			}
		sk_X509_push(ctx->extra_certs,(X509 *)parg);
		break;

	default:
		return(0);
		}
	return(1);
	}

long ssl3_ctx_callback_ctrl(SSL_CTX *ctx, int cmd, void (*fp)(void))
	{
	CERT *cert;

	cert=ctx->cert;

	switch (cmd)
		{
#ifndef OPENSSL_NO_RSA
	case SSL_CTRL_SET_TMP_RSA_CB:
		{
		cert->rsa_tmp_cb = (RSA *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_DH
	case SSL_CTRL_SET_TMP_DH_CB:
		{
		cert->dh_tmp_cb = (DH *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_ECDH
	case SSL_CTRL_SET_TMP_ECDH_CB:
		{
		cert->ecdh_tmp_cb = (EC_KEY *(*)(SSL *, int, int))fp;
		}
		break;
#endif
#ifndef OPENSSL_NO_TLSEXT
	case SSL_CTRL_SET_TLSEXT_SERVERNAME_CB:
		ctx->tlsext_servername_callback=(int (*)(SSL *,int *,void *))fp;
		break;
  
	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB:
		ctx->tlsext_status_cb=(int (*)(SSL *,void *))fp;
		break;

	case SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB:
		ctx->tlsext_ticket_key_cb=(int (*)(SSL *,unsigned char  *,
						unsigned char *,
						EVP_CIPHER_CTX *,
						HMAC_CTX *, int))fp;
		break;

#endif
	default:
		return(0);
		}
	return(1);
	}

/* This function needs to check if the ciphers required are actually
 * available */
SSL_CIPHER *ssl3_get_cipher_by_char(const unsigned char *p)
	{
	SSL_CIPHER c,*cp;
	unsigned long id;

	id=0x03000000L|((unsigned long)p[0]<<8L)|(unsigned long)p[1];
	c.id=id;
	cp = (SSL_CIPHER *)OBJ_bsearch((char *)&c,
		(char *)ssl3_ciphers,
		SSL3_NUM_CIPHERS,sizeof(SSL_CIPHER),
		FP_ICC ssl_cipher_id_cmp);
	if (cp == NULL || cp->valid == 0)
		return NULL;
	else
		return cp;
	}

int ssl3_put_cipher_by_char(const SSL_CIPHER *c, unsigned char *p)
	{
	long l;

	if (p != NULL)
		{
		l=c->id;
		if ((l & 0xff000000) != 0x03000000) return(0);
		p[0]=((unsigned char)(l>> 8L))&0xFF;
		p[1]=((unsigned char)(l     ))&0xFF;
		}
	return(2);
	}

SSL_CIPHER *ssl3_choose_cipher(SSL *s, STACK_OF(SSL_CIPHER) *clnt,
	     STACK_OF(SSL_CIPHER) *srvr)
	{
	SSL_CIPHER *c,*ret=NULL;
	STACK_OF(SSL_CIPHER) *prio, *allow;
	int i,j,ok;

	CERT *cert;
	unsigned long alg,mask,emask;

	/* Let's see which ciphers we can support */
	cert=s->cert;

#if 0
	/* Do not set the compare functions, because this may lead to a
	 * reordering by "id". We want to keep the original ordering.
	 * We may pay a price in performance during sk_SSL_CIPHER_find(),
	 * but would have to pay with the price of sk_SSL_CIPHER_dup().
	 */
	sk_SSL_CIPHER_set_cmp_func(srvr, ssl_cipher_ptr_id_cmp);
	sk_SSL_CIPHER_set_cmp_func(clnt, ssl_cipher_ptr_id_cmp);
#endif

#ifdef CIPHER_DEBUG
        printf("Server has %d from %p:\n", sk_SSL_CIPHER_num(srvr), srvr);
        for(i=0 ; i < sk_SSL_CIPHER_num(srvr) ; ++i)
	    {
	    c=sk_SSL_CIPHER_value(srvr,i);
	    printf("%p:%s\n",c,c->name);
	    }
        printf("Client sent %d from %p:\n", sk_SSL_CIPHER_num(clnt), clnt);
        for(i=0 ; i < sk_SSL_CIPHER_num(clnt) ; ++i)
	    {
	    c=sk_SSL_CIPHER_value(clnt,i);
	    printf("%p:%s\n",c,c->name);
	    }
#endif

	if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE)
	    {
	    prio = srvr;
	    allow = clnt;
	    }
	else
	    {
	    prio = clnt;
	    allow = srvr;
	    }

	for (i=0; i<sk_SSL_CIPHER_num(prio); i++)
		{
		c=sk_SSL_CIPHER_value(prio,i);

		ssl_set_cert_masks(cert,c);
		mask=cert->mask;
		emask=cert->export_mask;
			
#ifdef KSSL_DEBUG
		printf("ssl3_choose_cipher %d alg= %lx\n", i,c->algorithms);
#endif    /* KSSL_DEBUG */

		alg=c->algorithms&(SSL_MKEY_MASK|SSL_AUTH_MASK);
#ifndef OPENSSL_NO_KRB5
                if (alg & SSL_KRB5) 
                        {
                        if ( !kssl_keytab_is_available(s->kssl_ctx) )
                            continue;
                        }
#endif /* OPENSSL_NO_KRB5 */
		if (SSL_C_IS_EXPORT(c))
			{
			ok=((alg & emask) == alg)?1:0;
#ifdef CIPHER_DEBUG
			printf("%d:[%08lX:%08lX]%p:%s (export)\n",ok,alg,emask,
			       c,c->name);
#endif
			}
		else
			{
			ok=((alg & mask) == alg)?1:0;
#ifdef CIPHER_DEBUG
			printf("%d:[%08lX:%08lX]%p:%s\n",ok,alg,mask,c,
			       c->name);
#endif
			}

		if (!ok) continue;
		j=sk_SSL_CIPHER_find(allow,c);
		if (j >= 0)
			{
			ret=sk_SSL_CIPHER_value(allow,j);
			break;
			}
		}
	return(ret);
	}

int ssl3_get_req_cert_type(SSL *s, unsigned char *p)
	{
	int ret=0;
	unsigned long alg;

	alg=s->s3->tmp.new_cipher->algorithms;

#ifndef OPENSSL_NO_DH
	if (alg & (SSL_kDHr|SSL_kEDH))
		{
#  ifndef OPENSSL_NO_RSA
		p[ret++]=SSL3_CT_RSA_FIXED_DH;
#  endif
#  ifndef OPENSSL_NO_DSA
		p[ret++]=SSL3_CT_DSS_FIXED_DH;
#  endif
		}
	if ((s->version == SSL3_VERSION) &&
		(alg & (SSL_kEDH|SSL_kDHd|SSL_kDHr)))
		{
#  ifndef OPENSSL_NO_RSA
		p[ret++]=SSL3_CT_RSA_EPHEMERAL_DH;
#  endif
#  ifndef OPENSSL_NO_DSA
		p[ret++]=SSL3_CT_DSS_EPHEMERAL_DH;
#  endif
		}
#endif /* !OPENSSL_NO_DH */
#ifndef OPENSSL_NO_RSA
	p[ret++]=SSL3_CT_RSA_SIGN;
#endif
#ifndef OPENSSL_NO_DSA
	p[ret++]=SSL3_CT_DSS_SIGN;
#endif
#ifndef OPENSSL_NO_ECDH
	/* We should ask for fixed ECDH certificates only
	 * for SSL_kECDH (and not SSL_kECDHE)
	 */
	if ((alg & SSL_kECDH) && (s->version >= TLS1_VERSION))
		{
		p[ret++]=TLS_CT_RSA_FIXED_ECDH;
		p[ret++]=TLS_CT_ECDSA_FIXED_ECDH;
		}
#endif

#ifndef OPENSSL_NO_ECDSA
	/* ECDSA certs can be used with RSA cipher suites as well 
	 * so we don't need to check for SSL_kECDH or SSL_kECDHE
	 */
	if (s->version >= TLS1_VERSION)
		{
		p[ret++]=TLS_CT_ECDSA_SIGN;
		}
#endif	
	return(ret);
	}

int ssl3_shutdown(SSL *s)
	{
	int ret;

	/* Don't do anything much if we have not done the handshake or
	 * we don't want to send messages :-) */
	if ((s->quiet_shutdown) || (s->state == SSL_ST_BEFORE))
		{
		s->shutdown=(SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
		return(1);
		}

	if (!(s->shutdown & SSL_SENT_SHUTDOWN))
		{
		s->shutdown|=SSL_SENT_SHUTDOWN;
#if 1
		ssl3_send_alert(s,SSL3_AL_WARNING,SSL_AD_CLOSE_NOTIFY);
#endif
		/* our shutdown alert has been sent now, and if it still needs
	 	 * to be written, s->s3->alert_dispatch will be true */
	 	if (s->s3->alert_dispatch)
	 		return(-1);	/* return WANT_WRITE */
		}
	else if (s->s3->alert_dispatch)
		{
		/* resend it if not sent */
#if 1
		ret=s->method->ssl_dispatch_alert(s);
		if(ret == -1)
			{
			/* we only get to return -1 here the 2nd/Nth
			 * invocation, we must  have already signalled
			 * return 0 upon a previous invoation,
			 * return WANT_WRITE */
			return(ret);
			}
#endif
		}
	else if (!(s->shutdown & SSL_RECEIVED_SHUTDOWN))
		{
		/* If we are waiting for a close from our peer, we are closed */
		s->method->ssl_read_bytes(s,0,NULL,0,0);
		if(!(s->shutdown & SSL_RECEIVED_SHUTDOWN))
			{
			return(-1);	/* return WANT_READ */
			}
		}

	if ((s->shutdown == (SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN)) &&
		!s->s3->alert_dispatch)
		return(1);
	else
		return(0);
	}

int ssl3_write(SSL *s, const void *buf, int len)
	{
	int ret,n;

#if 0
	if (s->shutdown & SSL_SEND_SHUTDOWN)
		{
		s->rwstate=SSL_NOTHING;
		return(0);
		}
#endif
	clear_sys_error();
	if (s->s3->renegotiate) ssl3_renegotiate_check(s);

	/* This is an experimental flag that sends the
	 * last handshake message in the same packet as the first
	 * use data - used to see if it helps the TCP protocol during
	 * session-id reuse */
	/* The second test is because the buffer may have been removed */
	if ((s->s3->flags & SSL3_FLAGS_POP_BUFFER) && (s->wbio == s->bbio))
		{
		/* First time through, we write into the buffer */
		if (s->s3->delay_buf_pop_ret == 0)
			{
			ret=ssl3_write_bytes(s,SSL3_RT_APPLICATION_DATA,
					     buf,len);
			if (ret <= 0) return(ret);

			s->s3->delay_buf_pop_ret=ret;
			}

		s->rwstate=SSL_WRITING;
		n=BIO_flush(s->wbio);
		if (n <= 0) return(n);
		s->rwstate=SSL_NOTHING;

		/* We have flushed the buffer, so remove it */
		ssl_free_wbio_buffer(s);
		s->s3->flags&= ~SSL3_FLAGS_POP_BUFFER;

		ret=s->s3->delay_buf_pop_ret;
		s->s3->delay_buf_pop_ret=0;
		}
	else
		{
		ret=s->method->ssl_write_bytes(s,SSL3_RT_APPLICATION_DATA,
			buf,len);
		if (ret <= 0) return(ret);
		}

	return(ret);
	}

static int ssl3_read_internal(SSL *s, void *buf, int len, int peek)
	{
	int n,ret;
	
	clear_sys_error();
	if ((s->s3->flags & SSL3_FLAGS_POP_BUFFER) && (s->wbio == s->bbio))
		{
		/* Deal with an application that calls SSL_read() when handshake data
		 * is yet to be written.
		 */
		if (BIO_wpending(s->wbio) > 0)
			{
			s->rwstate=SSL_WRITING;
			n=BIO_flush(s->wbio);
			if (n <= 0) return(n);
			s->rwstate=SSL_NOTHING;
			}
		}
	if (s->s3->renegotiate) ssl3_renegotiate_check(s);
	s->s3->in_read_app_data=1;
	ret=s->method->ssl_read_bytes(s,SSL3_RT_APPLICATION_DATA,buf,len,peek);
	if ((ret == -1) && (s->s3->in_read_app_data == 2))
		{
		/* ssl3_read_bytes decided to call s->handshake_func, which
		 * called ssl3_read_bytes to read handshake data.
		 * However, ssl3_read_bytes actually found application data
		 * and thinks that application data makes sense here; so disable
		 * handshake processing and try to read application data again. */
		s->in_handshake++;
		ret=s->method->ssl_read_bytes(s,SSL3_RT_APPLICATION_DATA,buf,len,peek);
		s->in_handshake--;
		}
	else
		s->s3->in_read_app_data=0;

	return(ret);
	}

int ssl3_read(SSL *s, void *buf, int len)
	{
	return ssl3_read_internal(s, buf, len, 0);
	}

int ssl3_peek(SSL *s, void *buf, int len)
	{
	return ssl3_read_internal(s, buf, len, 1);
	}

int ssl3_renegotiate(SSL *s)
	{
	if (s->handshake_func == NULL)
		return(1);

	if (s->s3->flags & SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS)
		return(0);

	s->s3->renegotiate=1;
	return(1);
	}

int ssl3_renegotiate_check(SSL *s)
	{
	int ret=0;

	if (s->s3->renegotiate)
		{
		if (	(s->s3->rbuf.left == 0) &&
			(s->s3->wbuf.left == 0) &&
			!SSL_in_init(s))
			{
/*
if we are the server, and we have sent a 'RENEGOTIATE' message, we
need to go to SSL_ST_ACCEPT.
*/
			/* SSL_ST_ACCEPT */
			s->state=SSL_ST_RENEGOTIATE;
			s->s3->renegotiate=0;
			s->s3->num_renegotiations++;
			s->s3->total_renegotiations++;
			ret=1;
			}
		}
	return(ret);
	}

