/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
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

#ifndef HEADER_SSL_H
# define HEADER_SSL_H

# include <openssl/e_os2.h>
# include <openssl/opensslconf.h>
# include <openssl/comp.h>
# include <openssl/bio.h>
# if OPENSSL_API_COMPAT < 0x10100000L
#  include <openssl/x509.h>
#  include <openssl/crypto.h>
#  include <openssl/lhash.h>
#  include <openssl/buffer.h>
# endif
# include <openssl/pem.h>
# include <openssl/hmac.h>
# include <openssl/async.h>

# include <openssl/safestack.h>
# include <openssl/symhacks.h>
# include <openssl/ct.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* OpenSSL version number for ASN.1 encoding of the session information */
/*-
 * Version 0 - initial version
 * Version 1 - added the optional peer certificate
 */
# define SSL_SESSION_ASN1_VERSION 0x0001

# define SSL_MAX_SSL_SESSION_ID_LENGTH           32
# define SSL_MAX_SID_CTX_LENGTH                  32

# define SSL_MIN_RSA_MODULUS_LENGTH_IN_BYTES     (512/8)
# define SSL_MAX_KEY_ARG_LENGTH                  8
# define SSL_MAX_MASTER_KEY_LENGTH               48

/* The maximum number of encrypt/decrypt pipelines we can support */
# define SSL_MAX_PIPELINES  32

/* text strings for the ciphers */

/* These are used to specify which ciphers to use and not to use */

# define SSL_TXT_LOW             "LOW"
# define SSL_TXT_MEDIUM          "MEDIUM"
# define SSL_TXT_HIGH            "HIGH"
# define SSL_TXT_FIPS            "FIPS"

# define SSL_TXT_aNULL           "aNULL"
# define SSL_TXT_eNULL           "eNULL"
# define SSL_TXT_NULL            "NULL"

# define SSL_TXT_kRSA            "kRSA"
# define SSL_TXT_kDHr            "kDHr"/* this cipher class has been removed */
# define SSL_TXT_kDHd            "kDHd"/* this cipher class has been removed */
# define SSL_TXT_kDH             "kDH"/* this cipher class has been removed */
# define SSL_TXT_kEDH            "kEDH"/* alias for kDHE */
# define SSL_TXT_kDHE            "kDHE"
# define SSL_TXT_kECDHr          "kECDHr"/* this cipher class has been removed */
# define SSL_TXT_kECDHe          "kECDHe"/* this cipher class has been removed */
# define SSL_TXT_kECDH           "kECDH"/* this cipher class has been removed */
# define SSL_TXT_kEECDH          "kEECDH"/* alias for kECDHE */
# define SSL_TXT_kECDHE          "kECDHE"
# define SSL_TXT_kPSK            "kPSK"
# define SSL_TXT_kRSAPSK         "kRSAPSK"
# define SSL_TXT_kECDHEPSK       "kECDHEPSK"
# define SSL_TXT_kDHEPSK         "kDHEPSK"
# define SSL_TXT_kGOST           "kGOST"
# define SSL_TXT_kSRP            "kSRP"

# define SSL_TXT_aRSA            "aRSA"
# define SSL_TXT_aDSS            "aDSS"
# define SSL_TXT_aDH             "aDH"/* this cipher class has been removed */
# define SSL_TXT_aECDH           "aECDH"/* this cipher class has been removed */
# define SSL_TXT_aECDSA          "aECDSA"
# define SSL_TXT_aPSK            "aPSK"
# define SSL_TXT_aGOST94         "aGOST94"
# define SSL_TXT_aGOST01         "aGOST01"
# define SSL_TXT_aGOST12         "aGOST12"
# define SSL_TXT_aGOST           "aGOST"
# define SSL_TXT_aSRP            "aSRP"

# define SSL_TXT_DSS             "DSS"
# define SSL_TXT_DH              "DH"
# define SSL_TXT_DHE             "DHE"/* same as "kDHE:-ADH" */
# define SSL_TXT_EDH             "EDH"/* alias for DHE */
# define SSL_TXT_ADH             "ADH"
# define SSL_TXT_RSA             "RSA"
# define SSL_TXT_ECDH            "ECDH"
# define SSL_TXT_EECDH           "EECDH"/* alias for ECDHE" */
# define SSL_TXT_ECDHE           "ECDHE"/* same as "kECDHE:-AECDH" */
# define SSL_TXT_AECDH           "AECDH"
# define SSL_TXT_ECDSA           "ECDSA"
# define SSL_TXT_PSK             "PSK"
# define SSL_TXT_SRP             "SRP"

# define SSL_TXT_DES             "DES"
# define SSL_TXT_3DES            "3DES"
# define SSL_TXT_RC4             "RC4"
# define SSL_TXT_RC2             "RC2"
# define SSL_TXT_IDEA            "IDEA"
# define SSL_TXT_SEED            "SEED"
# define SSL_TXT_AES128          "AES128"
# define SSL_TXT_AES256          "AES256"
# define SSL_TXT_AES             "AES"
# define SSL_TXT_AES_GCM         "AESGCM"
# define SSL_TXT_AES_CCM         "AESCCM"
# define SSL_TXT_AES_CCM_8       "AESCCM8"
# define SSL_TXT_CAMELLIA128     "CAMELLIA128"
# define SSL_TXT_CAMELLIA256     "CAMELLIA256"
# define SSL_TXT_CAMELLIA        "CAMELLIA"
# define SSL_TXT_CHACHA20        "CHACHA20"
# define SSL_TXT_GOST            "GOST89"

# define SSL_TXT_MD5             "MD5"
# define SSL_TXT_SHA1            "SHA1"
# define SSL_TXT_SHA             "SHA"/* same as "SHA1" */
# define SSL_TXT_GOST94          "GOST94"
# define SSL_TXT_GOST89MAC       "GOST89MAC"
# define SSL_TXT_GOST12          "GOST12"
# define SSL_TXT_GOST89MAC12     "GOST89MAC12"
# define SSL_TXT_SHA256          "SHA256"
# define SSL_TXT_SHA384          "SHA384"

# define SSL_TXT_SSLV3           "SSLv3"
# define SSL_TXT_TLSV1           "TLSv1"
# define SSL_TXT_TLSV1_1         "TLSv1.1"
# define SSL_TXT_TLSV1_2         "TLSv1.2"

# define SSL_TXT_ALL             "ALL"

/*-
 * COMPLEMENTOF* definitions. These identifiers are used to (de-select)
 * ciphers normally not being used.
 * Example: "RC4" will activate all ciphers using RC4 including ciphers
 * without authentication, which would normally disabled by DEFAULT (due
 * the "!ADH" being part of default). Therefore "RC4:!COMPLEMENTOFDEFAULT"
 * will make sure that it is also disabled in the specific selection.
 * COMPLEMENTOF* identifiers are portable between version, as adjustments
 * to the default cipher setup will also be included here.
 *
 * COMPLEMENTOFDEFAULT does not experience the same special treatment that
 * DEFAULT gets, as only selection is being done and no sorting as needed
 * for DEFAULT.
 */
# define SSL_TXT_CMPALL          "COMPLEMENTOFALL"
# define SSL_TXT_CMPDEF          "COMPLEMENTOFDEFAULT"

/*
 * The following cipher list is used by default. It also is substituted when
 * an application-defined cipher list string starts with 'DEFAULT'.
 */
# define SSL_DEFAULT_CIPHER_LIST "ALL:!COMPLEMENTOFDEFAULT:!eNULL"
/*
 * As of OpenSSL 1.0.0, ssl_create_cipher_list() in ssl/ssl_ciph.c always
 * starts with a reasonable order, and all we have to do for DEFAULT is
 * throwing out anonymous and unencrypted ciphersuites! (The latter are not
 * actually enabled by ALL, but "ALL:RSA" would enable some of them.)
 */

/* Used in SSL_set_shutdown()/SSL_get_shutdown(); */
# define SSL_SENT_SHUTDOWN       1
# define SSL_RECEIVED_SHUTDOWN   2

#ifdef __cplusplus
}
#endif

#ifdef  __cplusplus
extern "C" {
#endif

# define SSL_FILETYPE_ASN1       X509_FILETYPE_ASN1
# define SSL_FILETYPE_PEM        X509_FILETYPE_PEM

/*
 * This is needed to stop compilers complaining about the 'struct ssl_st *'
 * function parameters used to prototype callbacks in SSL_CTX.
 */
typedef struct ssl_st *ssl_crock_st;
typedef struct tls_session_ticket_ext_st TLS_SESSION_TICKET_EXT;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_cipher_st SSL_CIPHER;
typedef struct ssl_session_st SSL_SESSION;
typedef struct tls_sigalgs_st TLS_SIGALGS;
typedef struct ssl_conf_ctx_st SSL_CONF_CTX;
typedef struct ssl_comp_st SSL_COMP;

STACK_OF(SSL_CIPHER);
STACK_OF(SSL_COMP);

/* SRTP protection profiles for use with the use_srtp extension (RFC 5764)*/
typedef struct srtp_protection_profile_st {
    const char *name;
    unsigned long id;
} SRTP_PROTECTION_PROFILE;

DEFINE_STACK_OF(SRTP_PROTECTION_PROFILE)

typedef int (*tls_session_ticket_ext_cb_fn) (SSL *s,
                                             const unsigned char *data,
                                             int len, void *arg);
typedef int (*tls_session_secret_cb_fn) (SSL *s, void *secret,
                                         int *secret_len,
                                         STACK_OF(SSL_CIPHER) *peer_ciphers,
                                         const SSL_CIPHER **cipher, void *arg);

/* Typedefs for handling custom extensions */

typedef int (*custom_ext_add_cb) (SSL *s, unsigned int ext_type,
                                  const unsigned char **out,
                                  size_t *outlen, int *al, void *add_arg);

typedef void (*custom_ext_free_cb) (SSL *s, unsigned int ext_type,
                                    const unsigned char *out, void *add_arg);

typedef int (*custom_ext_parse_cb) (SSL *s, unsigned int ext_type,
                                    const unsigned char *in,
                                    size_t inlen, int *al, void *parse_arg);

/* Typedef for verification callback */
typedef int (*SSL_verify_cb)(int preverify_ok, X509_STORE_CTX *x509_ctx);

/* Allow initial connection to servers that don't support RI */
# define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004U
# define SSL_OP_TLSEXT_PADDING                           0x00000010U
# define SSL_OP_SAFARI_ECDHE_ECDSA_BUG                   0x00000040U

/*
 * Disable SSL 3.0/TLS 1.0 CBC vulnerability workaround that was added in
 * OpenSSL 0.9.6d.  Usually (depending on the application protocol) the
 * workaround is not needed.  Unfortunately some broken SSL/TLS
 * implementations cannot handle it at all, which is why we include it in
 * SSL_OP_ALL. Added in 0.9.6e
 */
# define SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS              0x00000800U

/* DTLS options */
# define SSL_OP_NO_QUERY_MTU                             0x00001000U
/* Turn on Cookie Exchange (on relevant for servers) */
# define SSL_OP_COOKIE_EXCHANGE                          0x00002000U
/* Don't use RFC4507 ticket extension */
# define SSL_OP_NO_TICKET                                0x00004000U
# ifndef OPENSSL_NO_DTLS1_METHOD
/* Use Cisco's "speshul" version of DTLS_BAD_VER
 * (only with deprecated DTLSv1_client_method())  */
#  define SSL_OP_CISCO_ANYCONNECT                        0x00008000U
# endif

/* As server, disallow session resumption on renegotiation */
# define SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION   0x00010000U
/* Don't use compression even if supported */
# define SSL_OP_NO_COMPRESSION                           0x00020000U
/* Permit unsafe legacy renegotiation */
# define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION        0x00040000U
/* Disable encrypt-then-mac */
# define SSL_OP_NO_ENCRYPT_THEN_MAC                      0x00080000U
/*
 * Set on servers to choose the cipher according to the server's preferences
 */
# define SSL_OP_CIPHER_SERVER_PREFERENCE                 0x00400000U
/*
 * If set, a server will allow a client to issue a SSLv3.0 version number as
 * latest version supported in the premaster secret, even when TLSv1.0
 * (version 3.1) was announced in the client hello. Normally this is
 * forbidden to prevent version rollback attacks.
 */
# define SSL_OP_TLS_ROLLBACK_BUG                         0x00800000U

# define SSL_OP_NO_SSLv3                                 0x02000000U
# define SSL_OP_NO_TLSv1                                 0x04000000U
# define SSL_OP_NO_TLSv1_2                               0x08000000U
# define SSL_OP_NO_TLSv1_1                               0x10000000U

# define SSL_OP_NO_DTLSv1                                0x04000000U
# define SSL_OP_NO_DTLSv1_2                              0x08000000U

# define SSL_OP_NO_SSL_MASK (SSL_OP_NO_SSLv3|\
        SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1|SSL_OP_NO_TLSv1_2)
# define SSL_OP_NO_DTLS_MASK (SSL_OP_NO_DTLSv1|SSL_OP_NO_DTLSv1_2)

/* Disallow all renegotiation */
# define SSL_OP_NO_RENEGOTIATION                         0x40000000U

/*
 * Make server add server-hello extension from early version of cryptopro
 * draft, when GOST ciphersuite is negotiated. Required for interoperability
 * with CryptoPro CSP 3.x
 */
# define SSL_OP_CRYPTOPRO_TLSEXT_BUG                     0x80000000U

/*
 * SSL_OP_ALL: various bug workarounds that should be rather harmless.
 * This used to be 0x000FFFFFL before 0.9.7.
 * This used to be 0x80000BFFU before 1.1.1.
 */
# define SSL_OP_ALL        (SSL_OP_CRYPTOPRO_TLSEXT_BUG|\
                            SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS|\
                            SSL_OP_LEGACY_SERVER_CONNECT|\
                            SSL_OP_TLSEXT_PADDING|\
                            SSL_OP_SAFARI_ECDHE_ECDSA_BUG)

/* OBSOLETE OPTIONS: retained for compatibility */

/* Removed from OpenSSL 1.1.0. Was 0x00000001L */
/* Related to removed SSLv2. */
# define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x0
/* Removed from OpenSSL 1.1.0. Was 0x00000002L */
/* Related to removed SSLv2. */
# define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x0
/* Removed from OpenSSL 0.9.8q and 1.0.0c. Was 0x00000008L */
/* Dead forever, see CVE-2010-4180 */
# define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x0
/* Removed from OpenSSL 1.0.1h and 1.0.2. Was 0x00000010L */
/* Refers to ancient SSLREF and SSLv2. */
# define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x0
/* Removed from OpenSSL 1.1.0. Was 0x00000020 */
# define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x0
/* Removed from OpenSSL 0.9.7h and 0.9.8b. Was 0x00000040L */
# define SSL_OP_MSIE_SSLV2_RSA_PADDING                   0x0
/* Removed from OpenSSL 1.1.0. Was 0x00000080 */
/* Ancient SSLeay version. */
# define SSL_OP_SSLEAY_080_CLIENT_DH_BUG                 0x0
/* Removed from OpenSSL 1.1.0. Was 0x00000100L */
# define SSL_OP_TLS_D5_BUG                               0x0
/* Removed from OpenSSL 1.1.0. Was 0x00000200L */
# define SSL_OP_TLS_BLOCK_PADDING_BUG                    0x0
/* Removed from OpenSSL 1.1.0. Was 0x00080000L */
# define SSL_OP_SINGLE_ECDH_USE                          0x0
/* Removed from OpenSSL 1.1.0. Was 0x00100000L */
# define SSL_OP_SINGLE_DH_USE                            0x0
/* Removed from OpenSSL 1.0.1k and 1.0.2. Was 0x00200000L */
# define SSL_OP_EPHEMERAL_RSA                            0x0
/* Removed from OpenSSL 1.1.0. Was 0x01000000L */
# define SSL_OP_NO_SSLv2                                 0x0
/* Removed from OpenSSL 1.0.1. Was 0x08000000L */
# define SSL_OP_PKCS1_CHECK_1                            0x0
/* Removed from OpenSSL 1.0.1. Was 0x10000000L */
# define SSL_OP_PKCS1_CHECK_2                            0x0
/* Removed from OpenSSL 1.1.0. Was 0x20000000L */ 
# define SSL_OP_NETSCAPE_CA_DN_BUG                       0x0
/* Removed from OpenSSL 1.1.0. Was 0x40000000L */
# define SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG          0x0

/*
 * Allow SSL_write(..., n) to return r with 0 < r < n (i.e. report success
 * when just a single record has been written):
 */
# define SSL_MODE_ENABLE_PARTIAL_WRITE       0x00000001U
/*
 * Make it possible to retry SSL_write() with changed buffer location (buffer
 * contents must stay the same!); this is not the default to avoid the
 * misconception that non-blocking SSL_write() behaves like non-blocking
 * write():
 */
# define SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER 0x00000002U
/*
 * Never bother the application with retries if the transport is blocking:
 */
# define SSL_MODE_AUTO_RETRY 0x00000004U
/* Don't attempt to automatically build certificate chain */
# define SSL_MODE_NO_AUTO_CHAIN 0x00000008U
/*
 * Save RAM by releasing read and write buffers when they're empty. (SSL3 and
 * TLS only.) "Released" buffers are put onto a free-list in the context or
 * just freed (depending on the context's setting for freelist_max_len).
 */
# define SSL_MODE_RELEASE_BUFFERS 0x00000010U
/*
 * Send the current time in the Random fields of the ClientHello and
 * ServerHello records for compatibility with hypothetical implementations
 * that require it.
 */
# define SSL_MODE_SEND_CLIENTHELLO_TIME 0x00000020U
# define SSL_MODE_SEND_SERVERHELLO_TIME 0x00000040U
/*
 * Send TLS_FALLBACK_SCSV in the ClientHello. To be set only by applications
 * that reconnect with a downgraded protocol version; see
 * draft-ietf-tls-downgrade-scsv-00 for details. DO NOT ENABLE THIS if your
 * application attempts a normal handshake. Only use this in explicit
 * fallback retries, following the guidance in
 * draft-ietf-tls-downgrade-scsv-00.
 */
