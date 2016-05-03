/* ssl/ssl_locl.h */
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

#ifndef HEADER_SSL_LOCL_H
# define HEADER_SSL_LOCL_H
# include <stdlib.h>
# include <time.h>
# include <string.h>
# include <errno.h>

# include "e_os.h"

# include <openssl/buffer.h>
# ifndef OPENSSL_NO_COMP
#  include <openssl/comp.h>
# endif
# include <openssl/bio.h>
# include <openssl/stack.h>
# ifndef OPENSSL_NO_RSA
#  include <openssl/rsa.h>
# endif
# ifndef OPENSSL_NO_DSA
#  include <openssl/dsa.h>
# endif
# include <openssl/err.h>
# include <openssl/ssl.h>
# include <openssl/symhacks.h>

# ifdef OPENSSL_BUILD_SHLIBSSL
#  undef OPENSSL_EXTERN
#  define OPENSSL_EXTERN OPENSSL_EXPORT
# endif

# undef PKCS1_CHECK

# define c2l(c,l)        (l = ((unsigned long)(*((c)++)))     , \
                         l|=(((unsigned long)(*((c)++)))<< 8), \
                         l|=(((unsigned long)(*((c)++)))<<16), \
                         l|=(((unsigned long)(*((c)++)))<<24))

/* NOTE - c is not incremented as per c2l */
# define c2ln(c,l1,l2,n) { \
                        c+=n; \
                        l1=l2=0; \
                        switch (n) { \
                        case 8: l2 =((unsigned long)(*(--(c))))<<24; \
                        case 7: l2|=((unsigned long)(*(--(c))))<<16; \
                        case 6: l2|=((unsigned long)(*(--(c))))<< 8; \
                        case 5: l2|=((unsigned long)(*(--(c))));     \
                        case 4: l1 =((unsigned long)(*(--(c))))<<24; \
                        case 3: l1|=((unsigned long)(*(--(c))))<<16; \
                        case 2: l1|=((unsigned long)(*(--(c))))<< 8; \
                        case 1: l1|=((unsigned long)(*(--(c))));     \
                                } \
                        }

# define l2c(l,c)        (*((c)++)=(unsigned char)(((l)    )&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>24)&0xff))

# define n2l(c,l)        (l =((unsigned long)(*((c)++)))<<24, \
                         l|=((unsigned long)(*((c)++)))<<16, \
                         l|=((unsigned long)(*((c)++)))<< 8, \
                         l|=((unsigned long)(*((c)++))))

# define l2n(l,c)        (*((c)++)=(unsigned char)(((l)>>24)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
                         *((c)++)=(unsigned char)(((l)    )&0xff))

# define l2n6(l,c)       (*((c)++)=(unsigned char)(((l)>>40)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>32)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>24)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
                         *((c)++)=(unsigned char)(((l)    )&0xff))

# define l2n8(l,c)       (*((c)++)=(unsigned char)(((l)>>56)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>48)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>40)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>32)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>24)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
                         *((c)++)=(unsigned char)(((l)    )&0xff))

# define n2l6(c,l)       (l =((BN_ULLONG)(*((c)++)))<<40, \
                         l|=((BN_ULLONG)(*((c)++)))<<32, \
                         l|=((BN_ULLONG)(*((c)++)))<<24, \
                         l|=((BN_ULLONG)(*((c)++)))<<16, \
                         l|=((BN_ULLONG)(*((c)++)))<< 8, \
                         l|=((BN_ULLONG)(*((c)++))))

/* NOTE - c is not incremented as per l2c */
# define l2cn(l1,l2,c,n) { \
                        c+=n; \
                        switch (n) { \
                        case 8: *(--(c))=(unsigned char)(((l2)>>24)&0xff); \
                        case 7: *(--(c))=(unsigned char)(((l2)>>16)&0xff); \
                        case 6: *(--(c))=(unsigned char)(((l2)>> 8)&0xff); \
                        case 5: *(--(c))=(unsigned char)(((l2)    )&0xff); \
                        case 4: *(--(c))=(unsigned char)(((l1)>>24)&0xff); \
                        case 3: *(--(c))=(unsigned char)(((l1)>>16)&0xff); \
                        case 2: *(--(c))=(unsigned char)(((l1)>> 8)&0xff); \
                        case 1: *(--(c))=(unsigned char)(((l1)    )&0xff); \
                                } \
                        }

# define n2s(c,s)        ((s=(((unsigned int)(c[0]))<< 8)| \
                            (((unsigned int)(c[1]))    )),c+=2)
# define s2n(s,c)        ((c[0]=(unsigned char)(((s)>> 8)&0xff), \
                          c[1]=(unsigned char)(((s)    )&0xff)),c+=2)

# define n2l3(c,l)       ((l =(((unsigned long)(c[0]))<<16)| \
                             (((unsigned long)(c[1]))<< 8)| \
                             (((unsigned long)(c[2]))    )),c+=3)

# define l2n3(l,c)       ((c[0]=(unsigned char)(((l)>>16)&0xff), \
                          c[1]=(unsigned char)(((l)>> 8)&0xff), \
                          c[2]=(unsigned char)(((l)    )&0xff)),c+=3)

/* LOCAL STUFF */

# define SSL_DECRYPT     0
# define SSL_ENCRYPT     1

# define TWO_BYTE_BIT    0x80
# define SEC_ESC_BIT     0x40
# define TWO_BYTE_MASK   0x7fff
# define THREE_BYTE_MASK 0x3fff

# define INC32(a)        ((a)=((a)+1)&0xffffffffL)
# define DEC32(a)        ((a)=((a)-1)&0xffffffffL)
# define MAX_MAC_SIZE    20     /* up from 16 for SSLv3 */

/*
 * Define the Bitmasks for SSL_CIPHER.algorithms.
 * This bits are used packed as dense as possible. If new methods/ciphers
 * etc will be added, the bits a likely to change, so this information
 * is for internal library use only, even though SSL_CIPHER.algorithms
 * can be publicly accessed.
 * Use the according functions for cipher management instead.
 *
 * The bit mask handling in the selection and sorting scheme in
 * ssl_create_cipher_list() has only limited capabilities, reflecting
 * that the different entities within are mutually exclusive:
 * ONLY ONE BIT PER MASK CAN BE SET AT A TIME.
 */

/* Bits for algorithm_mkey (key exchange algorithm) */
/* RSA key exchange */
# define SSL_kRSA                0x00000001L
/* DH cert, RSA CA cert */
# define SSL_kDHr                0x00000002L
/* DH cert, DSA CA cert */
# define SSL_kDHd                0x00000004L
/* tmp DH key no DH cert */
# define SSL_kEDH                0x00000008L
/* forward-compatible synonym */
# define SSL_kDHE                SSL_kEDH
/* Kerberos5 key exchange */
# define SSL_kKRB5               0x00000010L
/* ECDH cert, RSA CA cert */
# define SSL_kECDHr              0x00000020L
/* ECDH cert, ECDSA CA cert */
# define SSL_kECDHe              0x00000040L
/* ephemeral ECDH */
# define SSL_kEECDH              0x00000080L
/* forward-compatible synonym */
# define SSL_kECDHE              SSL_kEECDH
/* PSK */
# define SSL_kPSK                0x00000100L
/* GOST key exchange */
# define SSL_kGOST       0x00000200L
/* SRP */
# define SSL_kSRP        0x00000400L

