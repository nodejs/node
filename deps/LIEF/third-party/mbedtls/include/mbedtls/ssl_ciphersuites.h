/**
 * \file ssl_ciphersuites.h
 *
 * \brief SSL Ciphersuites for Mbed TLS
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_SSL_CIPHERSUITES_H
#define MBEDTLS_SSL_CIPHERSUITES_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/pk.h"
#include "mbedtls/private/cipher.h"
#include "mbedtls/md.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Supported ciphersuites (Official IANA names)
 */
#define MBEDTLS_TLS_PSK_WITH_NULL_SHA                    0x2C   /**< Weak! */

#define MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA             0x8C
#define MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA             0x8D

#define MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256          0xA8   /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384          0xA9   /**< TLS 1.2 */

#define MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256          0xAE
#define MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA384          0xAF
#define MBEDTLS_TLS_PSK_WITH_NULL_SHA256                 0xB0   /**< Weak! */
#define MBEDTLS_TLS_PSK_WITH_NULL_SHA384                 0xB1   /**< Weak! */

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_NULL_SHA            0xC006 /**< Weak! */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA     0xC009
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA     0xC00A

#define MBEDTLS_TLS_ECDHE_RSA_WITH_NULL_SHA              0xC010 /**< Weak! */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA       0xC013
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA       0xC014

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256  0xC023 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384  0xC024 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256    0xC027 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384    0xC028 /**< TLS 1.2 */

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256  0xC02B /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384  0xC02C /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256    0xC02F /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384    0xC030 /**< TLS 1.2 */

#define MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA       0xC035
#define MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA       0xC036
#define MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256    0xC037
#define MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384    0xC038
#define MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA              0xC039
#define MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA256           0xC03A
#define MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA384           0xC03B

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256 0xC048 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384 0xC049 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256   0xC04C /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384   0xC04D /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256 0xC05C /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384 0xC05D /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256   0xC060 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384   0xC061 /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_ARIA_128_CBC_SHA256         0xC064 /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_ARIA_256_CBC_SHA384         0xC065 /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_ARIA_128_GCM_SHA256         0xC06A /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_ARIA_256_GCM_SHA384         0xC06B /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_PSK_WITH_ARIA_128_CBC_SHA256   0xC070 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_PSK_WITH_ARIA_256_CBC_SHA384   0xC071 /**< TLS 1.2 */

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 0xC072
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 0xC073
#define MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   0xC076
#define MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384   0xC077

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256 0xC086 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384 0xC087 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256   0xC08A /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384   0xC08B /**< TLS 1.2 */

#define MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256       0xC08E /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384       0xC08F /**< TLS 1.2 */

#define MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256       0xC094
#define MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384       0xC095
#define MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 0xC09A
#define MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 0xC09B

#define MBEDTLS_TLS_PSK_WITH_AES_128_CCM                0xC0A4  /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_AES_256_CCM                0xC0A5  /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8              0xC0A8  /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_AES_256_CCM_8              0xC0A9  /**< TLS 1.2 */
/* The last two are named with PSK_DHE in the RFC, which looks like a typo */

#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM        0xC0AC  /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM        0xC0AD  /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8      0xC0AE  /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8      0xC0AF  /**< TLS 1.2 */

#define MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8          0xC0FF  /**< experimental */

/* RFC 7905 */
#define MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256   0xCCA8 /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 0xCCA9 /**< TLS 1.2 */
#define MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256         0xCCAB /**< TLS 1.2 */
#define MBEDTLS_TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256   0xCCAC /**< TLS 1.2 */

/* RFC 8446, Appendix B.4 */
#define MBEDTLS_TLS1_3_AES_128_GCM_SHA256                     0x1301 /**< TLS 1.3 */
#define MBEDTLS_TLS1_3_AES_256_GCM_SHA384                     0x1302 /**< TLS 1.3 */
#define MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256               0x1303 /**< TLS 1.3 */
#define MBEDTLS_TLS1_3_AES_128_CCM_SHA256                     0x1304 /**< TLS 1.3 */
#define MBEDTLS_TLS1_3_AES_128_CCM_8_SHA256                   0x1305 /**< TLS 1.3 */

/* Reminder: update mbedtls_ssl_premaster_secret when adding a new key exchange.
 * Reminder: update MBEDTLS_KEY_EXCHANGE__xxx below
 */
typedef enum {
    MBEDTLS_KEY_EXCHANGE_NONE = 0,
    MBEDTLS_KEY_EXCHANGE_ECDHE_RSA,
    MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA,
    MBEDTLS_KEY_EXCHANGE_PSK,
    MBEDTLS_KEY_EXCHANGE_ECDHE_PSK,
    MBEDTLS_KEY_EXCHANGE_ECJPAKE,
} mbedtls_key_exchange_type_t;

/* Key exchanges using a certificate */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)     || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED
#endif

/* Key exchanges in either TLS 1.2 or 1.3 which are using an ECDSA
 * signature */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_WITH_ECDSA_ANY_ENABLED
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#define MBEDTLS_SSL_HANDSHAKE_WITH_CERT_ENABLED
#endif