# define SSL_MODE_SEND_FALLBACK_SCSV 0x00000080U
/*
 * Support Asynchronous operation
 */
# define SSL_MODE_ASYNC 0x00000100U

/* Cert related flags */
/*
 * Many implementations ignore some aspects of the TLS standards such as
 * enforcing certificate chain algorithms. When this is set we enforce them.
 */
# define SSL_CERT_FLAG_TLS_STRICT                0x00000001U

/* Suite B modes, takes same values as certificate verify flags */
# define SSL_CERT_FLAG_SUITEB_128_LOS_ONLY       0x10000
/* Suite B 192 bit only mode */
# define SSL_CERT_FLAG_SUITEB_192_LOS            0x20000
/* Suite B 128 bit mode allowing 192 bit algorithms */
# define SSL_CERT_FLAG_SUITEB_128_LOS            0x30000

/* Perform all sorts of protocol violations for testing purposes */
# define SSL_CERT_FLAG_BROKEN_PROTOCOL           0x10000000

/* Flags for building certificate chains */
/* Treat any existing certificates as untrusted CAs */
# define SSL_BUILD_CHAIN_FLAG_UNTRUSTED          0x1
/* Don't include root CA in chain */
# define SSL_BUILD_CHAIN_FLAG_NO_ROOT            0x2
/* Just check certificates already there */
# define SSL_BUILD_CHAIN_FLAG_CHECK              0x4
/* Ignore verification errors */
# define SSL_BUILD_CHAIN_FLAG_IGNORE_ERROR       0x8
/* Clear verification errors from queue */
# define SSL_BUILD_CHAIN_FLAG_CLEAR_ERROR        0x10

/* Flags returned by SSL_check_chain */
/* Certificate can be used with this session */
# define CERT_PKEY_VALID         0x1
/* Certificate can also be used for signing */
# define CERT_PKEY_SIGN          0x2
/* EE certificate signing algorithm OK */
# define CERT_PKEY_EE_SIGNATURE  0x10
/* CA signature algorithms OK */
# define CERT_PKEY_CA_SIGNATURE  0x20
/* EE certificate parameters OK */
# define CERT_PKEY_EE_PARAM      0x40
/* CA certificate parameters OK */
# define CERT_PKEY_CA_PARAM      0x80
/* Signing explicitly allowed as opposed to SHA1 fallback */
# define CERT_PKEY_EXPLICIT_SIGN 0x100
/* Client CA issuer names match (always set for server cert) */
# define CERT_PKEY_ISSUER_NAME   0x200
/* Cert type matches client types (always set for server cert) */
# define CERT_PKEY_CERT_TYPE     0x400
/* Cert chain suitable to Suite B */
# define CERT_PKEY_SUITEB        0x800

# define SSL_CONF_FLAG_CMDLINE           0x1
# define SSL_CONF_FLAG_FILE              0x2
# define SSL_CONF_FLAG_CLIENT            0x4
# define SSL_CONF_FLAG_SERVER            0x8
# define SSL_CONF_FLAG_SHOW_ERRORS       0x10
# define SSL_CONF_FLAG_CERTIFICATE       0x20
# define SSL_CONF_FLAG_REQUIRE_PRIVATE   0x40
/* Configuration value types */
# define SSL_CONF_TYPE_UNKNOWN           0x0
# define SSL_CONF_TYPE_STRING            0x1
# define SSL_CONF_TYPE_FILE              0x2
# define SSL_CONF_TYPE_DIR               0x3
# define SSL_CONF_TYPE_NONE              0x4

/*
 * Note: SSL[_CTX]_set_{options,mode} use |= op on the previous value, they
 * cannot be used to clear bits.
 */

unsigned long SSL_CTX_get_options(const SSL_CTX *ctx);
unsigned long SSL_get_options(const SSL* s);
unsigned long SSL_CTX_clear_options(SSL_CTX *ctx, unsigned long op);
unsigned long SSL_clear_options(SSL *s, unsigned long op);
unsigned long SSL_CTX_set_options(SSL_CTX *ctx, unsigned long op);
unsigned long SSL_set_options(SSL *s, unsigned long op);

# define SSL_CTX_set_mode(ctx,op) \
        SSL_CTX_ctrl((ctx),SSL_CTRL_MODE,(op),NULL)
# define SSL_CTX_clear_mode(ctx,op) \
        SSL_CTX_ctrl((ctx),SSL_CTRL_CLEAR_MODE,(op),NULL)
# define SSL_CTX_get_mode(ctx) \
        SSL_CTX_ctrl((ctx),SSL_CTRL_MODE,0,NULL)
# define SSL_clear_mode(ssl,op) \
        SSL_ctrl((ssl),SSL_CTRL_CLEAR_MODE,(op),NULL)
# define SSL_set_mode(ssl,op) \
        SSL_ctrl((ssl),SSL_CTRL_MODE,(op),NULL)
# define SSL_get_mode(ssl) \
        SSL_ctrl((ssl),SSL_CTRL_MODE,0,NULL)
# define SSL_set_mtu(ssl, mtu) \
        SSL_ctrl((ssl),SSL_CTRL_SET_MTU,(mtu),NULL)
# define DTLS_set_link_mtu(ssl, mtu) \
        SSL_ctrl((ssl),DTLS_CTRL_SET_LINK_MTU,(mtu),NULL)
# define DTLS_get_link_min_mtu(ssl) \
        SSL_ctrl((ssl),DTLS_CTRL_GET_LINK_MIN_MTU,0,NULL)

# define SSL_get_secure_renegotiation_support(ssl) \
        SSL_ctrl((ssl), SSL_CTRL_GET_RI_SUPPORT, 0, NULL)

# ifndef OPENSSL_NO_HEARTBEATS
#  define SSL_heartbeat(ssl) \
        SSL_ctrl((ssl),SSL_CTRL_DTLS_EXT_SEND_HEARTBEAT,0,NULL)
# endif

# define SSL_CTX_set_cert_flags(ctx,op) \
        SSL_CTX_ctrl((ctx),SSL_CTRL_CERT_FLAGS,(op),NULL)
# define SSL_set_cert_flags(s,op) \
        SSL_ctrl((s),SSL_CTRL_CERT_FLAGS,(op),NULL)
# define SSL_CTX_clear_cert_flags(ctx,op) \
        SSL_CTX_ctrl((ctx),SSL_CTRL_CLEAR_CERT_FLAGS,(op),NULL)
# define SSL_clear_cert_flags(s,op) \
        SSL_ctrl((s),SSL_CTRL_CLEAR_CERT_FLAGS,(op),NULL)

void SSL_CTX_set_msg_callback(SSL_CTX *ctx,
                              void (*cb) (int write_p, int version,
                                          int content_type, const void *buf,
                                          size_t len, SSL *ssl, void *arg));
void SSL_set_msg_callback(SSL *ssl,
                          void (*cb) (int write_p, int version,
                                      int content_type, const void *buf,
                                      size_t len, SSL *ssl, void *arg));
# define SSL_CTX_set_msg_callback_arg(ctx, arg) SSL_CTX_ctrl((ctx), SSL_CTRL_SET_MSG_CALLBACK_ARG, 0, (arg))
# define SSL_set_msg_callback_arg(ssl, arg) SSL_ctrl((ssl), SSL_CTRL_SET_MSG_CALLBACK_ARG, 0, (arg))

# define SSL_get_extms_support(s) \
        SSL_ctrl((s),SSL_CTRL_GET_EXTMS_SUPPORT,0,NULL)

# ifndef OPENSSL_NO_SRP

/* see tls_srp.c */
__owur int SSL_SRP_CTX_init(SSL *s);
__owur int SSL_CTX_SRP_CTX_init(SSL_CTX *ctx);
int SSL_SRP_CTX_free(SSL *ctx);
int SSL_CTX_SRP_CTX_free(SSL_CTX *ctx);
__owur int SSL_srp_server_param_with_username(SSL *s, int *ad);
__owur int SRP_Calc_A_param(SSL *s);

# endif

/* 100k max cert list */
# define SSL_MAX_CERT_LIST_DEFAULT 1024*100

# define SSL_SESSION_CACHE_MAX_SIZE_DEFAULT      (1024*20)

/*
 * This callback type is used inside SSL_CTX, SSL, and in the functions that
 * set them. It is used to override the generation of SSL/TLS session IDs in
 * a server. Return value should be zero on an error, non-zero to proceed.
 * Also, callbacks should themselves check if the id they generate is unique
 * otherwise the SSL handshake will fail with an error - callbacks can do
 * this using the 'ssl' value they're passed by;
 * SSL_has_matching_session_id(ssl, id, *id_len) The length value passed in
 * is set at the maximum size the session ID can be. In SSLv3/TLSv1 it is 32
 * bytes. The callback can alter this length to be less if desired. It is
 * also an error for the callback to set the size to zero.
 */
typedef int (*GEN_SESSION_CB) (const SSL *ssl, unsigned char *id,
                               unsigned int *id_len);

# define SSL_SESS_CACHE_OFF                      0x0000
# define SSL_SESS_CACHE_CLIENT                   0x0001
# define SSL_SESS_CACHE_SERVER                   0x0002
# define SSL_SESS_CACHE_BOTH     (SSL_SESS_CACHE_CLIENT|SSL_SESS_CACHE_SERVER)
# define SSL_SESS_CACHE_NO_AUTO_CLEAR            0x0080
/* enough comments already ... see SSL_CTX_set_session_cache_mode(3) */
# define SSL_SESS_CACHE_NO_INTERNAL_LOOKUP       0x0100
# define SSL_SESS_CACHE_NO_INTERNAL_STORE        0x0200
# define SSL_SESS_CACHE_NO_INTERNAL \
        (SSL_SESS_CACHE_NO_INTERNAL_LOOKUP|SSL_SESS_CACHE_NO_INTERNAL_STORE)

LHASH_OF(SSL_SESSION) *SSL_CTX_sessions(SSL_CTX *ctx);
# define SSL_CTX_sess_number(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_NUMBER,0,NULL)
# define SSL_CTX_sess_connect(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_CONNECT,0,NULL)
# define SSL_CTX_sess_connect_good(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_CONNECT_GOOD,0,NULL)
# define SSL_CTX_sess_connect_renegotiate(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_CONNECT_RENEGOTIATE,0,NULL)
# define SSL_CTX_sess_accept(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_ACCEPT,0,NULL)
# define SSL_CTX_sess_accept_renegotiate(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_ACCEPT_RENEGOTIATE,0,NULL)
# define SSL_CTX_sess_accept_good(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_ACCEPT_GOOD,0,NULL)
# define SSL_CTX_sess_hits(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_HIT,0,NULL)
# define SSL_CTX_sess_cb_hits(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_CB_HIT,0,NULL)
# define SSL_CTX_sess_misses(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_MISSES,0,NULL)
# define SSL_CTX_sess_timeouts(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_TIMEOUTS,0,NULL)
# define SSL_CTX_sess_cache_full(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SESS_CACHE_FULL,0,NULL)

void SSL_CTX_sess_set_new_cb(SSL_CTX *ctx,
                             int (*new_session_cb) (struct ssl_st *ssl,
                                                    SSL_SESSION *sess));
int (*SSL_CTX_sess_get_new_cb(SSL_CTX *ctx)) (struct ssl_st *ssl,
                                              SSL_SESSION *sess);
void SSL_CTX_sess_set_remove_cb(SSL_CTX *ctx,
                                void (*remove_session_cb) (struct ssl_ctx_st
                                                           *ctx,
                                                           SSL_SESSION
                                                           *sess));
void (*SSL_CTX_sess_get_remove_cb(SSL_CTX *ctx)) (struct ssl_ctx_st *ctx,
                                                  SSL_SESSION *sess);
void SSL_CTX_sess_set_get_cb(SSL_CTX *ctx,
                             SSL_SESSION *(*get_session_cb) (struct ssl_st
                                                             *ssl,
                                                             const unsigned char
                                                             *data, int len,
                                                             int *copy));
SSL_SESSION *(*SSL_CTX_sess_get_get_cb(SSL_CTX *ctx)) (struct ssl_st *ssl,
                                                       const unsigned char *data,
                                                       int len, int *copy);
void SSL_CTX_set_info_callback(SSL_CTX *ctx,
                               void (*cb) (const SSL *ssl, int type,
                                           int val));
void (*SSL_CTX_get_info_callback(SSL_CTX *ctx)) (const SSL *ssl, int type,
                                                 int val);
void SSL_CTX_set_client_cert_cb(SSL_CTX *ctx,
                                int (*client_cert_cb) (SSL *ssl, X509 **x509,
                                                       EVP_PKEY **pkey));
int (*SSL_CTX_get_client_cert_cb(SSL_CTX *ctx)) (SSL *ssl, X509 **x509,
                                                 EVP_PKEY **pkey);
# ifndef OPENSSL_NO_ENGINE
__owur int SSL_CTX_set_client_cert_engine(SSL_CTX *ctx, ENGINE *e);
# endif
void SSL_CTX_set_cookie_generate_cb(SSL_CTX *ctx,
                                    int (*app_gen_cookie_cb) (SSL *ssl,
                                                              unsigned char
                                                              *cookie,
                                                              unsigned int
                                                              *cookie_len));
void SSL_CTX_set_cookie_verify_cb(SSL_CTX *ctx,
                                  int (*app_verify_cookie_cb) (SSL *ssl,
                                                               const unsigned char
                                                               *cookie,
                                                               unsigned int
                                                               cookie_len));
# ifndef OPENSSL_NO_NEXTPROTONEG
void SSL_CTX_set_next_protos_advertised_cb(SSL_CTX *s,
                                           int (*cb) (SSL *ssl,
                                                      const unsigned char
                                                      **out,
                                                      unsigned int *outlen,
                                                      void *arg), void *arg);
void SSL_CTX_set_next_proto_select_cb(SSL_CTX *s,
                                      int (*cb) (SSL *ssl,
                                                 unsigned char **out,
                                                 unsigned char *outlen,
                                                 const unsigned char *in,
                                                 unsigned int inlen,
                                                 void *arg), void *arg);
void SSL_get0_next_proto_negotiated(const SSL *s, const unsigned char **data,
                                    unsigned *len);
# endif

__owur int SSL_select_next_proto(unsigned char **out, unsigned char *outlen,
                          const unsigned char *in, unsigned int inlen,
                          const unsigned char *client,
                          unsigned int client_len);

# define OPENSSL_NPN_UNSUPPORTED 0
# define OPENSSL_NPN_NEGOTIATED  1
# define OPENSSL_NPN_NO_OVERLAP  2

__owur int SSL_CTX_set_alpn_protos(SSL_CTX *ctx, const unsigned char *protos,
                                   unsigned int protos_len);
__owur int SSL_set_alpn_protos(SSL *ssl, const unsigned char *protos,
                               unsigned int protos_len);
void SSL_CTX_set_alpn_select_cb(SSL_CTX *ctx,
                                int (*cb) (SSL *ssl,
                                           const unsigned char **out,
                                           unsigned char *outlen,
                                           const unsigned char *in,
                                           unsigned int inlen,
                                           void *arg), void *arg);
void SSL_get0_alpn_selected(const SSL *ssl, const unsigned char **data,
                            unsigned int *len);

# ifndef OPENSSL_NO_PSK
/*
 * the maximum length of the buffer given to callbacks containing the
 * resulting identity/psk
 */
#  define PSK_MAX_IDENTITY_LEN 128
#  define PSK_MAX_PSK_LEN 256
void SSL_CTX_set_psk_client_callback(SSL_CTX *ctx,
                                     unsigned int (*psk_client_callback) (SSL
                                                                          *ssl,
                                                                          const
                                                                          char
                                                                          *hint,
                                                                          char
                                                                          *identity,
                                                                          unsigned
                                                                          int
                                                                          max_identity_len,
                                                                          unsigned
                                                                          char
                                                                          *psk,
                                                                          unsigned
                                                                          int
                                                                          max_psk_len));
void SSL_set_psk_client_callback(SSL *ssl,
                                 unsigned int (*psk_client_callback) (SSL
                                                                      *ssl,
                                                                      const
                                                                      char
                                                                      *hint,
                                                                      char
                                                                      *identity,
                                                                      unsigned
                                                                      int
                                                                      max_identity_len,
                                                                      unsigned
                                                                      char
                                                                      *psk,
                                                                      unsigned
                                                                      int
                                                                      max_psk_len));
void SSL_CTX_set_psk_server_callback(SSL_CTX *ctx,
                                     unsigned int (*psk_server_callback) (SSL
                                                                          *ssl,
                                                                          const
                                                                          char
                                                                          *identity,
                                                                          unsigned
                                                                          char
                                                                          *psk,
                                                                          unsigned
                                                                          int
                                                                          max_psk_len));
void SSL_set_psk_server_callback(SSL *ssl,
                                 unsigned int (*psk_server_callback) (SSL
                                                                      *ssl,
                                                                      const
                                                                      char
                                                                      *identity,
                                                                      unsigned
                                                                      char
                                                                      *psk,
                                                                      unsigned
                                                                      int
                                                                      max_psk_len));
__owur int SSL_CTX_use_psk_identity_hint(SSL_CTX *ctx, const char *identity_hint);
__owur int SSL_use_psk_identity_hint(SSL *s, const char *identity_hint);
const char *SSL_get_psk_identity_hint(const SSL *s);
const char *SSL_get_psk_identity(const SSL *s);
# endif

/* Register callbacks to handle custom TLS Extensions for client or server. */

__owur int SSL_CTX_has_client_custom_ext(const SSL_CTX *ctx,
                                         unsigned int ext_type);