/* Bits for algorithm_auth (server authentication) */
/* RSA auth */
# define SSL_aRSA                0x00000001L
/* DSS auth */
# define SSL_aDSS                0x00000002L
/* no auth (i.e. use ADH or AECDH) */
# define SSL_aNULL               0x00000004L
/* Fixed DH auth (kDHd or kDHr) */
# define SSL_aDH                 0x00000008L
/* Fixed ECDH auth (kECDHe or kECDHr) */
# define SSL_aECDH               0x00000010L
/* KRB5 auth */
# define SSL_aKRB5               0x00000020L
/* ECDSA auth*/
# define SSL_aECDSA              0x00000040L
/* PSK auth */
# define SSL_aPSK                0x00000080L
/* GOST R 34.10-94 signature auth */
# define SSL_aGOST94                             0x00000100L
/* GOST R 34.10-2001 signature auth */
# define SSL_aGOST01                     0x00000200L
/* SRP auth */
# define SSL_aSRP                0x00000400L

/* Bits for algorithm_enc (symmetric encryption) */
# define SSL_DES                 0x00000001L
# define SSL_3DES                0x00000002L
# define SSL_RC4                 0x00000004L
# define SSL_RC2                 0x00000008L
# define SSL_IDEA                0x00000010L
# define SSL_eNULL               0x00000020L
# define SSL_AES128              0x00000040L
# define SSL_AES256              0x00000080L
# define SSL_CAMELLIA128         0x00000100L
# define SSL_CAMELLIA256         0x00000200L
# define SSL_eGOST2814789CNT     0x00000400L
# define SSL_SEED                0x00000800L
# define SSL_AES128GCM           0x00001000L
# define SSL_AES256GCM           0x00002000L

# define SSL_AES                 (SSL_AES128|SSL_AES256|SSL_AES128GCM|SSL_AES256GCM)
# define SSL_CAMELLIA            (SSL_CAMELLIA128|SSL_CAMELLIA256)

/* Bits for algorithm_mac (symmetric authentication) */

# define SSL_MD5                 0x00000001L
# define SSL_SHA1                0x00000002L
# define SSL_GOST94      0x00000004L
# define SSL_GOST89MAC   0x00000008L
# define SSL_SHA256              0x00000010L
# define SSL_SHA384              0x00000020L
/* Not a real MAC, just an indication it is part of cipher */
# define SSL_AEAD                0x00000040L

/* Bits for algorithm_ssl (protocol version) */
# define SSL_SSLV2               0x00000001UL
# define SSL_SSLV3               0x00000002UL
# define SSL_TLSV1               SSL_SSLV3/* for now */
# define SSL_TLSV1_2             0x00000004UL

/* Bits for algorithm2 (handshake digests and other extra flags) */

# define SSL_HANDSHAKE_MAC_MD5 0x10
# define SSL_HANDSHAKE_MAC_SHA 0x20
# define SSL_HANDSHAKE_MAC_GOST94 0x40
# define SSL_HANDSHAKE_MAC_SHA256 0x80
# define SSL_HANDSHAKE_MAC_SHA384 0x100
# define SSL_HANDSHAKE_MAC_DEFAULT (SSL_HANDSHAKE_MAC_MD5 | SSL_HANDSHAKE_MAC_SHA)

/*
 * When adding new digest in the ssl_ciph.c and increment SSM_MD_NUM_IDX make
 * sure to update this constant too
 */
# define SSL_MAX_DIGEST 6

# define TLS1_PRF_DGST_MASK      (0xff << TLS1_PRF_DGST_SHIFT)

# define TLS1_PRF_DGST_SHIFT 10
# define TLS1_PRF_MD5 (SSL_HANDSHAKE_MAC_MD5 << TLS1_PRF_DGST_SHIFT)
# define TLS1_PRF_SHA1 (SSL_HANDSHAKE_MAC_SHA << TLS1_PRF_DGST_SHIFT)
# define TLS1_PRF_SHA256 (SSL_HANDSHAKE_MAC_SHA256 << TLS1_PRF_DGST_SHIFT)
# define TLS1_PRF_SHA384 (SSL_HANDSHAKE_MAC_SHA384 << TLS1_PRF_DGST_SHIFT)
# define TLS1_PRF_GOST94 (SSL_HANDSHAKE_MAC_GOST94 << TLS1_PRF_DGST_SHIFT)
# define TLS1_PRF (TLS1_PRF_MD5 | TLS1_PRF_SHA1)

/*
 * Stream MAC for GOST ciphersuites from cryptopro draft (currently this also
 * goes into algorithm2)
 */
# define TLS1_STREAM_MAC 0x04

/*
 * Export and cipher strength information. For each cipher we have to decide
 * whether it is exportable or not. This information is likely to change
 * over time, since the export control rules are no static technical issue.
 *
 * Independent of the export flag the cipher strength is sorted into classes.
 * SSL_EXP40 was denoting the 40bit US export limit of past times, which now
 * is at 56bit (SSL_EXP56). If the exportable cipher class is going to change
 * again (eg. to 64bit) the use of "SSL_EXP*" becomes blurred even more,
 * since SSL_EXP64 could be similar to SSL_LOW.
 * For this reason SSL_MICRO and SSL_MINI macros are included to widen the
 * namespace of SSL_LOW-SSL_HIGH to lower values. As development of speed
 * and ciphers goes, another extension to SSL_SUPER and/or SSL_ULTRA would
 * be possible.
 */
# define SSL_EXP_MASK            0x00000003L
# define SSL_STRONG_MASK         0x000001fcL

# define SSL_NOT_EXP             0x00000001L
# define SSL_EXPORT              0x00000002L

# define SSL_STRONG_NONE         0x00000004L
# define SSL_EXP40               0x00000008L
# define SSL_MICRO               (SSL_EXP40)
# define SSL_EXP56               0x00000010L
# define SSL_MINI                (SSL_EXP56)
# define SSL_LOW                 0x00000020L
# define SSL_MEDIUM              0x00000040L
# define SSL_HIGH                0x00000080L
# define SSL_FIPS                0x00000100L
# define SSL_NOT_DEFAULT         0x00000200L

/* we have used 000003ff - 22 bits left to go */

/*-
 * Macros to check the export status and cipher strength for export ciphers.
 * Even though the macros for EXPORT and EXPORT40/56 have similar names,
 * their meaning is different:
 * *_EXPORT macros check the 'exportable' status.
 * *_EXPORT40/56 macros are used to check whether a certain cipher strength
 *          is given.
 * Since the SSL_IS_EXPORT* and SSL_EXPORT* macros depend on the correct
 * algorithm structure element to be passed (algorithms, algo_strength) and no
 * typechecking can be done as they are all of type unsigned long, their
 * direct usage is discouraged.
 * Use the SSL_C_* macros instead.
 */
# define SSL_IS_EXPORT(a)        ((a)&SSL_EXPORT)
# define SSL_IS_EXPORT56(a)      ((a)&SSL_EXP56)
# define SSL_IS_EXPORT40(a)      ((a)&SSL_EXP40)
# define SSL_C_IS_EXPORT(c)      SSL_IS_EXPORT((c)->algo_strength)
# define SSL_C_IS_EXPORT56(c)    SSL_IS_EXPORT56((c)->algo_strength)
# define SSL_C_IS_EXPORT40(c)    SSL_IS_EXPORT40((c)->algo_strength)

# define SSL_EXPORT_KEYLENGTH(a,s)       (SSL_IS_EXPORT40(s) ? 5 : \
                                 (a) == SSL_DES ? 8 : 7)
# define SSL_EXPORT_PKEYLENGTH(a) (SSL_IS_EXPORT40(a) ? 512 : 1024)
# define SSL_C_EXPORT_KEYLENGTH(c)       SSL_EXPORT_KEYLENGTH((c)->algorithm_enc, \
                                (c)->algo_strength)
