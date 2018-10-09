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

#ifndef HEADER_SSL3_H
# define HEADER_SSL3_H

# include <openssl/comp.h>
# include <openssl/buffer.h>
# include <openssl/evp.h>
# include <openssl/ssl.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Signalling cipher suite value from RFC 5746
 * (TLS_EMPTY_RENEGOTIATION_INFO_SCSV)
 */
# define SSL3_CK_SCSV                            0x030000FF

/*
 * Signalling cipher suite value from draft-ietf-tls-downgrade-scsv-00
 * (TLS_FALLBACK_SCSV)
 */
# define SSL3_CK_FALLBACK_SCSV                   0x03005600

# define SSL3_CK_RSA_NULL_MD5                    0x03000001
# define SSL3_CK_RSA_NULL_SHA                    0x03000002
# define SSL3_CK_RSA_RC4_40_MD5                  0x03000003
# define SSL3_CK_RSA_RC4_128_MD5                 0x03000004
# define SSL3_CK_RSA_RC4_128_SHA                 0x03000005
# define SSL3_CK_RSA_RC2_40_MD5                  0x03000006
# define SSL3_CK_RSA_IDEA_128_SHA                0x03000007
# define SSL3_CK_RSA_DES_40_CBC_SHA              0x03000008
# define SSL3_CK_RSA_DES_64_CBC_SHA              0x03000009
# define SSL3_CK_RSA_DES_192_CBC3_SHA            0x0300000A

# define SSL3_CK_DH_DSS_DES_40_CBC_SHA           0x0300000B
# define SSL3_CK_DH_DSS_DES_64_CBC_SHA           0x0300000C
# define SSL3_CK_DH_DSS_DES_192_CBC3_SHA         0x0300000D
# define SSL3_CK_DH_RSA_DES_40_CBC_SHA           0x0300000E
# define SSL3_CK_DH_RSA_DES_64_CBC_SHA           0x0300000F
# define SSL3_CK_DH_RSA_DES_192_CBC3_SHA         0x03000010

# define SSL3_CK_DHE_DSS_DES_40_CBC_SHA          0x03000011
# define SSL3_CK_EDH_DSS_DES_40_CBC_SHA          SSL3_CK_DHE_DSS_DES_40_CBC_SHA
# define SSL3_CK_DHE_DSS_DES_64_CBC_SHA          0x03000012
# define SSL3_CK_EDH_DSS_DES_64_CBC_SHA          SSL3_CK_DHE_DSS_DES_64_CBC_SHA
# define SSL3_CK_DHE_DSS_DES_192_CBC3_SHA        0x03000013
# define SSL3_CK_EDH_DSS_DES_192_CBC3_SHA        SSL3_CK_DHE_DSS_DES_192_CBC3_SHA
# define SSL3_CK_DHE_RSA_DES_40_CBC_SHA          0x03000014
# define SSL3_CK_EDH_RSA_DES_40_CBC_SHA          SSL3_CK_DHE_RSA_DES_40_CBC_SHA
# define SSL3_CK_DHE_RSA_DES_64_CBC_SHA          0x03000015
# define SSL3_CK_EDH_RSA_DES_64_CBC_SHA          SSL3_CK_DHE_RSA_DES_64_CBC_SHA
# define SSL3_CK_DHE_RSA_DES_192_CBC3_SHA        0x03000016
# define SSL3_CK_EDH_RSA_DES_192_CBC3_SHA        SSL3_CK_DHE_RSA_DES_192_CBC3_SHA

# define SSL3_CK_ADH_RC4_40_MD5                  0x03000017
# define SSL3_CK_ADH_RC4_128_MD5                 0x03000018
# define SSL3_CK_ADH_DES_40_CBC_SHA              0x03000019
# define SSL3_CK_ADH_DES_64_CBC_SHA              0x0300001A
# define SSL3_CK_ADH_DES_192_CBC_SHA             0x0300001B

# define SSL3_TXT_RSA_NULL_MD5                   "NULL-MD5"
# define SSL3_TXT_RSA_NULL_SHA                   "NULL-SHA"
# define SSL3_TXT_RSA_RC4_40_MD5                 "EXP-RC4-MD5"
# define SSL3_TXT_RSA_RC4_128_MD5                "RC4-MD5"
# define SSL3_TXT_RSA_RC4_128_SHA                "RC4-SHA"
# define SSL3_TXT_RSA_RC2_40_MD5                 "EXP-RC2-CBC-MD5"
# define SSL3_TXT_RSA_IDEA_128_SHA               "IDEA-CBC-SHA"
# define SSL3_TXT_RSA_DES_40_CBC_SHA             "EXP-DES-CBC-SHA"
# define SSL3_TXT_RSA_DES_64_CBC_SHA             "DES-CBC-SHA"
# define SSL3_TXT_RSA_DES_192_CBC3_SHA           "DES-CBC3-SHA"