__owur int SSL_CTX_add_client_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb,
                                  void *parse_arg);

__owur int SSL_CTX_add_server_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb,
                                  void *parse_arg);

__owur int SSL_extension_supported(unsigned int ext_type);

# define SSL_NOTHING            1
# define SSL_WRITING            2
# define SSL_READING            3
# define SSL_X509_LOOKUP        4
# define SSL_ASYNC_PAUSED       5
# define SSL_ASYNC_NO_JOBS      6

/* These will only be used when doing non-blocking IO */
# define SSL_want_nothing(s)     (SSL_want(s) == SSL_NOTHING)
# define SSL_want_read(s)        (SSL_want(s) == SSL_READING)
# define SSL_want_write(s)       (SSL_want(s) == SSL_WRITING)
# define SSL_want_x509_lookup(s) (SSL_want(s) == SSL_X509_LOOKUP)
# define SSL_want_async(s)       (SSL_want(s) == SSL_ASYNC_PAUSED)
# define SSL_want_async_job(s)   (SSL_want(s) == SSL_ASYNC_NO_JOBS)

# define SSL_MAC_FLAG_READ_MAC_STREAM 1
# define SSL_MAC_FLAG_WRITE_MAC_STREAM 2

#ifdef __cplusplus
}
#endif

# include <openssl/ssl2.h>
# include <openssl/ssl3.h>
# include <openssl/tls1.h>      /* This is mostly sslv3 with a few tweaks */
# include <openssl/dtls1.h>     /* Datagram TLS */
# include <openssl/srtp.h>      /* Support for the use_srtp extension */

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * These need to be after the above set of includes due to a compiler bug
 * in VisualStudio 2015
 */
DEFINE_STACK_OF_CONST(SSL_CIPHER)
DEFINE_STACK_OF(SSL_COMP)

/* compatibility */
# define SSL_set_app_data(s,arg)         (SSL_set_ex_data(s,0,(char *)arg))
# define SSL_get_app_data(s)             (SSL_get_ex_data(s,0))
# define SSL_SESSION_set_app_data(s,a)   (SSL_SESSION_set_ex_data(s,0,(char *)a))
# define SSL_SESSION_get_app_data(s)     (SSL_SESSION_get_ex_data(s,0))
# define SSL_CTX_get_app_data(ctx)       (SSL_CTX_get_ex_data(ctx,0))
# define SSL_CTX_set_app_data(ctx,arg)   (SSL_CTX_set_ex_data(ctx,0,(char *)arg))
DEPRECATEDIN_1_1_0(void SSL_set_debug(SSL *s, int debug))


/*
 * The valid handshake states (one for each type message sent and one for each
 * type of message received). There are also two "special" states:
 * TLS = TLS or DTLS state
 * DTLS = DTLS specific state
 * CR/SR = Client Read/Server Read
 * CW/SW = Client Write/Server Write
 *
 * The "special" states are:
 * TLS_ST_BEFORE = No handshake has been initiated yet
 * TLS_ST_OK = A handshake has been successfully completed
 */
typedef enum {
    TLS_ST_BEFORE,
    TLS_ST_OK,
    DTLS_ST_CR_HELLO_VERIFY_REQUEST,
    TLS_ST_CR_SRVR_HELLO,
    TLS_ST_CR_CERT,
    TLS_ST_CR_CERT_STATUS,
    TLS_ST_CR_KEY_EXCH,
    TLS_ST_CR_CERT_REQ,
    TLS_ST_CR_SRVR_DONE,
    TLS_ST_CR_SESSION_TICKET,
    TLS_ST_CR_CHANGE,
    TLS_ST_CR_FINISHED,
    TLS_ST_CW_CLNT_HELLO,
    TLS_ST_CW_CERT,
    TLS_ST_CW_KEY_EXCH,
    TLS_ST_CW_CERT_VRFY,
    TLS_ST_CW_CHANGE,
    TLS_ST_CW_NEXT_PROTO,
    TLS_ST_CW_FINISHED,
    TLS_ST_SW_HELLO_REQ,
    TLS_ST_SR_CLNT_HELLO,
    DTLS_ST_SW_HELLO_VERIFY_REQUEST,
    TLS_ST_SW_SRVR_HELLO,
    TLS_ST_SW_CERT,
    TLS_ST_SW_KEY_EXCH,
    TLS_ST_SW_CERT_REQ,
    TLS_ST_SW_SRVR_DONE,
    TLS_ST_SR_CERT,
    TLS_ST_SR_KEY_EXCH,
    TLS_ST_SR_CERT_VRFY,
    TLS_ST_SR_NEXT_PROTO,
    TLS_ST_SR_CHANGE,
    TLS_ST_SR_FINISHED,
    TLS_ST_SW_SESSION_TICKET,
    TLS_ST_SW_CERT_STATUS,
    TLS_ST_SW_CHANGE,
    TLS_ST_SW_FINISHED
} OSSL_HANDSHAKE_STATE;

/*
 * Most of the following state values are no longer used and are defined to be
 * the closest equivalent value in the current state machine code. Not all
 * defines have an equivalent and are set to a dummy value (-1). SSL_ST_CONNECT
 * and SSL_ST_ACCEPT are still in use in the definition of SSL_CB_ACCEPT_LOOP,
 * SSL_CB_ACCEPT_EXIT, SSL_CB_CONNECT_LOOP and SSL_CB_CONNECT_EXIT.
 */

# define SSL_ST_CONNECT                  0x1000
# define SSL_ST_ACCEPT                   0x2000

# define SSL_ST_MASK                     0x0FFF

# define SSL_CB_LOOP                     0x01
# define SSL_CB_EXIT                     0x02
# define SSL_CB_READ                     0x04
# define SSL_CB_WRITE                    0x08
# define SSL_CB_ALERT                    0x4000/* used in callback */
# define SSL_CB_READ_ALERT               (SSL_CB_ALERT|SSL_CB_READ)
# define SSL_CB_WRITE_ALERT              (SSL_CB_ALERT|SSL_CB_WRITE)
# define SSL_CB_ACCEPT_LOOP              (SSL_ST_ACCEPT|SSL_CB_LOOP)
# define SSL_CB_ACCEPT_EXIT              (SSL_ST_ACCEPT|SSL_CB_EXIT)
# define SSL_CB_CONNECT_LOOP             (SSL_ST_CONNECT|SSL_CB_LOOP)
# define SSL_CB_CONNECT_EXIT             (SSL_ST_CONNECT|SSL_CB_EXIT)
# define SSL_CB_HANDSHAKE_START          0x10
# define SSL_CB_HANDSHAKE_DONE           0x20

/* Is the SSL_connection established? */
# define SSL_in_connect_init(a)          (SSL_in_init(a) && !SSL_is_server(a))
# define SSL_in_accept_init(a)           (SSL_in_init(a) && SSL_is_server(a))
int SSL_in_init(SSL *s);
int SSL_in_before(SSL *s);
int SSL_is_init_finished(SSL *s);

/*
 * The following 3 states are kept in ssl->rlayer.rstate when reads fail, you
 * should not need these
 */
# define SSL_ST_READ_HEADER                      0xF0
# define SSL_ST_READ_BODY                        0xF1
# define SSL_ST_READ_DONE                        0xF2

/*-
 * Obtain latest Finished message
 *   -- that we sent (SSL_get_finished)
 *   -- that we expected from peer (SSL_get_peer_finished).
 * Returns length (0 == no Finished so far), copies up to 'count' bytes.
 */
size_t SSL_get_finished(const SSL *s, void *buf, size_t count);
size_t SSL_get_peer_finished(const SSL *s, void *buf, size_t count);

/*
 * use either SSL_VERIFY_NONE or SSL_VERIFY_PEER, the last 2 options are
 * 'ored' with SSL_VERIFY_PEER if they are desired
 */
# define SSL_VERIFY_NONE                 0x00
# define SSL_VERIFY_PEER                 0x01
# define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02
# define SSL_VERIFY_CLIENT_ONCE          0x04

# if OPENSSL_API_COMPAT < 0x10100000L
#  define OpenSSL_add_ssl_algorithms()   SSL_library_init()
#  define SSLeay_add_ssl_algorithms()    SSL_library_init()
# endif

/* More backward compatibility */
# define SSL_get_cipher(s) \
                SSL_CIPHER_get_name(SSL_get_current_cipher(s))
# define SSL_get_cipher_bits(s,np) \
                SSL_CIPHER_get_bits(SSL_get_current_cipher(s),np)
# define SSL_get_cipher_version(s) \
                SSL_CIPHER_get_version(SSL_get_current_cipher(s))
# define SSL_get_cipher_name(s) \
                SSL_CIPHER_get_name(SSL_get_current_cipher(s))
# define SSL_get_time(a)         SSL_SESSION_get_time(a)
# define SSL_set_time(a,b)       SSL_SESSION_set_time((a),(b))
# define SSL_get_timeout(a)      SSL_SESSION_get_timeout(a)
# define SSL_set_timeout(a,b)    SSL_SESSION_set_timeout((a),(b))

# define d2i_SSL_SESSION_bio(bp,s_id) ASN1_d2i_bio_of(SSL_SESSION,SSL_SESSION_new,d2i_SSL_SESSION,bp,s_id)
# define i2d_SSL_SESSION_bio(bp,s_id) ASN1_i2d_bio_of(SSL_SESSION,i2d_SSL_SESSION,bp,s_id)

DECLARE_PEM_rw(SSL_SESSION, SSL_SESSION)
# define SSL_AD_REASON_OFFSET            1000/* offset to get SSL_R_... value
                                              * from SSL_AD_... */
/* These alert types are for SSLv3 and TLSv1 */
# define SSL_AD_CLOSE_NOTIFY             SSL3_AD_CLOSE_NOTIFY
/* fatal */
# define SSL_AD_UNEXPECTED_MESSAGE       SSL3_AD_UNEXPECTED_MESSAGE
/* fatal */
# define SSL_AD_BAD_RECORD_MAC           SSL3_AD_BAD_RECORD_MAC
# define SSL_AD_DECRYPTION_FAILED        TLS1_AD_DECRYPTION_FAILED
# define SSL_AD_RECORD_OVERFLOW          TLS1_AD_RECORD_OVERFLOW
/* fatal */
# define SSL_AD_DECOMPRESSION_FAILURE    SSL3_AD_DECOMPRESSION_FAILURE
/* fatal */
# define SSL_AD_HANDSHAKE_FAILURE        SSL3_AD_HANDSHAKE_FAILURE
/* Not for TLS */
# define SSL_AD_NO_CERTIFICATE           SSL3_AD_NO_CERTIFICATE
# define SSL_AD_BAD_CERTIFICATE          SSL3_AD_BAD_CERTIFICATE
# define SSL_AD_UNSUPPORTED_CERTIFICATE  SSL3_AD_UNSUPPORTED_CERTIFICATE
# define SSL_AD_CERTIFICATE_REVOKED      SSL3_AD_CERTIFICATE_REVOKED
# define SSL_AD_CERTIFICATE_EXPIRED      SSL3_AD_CERTIFICATE_EXPIRED
# define SSL_AD_CERTIFICATE_UNKNOWN      SSL3_AD_CERTIFICATE_UNKNOWN
/* fatal */
# define SSL_AD_ILLEGAL_PARAMETER        SSL3_AD_ILLEGAL_PARAMETER
/* fatal */
# define SSL_AD_UNKNOWN_CA               TLS1_AD_UNKNOWN_CA
/* fatal */
# define SSL_AD_ACCESS_DENIED            TLS1_AD_ACCESS_DENIED
/* fatal */
# define SSL_AD_DECODE_ERROR             TLS1_AD_DECODE_ERROR
# define SSL_AD_DECRYPT_ERROR            TLS1_AD_DECRYPT_ERROR
/* fatal */
# define SSL_AD_EXPORT_RESTRICTION       TLS1_AD_EXPORT_RESTRICTION
/* fatal */
# define SSL_AD_PROTOCOL_VERSION         TLS1_AD_PROTOCOL_VERSION
/* fatal */
# define SSL_AD_INSUFFICIENT_SECURITY    TLS1_AD_INSUFFICIENT_SECURITY
/* fatal */
# define SSL_AD_INTERNAL_ERROR           TLS1_AD_INTERNAL_ERROR
# define SSL_AD_USER_CANCELLED           TLS1_AD_USER_CANCELLED
# define SSL_AD_NO_RENEGOTIATION         TLS1_AD_NO_RENEGOTIATION
# define SSL_AD_UNSUPPORTED_EXTENSION    TLS1_AD_UNSUPPORTED_EXTENSION
# define SSL_AD_CERTIFICATE_UNOBTAINABLE TLS1_AD_CERTIFICATE_UNOBTAINABLE
# define SSL_AD_UNRECOGNIZED_NAME        TLS1_AD_UNRECOGNIZED_NAME
# define SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE
# define SSL_AD_BAD_CERTIFICATE_HASH_VALUE TLS1_AD_BAD_CERTIFICATE_HASH_VALUE
/* fatal */
# define SSL_AD_UNKNOWN_PSK_IDENTITY     TLS1_AD_UNKNOWN_PSK_IDENTITY
/* fatal */
# define SSL_AD_INAPPROPRIATE_FALLBACK   TLS1_AD_INAPPROPRIATE_FALLBACK
# define SSL_AD_NO_APPLICATION_PROTOCOL  TLS1_AD_NO_APPLICATION_PROTOCOL
# define SSL_ERROR_NONE                  0
# define SSL_ERROR_SSL                   1
# define SSL_ERROR_WANT_READ             2
# define SSL_ERROR_WANT_WRITE            3
# define SSL_ERROR_WANT_X509_LOOKUP      4
# define SSL_ERROR_SYSCALL               5/* look at error stack/return
                                           * value/errno */
# define SSL_ERROR_ZERO_RETURN           6
# define SSL_ERROR_WANT_CONNECT          7
# define SSL_ERROR_WANT_ACCEPT           8
# define SSL_ERROR_WANT_ASYNC            9
# define SSL_ERROR_WANT_ASYNC_JOB       10
# define SSL_CTRL_SET_TMP_DH                     3
# define SSL_CTRL_SET_TMP_ECDH                   4
# define SSL_CTRL_SET_TMP_DH_CB                  6
# define SSL_CTRL_GET_CLIENT_CERT_REQUEST        9
# define SSL_CTRL_GET_NUM_RENEGOTIATIONS         10
# define SSL_CTRL_CLEAR_NUM_RENEGOTIATIONS       11
# define SSL_CTRL_GET_TOTAL_RENEGOTIATIONS       12
# define SSL_CTRL_GET_FLAGS                      13
# define SSL_CTRL_EXTRA_CHAIN_CERT               14
# define SSL_CTRL_SET_MSG_CALLBACK               15
# define SSL_CTRL_SET_MSG_CALLBACK_ARG           16
/* only applies to datagram connections */
# define SSL_CTRL_SET_MTU                17
/* Stats */
# define SSL_CTRL_SESS_NUMBER                    20
# define SSL_CTRL_SESS_CONNECT                   21
# define SSL_CTRL_SESS_CONNECT_GOOD              22
# define SSL_CTRL_SESS_CONNECT_RENEGOTIATE       23
# define SSL_CTRL_SESS_ACCEPT                    24
# define SSL_CTRL_SESS_ACCEPT_GOOD               25
# define SSL_CTRL_SESS_ACCEPT_RENEGOTIATE        26
# define SSL_CTRL_SESS_HIT                       27
# define SSL_CTRL_SESS_CB_HIT                    28
# define SSL_CTRL_SESS_MISSES                    29
# define SSL_CTRL_SESS_TIMEOUTS                  30
# define SSL_CTRL_SESS_CACHE_FULL                31
# define SSL_CTRL_MODE                           33
# define SSL_CTRL_GET_READ_AHEAD                 40
# define SSL_CTRL_SET_READ_AHEAD                 41
# define SSL_CTRL_SET_SESS_CACHE_SIZE            42
# define SSL_CTRL_GET_SESS_CACHE_SIZE            43
# define SSL_CTRL_SET_SESS_CACHE_MODE            44
# define SSL_CTRL_GET_SESS_CACHE_MODE            45
# define SSL_CTRL_GET_MAX_CERT_LIST              50
# define SSL_CTRL_SET_MAX_CERT_LIST              51
# define SSL_CTRL_SET_MAX_SEND_FRAGMENT          52
/* see tls1.h for macros based on these */
# define SSL_CTRL_SET_TLSEXT_SERVERNAME_CB       53
# define SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG      54
# define SSL_CTRL_SET_TLSEXT_HOSTNAME            55
# define SSL_CTRL_SET_TLSEXT_DEBUG_CB            56
# define SSL_CTRL_SET_TLSEXT_DEBUG_ARG           57
# define SSL_CTRL_GET_TLSEXT_TICKET_KEYS         58
# define SSL_CTRL_SET_TLSEXT_TICKET_KEYS         59
/*# define SSL_CTRL_SET_TLSEXT_OPAQUE_PRF_INPUT    60 */
/*# define SSL_CTRL_SET_TLSEXT_OPAQUE_PRF_INPUT_CB 61 */
/*# define SSL_CTRL_SET_TLSEXT_OPAQUE_PRF_INPUT_CB_ARG 62 */
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB       63
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB_ARG   64
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE     65
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_EXTS     66
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_EXTS     67
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_IDS      68
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_IDS      69
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_OCSP_RESP        70
# define SSL_CTRL_SET_TLSEXT_STATUS_REQ_OCSP_RESP        71
# define SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB       72
# define SSL_CTRL_SET_TLS_EXT_SRP_USERNAME_CB    75
# define SSL_CTRL_SET_SRP_VERIFY_PARAM_CB                76
# define SSL_CTRL_SET_SRP_GIVE_CLIENT_PWD_CB             77
# define SSL_CTRL_SET_SRP_ARG            78
# define SSL_CTRL_SET_TLS_EXT_SRP_USERNAME               79
# define SSL_CTRL_SET_TLS_EXT_SRP_STRENGTH               80
# define SSL_CTRL_SET_TLS_EXT_SRP_PASSWORD               81
# ifndef OPENSSL_NO_HEARTBEATS
#  define SSL_CTRL_DTLS_EXT_SEND_HEARTBEAT               85
#  define SSL_CTRL_GET_DTLS_EXT_HEARTBEAT_PENDING        86
#  define SSL_CTRL_SET_DTLS_EXT_HEARTBEAT_NO_REQUESTS    87
# endif
# define DTLS_CTRL_GET_TIMEOUT           73
# define DTLS_CTRL_HANDLE_TIMEOUT        74
# define SSL_CTRL_GET_RI_SUPPORT                 76
# define SSL_CTRL_CLEAR_MODE                     78
# define SSL_CTRL_SET_NOT_RESUMABLE_SESS_CB      79
# define SSL_CTRL_GET_EXTRA_CHAIN_CERTS          82
# define SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS        83
# define SSL_CTRL_CHAIN                          88
# define SSL_CTRL_CHAIN_CERT                     89
# define SSL_CTRL_GET_CURVES                     90
# define SSL_CTRL_SET_CURVES                     91
# define SSL_CTRL_SET_CURVES_LIST                92
# define SSL_CTRL_GET_SHARED_CURVE               93
# define SSL_CTRL_SET_SIGALGS                    97
# define SSL_CTRL_SET_SIGALGS_LIST               98
# define SSL_CTRL_CERT_FLAGS                     99
# define SSL_CTRL_CLEAR_CERT_FLAGS               100
# define SSL_CTRL_SET_CLIENT_SIGALGS             101
# define SSL_CTRL_SET_CLIENT_SIGALGS_LIST        102
# define SSL_CTRL_GET_CLIENT_CERT_TYPES          103
# define SSL_CTRL_SET_CLIENT_CERT_TYPES          104
# define SSL_CTRL_BUILD_CERT_CHAIN               105
# define SSL_CTRL_SET_VERIFY_CERT_STORE          106
# define SSL_CTRL_SET_CHAIN_CERT_STORE           107
# define SSL_CTRL_GET_PEER_SIGNATURE_NID         108
# define SSL_CTRL_GET_SERVER_TMP_KEY             109
# define SSL_CTRL_GET_RAW_CIPHERLIST             110
# define SSL_CTRL_GET_EC_POINT_FORMATS           111
# define SSL_CTRL_GET_CHAIN_CERTS                115
# define SSL_CTRL_SELECT_CURRENT_CERT            116
# define SSL_CTRL_SET_CURRENT_CERT               117
# define SSL_CTRL_SET_DH_AUTO                    118
# define DTLS_CTRL_SET_LINK_MTU                  120
# define DTLS_CTRL_GET_LINK_MIN_MTU              121
# define SSL_CTRL_GET_EXTMS_SUPPORT              122
# define SSL_CTRL_SET_MIN_PROTO_VERSION          123
# define SSL_CTRL_SET_MAX_PROTO_VERSION          124
# define SSL_CTRL_SET_SPLIT_SEND_FRAGMENT        125
# define SSL_CTRL_SET_MAX_PIPELINES              126
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE     127
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB       128
# define SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB_ARG   129
# define SSL_CTRL_GET_MIN_PROTO_VERSION          130
# define SSL_CTRL_GET_MAX_PROTO_VERSION          131
# define SSL_CERT_SET_FIRST                      1
# define SSL_CERT_SET_NEXT                       2
# define SSL_CERT_SET_SERVER                     3
# define DTLSv1_get_timeout(ssl, arg) \
        SSL_ctrl(ssl,DTLS_CTRL_GET_TIMEOUT,0, (void *)arg)