# define SSL_C_EXPORT_PKEYLENGTH(c)      SSL_EXPORT_PKEYLENGTH((c)->algo_strength)

/* Check if an SSL structure is using DTLS */
# define SSL_IS_DTLS(s)  (s->method->ssl3_enc->enc_flags & SSL_ENC_FLAG_DTLS)
/* See if we need explicit IV */
# define SSL_USE_EXPLICIT_IV(s)  \
                (s->method->ssl3_enc->enc_flags & SSL_ENC_FLAG_EXPLICIT_IV)
/*
 * See if we use signature algorithms extension and signature algorithm
 * before signatures.
 */
# define SSL_USE_SIGALGS(s)      \
                        (s->method->ssl3_enc->enc_flags & SSL_ENC_FLAG_SIGALGS)
/*
 * Allow TLS 1.2 ciphersuites: applies to DTLS 1.2 as well as TLS 1.2: may
 * apply to others in future.
 */
# define SSL_USE_TLS1_2_CIPHERS(s)       \
                (s->method->ssl3_enc->enc_flags & SSL_ENC_FLAG_TLS1_2_CIPHERS)
/*
 * Determine if a client can use TLS 1.2 ciphersuites: can't rely on method
 * flags because it may not be set to correct version yet.
 */
# define SSL_CLIENT_USE_TLS1_2_CIPHERS(s)        \
                ((SSL_IS_DTLS(s) && s->client_version <= DTLS1_2_VERSION) || \
                (!SSL_IS_DTLS(s) && s->client_version >= TLS1_2_VERSION))

/* Mostly for SSLv3 */
# define SSL_PKEY_RSA_ENC        0
# define SSL_PKEY_RSA_SIGN       1
# define SSL_PKEY_DSA_SIGN       2
# define SSL_PKEY_DH_RSA         3
# define SSL_PKEY_DH_DSA         4
# define SSL_PKEY_ECC            5
# define SSL_PKEY_GOST94         6
# define SSL_PKEY_GOST01         7
# define SSL_PKEY_NUM            8

/*-
 * SSL_kRSA <- RSA_ENC | (RSA_TMP & RSA_SIGN) |
 *          <- (EXPORT & (RSA_ENC | RSA_TMP) & RSA_SIGN)
 * SSL_kDH  <- DH_ENC & (RSA_ENC | RSA_SIGN | DSA_SIGN)
 * SSL_kEDH <- RSA_ENC | RSA_SIGN | DSA_SIGN
 * SSL_aRSA <- RSA_ENC | RSA_SIGN
 * SSL_aDSS <- DSA_SIGN
 */

/*-
#define CERT_INVALID            0
#define CERT_PUBLIC_KEY         1
#define CERT_PRIVATE_KEY        2
*/

# ifndef OPENSSL_NO_EC
/*
 * From ECC-TLS draft, used in encoding the curve type in ECParameters
 */
#  define EXPLICIT_PRIME_CURVE_TYPE  1
#  define EXPLICIT_CHAR2_CURVE_TYPE  2
#  define NAMED_CURVE_TYPE           3
# endif                         /* OPENSSL_NO_EC */

typedef struct cert_pkey_st {
    X509 *x509;
    EVP_PKEY *privatekey;
    /* Digest to use when signing */
    const EVP_MD *digest;
    /* Chain for this certificate */
    STACK_OF(X509) *chain;
# ifndef OPENSSL_NO_TLSEXT
    /*-
     * serverinfo data for this certificate.  The data is in TLS Extension
     * wire format, specifically it's a series of records like:
     *   uint16_t extension_type; // (RFC 5246, 7.4.1.4, Extension)
     *   uint16_t length;
     *   uint8_t data[length];
     */
    unsigned char *serverinfo;
    size_t serverinfo_length;
# endif
    /*
     * Set if CERT_PKEY can be used with current SSL session: e.g.
     * appropriate curve, signature algorithms etc. If zero it can't be used
     * at all.
     */
    int valid_flags;
} CERT_PKEY;
/* Retrieve Suite B flags */
# define tls1_suiteb(s)  (s->cert->cert_flags & SSL_CERT_FLAG_SUITEB_128_LOS)
/* Uses to check strict mode: suite B modes are always strict */
# define SSL_CERT_FLAGS_CHECK_TLS_STRICT \
        (SSL_CERT_FLAG_SUITEB_128_LOS|SSL_CERT_FLAG_TLS_STRICT)

typedef struct {
    unsigned short ext_type;
    /*
     * Per-connection flags relating to this extension type: not used if
     * part of an SSL_CTX structure.
     */
    unsigned short ext_flags;
    custom_ext_add_cb add_cb;
    custom_ext_free_cb free_cb;
    void *add_arg;
    custom_ext_parse_cb parse_cb;
    void *parse_arg;
} custom_ext_method;

/* ext_flags values */

/*
 * Indicates an extension has been received. Used to check for unsolicited or
 * duplicate extensions.
 */
# define SSL_EXT_FLAG_RECEIVED   0x1
/*
 * Indicates an extension has been sent: used to enable sending of
 * corresponding ServerHello extension.
 */
# define SSL_EXT_FLAG_SENT       0x2

typedef struct {
    custom_ext_method *meths;
    size_t meths_count;
} custom_ext_methods;

typedef struct cert_st {
    /* Current active set */
    /*
     * ALWAYS points to an element of the pkeys array
     * Probably it would make more sense to store
     * an index, not a pointer.
     */
    CERT_PKEY *key;
    /*
     * For servers the following masks are for the key and auth algorithms
     * that are supported by the certs below. For clients they are masks of
     * *disabled* algorithms based on the current session.
     */
    int valid;
    unsigned long mask_k;
    unsigned long mask_a;
    unsigned long export_mask_k;
    unsigned long export_mask_a;
    /* Client only */
    unsigned long mask_ssl;
# ifndef OPENSSL_NO_RSA
    RSA *rsa_tmp;
    RSA *(*rsa_tmp_cb) (SSL *ssl, int is_export, int keysize);
# endif
# ifndef OPENSSL_NO_DH
    DH *dh_tmp;
    DH *(*dh_tmp_cb) (SSL *ssl, int is_export, int keysize);
# endif
# ifndef OPENSSL_NO_ECDH
    EC_KEY *ecdh_tmp;
    /* Callback for generating ephemeral ECDH keys */
    EC_KEY *(*ecdh_tmp_cb) (SSL *ssl, int is_export, int keysize);
    /* Select ECDH parameters automatically */
    int ecdh_tmp_auto;
# endif
    /* Flags related to certificates */
    unsigned int cert_flags;
    CERT_PKEY pkeys[SSL_PKEY_NUM];
    /*
     * Certificate types (received or sent) in certificate request message.
     * On receive this is only set if number of certificate types exceeds
     * SSL3_CT_NUMBER.
     */
    unsigned char *ctypes;
    size_t ctype_num;
    /*
     * signature algorithms peer reports: e.g. supported signature algorithms
     * extension for server or as part of a certificate request for client.
     */
    unsigned char *peer_sigalgs;
    /* Size of above array */
    size_t peer_sigalgslen;
    /*
     * suppported signature algorithms. When set on a client this is sent in
     * the client hello as the supported signature algorithms extension. For
     * servers it represents the signature algorithms we are willing to use.
     */
    unsigned char *conf_sigalgs;
    /* Size of above array */
    size_t conf_sigalgslen;
    /*
     * Client authentication signature algorithms, if not set then uses
     * conf_sigalgs. On servers these will be the signature algorithms sent
     * to the client in a cerificate request for TLS 1.2. On a client this
     * represents the signature algortithms we are willing to use for client
     * authentication.
     */
    unsigned char *client_sigalgs;
    /* Size of above array */
    size_t client_sigalgslen;
    /*
     * Signature algorithms shared by client and server: cached because these
     * are used most often.
     */
    TLS_SIGALGS *shared_sigalgs;
    size_t shared_sigalgslen;
    /*
     * Certificate setup callback: if set is called whenever a certificate
     * may be required (client or server). the callback can then examine any
     * appropriate parameters and setup any certificates required. This
     * allows advanced applications to select certificates on the fly: for
     * example based on supported signature algorithms or curves.
     */
    int (*cert_cb) (SSL *ssl, void *arg);
    void *cert_cb_arg;
    /*
     * Optional X509_STORE for chain building or certificate validation If
     * NULL the parent SSL_CTX store is used instead.
     */
    X509_STORE *chain_store;
    X509_STORE *verify_store;
    /* Raw values of the cipher list from a client */
    unsigned char *ciphers_raw;
    size_t ciphers_rawlen;
    /* Custom extension methods for server and client */
    custom_ext_methods cli_ext;
    custom_ext_methods srv_ext;
    int references;             /* >1 only if SSL_copy_session_id is used */
    /* non-optimal, but here due to compatibility */
    unsigned char *alpn_proposed;   /* server */
    unsigned int alpn_proposed_len;
    int alpn_sent;                  /* client */
} CERT;