/* Key exchanges allowing client certificate requests.
 *
 * This is now the same as MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED,
 * and the two macros could be unified.
 * Until Mbed TLS 3.x, the two sets were different because
 * MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED covers
 * MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED plus RSA-PSK.
 * But RSA-PSK was removed in Mbed TLS 4.0.
 */
#if defined(MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED
#endif

/* Helper to state that certificate-based client authentication through ECDSA
 * is supported in TLS 1.2 */
#if defined(MBEDTLS_KEY_EXCHANGE_CERT_REQ_ALLOWED_ENABLED) && \
    defined(PSA_HAVE_ALG_ECDSA_SIGN) && defined(PSA_HAVE_ALG_ECDSA_VERIFY)
#define MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED
#endif

/* ECDSA required for certificates in either TLS 1.2 or 1.3 */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ALLOWED_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_ECDSA_CERT_REQ_ANY_ALLOWED_ENABLED
#endif

/* Key exchanges involving server signature in ServerKeyExchange */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)     || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_WITH_SERVER_SIGNATURE_ENABLED
#endif

/* Key exchanges that involve ephemeral keys */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)     || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)     || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)   || \
    defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_PFS_ENABLED
#endif

/* Key exchanges using a PSK */
#if defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)           || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED
#endif

/* Key exchanges using ECDHE */
#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED)     || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)   || \
    defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_ECDHE_ENABLED
#endif

/* TLS 1.2 key exchanges using ECDH or ECDHE*/
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDHE_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED
#endif

/* TLS 1.3 PSK key exchanges */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED
#endif

/* TLS 1.2 or 1.3 key exchanges with PSK */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
#define MBEDTLS_SSL_HANDSHAKE_WITH_PSK_ENABLED
#endif

/* TLS 1.3 ephemeral key exchanges */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED
#endif

/* TLS 1.3 key exchanges using ECDHE */
#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED) && \
    defined(PSA_WANT_ALG_ECDH)
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_ECDHE_ENABLED
#endif

/* TLS 1.2 or 1.3 key exchanges using ECDH or ECDHE */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_ECDHE_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_ANY_ENABLED
#endif

/* The handshake params structure has a set of fields called xxdh_psa which are used:
 * - by TLS 1.2 to do ECDH or ECDHE;
 * - by TLS 1.3 to do ECDHE or FFDHE.
 * The following macros can be used to guard their declaration and use.
 */
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_ECDH_OR_ECDHE_1_2_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_XXDH_PSA_1_2_ENABLED
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_XXDH_PSA_1_2_ENABLED) || \
    defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_EPHEMERAL_ENABLED)
#define MBEDTLS_KEY_EXCHANGE_SOME_XXDH_PSA_ANY_ENABLED
#endif

typedef struct mbedtls_ssl_ciphersuite_t mbedtls_ssl_ciphersuite_t;

#define MBEDTLS_CIPHERSUITE_WEAK       0x01    /**< Weak ciphersuite flag  */
#define MBEDTLS_CIPHERSUITE_SHORT_TAG  0x02    /**< Short authentication tag,
                                                     eg for CCM_8 */
#define MBEDTLS_CIPHERSUITE_NODTLS     0x04    /**< Can't be used with DTLS */

/**
 * \brief   This structure is used for storing ciphersuite information
 *
 * \note    members are defined using integral types instead of enums
 *          in order to pack structure and reduce memory usage by internal
 *          \c ciphersuite_definitions[]
 */
struct mbedtls_ssl_ciphersuite_t {
    int MBEDTLS_PRIVATE(id);
    const char *MBEDTLS_PRIVATE(name);

    uint8_t MBEDTLS_PRIVATE(cipher);           /* mbedtls_cipher_type_t */
    uint8_t MBEDTLS_PRIVATE(mac);              /* mbedtls_md_type_t */
    uint8_t MBEDTLS_PRIVATE(key_exchange);     /* mbedtls_key_exchange_type_t */
    uint8_t MBEDTLS_PRIVATE(flags);

    uint16_t MBEDTLS_PRIVATE(min_tls_version); /* mbedtls_ssl_protocol_version */
    uint16_t MBEDTLS_PRIVATE(max_tls_version); /* mbedtls_ssl_protocol_version */
};

const int *mbedtls_ssl_list_ciphersuites(void);

const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite_from_string(const char *ciphersuite_name);
const mbedtls_ssl_ciphersuite_t *mbedtls_ssl_ciphersuite_from_id(int ciphersuite_id);

static inline const char *mbedtls_ssl_ciphersuite_get_name(const mbedtls_ssl_ciphersuite_t *info)
{
    return info->MBEDTLS_PRIVATE(name);
}

static inline int mbedtls_ssl_ciphersuite_get_id(const mbedtls_ssl_ciphersuite_t *info)
{
    return info->MBEDTLS_PRIVATE(id);
}

size_t mbedtls_ssl_ciphersuite_get_cipher_key_bitlen(const mbedtls_ssl_ciphersuite_t *info);

#ifdef __cplusplus
}
#endif

#endif /* ssl_ciphersuites.h */