# define DTLSv1_handle_timeout(ssl) \
        SSL_ctrl(ssl,DTLS_CTRL_HANDLE_TIMEOUT,0, NULL)
# define SSL_num_renegotiations(ssl) \
        SSL_ctrl((ssl),SSL_CTRL_GET_NUM_RENEGOTIATIONS,0,NULL)
# define SSL_clear_num_renegotiations(ssl) \
        SSL_ctrl((ssl),SSL_CTRL_CLEAR_NUM_RENEGOTIATIONS,0,NULL)
# define SSL_total_renegotiations(ssl) \
        SSL_ctrl((ssl),SSL_CTRL_GET_TOTAL_RENEGOTIATIONS,0,NULL)
# define SSL_CTX_set_tmp_dh(ctx,dh) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_TMP_DH,0,(char *)dh)
# define SSL_CTX_set_tmp_ecdh(ctx,ecdh) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_TMP_ECDH,0,(char *)ecdh)
# define SSL_CTX_set_dh_auto(ctx, onoff) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_DH_AUTO,onoff,NULL)
# define SSL_set_dh_auto(s, onoff) \
        SSL_ctrl(s,SSL_CTRL_SET_DH_AUTO,onoff,NULL)
# define SSL_set_tmp_dh(ssl,dh) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TMP_DH,0,(char *)dh)
# define SSL_set_tmp_ecdh(ssl,ecdh) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TMP_ECDH,0,(char *)ecdh)
# define SSL_CTX_add_extra_chain_cert(ctx,x509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_EXTRA_CHAIN_CERT,0,(char *)x509)
# define SSL_CTX_get_extra_chain_certs(ctx,px509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_EXTRA_CHAIN_CERTS,0,px509)
# define SSL_CTX_get_extra_chain_certs_only(ctx,px509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_EXTRA_CHAIN_CERTS,1,px509)
# define SSL_CTX_clear_extra_chain_certs(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS,0,NULL)
# define SSL_CTX_set0_chain(ctx,sk) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_CHAIN,0,(char *)sk)
# define SSL_CTX_set1_chain(ctx,sk) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_CHAIN,1,(char *)sk)
# define SSL_CTX_add0_chain_cert(ctx,x509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_CHAIN_CERT,0,(char *)x509)
# define SSL_CTX_add1_chain_cert(ctx,x509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_CHAIN_CERT,1,(char *)x509)
# define SSL_CTX_get0_chain_certs(ctx,px509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_CHAIN_CERTS,0,px509)
# define SSL_CTX_clear_chain_certs(ctx) \
        SSL_CTX_set0_chain(ctx,NULL)
# define SSL_CTX_build_cert_chain(ctx, flags) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_BUILD_CERT_CHAIN, flags, NULL)
# define SSL_CTX_select_current_cert(ctx,x509) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SELECT_CURRENT_CERT,0,(char *)x509)
# define SSL_CTX_set_current_cert(ctx, op) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CURRENT_CERT, op, NULL)
# define SSL_CTX_set0_verify_cert_store(ctx,st) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_VERIFY_CERT_STORE,0,(char *)st)
# define SSL_CTX_set1_verify_cert_store(ctx,st) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_VERIFY_CERT_STORE,1,(char *)st)
# define SSL_CTX_set0_chain_cert_store(ctx,st) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CHAIN_CERT_STORE,0,(char *)st)
# define SSL_CTX_set1_chain_cert_store(ctx,st) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CHAIN_CERT_STORE,1,(char *)st)
# define SSL_set0_chain(ctx,sk) \
        SSL_ctrl(ctx,SSL_CTRL_CHAIN,0,(char *)sk)
# define SSL_set1_chain(ctx,sk) \
        SSL_ctrl(ctx,SSL_CTRL_CHAIN,1,(char *)sk)
# define SSL_add0_chain_cert(ctx,x509) \
        SSL_ctrl(ctx,SSL_CTRL_CHAIN_CERT,0,(char *)x509)
# define SSL_add1_chain_cert(ctx,x509) \
        SSL_ctrl(ctx,SSL_CTRL_CHAIN_CERT,1,(char *)x509)
# define SSL_get0_chain_certs(ctx,px509) \
        SSL_ctrl(ctx,SSL_CTRL_GET_CHAIN_CERTS,0,px509)
# define SSL_clear_chain_certs(ctx) \
        SSL_set0_chain(ctx,NULL)
# define SSL_build_cert_chain(s, flags) \
        SSL_ctrl(s,SSL_CTRL_BUILD_CERT_CHAIN, flags, NULL)
# define SSL_select_current_cert(ctx,x509) \
        SSL_ctrl(ctx,SSL_CTRL_SELECT_CURRENT_CERT,0,(char *)x509)
# define SSL_set_current_cert(ctx,op) \
        SSL_ctrl(ctx,SSL_CTRL_SET_CURRENT_CERT, op, NULL)
# define SSL_set0_verify_cert_store(s,st) \
        SSL_ctrl(s,SSL_CTRL_SET_VERIFY_CERT_STORE,0,(char *)st)
# define SSL_set1_verify_cert_store(s,st) \
        SSL_ctrl(s,SSL_CTRL_SET_VERIFY_CERT_STORE,1,(char *)st)
# define SSL_set0_chain_cert_store(s,st) \
        SSL_ctrl(s,SSL_CTRL_SET_CHAIN_CERT_STORE,0,(char *)st)
# define SSL_set1_chain_cert_store(s,st) \
        SSL_ctrl(s,SSL_CTRL_SET_CHAIN_CERT_STORE,1,(char *)st)
# define SSL_get1_curves(ctx, s) \
        SSL_ctrl(ctx,SSL_CTRL_GET_CURVES,0,(char *)s)
# define SSL_CTX_set1_curves(ctx, clist, clistlen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CURVES,clistlen,(char *)clist)
# define SSL_CTX_set1_curves_list(ctx, s) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CURVES_LIST,0,(char *)s)
# define SSL_set1_curves(ctx, clist, clistlen) \
        SSL_ctrl(ctx,SSL_CTRL_SET_CURVES,clistlen,(char *)clist)
# define SSL_set1_curves_list(ctx, s) \
        SSL_ctrl(ctx,SSL_CTRL_SET_CURVES_LIST,0,(char *)s)
# define SSL_get_shared_curve(s, n) \
        SSL_ctrl(s,SSL_CTRL_GET_SHARED_CURVE,n,NULL)
# define SSL_CTX_set1_sigalgs(ctx, slist, slistlen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_SIGALGS,slistlen,(int *)slist)
# define SSL_CTX_set1_sigalgs_list(ctx, s) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_SIGALGS_LIST,0,(char *)s)
# define SSL_set1_sigalgs(ctx, slist, slistlen) \
        SSL_ctrl(ctx,SSL_CTRL_SET_SIGALGS,slistlen,(int *)slist)
# define SSL_set1_sigalgs_list(ctx, s) \
        SSL_ctrl(ctx,SSL_CTRL_SET_SIGALGS_LIST,0,(char *)s)
# define SSL_CTX_set1_client_sigalgs(ctx, slist, slistlen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CLIENT_SIGALGS,slistlen,(int *)slist)
# define SSL_CTX_set1_client_sigalgs_list(ctx, s) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CLIENT_SIGALGS_LIST,0,(char *)s)
# define SSL_set1_client_sigalgs(ctx, slist, slistlen) \
        SSL_ctrl(ctx,SSL_CTRL_SET_CLIENT_SIGALGS,clistlen,(int *)slist)
# define SSL_set1_client_sigalgs_list(ctx, s) \
        SSL_ctrl(ctx,SSL_CTRL_SET_CLIENT_SIGALGS_LIST,0,(char *)s)
# define SSL_get0_certificate_types(s, clist) \
        SSL_ctrl(s, SSL_CTRL_GET_CLIENT_CERT_TYPES, 0, (char *)clist)
# define SSL_CTX_set1_client_certificate_types(ctx, clist, clistlen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_CLIENT_CERT_TYPES,clistlen,(char *)clist)
# define SSL_set1_client_certificate_types(s, clist, clistlen) \
        SSL_ctrl(s,SSL_CTRL_SET_CLIENT_CERT_TYPES,clistlen,(char *)clist)
# define SSL_get_peer_signature_nid(s, pn) \
        SSL_ctrl(s,SSL_CTRL_GET_PEER_SIGNATURE_NID,0,pn)
# define SSL_get_server_tmp_key(s, pk) \
        SSL_ctrl(s,SSL_CTRL_GET_SERVER_TMP_KEY,0,pk)
# define SSL_get0_raw_cipherlist(s, plst) \
        SSL_ctrl(s,SSL_CTRL_GET_RAW_CIPHERLIST,0,plst)
# define SSL_get0_ec_point_formats(s, plst) \
        SSL_ctrl(s,SSL_CTRL_GET_EC_POINT_FORMATS,0,plst)
#define SSL_CTX_set_min_proto_version(ctx, version) \
        SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MIN_PROTO_VERSION, version, NULL)
#define SSL_CTX_set_max_proto_version(ctx, version) \
        SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MAX_PROTO_VERSION, version, NULL)
#define SSL_CTX_get_min_proto_version(ctx) \
        SSL_CTX_ctrl(ctx, SSL_CTRL_GET_MIN_PROTO_VERSION, 0, NULL)
#define SSL_CTX_get_max_proto_version(ctx) \
        SSL_CTX_ctrl(ctx, SSL_CTRL_GET_MAX_PROTO_VERSION, 0, NULL)
#define SSL_set_min_proto_version(s, version) \
        SSL_ctrl(s, SSL_CTRL_SET_MIN_PROTO_VERSION, version, NULL)
#define SSL_set_max_proto_version(s, version) \
        SSL_ctrl(s, SSL_CTRL_SET_MAX_PROTO_VERSION, version, NULL)
#define SSL_get_min_proto_version(s) \
        SSL_ctrl(s, SSL_CTRL_GET_MIN_PROTO_VERSION, 0, NULL)
#define SSL_get_max_proto_version(s) \
        SSL_ctrl(s, SSL_CTRL_GET_MAX_PROTO_VERSION, 0, NULL)

#if OPENSSL_API_COMPAT < 0x10100000L
/* Provide some compatibility macros for removed functionality. */
# define SSL_CTX_need_tmp_RSA(ctx)                0
# define SSL_CTX_set_tmp_rsa(ctx,rsa)             1
# define SSL_need_tmp_RSA(ssl)                    0
# define SSL_set_tmp_rsa(ssl,rsa)                 1
# define SSL_CTX_set_ecdh_auto(dummy, onoff)      ((onoff) != 0)
# define SSL_set_ecdh_auto(dummy, onoff)          ((onoff) != 0)
/*
 * We "pretend" to call the callback to avoid warnings about unused static
 * functions.
 */
# define SSL_CTX_set_tmp_rsa_callback(ctx, cb)    while(0) (cb)(NULL, 0, 0)
# define SSL_set_tmp_rsa_callback(ssl, cb)        while(0) (cb)(NULL, 0, 0)
#endif

__owur const BIO_METHOD *BIO_f_ssl(void);
__owur BIO *BIO_new_ssl(SSL_CTX *ctx, int client);
__owur BIO *BIO_new_ssl_connect(SSL_CTX *ctx);
__owur BIO *BIO_new_buffer_ssl_connect(SSL_CTX *ctx);
__owur int BIO_ssl_copy_session_id(BIO *to, BIO *from);
void BIO_ssl_shutdown(BIO *ssl_bio);

__owur int SSL_CTX_set_cipher_list(SSL_CTX *, const char *str);
__owur SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth);
int SSL_CTX_up_ref(SSL_CTX *ctx);
void SSL_CTX_free(SSL_CTX *);
__owur long SSL_CTX_set_timeout(SSL_CTX *ctx, long t);
__owur long SSL_CTX_get_timeout(const SSL_CTX *ctx);
__owur X509_STORE *SSL_CTX_get_cert_store(const SSL_CTX *);
void SSL_CTX_set_cert_store(SSL_CTX *, X509_STORE *);
__owur int SSL_want(const SSL *s);
__owur int SSL_clear(SSL *s);

void SSL_CTX_flush_sessions(SSL_CTX *ctx, long tm);

__owur const SSL_CIPHER *SSL_get_current_cipher(const SSL *s);
__owur int SSL_CIPHER_get_bits(const SSL_CIPHER *c, int *alg_bits);
__owur const char *SSL_CIPHER_get_version(const SSL_CIPHER *c);
__owur const char *SSL_CIPHER_get_name(const SSL_CIPHER *c);
__owur uint32_t SSL_CIPHER_get_id(const SSL_CIPHER *c);
__owur int SSL_CIPHER_get_kx_nid(const SSL_CIPHER *c);
__owur int SSL_CIPHER_get_auth_nid(const SSL_CIPHER *c);
__owur int SSL_CIPHER_is_aead(const SSL_CIPHER *c);

__owur int SSL_get_fd(const SSL *s);
__owur int SSL_get_rfd(const SSL *s);
__owur int SSL_get_wfd(const SSL *s);
__owur const char *SSL_get_cipher_list(const SSL *s, int n);
__owur char *SSL_get_shared_ciphers(const SSL *s, char *buf, int size);
__owur int SSL_get_read_ahead(const SSL *s);
__owur int SSL_pending(const SSL *s);
__owur int SSL_has_pending(const SSL *s);
# ifndef OPENSSL_NO_SOCK
__owur int SSL_set_fd(SSL *s, int fd);
__owur int SSL_set_rfd(SSL *s, int fd);
__owur int SSL_set_wfd(SSL *s, int fd);
# endif
void SSL_set0_rbio(SSL *s, BIO *rbio);
void SSL_set0_wbio(SSL *s, BIO *wbio);
void SSL_set_bio(SSL *s, BIO *rbio, BIO *wbio);
__owur BIO *SSL_get_rbio(const SSL *s);
__owur BIO *SSL_get_wbio(const SSL *s);
__owur int SSL_set_cipher_list(SSL *s, const char *str);
void SSL_set_read_ahead(SSL *s, int yes);
__owur int SSL_get_verify_mode(const SSL *s);
__owur int SSL_get_verify_depth(const SSL *s);
__owur SSL_verify_cb SSL_get_verify_callback(const SSL *s);
void SSL_set_verify(SSL *s, int mode, SSL_verify_cb callback);
void SSL_set_verify_depth(SSL *s, int depth);
void SSL_set_cert_cb(SSL *s, int (*cb) (SSL *ssl, void *arg), void *arg);
# ifndef OPENSSL_NO_RSA
__owur int SSL_use_RSAPrivateKey(SSL *ssl, RSA *rsa);
__owur int SSL_use_RSAPrivateKey_ASN1(SSL *ssl, const unsigned char *d, long len);
# endif
__owur int SSL_use_PrivateKey(SSL *ssl, EVP_PKEY *pkey);
__owur int SSL_use_PrivateKey_ASN1(int pk, SSL *ssl, const unsigned char *d,
                            long len);