typedef struct sess_cert_st {
    STACK_OF(X509) *cert_chain; /* as received from peer (not for SSL2) */
    /* The 'peer_...' members are used only by clients. */
    int peer_cert_type;
    CERT_PKEY *peer_key;        /* points to an element of peer_pkeys (never
                                 * NULL!) */
    CERT_PKEY peer_pkeys[SSL_PKEY_NUM];
    /*
     * Obviously we don't have the private keys of these, so maybe we
     * shouldn't even use the CERT_PKEY type here.
     */
# ifndef OPENSSL_NO_RSA
    RSA *peer_rsa_tmp;          /* not used for SSL 2 */
# endif
# ifndef OPENSSL_NO_DH
    DH *peer_dh_tmp;            /* not used for SSL 2 */
# endif
# ifndef OPENSSL_NO_ECDH
    EC_KEY *peer_ecdh_tmp;
# endif
    int references;             /* actually always 1 at the moment */
} SESS_CERT;
/* Structure containing decoded values of signature algorithms extension */
struct tls_sigalgs_st {
    /* NID of hash algorithm */
    int hash_nid;
    /* NID of signature algorithm */
    int sign_nid;
    /* Combined hash and signature NID */
    int signandhash_nid;
    /* Raw values used in extension */
    unsigned char rsign;
    unsigned char rhash;
};

/*
 * #define MAC_DEBUG
 */

/*
 * #define ERR_DEBUG
 */
/*
 * #define ABORT_DEBUG
 */
/*
 * #define PKT_DEBUG 1
 */
/*
 * #define DES_DEBUG
 */
/*
 * #define DES_OFB_DEBUG
 */
/*
 * #define SSL_DEBUG
 */
/*
 * #define RSA_DEBUG
 */
/*
 * #define IDEA_DEBUG
 */

# define FP_ICC  (int (*)(const void *,const void *))
# define ssl_put_cipher_by_char(ssl,ciph,ptr) \
                ((ssl)->method->put_cipher_by_char((ciph),(ptr)))

/*
 * This is for the SSLv3/TLSv1.0 differences in crypto/hash stuff It is a bit
 * of a mess of functions, but hell, think of it as an opaque structure :-)
 */
typedef struct ssl3_enc_method {
    int (*enc) (SSL *, int);
    int (*mac) (SSL *, unsigned char *, int);
    int (*setup_key_block) (SSL *);
    int (*generate_master_secret) (SSL *, unsigned char *, unsigned char *,
                                   int);
    int (*change_cipher_state) (SSL *, int);
    int (*final_finish_mac) (SSL *, const char *, int, unsigned char *);
    int finish_mac_length;
    int (*cert_verify_mac) (SSL *, int, unsigned char *);
    const char *client_finished_label;
    int client_finished_label_len;
    const char *server_finished_label;
    int server_finished_label_len;
    int (*alert_value) (int);
    int (*export_keying_material) (SSL *, unsigned char *, size_t,
                                   const char *, size_t,
                                   const unsigned char *, size_t,
                                   int use_context);
    /* Various flags indicating protocol version requirements */
    unsigned int enc_flags;
    /* Handshake header length */
    unsigned int hhlen;
    /* Set the handshake header */
    void (*set_handshake_header) (SSL *s, int type, unsigned long len);
    /* Write out handshake message */
    int (*do_write) (SSL *s);
} SSL3_ENC_METHOD;

# define SSL_HM_HEADER_LENGTH(s) s->method->ssl3_enc->hhlen
# define ssl_handshake_start(s) \
        (((unsigned char *)s->init_buf->data) + s->method->ssl3_enc->hhlen)
# define ssl_set_handshake_header(s, htype, len) \
        s->method->ssl3_enc->set_handshake_header(s, htype, len)
# define ssl_do_write(s)  s->method->ssl3_enc->do_write(s)

/* Values for enc_flags */

/* Uses explicit IV for CBC mode */
# define SSL_ENC_FLAG_EXPLICIT_IV        0x1
/* Uses signature algorithms extension */
# define SSL_ENC_FLAG_SIGALGS            0x2
/* Uses SHA256 default PRF */
# define SSL_ENC_FLAG_SHA256_PRF         0x4
/* Is DTLS */
# define SSL_ENC_FLAG_DTLS               0x8
/*
 * Allow TLS 1.2 ciphersuites: applies to DTLS 1.2 as well as TLS 1.2: may
 * apply to others in future.
 */
# define SSL_ENC_FLAG_TLS1_2_CIPHERS     0x10

# ifndef OPENSSL_NO_COMP
/* Used for holding the relevant compression methods loaded into SSL_CTX */
typedef struct ssl3_comp_st {
    int comp_id;                /* The identifier byte for this compression
                                 * type */
    char *name;                 /* Text name used for the compression type */
    COMP_METHOD *method;        /* The method :-) */
} SSL3_COMP;
# endif

# ifndef OPENSSL_NO_BUF_FREELISTS
typedef struct ssl3_buf_freelist_st {
    size_t chunklen;
    unsigned int len;
    struct ssl3_buf_freelist_entry_st *head;
} SSL3_BUF_FREELIST;

typedef struct ssl3_buf_freelist_entry_st {
    struct ssl3_buf_freelist_entry_st *next;
} SSL3_BUF_FREELIST_ENTRY;
# endif

extern SSL3_ENC_METHOD ssl3_undef_enc_method;
OPENSSL_EXTERN const SSL_CIPHER ssl2_ciphers[];
OPENSSL_EXTERN SSL_CIPHER ssl3_ciphers[];

SSL_METHOD *ssl_bad_method(int ver);

extern SSL3_ENC_METHOD TLSv1_enc_data;
extern SSL3_ENC_METHOD TLSv1_1_enc_data;
extern SSL3_ENC_METHOD TLSv1_2_enc_data;
extern SSL3_ENC_METHOD SSLv3_enc_data;
extern SSL3_ENC_METHOD DTLSv1_enc_data;
extern SSL3_ENC_METHOD DTLSv1_2_enc_data;