# define SSL3_TXT_DH_DSS_DES_40_CBC_SHA          "EXP-DH-DSS-DES-CBC-SHA"
# define SSL3_TXT_DH_DSS_DES_64_CBC_SHA          "DH-DSS-DES-CBC-SHA"
# define SSL3_TXT_DH_DSS_DES_192_CBC3_SHA        "DH-DSS-DES-CBC3-SHA"
# define SSL3_TXT_DH_RSA_DES_40_CBC_SHA          "EXP-DH-RSA-DES-CBC-SHA"
# define SSL3_TXT_DH_RSA_DES_64_CBC_SHA          "DH-RSA-DES-CBC-SHA"
# define SSL3_TXT_DH_RSA_DES_192_CBC3_SHA        "DH-RSA-DES-CBC3-SHA"

# define SSL3_TXT_DHE_DSS_DES_40_CBC_SHA         "EXP-DHE-DSS-DES-CBC-SHA"
# define SSL3_TXT_DHE_DSS_DES_64_CBC_SHA         "DHE-DSS-DES-CBC-SHA"
# define SSL3_TXT_DHE_DSS_DES_192_CBC3_SHA       "DHE-DSS-DES-CBC3-SHA"
# define SSL3_TXT_DHE_RSA_DES_40_CBC_SHA         "EXP-DHE-RSA-DES-CBC-SHA"
# define SSL3_TXT_DHE_RSA_DES_64_CBC_SHA         "DHE-RSA-DES-CBC-SHA"
# define SSL3_TXT_DHE_RSA_DES_192_CBC3_SHA       "DHE-RSA-DES-CBC3-SHA"

/*
 * This next block of six "EDH" labels is for backward compatibility with
 * older versions of OpenSSL.  New code should use the six "DHE" labels above
 * instead:
 */
# define SSL3_TXT_EDH_DSS_DES_40_CBC_SHA         "EXP-EDH-DSS-DES-CBC-SHA"
# define SSL3_TXT_EDH_DSS_DES_64_CBC_SHA         "EDH-DSS-DES-CBC-SHA"
# define SSL3_TXT_EDH_DSS_DES_192_CBC3_SHA       "EDH-DSS-DES-CBC3-SHA"
# define SSL3_TXT_EDH_RSA_DES_40_CBC_SHA         "EXP-EDH-RSA-DES-CBC-SHA"
# define SSL3_TXT_EDH_RSA_DES_64_CBC_SHA         "EDH-RSA-DES-CBC-SHA"
# define SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA       "EDH-RSA-DES-CBC3-SHA"

# define SSL3_TXT_ADH_RC4_40_MD5                 "EXP-ADH-RC4-MD5"
# define SSL3_TXT_ADH_RC4_128_MD5                "ADH-RC4-MD5"
# define SSL3_TXT_ADH_DES_40_CBC_SHA             "EXP-ADH-DES-CBC-SHA"
# define SSL3_TXT_ADH_DES_64_CBC_SHA             "ADH-DES-CBC-SHA"
# define SSL3_TXT_ADH_DES_192_CBC_SHA            "ADH-DES-CBC3-SHA"

# define SSL3_SSL_SESSION_ID_LENGTH              32
# define SSL3_MAX_SSL_SESSION_ID_LENGTH          32

# define SSL3_MASTER_SECRET_SIZE                 48
# define SSL3_RANDOM_SIZE                        32
# define SSL3_SESSION_ID_SIZE                    32
# define SSL3_RT_HEADER_LENGTH                   5

# define SSL3_HM_HEADER_LENGTH                  4

# ifndef SSL3_ALIGN_PAYLOAD
 /*
  * Some will argue that this increases memory footprint, but it's not
  * actually true. Point is that malloc has to return at least 64-bit aligned
  * pointers, meaning that allocating 5 bytes wastes 3 bytes in either case.
  * Suggested pre-gaping simply moves these wasted bytes from the end of
  * allocated region to its front, but makes data payload aligned, which
  * improves performance:-)
  */