__owur int SSL_use_certificate(SSL *ssl, X509 *x);
__owur int SSL_use_certificate_ASN1(SSL *ssl, const unsigned char *d, int len);

/* Set serverinfo data for the current active cert. */
__owur int SSL_CTX_use_serverinfo(SSL_CTX *ctx, const unsigned char *serverinfo,
                           size_t serverinfo_length);
__owur int SSL_CTX_use_serverinfo_file(SSL_CTX *ctx, const char *file);

#ifndef OPENSSL_NO_RSA
__owur int SSL_use_RSAPrivateKey_file(SSL *ssl, const char *file, int type);
#endif

__owur int SSL_use_PrivateKey_file(SSL *ssl, const char *file, int type);
__owur int SSL_use_certificate_file(SSL *ssl, const char *file, int type);

#ifndef OPENSSL_NO_RSA
__owur int SSL_CTX_use_RSAPrivateKey_file(SSL_CTX *ctx, const char *file, int type);
#endif
__owur int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
__owur int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type);
/* PEM type */
__owur int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file);
__owur int SSL_use_certificate_chain_file(SSL *ssl, const char *file);
__owur STACK_OF(X509_NAME) *SSL_load_client_CA_file(const char *file);
__owur int SSL_add_file_cert_subjects_to_stack(STACK_OF(X509_NAME) *stackCAs,
                                        const char *file);
int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *stackCAs,
                                       const char *dir);

#if OPENSSL_API_COMPAT < 0x10100000L
# define SSL_load_error_strings() \
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS \
                     | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL)
#endif

__owur const char *SSL_state_string(const SSL *s);
__owur const char *SSL_rstate_string(const SSL *s);
__owur const char *SSL_state_string_long(const SSL *s);
__owur const char *SSL_rstate_string_long(const SSL *s);
__owur long SSL_SESSION_get_time(const SSL_SESSION *s);
__owur long SSL_SESSION_set_time(SSL_SESSION *s, long t);
__owur long SSL_SESSION_get_timeout(const SSL_SESSION *s);
__owur long SSL_SESSION_set_timeout(SSL_SESSION *s, long t);
__owur int SSL_SESSION_get_protocol_version(const SSL_SESSION *s);
__owur const char *SSL_SESSION_get0_hostname(const SSL_SESSION *s);
__owur const SSL_CIPHER *SSL_SESSION_get0_cipher(const SSL_SESSION *s);
__owur int SSL_SESSION_has_ticket(const SSL_SESSION *s);
__owur unsigned long SSL_SESSION_get_ticket_lifetime_hint(const SSL_SESSION *s);
void SSL_SESSION_get0_ticket(const SSL_SESSION *s, const unsigned char **tick,
                            size_t *len);
__owur int SSL_copy_session_id(SSL *to, const SSL *from);
__owur X509 *SSL_SESSION_get0_peer(SSL_SESSION *s);
__owur int SSL_SESSION_set1_id_context(SSL_SESSION *s, const unsigned char *sid_ctx,
                                unsigned int sid_ctx_len);
__owur int SSL_SESSION_set1_id(SSL_SESSION *s, const unsigned char *sid,
                               unsigned int sid_len);

__owur SSL_SESSION *SSL_SESSION_new(void);
const unsigned char *SSL_SESSION_get_id(const SSL_SESSION *s,
                                        unsigned int *len);
const unsigned char *SSL_SESSION_get0_id_context(const SSL_SESSION *s,
                                                unsigned int *len);
__owur unsigned int SSL_SESSION_get_compress_id(const SSL_SESSION *s);
# ifndef OPENSSL_NO_STDIO
int SSL_SESSION_print_fp(FILE *fp, const SSL_SESSION *ses);
# endif
int SSL_SESSION_print(BIO *fp, const SSL_SESSION *ses);
int SSL_SESSION_print_keylog(BIO *bp, const SSL_SESSION *x);
int SSL_SESSION_up_ref(SSL_SESSION *ses);
void SSL_SESSION_free(SSL_SESSION *ses);
__owur int i2d_SSL_SESSION(SSL_SESSION *in, unsigned char **pp);
__owur int SSL_set_session(SSL *to, SSL_SESSION *session);
int SSL_CTX_add_session(SSL_CTX *s, SSL_SESSION *c);
int SSL_CTX_remove_session(SSL_CTX *, SSL_SESSION *c);
__owur int SSL_CTX_set_generate_session_id(SSL_CTX *, GEN_SESSION_CB);
__owur int SSL_set_generate_session_id(SSL *, GEN_SESSION_CB);
__owur int SSL_has_matching_session_id(const SSL *ssl, const unsigned char *id,
                                unsigned int id_len);
SSL_SESSION *d2i_SSL_SESSION(SSL_SESSION **a, const unsigned char **pp,
                             long length);

# ifdef HEADER_X509_H
__owur X509 *SSL_get_peer_certificate(const SSL *s);
# endif

__owur STACK_OF(X509) *SSL_get_peer_cert_chain(const SSL *s);

__owur int SSL_CTX_get_verify_mode(const SSL_CTX *ctx);
__owur int SSL_CTX_get_verify_depth(const SSL_CTX *ctx);
__owur SSL_verify_cb SSL_CTX_get_verify_callback(const SSL_CTX *ctx);
void SSL_CTX_set_verify(SSL_CTX *ctx, int mode, SSL_verify_cb callback);
void SSL_CTX_set_verify_depth(SSL_CTX *ctx, int depth);
void SSL_CTX_set_cert_verify_callback(SSL_CTX *ctx,
                                      int (*cb) (X509_STORE_CTX *, void *),
                                      void *arg);
void SSL_CTX_set_cert_cb(SSL_CTX *c, int (*cb) (SSL *ssl, void *arg),
                         void *arg);
# ifndef OPENSSL_NO_RSA
__owur int SSL_CTX_use_RSAPrivateKey(SSL_CTX *ctx, RSA *rsa);
__owur int SSL_CTX_use_RSAPrivateKey_ASN1(SSL_CTX *ctx, const unsigned char *d,
                                   long len);
# endif
__owur int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey);
__owur int SSL_CTX_use_PrivateKey_ASN1(int pk, SSL_CTX *ctx,
                                const unsigned char *d, long len);
__owur int SSL_CTX_use_certificate(SSL_CTX *ctx, X509 *x);
__owur int SSL_CTX_use_certificate_ASN1(SSL_CTX *ctx, int len,
                                 const unsigned char *d);

void SSL_CTX_set_default_passwd_cb(SSL_CTX *ctx, pem_password_cb *cb);
void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX *ctx, void *u);
pem_password_cb *SSL_CTX_get_default_passwd_cb(SSL_CTX *ctx);
void *SSL_CTX_get_default_passwd_cb_userdata(SSL_CTX *ctx);
void SSL_set_default_passwd_cb(SSL *s, pem_password_cb *cb);
void SSL_set_default_passwd_cb_userdata(SSL *s, void *u);
pem_password_cb *SSL_get_default_passwd_cb(SSL *s);
void *SSL_get_default_passwd_cb_userdata(SSL *s);

__owur int SSL_CTX_check_private_key(const SSL_CTX *ctx);
__owur int SSL_check_private_key(const SSL *ctx);

__owur int SSL_CTX_set_session_id_context(SSL_CTX *ctx, const unsigned char *sid_ctx,
                                   unsigned int sid_ctx_len);

SSL *SSL_new(SSL_CTX *ctx);
int SSL_up_ref(SSL *s);
int SSL_is_dtls(const SSL *s);
__owur int SSL_set_session_id_context(SSL *ssl, const unsigned char *sid_ctx,
                               unsigned int sid_ctx_len);

__owur int SSL_CTX_set_purpose(SSL_CTX *s, int purpose);
__owur int SSL_set_purpose(SSL *s, int purpose);
__owur int SSL_CTX_set_trust(SSL_CTX *s, int trust);
__owur int SSL_set_trust(SSL *s, int trust);

__owur int SSL_set1_host(SSL *s, const char *hostname);
__owur int SSL_add1_host(SSL *s, const char *hostname);
__owur const char *SSL_get0_peername(SSL *s);
void SSL_set_hostflags(SSL *s, unsigned int flags);

__owur int SSL_CTX_dane_enable(SSL_CTX *ctx);
__owur int SSL_CTX_dane_mtype_set(SSL_CTX *ctx, const EVP_MD *md,
                                  uint8_t mtype, uint8_t ord);
__owur int SSL_dane_enable(SSL *s, const char *basedomain);
__owur int SSL_dane_tlsa_add(SSL *s, uint8_t usage, uint8_t selector,
                             uint8_t mtype, unsigned const char *data, size_t dlen);
__owur int SSL_get0_dane_authority(SSL *s, X509 **mcert, EVP_PKEY **mspki);
__owur int SSL_get0_dane_tlsa(SSL *s, uint8_t *usage, uint8_t *selector,
                              uint8_t *mtype, unsigned const char **data,
                              size_t *dlen);
/*
 * Bridge opacity barrier between libcrypt and libssl, also needed to support
 * offline testing in test/danetest.c
 */
SSL_DANE *SSL_get0_dane(SSL *ssl);
/*
 * DANE flags
 */
unsigned long SSL_CTX_dane_set_flags(SSL_CTX *ctx, unsigned long flags);
unsigned long SSL_CTX_dane_clear_flags(SSL_CTX *ctx, unsigned long flags);
unsigned long SSL_dane_set_flags(SSL *ssl, unsigned long flags);
unsigned long SSL_dane_clear_flags(SSL *ssl, unsigned long flags);

__owur int SSL_CTX_set1_param(SSL_CTX *ctx, X509_VERIFY_PARAM *vpm);
__owur int SSL_set1_param(SSL *ssl, X509_VERIFY_PARAM *vpm);

__owur X509_VERIFY_PARAM *SSL_CTX_get0_param(SSL_CTX *ctx);
__owur X509_VERIFY_PARAM *SSL_get0_param(SSL *ssl);

# ifndef OPENSSL_NO_SRP
int SSL_CTX_set_srp_username(SSL_CTX *ctx, char *name);
int SSL_CTX_set_srp_password(SSL_CTX *ctx, char *password);
int SSL_CTX_set_srp_strength(SSL_CTX *ctx, int strength);
int SSL_CTX_set_srp_client_pwd_callback(SSL_CTX *ctx,
                                        char *(*cb) (SSL *, void *));
int SSL_CTX_set_srp_verify_param_callback(SSL_CTX *ctx,
                                          int (*cb) (SSL *, void *));
int SSL_CTX_set_srp_username_callback(SSL_CTX *ctx,
                                      int (*cb) (SSL *, int *, void *));
int SSL_CTX_set_srp_cb_arg(SSL_CTX *ctx, void *arg);

int SSL_set_srp_server_param(SSL *s, const BIGNUM *N, const BIGNUM *g,
                             BIGNUM *sa, BIGNUM *v, char *info);
int SSL_set_srp_server_param_pw(SSL *s, const char *user, const char *pass,
                                const char *grp);

__owur BIGNUM *SSL_get_srp_g(SSL *s);
__owur BIGNUM *SSL_get_srp_N(SSL *s);

__owur char *SSL_get_srp_username(SSL *s);
__owur char *SSL_get_srp_userinfo(SSL *s);
# endif

void SSL_certs_clear(SSL *s);
void SSL_free(SSL *ssl);
# ifdef OSSL_ASYNC_FD
/*
 * Windows application developer has to include windows.h to use these.
 */
__owur int SSL_waiting_for_async(SSL *s);
__owur int SSL_get_all_async_fds(SSL *s, OSSL_ASYNC_FD *fds, size_t *numfds);
__owur int SSL_get_changed_async_fds(SSL *s, OSSL_ASYNC_FD *addfd,
                                     size_t *numaddfds, OSSL_ASYNC_FD *delfd,
                                     size_t *numdelfds);
# endif
__owur int SSL_accept(SSL *ssl);
__owur int SSL_connect(SSL *ssl);
__owur int SSL_read(SSL *ssl, void *buf, int num);
__owur int SSL_peek(SSL *ssl, void *buf, int num);
__owur int SSL_write(SSL *ssl, const void *buf, int num);
long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg);
long SSL_callback_ctrl(SSL *, int, void (*)(void));
long SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg);
long SSL_CTX_callback_ctrl(SSL_CTX *, int, void (*)(void));

__owur int SSL_get_error(const SSL *s, int ret_code);
__owur const char *SSL_get_version(const SSL *s);

/* This sets the 'default' SSL version that SSL_new() will create */
__owur int SSL_CTX_set_ssl_version(SSL_CTX *ctx, const SSL_METHOD *meth);

# ifndef OPENSSL_NO_SSL3_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *SSLv3_method(void)) /* SSLv3 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *SSLv3_server_method(void)) /* SSLv3 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *SSLv3_client_method(void)) /* SSLv3 */
# endif

#define SSLv23_method           TLS_method
#define SSLv23_server_method    TLS_server_method
#define SSLv23_client_method    TLS_client_method

/* Negotiate highest available SSL/TLS version */
__owur const SSL_METHOD *TLS_method(void);
__owur const SSL_METHOD *TLS_server_method(void);
__owur const SSL_METHOD *TLS_client_method(void);

# ifndef OPENSSL_NO_TLS1_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_method(void)) /* TLSv1.0 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_server_method(void)) /* TLSv1.0 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_client_method(void)) /* TLSv1.0 */
# endif

# ifndef OPENSSL_NO_TLS1_1_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_1_method(void)) /* TLSv1.1 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_1_server_method(void)) /* TLSv1.1 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_1_client_method(void)) /* TLSv1.1 */
# endif

# ifndef OPENSSL_NO_TLS1_2_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_2_method(void)) /* TLSv1.2 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_2_server_method(void)) /* TLSv1.2 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *TLSv1_2_client_method(void)) /* TLSv1.2 */
# endif

# ifndef OPENSSL_NO_DTLS1_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_method(void)) /* DTLSv1.0 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_server_method(void)) /* DTLSv1.0 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_client_method(void)) /* DTLSv1.0 */
# endif

# ifndef OPENSSL_NO_DTLS1_2_METHOD
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_2_method(void)) /* DTLSv1.2 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_2_server_method(void)) /* DTLSv1.2 */
DEPRECATEDIN_1_1_0(__owur const SSL_METHOD *DTLSv1_2_client_method(void)) /* DTLSv1.2 */
#endif

__owur const SSL_METHOD *DTLS_method(void); /* DTLS 1.0 and 1.2 */
__owur const SSL_METHOD *DTLS_server_method(void); /* DTLS 1.0 and 1.2 */
__owur const SSL_METHOD *DTLS_client_method(void); /* DTLS 1.0 and 1.2 */

__owur STACK_OF(SSL_CIPHER) *SSL_get_ciphers(const SSL *s);
__owur STACK_OF(SSL_CIPHER) *SSL_CTX_get_ciphers(const SSL_CTX *ctx);
__owur STACK_OF(SSL_CIPHER) *SSL_get_client_ciphers(const SSL *s);
__owur STACK_OF(SSL_CIPHER) *SSL_get1_supported_ciphers(SSL *s);

__owur int SSL_do_handshake(SSL *s);
int SSL_renegotiate(SSL *s);
__owur int SSL_renegotiate_abbreviated(SSL *s);
__owur int SSL_renegotiate_pending(SSL *s);
int SSL_shutdown(SSL *s);

__owur const SSL_METHOD *SSL_CTX_get_ssl_method(SSL_CTX *ctx);
__owur const SSL_METHOD *SSL_get_ssl_method(SSL *s);
__owur int SSL_set_ssl_method(SSL *s, const SSL_METHOD *method);
__owur const char *SSL_alert_type_string_long(int value);
__owur const char *SSL_alert_type_string(int value);
__owur const char *SSL_alert_desc_string_long(int value);
__owur const char *SSL_alert_desc_string(int value);

void SSL_set_client_CA_list(SSL *s, STACK_OF(X509_NAME) *name_list);
void SSL_CTX_set_client_CA_list(SSL_CTX *ctx, STACK_OF(X509_NAME) *name_list);
__owur STACK_OF(X509_NAME) *SSL_get_client_CA_list(const SSL *s);
__owur STACK_OF(X509_NAME) *SSL_CTX_get_client_CA_list(const SSL_CTX *s);
__owur int SSL_add_client_CA(SSL *ssl, X509 *x);
__owur int SSL_CTX_add_client_CA(SSL_CTX *ctx, X509 *x);

void SSL_set_connect_state(SSL *s);
void SSL_set_accept_state(SSL *s);

__owur long SSL_get_default_timeout(const SSL *s);

#if OPENSSL_API_COMPAT < 0x10100000L
# define SSL_library_init() OPENSSL_init_ssl(0, NULL)
#endif

__owur char *SSL_CIPHER_description(const SSL_CIPHER *, char *buf, int size);
__owur STACK_OF(X509_NAME) *SSL_dup_CA_list(STACK_OF(X509_NAME) *sk);

__owur SSL *SSL_dup(SSL *ssl);