# define IMPLEMENT_tls_meth_func(version, func_name, s_accept, s_connect, \
                                s_get_meth, enc_data) \
const SSL_METHOD *func_name(void)  \
        { \
        static const SSL_METHOD func_name##_data= { \
                version, \
                tls1_new, \
                tls1_clear, \
                tls1_free, \
                s_accept, \
                s_connect, \
                ssl3_read, \
                ssl3_peek, \
                ssl3_write, \
                ssl3_shutdown, \
                ssl3_renegotiate, \
                ssl3_renegotiate_check, \
                ssl3_get_message, \
                ssl3_read_bytes, \
                ssl3_write_bytes, \
                ssl3_dispatch_alert, \
                ssl3_ctrl, \
                ssl3_ctx_ctrl, \
                ssl3_get_cipher_by_char, \
                ssl3_put_cipher_by_char, \
                ssl3_pending, \
                ssl3_num_ciphers, \
                ssl3_get_cipher, \
                s_get_meth, \
                tls1_default_timeout, \
                &enc_data, \
                ssl_undefined_void_function, \
                ssl3_callback_ctrl, \
                ssl3_ctx_callback_ctrl, \
        }; \
        return &func_name##_data; \
        }

# define IMPLEMENT_ssl3_meth_func(func_name, s_accept, s_connect, s_get_meth) \
const SSL_METHOD *func_name(void)  \
        { \
        static const SSL_METHOD func_name##_data= { \
                SSL3_VERSION, \
                ssl3_new, \
                ssl3_clear, \
                ssl3_free, \
                s_accept, \
                s_connect, \
                ssl3_read, \
                ssl3_peek, \
                ssl3_write, \
                ssl3_shutdown, \
                ssl3_renegotiate, \
                ssl3_renegotiate_check, \
                ssl3_get_message, \
                ssl3_read_bytes, \
                ssl3_write_bytes, \
                ssl3_dispatch_alert, \
                ssl3_ctrl, \
                ssl3_ctx_ctrl, \
                ssl3_get_cipher_by_char, \
                ssl3_put_cipher_by_char, \
                ssl3_pending, \
                ssl3_num_ciphers, \
                ssl3_get_cipher, \
                s_get_meth, \
                ssl3_default_timeout, \
                &SSLv3_enc_data, \
                ssl_undefined_void_function, \
                ssl3_callback_ctrl, \
                ssl3_ctx_callback_ctrl, \
        }; \
        return &func_name##_data; \
        }

# define IMPLEMENT_ssl23_meth_func(func_name, s_accept, s_connect, s_get_meth) \
const SSL_METHOD *func_name(void)  \
        { \
        static const SSL_METHOD func_name##_data= { \
        TLS1_2_VERSION, \
        tls1_new, \
        tls1_clear, \
        tls1_free, \
        s_accept, \
        s_connect, \
        ssl23_read, \
        ssl23_peek, \
        ssl23_write, \
        ssl_undefined_function, \
        ssl_undefined_function, \
        ssl_ok, \
        ssl3_get_message, \
        ssl3_read_bytes, \
        ssl3_write_bytes, \
        ssl3_dispatch_alert, \
        ssl3_ctrl, \
        ssl3_ctx_ctrl, \
        ssl23_get_cipher_by_char, \
        ssl23_put_cipher_by_char, \
        ssl_undefined_const_function, \
        ssl23_num_ciphers, \
        ssl23_get_cipher, \
        s_get_meth, \
        ssl23_default_timeout, \
        &TLSv1_2_enc_data, \
        ssl_undefined_void_function, \
        ssl3_callback_ctrl, \
        ssl3_ctx_callback_ctrl, \
        }; \
        return &func_name##_data; \
        }

# define IMPLEMENT_ssl2_meth_func(func_name, s_accept, s_connect, s_get_meth) \
const SSL_METHOD *func_name(void)  \
        { \
        static const SSL_METHOD func_name##_data= { \
                SSL2_VERSION, \
                ssl2_new,       /* local */ \
                ssl2_clear,     /* local */ \
                ssl2_free,      /* local */ \
                s_accept, \
                s_connect, \
                ssl2_read, \
                ssl2_peek, \
                ssl2_write, \
                ssl2_shutdown, \
                ssl_ok, /* NULL - renegotiate */ \
                ssl_ok, /* NULL - check renegotiate */ \
                NULL, /* NULL - ssl_get_message */ \
                NULL, /* NULL - ssl_get_record */ \
                NULL, /* NULL - ssl_write_bytes */ \
                NULL, /* NULL - dispatch_alert */ \
                ssl2_ctrl,      /* local */ \
                ssl2_ctx_ctrl,  /* local */ \
                ssl2_get_cipher_by_char, \
                ssl2_put_cipher_by_char, \
                ssl2_pending, \
                ssl2_num_ciphers, \
                ssl2_get_cipher, \
                s_get_meth, \
                ssl2_default_timeout, \
                &ssl3_undef_enc_method, \
                ssl_undefined_void_function, \
                ssl2_callback_ctrl,     /* local */ \
                ssl2_ctx_callback_ctrl, /* local */ \
        }; \
        return &func_name##_data; \
        }

# define IMPLEMENT_dtls1_meth_func(version, func_name, s_accept, s_connect, \
                                        s_get_meth, enc_data) \
const SSL_METHOD *func_name(void)  \
        { \
        static const SSL_METHOD func_name##_data= { \
                version, \
                dtls1_new, \
                dtls1_clear, \
                dtls1_free, \
                s_accept, \
                s_connect, \
                ssl3_read, \
                ssl3_peek, \
                ssl3_write, \
                dtls1_shutdown, \
                ssl3_renegotiate, \
                ssl3_renegotiate_check, \
                dtls1_get_message, \
                dtls1_read_bytes, \
                dtls1_write_app_data_bytes, \
                dtls1_dispatch_alert, \
                dtls1_ctrl, \
                ssl3_ctx_ctrl, \
                ssl3_get_cipher_by_char, \
                ssl3_put_cipher_by_char, \
                ssl3_pending, \
                ssl3_num_ciphers, \
                dtls1_get_cipher, \
                s_get_meth, \
                dtls1_default_timeout, \
                &enc_data, \
                ssl_undefined_void_function, \
                ssl3_callback_ctrl, \
                ssl3_ctx_callback_ctrl, \
        }; \
        return &func_name##_data; \
        }

struct openssl_ssl_test_functions {
    int (*p_ssl_init_wbio_buffer) (SSL *s, int push);
    int (*p_ssl3_setup_buffers) (SSL *s);
    int (*p_tls1_process_heartbeat) (SSL *s);
    int (*p_dtls1_process_heartbeat) (SSL *s);
};

# ifndef OPENSSL_UNIT_TEST

void ssl_clear_cipher_ctx(SSL *s);
int ssl_clear_bad_session(SSL *s);
CERT *ssl_cert_new(void);
CERT *ssl_cert_dup(CERT *cert);
void ssl_cert_set_default_md(CERT *cert);
int ssl_cert_inst(CERT **o);
void ssl_cert_clear_certs(CERT *c);
void ssl_cert_free(CERT *c);
SESS_CERT *ssl_sess_cert_new(void);
void ssl_sess_cert_free(SESS_CERT *sc);
int ssl_set_peer_cert_type(SESS_CERT *c, int type);
int ssl_get_new_session(SSL *s, int session);
int ssl_get_prev_session(SSL *s, unsigned char *session, int len,
                         const unsigned char *limit);