#  define SSL3_ALIGN_PAYLOAD                     8
# else
#  if (SSL3_ALIGN_PAYLOAD&(SSL3_ALIGN_PAYLOAD-1))!=0
#   error "insane SSL3_ALIGN_PAYLOAD"
#   undef SSL3_ALIGN_PAYLOAD
#  endif
# endif

/*
 * This is the maximum MAC (digest) size used by the SSL library. Currently
 * maximum of 20 is used by SHA1, but we reserve for future extension for
 * 512-bit hashes.
 */

# define SSL3_RT_MAX_MD_SIZE                     64

/*
 * Maximum block size used in all ciphersuites. Currently 16 for AES.
 */

# define SSL_RT_MAX_CIPHER_BLOCK_SIZE            16

# define SSL3_RT_MAX_EXTRA                       (16384)

/* Maximum plaintext length: defined by SSL/TLS standards */
# define SSL3_RT_MAX_PLAIN_LENGTH                16384
/* Maximum compression overhead: defined by SSL/TLS standards */
# define SSL3_RT_MAX_COMPRESSED_OVERHEAD         1024

/*
 * The standards give a maximum encryption overhead of 1024 bytes. In
 * practice the value is lower than this. The overhead is the maximum number
 * of padding bytes (256) plus the mac size.
 */
# define SSL3_RT_MAX_ENCRYPTED_OVERHEAD  (256 + SSL3_RT_MAX_MD_SIZE)

/*
 * OpenSSL currently only uses a padding length of at most one block so the
 * send overhead is smaller.
 */

# define SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD \
                        (SSL_RT_MAX_CIPHER_BLOCK_SIZE + SSL3_RT_MAX_MD_SIZE)

/* If compression isn't used don't include the compression overhead */

# ifdef OPENSSL_NO_COMP
#  define SSL3_RT_MAX_COMPRESSED_LENGTH           SSL3_RT_MAX_PLAIN_LENGTH
# else
#  define SSL3_RT_MAX_COMPRESSED_LENGTH   \
                (SSL3_RT_MAX_PLAIN_LENGTH+SSL3_RT_MAX_COMPRESSED_OVERHEAD)
# endif
# define SSL3_RT_MAX_ENCRYPTED_LENGTH    \
                (SSL3_RT_MAX_ENCRYPTED_OVERHEAD+SSL3_RT_MAX_COMPRESSED_LENGTH)
# define SSL3_RT_MAX_PACKET_SIZE         \
                (SSL3_RT_MAX_ENCRYPTED_LENGTH+SSL3_RT_HEADER_LENGTH)

# define SSL3_MD_CLIENT_FINISHED_CONST   "\x43\x4C\x4E\x54"
# define SSL3_MD_SERVER_FINISHED_CONST   "\x53\x52\x56\x52"

# define SSL3_VERSION                    0x0300
# define SSL3_VERSION_MAJOR              0x03
# define SSL3_VERSION_MINOR              0x00

# define SSL3_RT_CHANGE_CIPHER_SPEC      20
# define SSL3_RT_ALERT                   21
# define SSL3_RT_HANDSHAKE               22
# define SSL3_RT_APPLICATION_DATA        23
# define DTLS1_RT_HEARTBEAT              24

/* Pseudo content types to indicate additional parameters */
# define TLS1_RT_CRYPTO                  0x1000
# define TLS1_RT_CRYPTO_PREMASTER        (TLS1_RT_CRYPTO | 0x1)
# define TLS1_RT_CRYPTO_CLIENT_RANDOM    (TLS1_RT_CRYPTO | 0x2)
# define TLS1_RT_CRYPTO_SERVER_RANDOM    (TLS1_RT_CRYPTO | 0x3)
# define TLS1_RT_CRYPTO_MASTER           (TLS1_RT_CRYPTO | 0x4)

# define TLS1_RT_CRYPTO_READ             0x0000
# define TLS1_RT_CRYPTO_WRITE            0x0100
# define TLS1_RT_CRYPTO_MAC              (TLS1_RT_CRYPTO | 0x5)
# define TLS1_RT_CRYPTO_KEY              (TLS1_RT_CRYPTO | 0x6)
# define TLS1_RT_CRYPTO_IV               (TLS1_RT_CRYPTO | 0x7)
# define TLS1_RT_CRYPTO_FIXED_IV         (TLS1_RT_CRYPTO | 0x8)

/* Pseudo content type for SSL/TLS header info */
# define SSL3_RT_HEADER                  0x100

# define SSL3_AL_WARNING                 1
# define SSL3_AL_FATAL                   2