__owur X509 *SSL_get_certificate(const SSL *ssl);
/*
 * EVP_PKEY
 */ struct evp_pkey_st *SSL_get_privatekey(const SSL *ssl);

__owur X509 *SSL_CTX_get0_certificate(const SSL_CTX *ctx);
__owur EVP_PKEY *SSL_CTX_get0_privatekey(const SSL_CTX *ctx);

void SSL_CTX_set_quiet_shutdown(SSL_CTX *ctx, int mode);
__owur int SSL_CTX_get_quiet_shutdown(const SSL_CTX *ctx);
void SSL_set_quiet_shutdown(SSL *ssl, int mode);
__owur int SSL_get_quiet_shutdown(const SSL *ssl);
void SSL_set_shutdown(SSL *ssl, int mode);
__owur int SSL_get_shutdown(const SSL *ssl);
__owur int SSL_version(const SSL *ssl);
__owur int SSL_client_version(const SSL *s);
__owur int SSL_CTX_set_default_verify_paths(SSL_CTX *ctx);
__owur int SSL_CTX_set_default_verify_dir(SSL_CTX *ctx);
__owur int SSL_CTX_set_default_verify_file(SSL_CTX *ctx);
__owur int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
                                  const char *CApath);
# define SSL_get0_session SSL_get_session/* just peek at pointer */
__owur SSL_SESSION *SSL_get_session(const SSL *ssl);
__owur SSL_SESSION *SSL_get1_session(SSL *ssl); /* obtain a reference count */
__owur SSL_CTX *SSL_get_SSL_CTX(const SSL *ssl);
SSL_CTX *SSL_set_SSL_CTX(SSL *ssl, SSL_CTX *ctx);
void SSL_set_info_callback(SSL *ssl,
                           void (*cb) (const SSL *ssl, int type, int val));
void (*SSL_get_info_callback(const SSL *ssl)) (const SSL *ssl, int type,
                                               int val);
__owur OSSL_HANDSHAKE_STATE SSL_get_state(const SSL *ssl);

void SSL_set_verify_result(SSL *ssl, long v);
__owur long SSL_get_verify_result(const SSL *ssl);
__owur STACK_OF(X509) *SSL_get0_verified_chain(const SSL *s);

__owur size_t SSL_get_client_random(const SSL *ssl, unsigned char *out,
                                    size_t outlen);
__owur size_t SSL_get_server_random(const SSL *ssl, unsigned char *out,
                                    size_t outlen);
__owur size_t SSL_SESSION_get_master_key(const SSL_SESSION *ssl,
                                         unsigned char *out, size_t outlen);

#define SSL_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL, l, p, newf, dupf, freef)
__owur int SSL_set_ex_data(SSL *ssl, int idx, void *data);
void *SSL_get_ex_data(const SSL *ssl, int idx);
#define SSL_SESSION_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL_SESSION, l, p, newf, dupf, freef)
__owur int SSL_SESSION_set_ex_data(SSL_SESSION *ss, int idx, void *data);
void *SSL_SESSION_get_ex_data(const SSL_SESSION *ss, int idx);
#define SSL_CTX_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_SSL_CTX, l, p, newf, dupf, freef)
__owur int SSL_CTX_set_ex_data(SSL_CTX *ssl, int idx, void *data);
void *SSL_CTX_get_ex_data(const SSL_CTX *ssl, int idx);

__owur int SSL_get_ex_data_X509_STORE_CTX_idx(void);

# define SSL_CTX_sess_set_cache_size(ctx,t) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_SESS_CACHE_SIZE,t,NULL)
# define SSL_CTX_sess_get_cache_size(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_SESS_CACHE_SIZE,0,NULL)
# define SSL_CTX_set_session_cache_mode(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_SESS_CACHE_MODE,m,NULL)
# define SSL_CTX_get_session_cache_mode(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_SESS_CACHE_MODE,0,NULL)

# define SSL_CTX_get_default_read_ahead(ctx) SSL_CTX_get_read_ahead(ctx)
# define SSL_CTX_set_default_read_ahead(ctx,m) SSL_CTX_set_read_ahead(ctx,m)
# define SSL_CTX_get_read_ahead(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_READ_AHEAD,0,NULL)
# define SSL_CTX_set_read_ahead(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_READ_AHEAD,m,NULL)
# define SSL_CTX_get_max_cert_list(ctx) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_MAX_CERT_LIST,0,NULL)
# define SSL_CTX_set_max_cert_list(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_MAX_CERT_LIST,m,NULL)
# define SSL_get_max_cert_list(ssl) \
        SSL_ctrl(ssl,SSL_CTRL_GET_MAX_CERT_LIST,0,NULL)
# define SSL_set_max_cert_list(ssl,m) \
        SSL_ctrl(ssl,SSL_CTRL_SET_MAX_CERT_LIST,m,NULL)

# define SSL_CTX_set_max_send_fragment(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_MAX_SEND_FRAGMENT,m,NULL)
# define SSL_set_max_send_fragment(ssl,m) \
        SSL_ctrl(ssl,SSL_CTRL_SET_MAX_SEND_FRAGMENT,m,NULL)
# define SSL_CTX_set_split_send_fragment(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_SPLIT_SEND_FRAGMENT,m,NULL)
# define SSL_set_split_send_fragment(ssl,m) \
        SSL_ctrl(ssl,SSL_CTRL_SET_SPLIT_SEND_FRAGMENT,m,NULL)
# define SSL_CTX_set_max_pipelines(ctx,m) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_MAX_PIPELINES,m,NULL)
# define SSL_set_max_pipelines(ssl,m) \
        SSL_ctrl(ssl,SSL_CTRL_SET_MAX_PIPELINES,m,NULL)

void SSL_CTX_set_default_read_buffer_len(SSL_CTX *ctx, size_t len);
void SSL_set_default_read_buffer_len(SSL *s, size_t len);

# ifndef OPENSSL_NO_DH
/* NB: the |keylength| is only applicable when is_export is true */
void SSL_CTX_set_tmp_dh_callback(SSL_CTX *ctx,
                                 DH *(*dh) (SSL *ssl, int is_export,
                                            int keylength));
void SSL_set_tmp_dh_callback(SSL *ssl,
                             DH *(*dh) (SSL *ssl, int is_export,
                                        int keylength));
# endif

__owur const COMP_METHOD *SSL_get_current_compression(SSL *s);
__owur const COMP_METHOD *SSL_get_current_expansion(SSL *s);
__owur const char *SSL_COMP_get_name(const COMP_METHOD *comp);
__owur const char *SSL_COMP_get0_name(const SSL_COMP *comp);
__owur int SSL_COMP_get_id(const SSL_COMP *comp);
STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void);
__owur STACK_OF(SSL_COMP) *SSL_COMP_set0_compression_methods(STACK_OF(SSL_COMP)
                                                      *meths);
#if OPENSSL_API_COMPAT < 0x10100000L
# define SSL_COMP_free_compression_methods() while(0) continue
#endif
__owur int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm);

const SSL_CIPHER *SSL_CIPHER_find(SSL *ssl, const unsigned char *ptr);
int SSL_CIPHER_get_cipher_nid(const SSL_CIPHER *c);
int SSL_CIPHER_get_digest_nid(const SSL_CIPHER *c);

/* TLS extensions functions */
__owur int SSL_set_session_ticket_ext(SSL *s, void *ext_data, int ext_len);

__owur int SSL_set_session_ticket_ext_cb(SSL *s, tls_session_ticket_ext_cb_fn cb,
                                  void *arg);

/* Pre-shared secret session resumption functions */
__owur int SSL_set_session_secret_cb(SSL *s,
                              tls_session_secret_cb_fn tls_session_secret_cb,
                              void *arg);

void SSL_CTX_set_not_resumable_session_callback(SSL_CTX *ctx,
                                                int (*cb) (SSL *ssl,
                                                           int
                                                           is_forward_secure));

void SSL_set_not_resumable_session_callback(SSL *ssl,
                                            int (*cb) (SSL *ssl,
                                                       int
                                                       is_forward_secure));
# if OPENSSL_API_COMPAT < 0x10100000L
#  define SSL_cache_hit(s) SSL_session_reused(s)
# endif

__owur int SSL_session_reused(SSL *s);
__owur int SSL_is_server(const SSL *s);

__owur __owur SSL_CONF_CTX *SSL_CONF_CTX_new(void);
int SSL_CONF_CTX_finish(SSL_CONF_CTX *cctx);
void SSL_CONF_CTX_free(SSL_CONF_CTX *cctx);
unsigned int SSL_CONF_CTX_set_flags(SSL_CONF_CTX *cctx, unsigned int flags);
__owur unsigned int SSL_CONF_CTX_clear_flags(SSL_CONF_CTX *cctx, unsigned int flags);
__owur int SSL_CONF_CTX_set1_prefix(SSL_CONF_CTX *cctx, const char *pre);

void SSL_CONF_CTX_set_ssl(SSL_CONF_CTX *cctx, SSL *ssl);
void SSL_CONF_CTX_set_ssl_ctx(SSL_CONF_CTX *cctx, SSL_CTX *ctx);

__owur int SSL_CONF_cmd(SSL_CONF_CTX *cctx, const char *cmd, const char *value);
__owur int SSL_CONF_cmd_argv(SSL_CONF_CTX *cctx, int *pargc, char ***pargv);
__owur int SSL_CONF_cmd_value_type(SSL_CONF_CTX *cctx, const char *cmd);

void SSL_add_ssl_module(void);
int SSL_config(SSL *s, const char *name);
int SSL_CTX_config(SSL_CTX *ctx, const char *name);

# ifndef OPENSSL_NO_SSL_TRACE
void SSL_trace(int write_p, int version, int content_type,
               const void *buf, size_t len, SSL *ssl, void *arg);
__owur const char *SSL_CIPHER_standard_name(const SSL_CIPHER *c);
# endif

# ifndef OPENSSL_NO_SOCK
int DTLSv1_listen(SSL *s, BIO_ADDR *client);
# endif

# ifndef OPENSSL_NO_CT

/*
 * A callback for verifying that the received SCTs are sufficient.
 * Expected to return 1 if they are sufficient, otherwise 0.
 * May return a negative integer if an error occurs.
 * A connection should be aborted if the SCTs are deemed insufficient.
 */
typedef int(*ssl_ct_validation_cb)(const CT_POLICY_EVAL_CTX *ctx,
                                   const STACK_OF(SCT) *scts, void *arg);

/*
 * Sets a |callback| that is invoked upon receipt of ServerHelloDone to validate
 * the received SCTs.
 * If the callback returns a non-positive result, the connection is terminated.
 * Call this function before beginning a handshake.
 * If a NULL |callback| is provided, SCT validation is disabled.
 * |arg| is arbitrary userdata that will be passed to the callback whenever it
 * is invoked. Ownership of |arg| remains with the caller.
 *
 * NOTE: A side-effect of setting a CT callback is that an OCSP stapled response
 *       will be requested.
 */
int SSL_set_ct_validation_callback(SSL *s, ssl_ct_validation_cb callback,
                                   void *arg);
int SSL_CTX_set_ct_validation_callback(SSL_CTX *ctx,
                                       ssl_ct_validation_cb callback,
                                       void *arg);
#define SSL_disable_ct(s) \
        ((void) SSL_set_validation_callback((s), NULL, NULL))
#define SSL_CTX_disable_ct(ctx) \
        ((void) SSL_CTX_set_validation_callback((ctx), NULL, NULL))

/*
 * The validation type enumerates the available behaviours of the built-in SSL
 * CT validation callback selected via SSL_enable_ct() and SSL_CTX_enable_ct().
 * The underlying callback is a static function in libssl.
 */
enum {
    SSL_CT_VALIDATION_PERMISSIVE = 0,
    SSL_CT_VALIDATION_STRICT
};

/*
 * Enable CT by setting up a callback that implements one of the built-in
 * validation variants.  The SSL_CT_VALIDATION_PERMISSIVE variant always
 * continues the handshake, the application can make appropriate decisions at
 * handshake completion.  The SSL_CT_VALIDATION_STRICT variant requires at
 * least one valid SCT, or else handshake termination will be requested.  The
 * handshake may continue anyway if SSL_VERIFY_NONE is in effect.
 */
int SSL_enable_ct(SSL *s, int validation_mode);
int SSL_CTX_enable_ct(SSL_CTX *ctx, int validation_mode);

/*
 * Report whether a non-NULL callback is enabled.
 */
int SSL_ct_is_enabled(const SSL *s);
int SSL_CTX_ct_is_enabled(const SSL_CTX *ctx);

/* Gets the SCTs received from a connection */
const STACK_OF(SCT) *SSL_get0_peer_scts(SSL *s);

/*
 * Loads the CT log list from the default location.
 * If a CTLOG_STORE has previously been set using SSL_CTX_set_ctlog_store,
 * the log information loaded from this file will be appended to the
 * CTLOG_STORE.
 * Returns 1 on success, 0 otherwise.
 */
int SSL_CTX_set_default_ctlog_list_file(SSL_CTX *ctx);

/*
 * Loads the CT log list from the specified file path.
 * If a CTLOG_STORE has previously been set using SSL_CTX_set_ctlog_store,
 * the log information loaded from this file will be appended to the
 * CTLOG_STORE.
 * Returns 1 on success, 0 otherwise.
 */
int SSL_CTX_set_ctlog_list_file(SSL_CTX *ctx, const char *path);

/*
 * Sets the CT log list used by all SSL connections created from this SSL_CTX.
 * Ownership of the CTLOG_STORE is transferred to the SSL_CTX.
 */
void SSL_CTX_set0_ctlog_store(SSL_CTX *ctx, CTLOG_STORE *logs);

/*
 * Gets the CT log list used by all SSL connections created from this SSL_CTX.
 * This will be NULL unless one of the following functions has been called:
 * - SSL_CTX_set_default_ctlog_list_file
 * - SSL_CTX_set_ctlog_list_file
 * - SSL_CTX_set_ctlog_store
 */
const CTLOG_STORE *SSL_CTX_get0_ctlog_store(const SSL_CTX *ctx);

# endif /* OPENSSL_NO_CT */

/* What the "other" parameter contains in security callback */
/* Mask for type */
# define SSL_SECOP_OTHER_TYPE    0xffff0000
# define SSL_SECOP_OTHER_NONE    0
# define SSL_SECOP_OTHER_CIPHER  (1 << 16)
# define SSL_SECOP_OTHER_CURVE   (2 << 16)
# define SSL_SECOP_OTHER_DH      (3 << 16)
# define SSL_SECOP_OTHER_PKEY    (4 << 16)
# define SSL_SECOP_OTHER_SIGALG  (5 << 16)
# define SSL_SECOP_OTHER_CERT    (6 << 16)

/* Indicated operation refers to peer key or certificate */
# define SSL_SECOP_PEER          0x1000

/* Values for "op" parameter in security callback */

/* Called to filter ciphers */
/* Ciphers client supports */
# define SSL_SECOP_CIPHER_SUPPORTED      (1 | SSL_SECOP_OTHER_CIPHER)
/* Cipher shared by client/server */
# define SSL_SECOP_CIPHER_SHARED         (2 | SSL_SECOP_OTHER_CIPHER)
/* Sanity check of cipher server selects */
# define SSL_SECOP_CIPHER_CHECK          (3 | SSL_SECOP_OTHER_CIPHER)
/* Curves supported by client */
# define SSL_SECOP_CURVE_SUPPORTED       (4 | SSL_SECOP_OTHER_CURVE)
/* Curves shared by client/server */
# define SSL_SECOP_CURVE_SHARED          (5 | SSL_SECOP_OTHER_CURVE)
/* Sanity check of curve server selects */
# define SSL_SECOP_CURVE_CHECK           (6 | SSL_SECOP_OTHER_CURVE)
/* Temporary DH key */
# define SSL_SECOP_TMP_DH                (7 | SSL_SECOP_OTHER_PKEY)
/* SSL/TLS version */
# define SSL_SECOP_VERSION               (9 | SSL_SECOP_OTHER_NONE)
/* Session tickets */
# define SSL_SECOP_TICKET                (10 | SSL_SECOP_OTHER_NONE)
/* Supported signature algorithms sent to peer */
# define SSL_SECOP_SIGALG_SUPPORTED      (11 | SSL_SECOP_OTHER_SIGALG)
/* Shared signature algorithm */
# define SSL_SECOP_SIGALG_SHARED         (12 | SSL_SECOP_OTHER_SIGALG)
/* Sanity check signature algorithm allowed */
# define SSL_SECOP_SIGALG_CHECK          (13 | SSL_SECOP_OTHER_SIGALG)
/* Used to get mask of supported public key signature algorithms */
# define SSL_SECOP_SIGALG_MASK           (14 | SSL_SECOP_OTHER_SIGALG)
/* Use to see if compression is allowed */
# define SSL_SECOP_COMPRESSION           (15 | SSL_SECOP_OTHER_NONE)
/* EE key in certificate */
# define SSL_SECOP_EE_KEY                (16 | SSL_SECOP_OTHER_CERT)
/* CA key in certificate */
# define SSL_SECOP_CA_KEY                (17 | SSL_SECOP_OTHER_CERT)
/* CA digest algorithm in certificate */
# define SSL_SECOP_CA_MD                 (18 | SSL_SECOP_OTHER_CERT)
/* Peer EE key in certificate */
# define SSL_SECOP_PEER_EE_KEY           (SSL_SECOP_EE_KEY | SSL_SECOP_PEER)
/* Peer CA key in certificate */
# define SSL_SECOP_PEER_CA_KEY           (SSL_SECOP_CA_KEY | SSL_SECOP_PEER)
/* Peer CA digest algorithm in certificate */
# define SSL_SECOP_PEER_CA_MD            (SSL_SECOP_CA_MD | SSL_SECOP_PEER)