SSL_SESSION *ssl_session_dup(SSL_SESSION *src, int ticket);
int ssl_cipher_id_cmp(const SSL_CIPHER *a, const SSL_CIPHER *b);
DECLARE_OBJ_BSEARCH_GLOBAL_CMP_FN(SSL_CIPHER, SSL_CIPHER, ssl_cipher_id);
int ssl_cipher_ptr_id_cmp(const SSL_CIPHER *const *ap,
                          const SSL_CIPHER *const *bp);
STACK_OF(SSL_CIPHER) *ssl_bytes_to_cipher_list(SSL *s, unsigned char *p,
                                               int num,
                                               STACK_OF(SSL_CIPHER) **skp);
int ssl_cipher_list_to_bytes(SSL *s, STACK_OF(SSL_CIPHER) *sk,
                             unsigned char *p,
                             int (*put_cb) (const SSL_CIPHER *,
                                            unsigned char *));
STACK_OF(SSL_CIPHER) *ssl_create_cipher_list(const SSL_METHOD *meth,
                                             STACK_OF(SSL_CIPHER) **pref,
                                             STACK_OF(SSL_CIPHER) **sorted,
                                             const char *rule_str, CERT *c);
void ssl_update_cache(SSL *s, int mode);
int ssl_cipher_get_evp(const SSL_SESSION *s, const EVP_CIPHER **enc,
                       const EVP_MD **md, int *mac_pkey_type,
                       int *mac_secret_size, SSL_COMP **comp);
int ssl_get_handshake_digest(int i, long *mask, const EVP_MD **md);
int ssl_cipher_get_cert_index(const SSL_CIPHER *c);
const SSL_CIPHER *ssl_get_cipher_by_char(SSL *ssl, const unsigned char *ptr);
int ssl_cert_set0_chain(CERT *c, STACK_OF(X509) *chain);
int ssl_cert_set1_chain(CERT *c, STACK_OF(X509) *chain);
int ssl_cert_add0_chain_cert(CERT *c, X509 *x);
int ssl_cert_add1_chain_cert(CERT *c, X509 *x);
int ssl_cert_select_current(CERT *c, X509 *x);
int ssl_cert_set_current(CERT *c, long arg);
X509 *ssl_cert_get0_next_certificate(CERT *c, int first);
void ssl_cert_set_cert_cb(CERT *c, int (*cb) (SSL *ssl, void *arg),
                          void *arg);

int ssl_verify_cert_chain(SSL *s, STACK_OF(X509) *sk);
int ssl_add_cert_chain(SSL *s, CERT_PKEY *cpk, unsigned long *l);
int ssl_build_cert_chain(CERT *c, X509_STORE *chain_store, int flags);
int ssl_cert_set_cert_store(CERT *c, X509_STORE *store, int chain, int ref);
int ssl_undefined_function(SSL *s);
int ssl_undefined_void_function(void);
int ssl_undefined_const_function(const SSL *s);
CERT_PKEY *ssl_get_server_send_pkey(const SSL *s);
#  ifndef OPENSSL_NO_TLSEXT
int ssl_get_server_cert_serverinfo(SSL *s, const unsigned char **serverinfo,
                                   size_t *serverinfo_length);
#  endif
EVP_PKEY *ssl_get_sign_pkey(SSL *s, const SSL_CIPHER *c, const EVP_MD **pmd);
int ssl_cert_type(X509 *x, EVP_PKEY *pkey);
void ssl_set_cert_masks(CERT *c, const SSL_CIPHER *cipher);
STACK_OF(SSL_CIPHER) *ssl_get_ciphers_by_id(SSL *s);
int ssl_verify_alarm_type(long type);
void ssl_load_ciphers(void);
int ssl_fill_hello_random(SSL *s, int server, unsigned char *field, int len);

int ssl2_enc_init(SSL *s, int client);
int ssl2_generate_key_material(SSL *s);
int ssl2_enc(SSL *s, int send_data);
void ssl2_mac(SSL *s, unsigned char *mac, int send_data);
const SSL_CIPHER *ssl2_get_cipher_by_char(const unsigned char *p);
int ssl2_put_cipher_by_char(const SSL_CIPHER *c, unsigned char *p);
int ssl2_part_read(SSL *s, unsigned long f, int i);
int ssl2_do_write(SSL *s);
int ssl2_set_certificate(SSL *s, int type, int len,
                         const unsigned char *data);
void ssl2_return_error(SSL *s, int reason);
void ssl2_write_error(SSL *s);
int ssl2_num_ciphers(void);
const SSL_CIPHER *ssl2_get_cipher(unsigned int u);
int ssl2_new(SSL *s);
void ssl2_free(SSL *s);
int ssl2_accept(SSL *s);
int ssl2_connect(SSL *s);
int ssl2_read(SSL *s, void *buf, int len);
int ssl2_peek(SSL *s, void *buf, int len);
int ssl2_write(SSL *s, const void *buf, int len);
int ssl2_shutdown(SSL *s);
void ssl2_clear(SSL *s);
long ssl2_ctrl(SSL *s, int cmd, long larg, void *parg);
long ssl2_ctx_ctrl(SSL_CTX *s, int cmd, long larg, void *parg);
long ssl2_callback_ctrl(SSL *s, int cmd, void (*fp) (void));
long ssl2_ctx_callback_ctrl(SSL_CTX *s, int cmd, void (*fp) (void));
int ssl2_pending(const SSL *s);
long ssl2_default_timeout(void);

const SSL_CIPHER *ssl3_get_cipher_by_char(const unsigned char *p);
int ssl3_put_cipher_by_char(const SSL_CIPHER *c, unsigned char *p);
void ssl3_init_finished_mac(SSL *s);
int ssl3_send_server_certificate(SSL *s);
int ssl3_send_newsession_ticket(SSL *s);
int ssl3_send_cert_status(SSL *s);
int ssl3_get_finished(SSL *s, int state_a, int state_b);
int ssl3_setup_key_block(SSL *s);
int ssl3_send_change_cipher_spec(SSL *s, int state_a, int state_b);
int ssl3_change_cipher_state(SSL *s, int which);
void ssl3_cleanup_key_block(SSL *s);
int ssl3_do_write(SSL *s, int type);
int ssl3_send_alert(SSL *s, int level, int desc);
int ssl3_generate_master_secret(SSL *s, unsigned char *out,
                                unsigned char *p, int len);
int ssl3_get_req_cert_type(SSL *s, unsigned char *p);
long ssl3_get_message(SSL *s, int st1, int stn, int mt, long max, int *ok);
int ssl3_send_finished(SSL *s, int a, int b, const char *sender, int slen);
int ssl3_num_ciphers(void);
const SSL_CIPHER *ssl3_get_cipher(unsigned int u);
int ssl3_renegotiate(SSL *ssl);
int ssl3_renegotiate_check(SSL *ssl);
int ssl3_dispatch_alert(SSL *s);
int ssl3_read_bytes(SSL *s, int type, unsigned char *buf, int len, int peek);
int ssl3_write_bytes(SSL *s, int type, const void *buf, int len);
int ssl3_final_finish_mac(SSL *s, const char *sender, int slen,
                          unsigned char *p);
int ssl3_cert_verify_mac(SSL *s, int md_nid, unsigned char *p);
void ssl3_finish_mac(SSL *s, const unsigned char *buf, int len);
int ssl3_enc(SSL *s, int send_data);
int n_ssl3_mac(SSL *ssl, unsigned char *md, int send_data);
void ssl3_free_digest_list(SSL *s);
unsigned long ssl3_output_cert_chain(SSL *s, CERT_PKEY *cpk);
SSL_CIPHER *ssl3_choose_cipher(SSL *ssl, STACK_OF(SSL_CIPHER) *clnt,
                               STACK_OF(SSL_CIPHER) *srvr);