# define SSL3_AD_CLOSE_NOTIFY             0
# define SSL3_AD_UNEXPECTED_MESSAGE      10/* fatal */
# define SSL3_AD_BAD_RECORD_MAC          20/* fatal */
# define SSL3_AD_DECOMPRESSION_FAILURE   30/* fatal */
# define SSL3_AD_HANDSHAKE_FAILURE       40/* fatal */
# define SSL3_AD_NO_CERTIFICATE          41
# define SSL3_AD_BAD_CERTIFICATE         42
# define SSL3_AD_UNSUPPORTED_CERTIFICATE 43
# define SSL3_AD_CERTIFICATE_REVOKED     44
# define SSL3_AD_CERTIFICATE_EXPIRED     45
# define SSL3_AD_CERTIFICATE_UNKNOWN     46
# define SSL3_AD_ILLEGAL_PARAMETER       47/* fatal */

# define TLS1_HB_REQUEST         1
# define TLS1_HB_RESPONSE        2


# define SSL3_CT_RSA_SIGN                        1
# define SSL3_CT_DSS_SIGN                        2
# define SSL3_CT_RSA_FIXED_DH                    3
# define SSL3_CT_DSS_FIXED_DH                    4
# define SSL3_CT_RSA_EPHEMERAL_DH                5
# define SSL3_CT_DSS_EPHEMERAL_DH                6
# define SSL3_CT_FORTEZZA_DMS                    20
/*
 * SSL3_CT_NUMBER is used to size arrays and it must be large enough to
 * contain all of the cert types defined for *either* SSLv3 and TLSv1.
 */
# define SSL3_CT_NUMBER                  10

# if defined(TLS_CT_NUMBER)
#  if TLS_CT_NUMBER != SSL3_CT_NUMBER
#    error "SSL/TLS CT_NUMBER values do not match"
#  endif
# endif

# define SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS       0x0001

/* Removed from OpenSSL 1.1.0 */
# define TLS1_FLAGS_TLS_PADDING_BUG              0x0

# define TLS1_FLAGS_SKIP_CERT_VERIFY             0x0010

/* Set if we encrypt then mac instead of usual mac then encrypt */
# define TLS1_FLAGS_ENCRYPT_THEN_MAC_READ        0x0100
# define TLS1_FLAGS_ENCRYPT_THEN_MAC             TLS1_FLAGS_ENCRYPT_THEN_MAC_READ

/* Set if extended master secret extension received from peer */
# define TLS1_FLAGS_RECEIVED_EXTMS               0x0200

# define TLS1_FLAGS_ENCRYPT_THEN_MAC_WRITE       0x0400

# define SSL3_MT_HELLO_REQUEST                   0
# define SSL3_MT_CLIENT_HELLO                    1
# define SSL3_MT_SERVER_HELLO                    2
# define SSL3_MT_NEWSESSION_TICKET               4
# define SSL3_MT_CERTIFICATE                     11
# define SSL3_MT_SERVER_KEY_EXCHANGE             12
# define SSL3_MT_CERTIFICATE_REQUEST             13
# define SSL3_MT_SERVER_DONE                     14
# define SSL3_MT_CERTIFICATE_VERIFY              15
# define SSL3_MT_CLIENT_KEY_EXCHANGE             16
# define SSL3_MT_FINISHED                        20
# define SSL3_MT_CERTIFICATE_STATUS              22
# ifndef OPENSSL_NO_NEXTPROTONEG
#  define SSL3_MT_NEXT_PROTO                      67
# endif
# define DTLS1_MT_HELLO_VERIFY_REQUEST    3

/* Dummy message type for handling CCS like a normal handshake message */
# define SSL3_MT_CHANGE_CIPHER_SPEC              0x0101

# define SSL3_MT_CCS                             1

/* These are used when changing over to a new cipher */
# define SSL3_CC_READ            0x01
# define SSL3_CC_WRITE           0x02
# define SSL3_CC_CLIENT          0x10
# define SSL3_CC_SERVER          0x20
# define SSL3_CHANGE_CIPHER_CLIENT_WRITE (SSL3_CC_CLIENT|SSL3_CC_WRITE)
# define SSL3_CHANGE_CIPHER_SERVER_READ  (SSL3_CC_SERVER|SSL3_CC_READ)
# define SSL3_CHANGE_CIPHER_CLIENT_READ  (SSL3_CC_CLIENT|SSL3_CC_READ)
# define SSL3_CHANGE_CIPHER_SERVER_WRITE (SSL3_CC_SERVER|SSL3_CC_WRITE)

#ifdef  __cplusplus
}
#endif
#endif