void SSL_set_security_level(SSL *s, int level);
__owur int SSL_get_security_level(const SSL *s);
void SSL_set_security_callback(SSL *s,
                               int (*cb) (const SSL *s, const SSL_CTX *ctx, int op,
                                          int bits, int nid, void *other,
                                          void *ex));
int (*SSL_get_security_callback(const SSL *s)) (const SSL *s, const SSL_CTX *ctx, int op,
                                                int bits, int nid,
                                                void *other, void *ex);
void SSL_set0_security_ex_data(SSL *s, void *ex);
__owur void *SSL_get0_security_ex_data(const SSL *s);

void SSL_CTX_set_security_level(SSL_CTX *ctx, int level);
__owur int SSL_CTX_get_security_level(const SSL_CTX *ctx);
void SSL_CTX_set_security_callback(SSL_CTX *ctx,
                                   int (*cb) (const SSL *s, const SSL_CTX *ctx, int op,
                                              int bits, int nid, void *other,
                                              void *ex));
int (*SSL_CTX_get_security_callback(const SSL_CTX *ctx)) (const SSL *s,
                                                          const SSL_CTX *ctx,
                                                          int op, int bits,
                                                          int nid,
                                                          void *other,
                                                          void *ex);
void SSL_CTX_set0_security_ex_data(SSL_CTX *ctx, void *ex);
__owur void *SSL_CTX_get0_security_ex_data(const SSL_CTX *ctx);

/* OPENSSL_INIT flag 0x010000 reserved for internal use */
#define OPENSSL_INIT_NO_LOAD_SSL_STRINGS    0x00100000L
#define OPENSSL_INIT_LOAD_SSL_STRINGS       0x00200000L

#define OPENSSL_INIT_SSL_DEFAULT \
        (OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS)

int OPENSSL_init_ssl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings);

# ifndef OPENSSL_NO_UNIT_TEST
__owur const struct openssl_ssl_test_functions *SSL_test_functions(void);
# endif

extern const char SSL_version_str[];

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

int ERR_load_SSL_strings(void);

/* Error codes for the SSL functions. */

/* Function codes. */
# define SSL_F_CHECK_SUITEB_CIPHER_LIST                   331
# define SSL_F_CT_MOVE_SCTS                               345
# define SSL_F_CT_STRICT                                  349
# define SSL_F_D2I_SSL_SESSION                            103
# define SSL_F_DANE_CTX_ENABLE                            347
# define SSL_F_DANE_MTYPE_SET                             393
# define SSL_F_DANE_TLSA_ADD                              394
# define SSL_F_DO_DTLS1_WRITE                             245
# define SSL_F_DO_SSL3_WRITE                              104
# define SSL_F_DTLS1_BUFFER_RECORD                        247
# define SSL_F_DTLS1_CHECK_TIMEOUT_NUM                    318
# define SSL_F_DTLS1_HEARTBEAT                            305
# define SSL_F_DTLS1_PREPROCESS_FRAGMENT                  288
# define SSL_F_DTLS1_PROCESS_BUFFERED_RECORDS             424
# define SSL_F_DTLS1_PROCESS_RECORD                       257
# define SSL_F_DTLS1_READ_BYTES                           258
# define SSL_F_DTLS1_READ_FAILED                          339
# define SSL_F_DTLS1_RETRANSMIT_MESSAGE                   390
# define SSL_F_DTLS1_WRITE_APP_DATA_BYTES                 268
# define SSL_F_DTLSV1_LISTEN                              350
# define SSL_F_DTLS_CONSTRUCT_CHANGE_CIPHER_SPEC          371
# define SSL_F_DTLS_CONSTRUCT_HELLO_VERIFY_REQUEST        385
# define SSL_F_DTLS_GET_REASSEMBLED_MESSAGE               370
# define SSL_F_DTLS_PROCESS_HELLO_VERIFY                  386
# define SSL_F_DTLS_WAIT_FOR_DRY                          592
# define SSL_F_OPENSSL_INIT_SSL                           342
# define SSL_F_OSSL_STATEM_CLIENT_READ_TRANSITION         417
# define SSL_F_OSSL_STATEM_SERVER_READ_TRANSITION         418
# define SSL_F_READ_STATE_MACHINE                         352
# define SSL_F_SSL3_CHANGE_CIPHER_STATE                   129
# define SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM              130
# define SSL_F_SSL3_CTRL                                  213
# define SSL_F_SSL3_CTX_CTRL                              133
# define SSL_F_SSL3_DIGEST_CACHED_RECORDS                 293
# define SSL_F_SSL3_DO_CHANGE_CIPHER_SPEC                 292
# define SSL_F_SSL3_FINAL_FINISH_MAC                      285
# define SSL_F_SSL3_GENERATE_KEY_BLOCK                    238
# define SSL_F_SSL3_GENERATE_MASTER_SECRET                388
# define SSL_F_SSL3_GET_RECORD                            143
# define SSL_F_SSL3_INIT_FINISHED_MAC                     397
# define SSL_F_SSL3_OUTPUT_CERT_CHAIN                     147
# define SSL_F_SSL3_READ_BYTES                            148
# define SSL_F_SSL3_READ_N                                149
# define SSL_F_SSL3_SETUP_KEY_BLOCK                       157
# define SSL_F_SSL3_SETUP_READ_BUFFER                     156
# define SSL_F_SSL3_SETUP_WRITE_BUFFER                    291
# define SSL_F_SSL3_TAKE_MAC                              425
# define SSL_F_SSL3_WRITE_BYTES                           158
# define SSL_F_SSL3_WRITE_PENDING                         159
# define SSL_F_SSL_ADD_CERT_CHAIN                         316
# define SSL_F_SSL_ADD_CERT_TO_BUF                        319
# define SSL_F_SSL_ADD_CLIENTHELLO_RENEGOTIATE_EXT        298
# define SSL_F_SSL_ADD_CLIENTHELLO_TLSEXT                 277
# define SSL_F_SSL_ADD_CLIENTHELLO_USE_SRTP_EXT           307
# define SSL_F_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK         215
# define SSL_F_SSL_ADD_FILE_CERT_SUBJECTS_TO_STACK        216
# define SSL_F_SSL_ADD_SERVERHELLO_RENEGOTIATE_EXT        299
# define SSL_F_SSL_ADD_SERVERHELLO_TLSEXT                 278
# define SSL_F_SSL_ADD_SERVERHELLO_USE_SRTP_EXT           308
# define SSL_F_SSL_BAD_METHOD                             160
# define SSL_F_SSL_BUILD_CERT_CHAIN                       332
# define SSL_F_SSL_BYTES_TO_CIPHER_LIST                   161
# define SSL_F_SSL_CERT_ADD0_CHAIN_CERT                   346
# define SSL_F_SSL_CERT_DUP                               221
# define SSL_F_SSL_CERT_NEW                               162
# define SSL_F_SSL_CERT_SET0_CHAIN                        340
# define SSL_F_SSL_CHECK_PRIVATE_KEY                      163
# define SSL_F_SSL_CHECK_SERVERHELLO_TLSEXT               280
# define SSL_F_SSL_CHECK_SRVR_ECC_CERT_AND_ALG            279
# define SSL_F_SSL_CIPHER_PROCESS_RULESTR                 230
# define SSL_F_SSL_CIPHER_STRENGTH_SORT                   231
# define SSL_F_SSL_CLEAR                                  164
# define SSL_F_SSL_COMP_ADD_COMPRESSION_METHOD            165
# define SSL_F_SSL_CONF_CMD                               334
# define SSL_F_SSL_CREATE_CIPHER_LIST                     166
# define SSL_F_SSL_CTRL                                   232
# define SSL_F_SSL_CTX_CHECK_PRIVATE_KEY                  168
# define SSL_F_SSL_CTX_ENABLE_CT                          398
# define SSL_F_SSL_CTX_MAKE_PROFILES                      309
# define SSL_F_SSL_CTX_NEW                                169
# define SSL_F_SSL_CTX_SET_ALPN_PROTOS                    343
# define SSL_F_SSL_CTX_SET_CIPHER_LIST                    269
# define SSL_F_SSL_CTX_SET_CLIENT_CERT_ENGINE             290
# define SSL_F_SSL_CTX_SET_CT_VALIDATION_CALLBACK         396
# define SSL_F_SSL_CTX_SET_SESSION_ID_CONTEXT             219
# define SSL_F_SSL_CTX_SET_SSL_VERSION                    170
# define SSL_F_SSL_CTX_USE_CERTIFICATE                    171
# define SSL_F_SSL_CTX_USE_CERTIFICATE_ASN1               172
# define SSL_F_SSL_CTX_USE_CERTIFICATE_FILE               173
# define SSL_F_SSL_CTX_USE_PRIVATEKEY                     174
# define SSL_F_SSL_CTX_USE_PRIVATEKEY_ASN1                175
# define SSL_F_SSL_CTX_USE_PRIVATEKEY_FILE                176
# define SSL_F_SSL_CTX_USE_PSK_IDENTITY_HINT              272
# define SSL_F_SSL_CTX_USE_RSAPRIVATEKEY                  177
# define SSL_F_SSL_CTX_USE_RSAPRIVATEKEY_ASN1             178
# define SSL_F_SSL_CTX_USE_RSAPRIVATEKEY_FILE             179
# define SSL_F_SSL_CTX_USE_SERVERINFO                     336
# define SSL_F_SSL_CTX_USE_SERVERINFO_FILE                337
# define SSL_F_SSL_DANE_DUP                               403
# define SSL_F_SSL_DANE_ENABLE                            395
# define SSL_F_SSL_DO_CONFIG                              391
# define SSL_F_SSL_DO_HANDSHAKE                           180
# define SSL_F_SSL_DUP_CA_LIST                            408
# define SSL_F_SSL_ENABLE_CT                              402
# define SSL_F_SSL_GET_NEW_SESSION                        181
# define SSL_F_SSL_GET_PREV_SESSION                       217
# define SSL_F_SSL_GET_SERVER_CERT_INDEX                  322
# define SSL_F_SSL_GET_SIGN_PKEY                          183
# define SSL_F_SSL_INIT_WBIO_BUFFER                       184
# define SSL_F_SSL_LOAD_CLIENT_CA_FILE                    185
# define SSL_F_SSL_MODULE_INIT                            392
# define SSL_F_SSL_NEW                                    186
# define SSL_F_SSL_PARSE_CLIENTHELLO_RENEGOTIATE_EXT      300
# define SSL_F_SSL_PARSE_CLIENTHELLO_TLSEXT               302
# define SSL_F_SSL_PARSE_CLIENTHELLO_USE_SRTP_EXT         310
# define SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT      301
# define SSL_F_SSL_PARSE_SERVERHELLO_TLSEXT               303
# define SSL_F_SSL_PARSE_SERVERHELLO_USE_SRTP_EXT         311
# define SSL_F_SSL_PEEK                                   270
# define SSL_F_SSL_READ                                   223
# define SSL_F_SSL_RENEGOTIATE                            516
# define SSL_F_SSL_RENEGOTIATE_ABBREVIATED                546
# define SSL_F_SSL_SCAN_CLIENTHELLO_TLSEXT                320
# define SSL_F_SSL_SCAN_SERVERHELLO_TLSEXT                321
# define SSL_F_SSL_SESSION_DUP                            348
# define SSL_F_SSL_SESSION_NEW                            189
# define SSL_F_SSL_SESSION_PRINT_FP                       190
# define SSL_F_SSL_SESSION_SET1_ID                        423
# define SSL_F_SSL_SESSION_SET1_ID_CONTEXT                312
# define SSL_F_SSL_SET_ALPN_PROTOS                        344
# define SSL_F_SSL_SET_CERT                               191
# define SSL_F_SSL_SET_CIPHER_LIST                        271
# define SSL_F_SSL_SET_CT_VALIDATION_CALLBACK             399
# define SSL_F_SSL_SET_FD                                 192
# define SSL_F_SSL_SET_PKEY                               193
# define SSL_F_SSL_SET_RFD                                194
# define SSL_F_SSL_SET_SESSION                            195
# define SSL_F_SSL_SET_SESSION_ID_CONTEXT                 218
# define SSL_F_SSL_SET_SESSION_TICKET_EXT                 294
# define SSL_F_SSL_SET_WFD                                196
# define SSL_F_SSL_SHUTDOWN                               224
# define SSL_F_SSL_SRP_CTX_INIT                           313
# define SSL_F_SSL_START_ASYNC_JOB                        389
# define SSL_F_SSL_UNDEFINED_FUNCTION                     197
# define SSL_F_SSL_UNDEFINED_VOID_FUNCTION                244
# define SSL_F_SSL_USE_CERTIFICATE                        198
# define SSL_F_SSL_USE_CERTIFICATE_ASN1                   199
# define SSL_F_SSL_USE_CERTIFICATE_FILE                   200
# define SSL_F_SSL_USE_PRIVATEKEY                         201
# define SSL_F_SSL_USE_PRIVATEKEY_ASN1                    202
# define SSL_F_SSL_USE_PRIVATEKEY_FILE                    203
# define SSL_F_SSL_USE_PSK_IDENTITY_HINT                  273
# define SSL_F_SSL_USE_RSAPRIVATEKEY                      204
# define SSL_F_SSL_USE_RSAPRIVATEKEY_ASN1                 205
# define SSL_F_SSL_USE_RSAPRIVATEKEY_FILE                 206
# define SSL_F_SSL_VALIDATE_CT                            400
# define SSL_F_SSL_VERIFY_CERT_CHAIN                      207
# define SSL_F_SSL_WRITE                                  208
# define SSL_F_STATE_MACHINE                              353
# define SSL_F_TLS12_CHECK_PEER_SIGALG                    333
# define SSL_F_TLS1_CHANGE_CIPHER_STATE                   209
# define SSL_F_TLS1_CHECK_DUPLICATE_EXTENSIONS            341
# define SSL_F_TLS1_ENC                                   401
# define SSL_F_TLS1_EXPORT_KEYING_MATERIAL                314
# define SSL_F_TLS1_GET_CURVELIST                         338
# define SSL_F_TLS1_PRF                                   284
# define SSL_F_TLS1_SETUP_KEY_BLOCK                       211
# define SSL_F_TLS1_SET_SERVER_SIGALGS                    335
# define SSL_F_TLS_CLIENT_KEY_EXCHANGE_POST_WORK          354
# define SSL_F_TLS_CONSTRUCT_CERTIFICATE_REQUEST          372
# define SSL_F_TLS_CONSTRUCT_CKE_DHE                      404
# define SSL_F_TLS_CONSTRUCT_CKE_ECDHE                    405
# define SSL_F_TLS_CONSTRUCT_CKE_GOST                     406
# define SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE             407
# define SSL_F_TLS_CONSTRUCT_CKE_RSA                      409
# define SSL_F_TLS_CONSTRUCT_CKE_SRP                      410
# define SSL_F_TLS_CONSTRUCT_CLIENT_CERTIFICATE           355
# define SSL_F_TLS_CONSTRUCT_CLIENT_HELLO                 356
# define SSL_F_TLS_CONSTRUCT_CLIENT_KEY_EXCHANGE          357
# define SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY                358
# define SSL_F_TLS_CONSTRUCT_FINISHED                     359
# define SSL_F_TLS_CONSTRUCT_HELLO_REQUEST                373
# define SSL_F_TLS_CONSTRUCT_NEW_SESSION_TICKET           428
# define SSL_F_TLS_CONSTRUCT_SERVER_CERTIFICATE           374
# define SSL_F_TLS_CONSTRUCT_SERVER_DONE                  375
# define SSL_F_TLS_CONSTRUCT_SERVER_HELLO                 376
# define SSL_F_TLS_CONSTRUCT_SERVER_KEY_EXCHANGE          377
# define SSL_F_TLS_GET_MESSAGE_BODY                       351
# define SSL_F_TLS_GET_MESSAGE_HEADER                     387
# define SSL_F_TLS_POST_PROCESS_CLIENT_HELLO              378
# define SSL_F_TLS_POST_PROCESS_CLIENT_KEY_EXCHANGE       384
# define SSL_F_TLS_PREPARE_CLIENT_CERTIFICATE             360
# define SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST            361
# define SSL_F_TLS_PROCESS_CERT_STATUS                    362
# define SSL_F_TLS_PROCESS_CERT_VERIFY                    379
# define SSL_F_TLS_PROCESS_CHANGE_CIPHER_SPEC             363
# define SSL_F_TLS_PROCESS_CKE_DHE                        411
# define SSL_F_TLS_PROCESS_CKE_ECDHE                      412
# define SSL_F_TLS_PROCESS_CKE_GOST                       413
# define SSL_F_TLS_PROCESS_CKE_PSK_PREAMBLE               414
# define SSL_F_TLS_PROCESS_CKE_RSA                        415
# define SSL_F_TLS_PROCESS_CKE_SRP                        416
# define SSL_F_TLS_PROCESS_CLIENT_CERTIFICATE             380
# define SSL_F_TLS_PROCESS_CLIENT_HELLO                   381
# define SSL_F_TLS_PROCESS_CLIENT_KEY_EXCHANGE            382
# define SSL_F_TLS_PROCESS_FINISHED                       364
# define SSL_F_TLS_PROCESS_KEY_EXCHANGE                   365
# define SSL_F_TLS_PROCESS_NEW_SESSION_TICKET             366
# define SSL_F_TLS_PROCESS_NEXT_PROTO                     383
# define SSL_F_TLS_PROCESS_SERVER_CERTIFICATE             367
# define SSL_F_TLS_PROCESS_SERVER_DONE                    368
# define SSL_F_TLS_PROCESS_SERVER_HELLO                   369
# define SSL_F_TLS_PROCESS_SKE_DHE                        419
# define SSL_F_TLS_PROCESS_SKE_ECDHE                      420
# define SSL_F_TLS_PROCESS_SKE_PSK_PREAMBLE               421
# define SSL_F_TLS_PROCESS_SKE_SRP                        422
# define SSL_F_USE_CERTIFICATE_CHAIN_FILE                 220