int ssl3_setup_buffers(SSL *s);
int ssl3_setup_read_buffer(SSL *s);
int ssl3_setup_write_buffer(SSL *s);
int ssl3_release_read_buffer(SSL *s);
int ssl3_release_write_buffer(SSL *s);
int ssl3_digest_cached_records(SSL *s);
int ssl3_new(SSL *s);
void ssl3_free(SSL *s);
int ssl3_accept(SSL *s);
int ssl3_connect(SSL *s);
int ssl3_read(SSL *s, void *buf, int len);
int ssl3_peek(SSL *s, void *buf, int len);
int ssl3_write(SSL *s, const void *buf, int len);
int ssl3_shutdown(SSL *s);
void ssl3_clear(SSL *s);
long ssl3_ctrl(SSL *s, int cmd, long larg, void *parg);
long ssl3_ctx_ctrl(SSL_CTX *s, int cmd, long larg, void *parg);
long ssl3_callback_ctrl(SSL *s, int cmd, void (*fp) (void));
long ssl3_ctx_callback_ctrl(SSL_CTX *s, int cmd, void (*fp) (void));
int ssl3_pending(const SSL *s);

void ssl3_record_sequence_update(unsigned char *seq);
int ssl3_do_change_cipher_spec(SSL *ssl);
long ssl3_default_timeout(void);

void ssl3_set_handshake_header(SSL *s, int htype, unsigned long len);
int ssl3_handshake_write(SSL *s);

int ssl23_num_ciphers(void);
const SSL_CIPHER *ssl23_get_cipher(unsigned int u);
int ssl23_read(SSL *s, void *buf, int len);
int ssl23_peek(SSL *s, void *buf, int len);
int ssl23_write(SSL *s, const void *buf, int len);
int ssl23_put_cipher_by_char(const SSL_CIPHER *c, unsigned char *p);
const SSL_CIPHER *ssl23_get_cipher_by_char(const unsigned char *p);
long ssl23_default_timeout(void);

long tls1_default_timeout(void);
int dtls1_do_write(SSL *s, int type);
int ssl3_read_n(SSL *s, int n, int max, int extend);
int dtls1_read_bytes(SSL *s, int type, unsigned char *buf, int len, int peek);
int ssl3_do_compress(SSL *ssl);
int ssl3_do_uncompress(SSL *ssl);
int ssl3_write_pending(SSL *s, int type, const unsigned char *buf,
                       unsigned int len);
unsigned char *dtls1_set_message_header(SSL *s,
                                        unsigned char *p, unsigned char mt,
                                        unsigned long len,
                                        unsigned long frag_off,
                                        unsigned long frag_len);

int dtls1_write_app_data_bytes(SSL *s, int type, const void *buf, int len);
int dtls1_write_bytes(SSL *s, int type, const void *buf, int len);

int dtls1_send_change_cipher_spec(SSL *s, int a, int b);
int dtls1_read_failed(SSL *s, int code);
int dtls1_buffer_message(SSL *s, int ccs);
int dtls1_retransmit_message(SSL *s, unsigned short seq,
                             unsigned long frag_off, int *found);
int dtls1_get_queue_priority(unsigned short seq, int is_ccs);
int dtls1_retransmit_buffered_messages(SSL *s);
void dtls1_clear_record_buffer(SSL *s);
void dtls1_get_message_header(unsigned char *data,
                              struct hm_header_st *msg_hdr);
void dtls1_get_ccs_header(unsigned char *data, struct ccs_header_st *ccs_hdr);
void dtls1_reset_seq_numbers(SSL *s, int rw);
long dtls1_default_timeout(void);
struct timeval *dtls1_get_timeout(SSL *s, struct timeval *timeleft);
int dtls1_check_timeout_num(SSL *s);
int dtls1_handle_timeout(SSL *s);
const SSL_CIPHER *dtls1_get_cipher(unsigned int u);
void dtls1_start_timer(SSL *s);
void dtls1_stop_timer(SSL *s);
int dtls1_is_timer_expired(SSL *s);
void dtls1_double_timeout(SSL *s);
int dtls1_send_newsession_ticket(SSL *s);
unsigned int dtls1_min_mtu(SSL *s);
unsigned int dtls1_link_min_mtu(void);
void dtls1_hm_fragment_free(hm_fragment *frag);

/* some client-only functions */
int ssl3_client_hello(SSL *s);
int ssl3_get_server_hello(SSL *s);
int ssl3_get_certificate_request(SSL *s);
int ssl3_get_new_session_ticket(SSL *s);
int ssl3_get_cert_status(SSL *s);
int ssl3_get_server_done(SSL *s);
int ssl3_send_client_verify(SSL *s);
int ssl3_send_client_certificate(SSL *s);
int ssl_do_client_cert_cb(SSL *s, X509 **px509, EVP_PKEY **ppkey);
int ssl3_send_client_key_exchange(SSL *s);
int ssl3_get_key_exchange(SSL *s);
int ssl3_get_server_certificate(SSL *s);
int ssl3_check_cert_and_algorithm(SSL *s);
#  ifndef OPENSSL_NO_TLSEXT
#   ifndef OPENSSL_NO_NEXTPROTONEG
int ssl3_send_next_proto(SSL *s);
#   endif
#  endif

int dtls1_client_hello(SSL *s);

/* some server-only functions */
int ssl3_get_client_hello(SSL *s);
int ssl3_send_server_hello(SSL *s);
int ssl3_send_hello_request(SSL *s);
int ssl3_send_server_key_exchange(SSL *s);
int ssl3_send_certificate_request(SSL *s);
int ssl3_send_server_done(SSL *s);
int ssl3_get_client_certificate(SSL *s);
int ssl3_get_client_key_exchange(SSL *s);
int ssl3_get_cert_verify(SSL *s);
#  ifndef OPENSSL_NO_NEXTPROTONEG
int ssl3_get_next_proto(SSL *s);
#  endif

int ssl23_accept(SSL *s);
int ssl23_connect(SSL *s);
int ssl23_read_bytes(SSL *s, int n);
int ssl23_write_bytes(SSL *s);

int tls1_new(SSL *s);
void tls1_free(SSL *s);
void tls1_clear(SSL *s);
long tls1_ctrl(SSL *s, int cmd, long larg, void *parg);
long tls1_callback_ctrl(SSL *s, int cmd, void (*fp) (void));

int dtls1_new(SSL *s);
int dtls1_accept(SSL *s);
int dtls1_connect(SSL *s);
void dtls1_free(SSL *s);
void dtls1_clear(SSL *s);
long dtls1_ctrl(SSL *s, int cmd, long larg, void *parg);
int dtls1_shutdown(SSL *s);

long dtls1_get_message(SSL *s, int st1, int stn, int mt, long max, int *ok);
int dtls1_get_record(SSL *s);
int do_dtls1_write(SSL *s, int type, const unsigned char *buf,
                   unsigned int len, int create_empty_fragement);
int dtls1_dispatch_alert(SSL *s);

int ssl_init_wbio_buffer(SSL *s, int push);
void ssl_free_wbio_buffer(SSL *s);

int tls1_change_cipher_state(SSL *s, int which);
int tls1_setup_key_block(SSL *s);
int tls1_enc(SSL *s, int snd);
int tls1_final_finish_mac(SSL *s,
                          const char *str, int slen, unsigned char *p);
int tls1_cert_verify_mac(SSL *s, int md_nid, unsigned char *p);
int tls1_mac(SSL *ssl, unsigned char *md, int snd);
int tls1_generate_master_secret(SSL *s, unsigned char *out,
                                unsigned char *p, int len);
int tls1_export_keying_material(SSL *s, unsigned char *out, size_t olen,
                                const char *label, size_t llen,
                                const unsigned char *p, size_t plen,
                                int use_context);
int tls1_alert_code(int code);
int ssl3_alert_code(int code);
int ssl_ok(SSL *s);

#  ifndef OPENSSL_NO_ECDH
int ssl_check_srvr_ecc_cert_and_alg(X509 *x, SSL *s);
#  endif

SSL_COMP *ssl3_comp_find(STACK_OF(SSL_COMP) *sk, int n);

#  ifndef OPENSSL_NO_EC
int tls1_ec_curve_id2nid(int curve_id);
int tls1_ec_nid2curve_id(int nid);
int tls1_check_curve(SSL *s, const unsigned char *p, size_t len);
int tls1_shared_curve(SSL *s, int nmatch);
int tls1_set_curves(unsigned char **pext, size_t *pextlen,
                    int *curves, size_t ncurves);
int tls1_set_curves_list(unsigned char **pext, size_t *pextlen,
                         const char *str);
#   ifndef OPENSSL_NO_ECDH
int tls1_check_ec_tmp_key(SSL *s, unsigned long id);
#   endif                       /* OPENSSL_NO_ECDH */
#  endif                        /* OPENSSL_NO_EC */

#  ifndef OPENSSL_NO_TLSEXT
int tls1_shared_list(SSL *s,
                     const unsigned char *l1, size_t l1len,
                     const unsigned char *l2, size_t l2len, int nmatch);
unsigned char *ssl_add_clienthello_tlsext(SSL *s, unsigned char *buf,
                                          unsigned char *limit, int *al);
unsigned char *ssl_add_serverhello_tlsext(SSL *s, unsigned char *buf,
                                          unsigned char *limit, int *al);
int ssl_parse_clienthello_tlsext(SSL *s, unsigned char **data,
                                 unsigned char *limit);
int tls1_set_server_sigalgs(SSL *s);
int ssl_check_clienthello_tlsext_late(SSL *s);
int ssl_parse_serverhello_tlsext(SSL *s, unsigned char **data,
                                 unsigned char *d, int n);
int ssl_prepare_clienthello_tlsext(SSL *s);
int ssl_prepare_serverhello_tlsext(SSL *s);

#   ifndef OPENSSL_NO_HEARTBEATS
int tls1_heartbeat(SSL *s);
int dtls1_heartbeat(SSL *s);
int tls1_process_heartbeat(SSL *s);
int dtls1_process_heartbeat(SSL *s);
#   endif

#   ifdef OPENSSL_NO_SHA256
#    define tlsext_tick_md  EVP_sha1
#   else
#    define tlsext_tick_md  EVP_sha256
#   endif
int tls1_process_ticket(SSL *s, unsigned char *session_id, int len,
                        const unsigned char *limit, SSL_SESSION **ret);

int tls12_get_sigandhash(unsigned char *p, const EVP_PKEY *pk,
                         const EVP_MD *md);
int tls12_get_sigid(const EVP_PKEY *pk);
const EVP_MD *tls12_get_hash(unsigned char hash_alg);

int tls1_set_sigalgs_list(CERT *c, const char *str, int client);
int tls1_set_sigalgs(CERT *c, const int *salg, size_t salglen, int client);
int tls1_check_chain(SSL *s, X509 *x, EVP_PKEY *pk, STACK_OF(X509) *chain,
                     int idx);
void tls1_set_cert_validity(SSL *s);

#  endif
EVP_MD_CTX *ssl_replace_hash(EVP_MD_CTX **hash, const EVP_MD *md);
void ssl_clear_hash_ctx(EVP_MD_CTX **hash);
int ssl_add_serverhello_renegotiate_ext(SSL *s, unsigned char *p, int *len,
                                        int maxlen);
int ssl_parse_serverhello_renegotiate_ext(SSL *s, unsigned char *d, int len,
                                          int *al);
int ssl_add_clienthello_renegotiate_ext(SSL *s, unsigned char *p, int *len,
                                        int maxlen);
int ssl_parse_clienthello_renegotiate_ext(SSL *s, unsigned char *d, int len,
                                          int *al);
long ssl_get_algorithm2(SSL *s);
int tls1_save_sigalgs(SSL *s, const unsigned char *data, int dsize);
int tls1_process_sigalgs(SSL *s);
size_t tls12_get_psigalgs(SSL *s, const unsigned char **psigs);
int tls12_check_peer_sigalg(const EVP_MD **pmd, SSL *s,
                            const unsigned char *sig, EVP_PKEY *pkey);
void ssl_set_client_disabled(SSL *s);

int ssl_add_clienthello_use_srtp_ext(SSL *s, unsigned char *p, int *len,
                                     int maxlen);
int ssl_parse_clienthello_use_srtp_ext(SSL *s, unsigned char *d, int len,
                                       int *al);
int ssl_add_serverhello_use_srtp_ext(SSL *s, unsigned char *p, int *len,
                                     int maxlen);
int ssl_parse_serverhello_use_srtp_ext(SSL *s, unsigned char *d, int len,
                                       int *al);

/* s3_cbc.c */
void ssl3_cbc_copy_mac(unsigned char *out,
                       const SSL3_RECORD *rec,
                       unsigned md_size, unsigned orig_len);
int ssl3_cbc_remove_padding(const SSL *s,
                            SSL3_RECORD *rec,
                            unsigned block_size, unsigned mac_size);
int tls1_cbc_remove_padding(const SSL *s,
                            SSL3_RECORD *rec,
                            unsigned block_size, unsigned mac_size);
char ssl3_cbc_record_digest_supported(const EVP_MD_CTX *ctx);
int ssl3_cbc_digest_record(const EVP_MD_CTX *ctx,
                           unsigned char *md_out,
                           size_t *md_out_size,
                           const unsigned char header[13],
                           const unsigned char *data,
                           size_t data_plus_mac_size,
                           size_t data_plus_mac_plus_padding_size,
                           const unsigned char *mac_secret,
                           unsigned mac_secret_length, char is_sslv3);

void tls_fips_digest_extra(const EVP_CIPHER_CTX *cipher_ctx,
                           EVP_MD_CTX *mac_ctx, const unsigned char *data,
                           size_t data_len, size_t orig_len);

int srp_verify_server_param(SSL *s, int *al);

/* t1_ext.c */

void custom_ext_init(custom_ext_methods *meths);

int custom_ext_parse(SSL *s, int server,
                     unsigned int ext_type,
                     const unsigned char *ext_data, size_t ext_size, int *al);
int custom_ext_add(SSL *s, int server,
                   unsigned char **pret, unsigned char *limit, int *al);

int custom_exts_copy(custom_ext_methods *dst, const custom_ext_methods *src);
void custom_exts_free(custom_ext_methods *exts);

# else

#  define ssl_init_wbio_buffer SSL_test_functions()->p_ssl_init_wbio_buffer
#  define ssl3_setup_buffers SSL_test_functions()->p_ssl3_setup_buffers
#  define tls1_process_heartbeat SSL_test_functions()->p_tls1_process_heartbeat
#  define dtls1_process_heartbeat SSL_test_functions()->p_dtls1_process_heartbeat

# endif
#endif