/* Reason codes. */
# define SSL_R_APP_DATA_IN_HANDSHAKE                      100
# define SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT 272
# define SSL_R_AT_LEAST_TLS_1_0_NEEDED_IN_FIPS_MODE       143
# define SSL_R_AT_LEAST_TLS_1_2_NEEDED_IN_SUITEB_MODE     158
# define SSL_R_BAD_CHANGE_CIPHER_SPEC                     103
# define SSL_R_BAD_DATA                                   390
# define SSL_R_BAD_DATA_RETURNED_BY_CALLBACK              106
# define SSL_R_BAD_DECOMPRESSION                          107
# define SSL_R_BAD_DH_VALUE                               102
# define SSL_R_BAD_DIGEST_LENGTH                          111
# define SSL_R_BAD_ECC_CERT                               304
# define SSL_R_BAD_ECPOINT                                306
# define SSL_R_BAD_HANDSHAKE_LENGTH                       332
# define SSL_R_BAD_HELLO_REQUEST                          105
# define SSL_R_BAD_LENGTH                                 271
# define SSL_R_BAD_PACKET_LENGTH                          115
# define SSL_R_BAD_PROTOCOL_VERSION_NUMBER                116
# define SSL_R_BAD_RSA_ENCRYPT                            119
# define SSL_R_BAD_SIGNATURE                              123
# define SSL_R_BAD_SRP_A_LENGTH                           347
# define SSL_R_BAD_SRP_PARAMETERS                         371
# define SSL_R_BAD_SRTP_MKI_VALUE                         352
# define SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST           353
# define SSL_R_BAD_SSL_FILETYPE                           124
# define SSL_R_BAD_VALUE                                  384
# define SSL_R_BAD_WRITE_RETRY                            127
# define SSL_R_BIO_NOT_SET                                128
# define SSL_R_BLOCK_CIPHER_PAD_IS_WRONG                  129
# define SSL_R_BN_LIB                                     130
# define SSL_R_CA_DN_LENGTH_MISMATCH                      131
# define SSL_R_CA_KEY_TOO_SMALL                           397
# define SSL_R_CA_MD_TOO_WEAK                             398
# define SSL_R_CCS_RECEIVED_EARLY                         133
# define SSL_R_CERTIFICATE_VERIFY_FAILED                  134
# define SSL_R_CERT_CB_ERROR                              377
# define SSL_R_CERT_LENGTH_MISMATCH                       135
# define SSL_R_CIPHER_CODE_WRONG_LENGTH                   137
# define SSL_R_CIPHER_OR_HASH_UNAVAILABLE                 138
# define SSL_R_CLIENTHELLO_TLSEXT                         226
# define SSL_R_COMPRESSED_LENGTH_TOO_LONG                 140
# define SSL_R_COMPRESSION_DISABLED                       343
# define SSL_R_COMPRESSION_FAILURE                        141
# define SSL_R_COMPRESSION_ID_NOT_WITHIN_PRIVATE_RANGE    307
# define SSL_R_COMPRESSION_LIBRARY_ERROR                  142
# define SSL_R_CONNECTION_TYPE_NOT_SET                    144
# define SSL_R_CONTEXT_NOT_DANE_ENABLED                   167
# define SSL_R_COOKIE_GEN_CALLBACK_FAILURE                400
# define SSL_R_COOKIE_MISMATCH                            308
# define SSL_R_CUSTOM_EXT_HANDLER_ALREADY_INSTALLED       206
# define SSL_R_DANE_ALREADY_ENABLED                       172
# define SSL_R_DANE_CANNOT_OVERRIDE_MTYPE_FULL            173
# define SSL_R_DANE_NOT_ENABLED                           175
# define SSL_R_DANE_TLSA_BAD_CERTIFICATE                  180
# define SSL_R_DANE_TLSA_BAD_CERTIFICATE_USAGE            184
# define SSL_R_DANE_TLSA_BAD_DATA_LENGTH                  189
# define SSL_R_DANE_TLSA_BAD_DIGEST_LENGTH                192
# define SSL_R_DANE_TLSA_BAD_MATCHING_TYPE                200
# define SSL_R_DANE_TLSA_BAD_PUBLIC_KEY                   201
# define SSL_R_DANE_TLSA_BAD_SELECTOR                     202
# define SSL_R_DANE_TLSA_NULL_DATA                        203
# define SSL_R_DATA_BETWEEN_CCS_AND_FINISHED              145
# define SSL_R_DATA_LENGTH_TOO_LONG                       146
# define SSL_R_DECRYPTION_FAILED                          147
# define SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC        281
# define SSL_R_DH_KEY_TOO_SMALL                           394
# define SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG            148
# define SSL_R_DIGEST_CHECK_FAILED                        149
# define SSL_R_DTLS_MESSAGE_TOO_BIG                       334
# define SSL_R_DUPLICATE_COMPRESSION_ID                   309
# define SSL_R_ECC_CERT_NOT_FOR_SIGNING                   318
# define SSL_R_ECDH_REQUIRED_FOR_SUITEB_MODE              374
# define SSL_R_EE_KEY_TOO_SMALL                           399
# define SSL_R_EMPTY_SRTP_PROTECTION_PROFILE_LIST         354
# define SSL_R_ENCRYPTED_LENGTH_TOO_LONG                  150
# define SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST              151
# define SSL_R_ERROR_SETTING_TLSA_BASE_DOMAIN             204
# define SSL_R_EXCEEDS_MAX_FRAGMENT_SIZE                  194
# define SSL_R_EXCESSIVE_MESSAGE_SIZE                     152
# define SSL_R_EXTRA_DATA_IN_MESSAGE                      153
# define SSL_R_FAILED_TO_INIT_ASYNC                       405
# define SSL_R_FRAGMENTED_CLIENT_HELLO                    401
# define SSL_R_GOT_A_FIN_BEFORE_A_CCS                     154
# define SSL_R_HTTPS_PROXY_REQUEST                        155
# define SSL_R_HTTP_REQUEST                               156
# define SSL_R_ILLEGAL_SUITEB_DIGEST                      380
# define SSL_R_INAPPROPRIATE_FALLBACK                     373
# define SSL_R_INCONSISTENT_COMPRESSION                   340
# define SSL_R_INCONSISTENT_EXTMS                         104
# define SSL_R_INVALID_COMMAND                            280
# define SSL_R_INVALID_COMPRESSION_ALGORITHM              341
# define SSL_R_INVALID_CONFIGURATION_NAME                 113
# define SSL_R_INVALID_CT_VALIDATION_TYPE                 212
# define SSL_R_INVALID_NULL_CMD_NAME                      385
# define SSL_R_INVALID_SEQUENCE_NUMBER                    402
# define SSL_R_INVALID_SERVERINFO_DATA                    388
# define SSL_R_INVALID_SRP_USERNAME                       357
# define SSL_R_INVALID_STATUS_RESPONSE                    328
# define SSL_R_INVALID_TICKET_KEYS_LENGTH                 325
# define SSL_R_LENGTH_MISMATCH                            159
# define SSL_R_LENGTH_TOO_LONG                            404
# define SSL_R_LENGTH_TOO_SHORT                           160
# define SSL_R_LIBRARY_BUG                                274
# define SSL_R_LIBRARY_HAS_NO_CIPHERS                     161
# define SSL_R_MISSING_DSA_SIGNING_CERT                   165
# define SSL_R_MISSING_ECDSA_SIGNING_CERT                 381
# define SSL_R_MISSING_RSA_CERTIFICATE                    168
# define SSL_R_MISSING_RSA_ENCRYPTING_CERT                169
# define SSL_R_MISSING_RSA_SIGNING_CERT                   170
# define SSL_R_MISSING_SRP_PARAM                          358
# define SSL_R_MISSING_TMP_DH_KEY                         171
# define SSL_R_MISSING_TMP_ECDH_KEY                       311
# define SSL_R_NO_CERTIFICATES_RETURNED                   176
# define SSL_R_NO_CERTIFICATE_ASSIGNED                    177
# define SSL_R_NO_CERTIFICATE_SET                         179
# define SSL_R_NO_CIPHERS_AVAILABLE                       181
# define SSL_R_NO_CIPHERS_SPECIFIED                       183
# define SSL_R_NO_CIPHER_MATCH                            185
# define SSL_R_NO_CLIENT_CERT_METHOD                      331
# define SSL_R_NO_COMPRESSION_SPECIFIED                   187
# define SSL_R_NO_GOST_CERTIFICATE_SENT_BY_PEER           330
# define SSL_R_NO_METHOD_SPECIFIED                        188
# define SSL_R_NO_PEM_EXTENSIONS                          389
# define SSL_R_NO_PRIVATE_KEY_ASSIGNED                    190
# define SSL_R_NO_PROTOCOLS_AVAILABLE                     191
# define SSL_R_NO_RENEGOTIATION                           339
# define SSL_R_NO_REQUIRED_DIGEST                         324
# define SSL_R_NO_SHARED_CIPHER                           193
# define SSL_R_NO_SHARED_SIGNATURE_ALGORITHMS             376
# define SSL_R_NO_SRTP_PROFILES                           359
# define SSL_R_NO_VALID_SCTS                              216
# define SSL_R_NO_VERIFY_COOKIE_CALLBACK                  403
# define SSL_R_NULL_SSL_CTX                               195
# define SSL_R_NULL_SSL_METHOD_PASSED                     196
# define SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED            197
# define SSL_R_OLD_SESSION_COMPRESSION_ALGORITHM_NOT_RETURNED 344
# define SSL_R_PACKET_LENGTH_TOO_LONG                     198
# define SSL_R_PARSE_TLSEXT                               227
# define SSL_R_PATH_TOO_LONG                              270
# define SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE          199
# define SSL_R_PEM_NAME_BAD_PREFIX                        391
# define SSL_R_PEM_NAME_TOO_SHORT                         392
# define SSL_R_PIPELINE_FAILURE                           406
# define SSL_R_PROTOCOL_IS_SHUTDOWN                       207
# define SSL_R_PSK_IDENTITY_NOT_FOUND                     223
# define SSL_R_PSK_NO_CLIENT_CB                           224
# define SSL_R_PSK_NO_SERVER_CB                           225
# define SSL_R_READ_BIO_NOT_SET                           211
# define SSL_R_READ_TIMEOUT_EXPIRED                       312
# define SSL_R_RECORD_LENGTH_MISMATCH                     213
# define SSL_R_RECORD_TOO_SMALL                           298
# define SSL_R_RENEGOTIATE_EXT_TOO_LONG                   335
# define SSL_R_RENEGOTIATION_ENCODING_ERR                 336
# define SSL_R_RENEGOTIATION_MISMATCH                     337
# define SSL_R_REQUIRED_CIPHER_MISSING                    215
# define SSL_R_REQUIRED_COMPRESSION_ALGORITHM_MISSING     342
# define SSL_R_SCSV_RECEIVED_WHEN_RENEGOTIATING           345
# define SSL_R_SCT_VERIFICATION_FAILED                    208
# define SSL_R_SERVERHELLO_TLSEXT                         275
# define SSL_R_SESSION_ID_CONTEXT_UNINITIALIZED           277
# define SSL_R_SHUTDOWN_WHILE_IN_INIT                     407
# define SSL_R_SIGNATURE_ALGORITHMS_ERROR                 360
# define SSL_R_SIGNATURE_FOR_NON_SIGNING_CERTIFICATE      220
# define SSL_R_SRP_A_CALC                                 361
# define SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES           362
# define SSL_R_SRTP_PROTECTION_PROFILE_LIST_TOO_LONG      363
# define SSL_R_SRTP_UNKNOWN_PROTECTION_PROFILE            364
# define SSL_R_SSL3_EXT_INVALID_SERVERNAME                319
# define SSL_R_SSL3_EXT_INVALID_SERVERNAME_TYPE           320
# define SSL_R_SSL3_SESSION_ID_TOO_LONG                   300
# define SSL_R_SSLV3_ALERT_BAD_CERTIFICATE                1042
# define SSL_R_SSLV3_ALERT_BAD_RECORD_MAC                 1020
# define SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED            1045
# define SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED            1044
# define SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN            1046
# define SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE          1030
# define SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE              1040
# define SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER              1047
# define SSL_R_SSLV3_ALERT_NO_CERTIFICATE                 1041
# define SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE             1010
# define SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE        1043
# define SSL_R_SSL_COMMAND_SECTION_EMPTY                  117
# define SSL_R_SSL_COMMAND_SECTION_NOT_FOUND              125
# define SSL_R_SSL_CTX_HAS_NO_DEFAULT_SSL_VERSION         228
# define SSL_R_SSL_HANDSHAKE_FAILURE                      229
# define SSL_R_SSL_LIBRARY_HAS_NO_CIPHERS                 230
# define SSL_R_SSL_NEGATIVE_LENGTH                        372
# define SSL_R_SSL_SECTION_EMPTY                          126
# define SSL_R_SSL_SECTION_NOT_FOUND                      136
# define SSL_R_SSL_SESSION_ID_CALLBACK_FAILED             301
# define SSL_R_SSL_SESSION_ID_CONFLICT                    302
# define SSL_R_SSL_SESSION_ID_TOO_LONG                    408
# define SSL_R_SSL_SESSION_ID_CONTEXT_TOO_LONG            273
# define SSL_R_SSL_SESSION_ID_HAS_BAD_LENGTH              303
# define SSL_R_SSL_SESSION_VERSION_MISMATCH               210
# define SSL_R_TLSV1_ALERT_ACCESS_DENIED                  1049
# define SSL_R_TLSV1_ALERT_DECODE_ERROR                   1050
# define SSL_R_TLSV1_ALERT_DECRYPTION_FAILED              1021
# define SSL_R_TLSV1_ALERT_DECRYPT_ERROR                  1051
# define SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION             1060
# define SSL_R_TLSV1_ALERT_INAPPROPRIATE_FALLBACK         1086
# define SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY          1071
# define SSL_R_TLSV1_ALERT_INTERNAL_ERROR                 1080
# define SSL_R_TLSV1_ALERT_NO_RENEGOTIATION               1100
# define SSL_R_TLSV1_ALERT_PROTOCOL_VERSION               1070
# define SSL_R_TLSV1_ALERT_RECORD_OVERFLOW                1022
# define SSL_R_TLSV1_ALERT_UNKNOWN_CA                     1048
# define SSL_R_TLSV1_ALERT_USER_CANCELLED                 1090
# define SSL_R_TLSV1_BAD_CERTIFICATE_HASH_VALUE           1114
# define SSL_R_TLSV1_BAD_CERTIFICATE_STATUS_RESPONSE      1113
# define SSL_R_TLSV1_CERTIFICATE_UNOBTAINABLE             1111
# define SSL_R_TLSV1_UNRECOGNIZED_NAME                    1112
# define SSL_R_TLSV1_UNSUPPORTED_EXTENSION                1110
# define SSL_R_TLS_HEARTBEAT_PEER_DOESNT_ACCEPT           365
# define SSL_R_TLS_HEARTBEAT_PENDING                      366
# define SSL_R_TLS_ILLEGAL_EXPORTER_LABEL                 367
# define SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST             157
# define SSL_R_TOO_MANY_WARN_ALERTS                       409
# define SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS             314
# define SSL_R_UNABLE_TO_FIND_PUBLIC_KEY_PARAMETERS       239
# define SSL_R_UNABLE_TO_LOAD_SSL3_MD5_ROUTINES           242
# define SSL_R_UNABLE_TO_LOAD_SSL3_SHA1_ROUTINES          243
# define SSL_R_UNEXPECTED_MESSAGE                         244
# define SSL_R_UNEXPECTED_RECORD                          245
# define SSL_R_UNINITIALIZED                              276
# define SSL_R_UNKNOWN_ALERT_TYPE                         246
# define SSL_R_UNKNOWN_CERTIFICATE_TYPE                   247
# define SSL_R_UNKNOWN_CIPHER_RETURNED                    248
# define SSL_R_UNKNOWN_CIPHER_TYPE                        249
# define SSL_R_UNKNOWN_CMD_NAME                           386
# define SSL_R_UNKNOWN_COMMAND                            139
# define SSL_R_UNKNOWN_DIGEST                             368
# define SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE                  250
# define SSL_R_UNKNOWN_PKEY_TYPE                          251
# define SSL_R_UNKNOWN_PROTOCOL                           252
# define SSL_R_UNKNOWN_SSL_VERSION                        254
# define SSL_R_UNKNOWN_STATE                              255
# define SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED       338
# define SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM          257
# define SSL_R_UNSUPPORTED_ELLIPTIC_CURVE                 315
# define SSL_R_UNSUPPORTED_PROTOCOL                       258
# define SSL_R_UNSUPPORTED_SSL_VERSION                    259
# define SSL_R_UNSUPPORTED_STATUS_TYPE                    329
# define SSL_R_USE_SRTP_NOT_NEGOTIATED                    369
# define SSL_R_VERSION_TOO_HIGH                           166
# define SSL_R_VERSION_TOO_LOW                            396
# define SSL_R_WRONG_CERTIFICATE_TYPE                     383
# define SSL_R_WRONG_CIPHER_RETURNED                      261
# define SSL_R_WRONG_CURVE                                378
# define SSL_R_WRONG_SIGNATURE_LENGTH                     264
# define SSL_R_WRONG_SIGNATURE_SIZE                       265
# define SSL_R_WRONG_SIGNATURE_TYPE                       370
# define SSL_R_WRONG_SSL_VERSION                          266
# define SSL_R_WRONG_VERSION_NUMBER                       267
# define SSL_R_X509_LIB                                   268
# define SSL_R_X509_VERIFICATION_SETUP_PROBLEMS           269

# ifdef  __cplusplus
}
# endif
#endif
