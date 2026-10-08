/**
 * \file mbedtls/config_adjust_legacy_crypto.h
 * \brief Adjust legacy configuration configuration
 *
 * This is an internal header. Do not include it directly.
 *
 * Automatically enable certain dependencies. Generally, MBEDTLS_xxx
 * configurations need to be explicitly enabled by the user: enabling
 * MBEDTLS_xxx_A but not MBEDTLS_xxx_B when A requires B results in a
 * compilation error. However, we do automatically enable certain options
 * in some circumstances. One case is if MBEDTLS_xxx_B is an internal option
 * used to identify parts of a module that are used by other module, and we
 * don't want to make the symbol MBEDTLS_xxx_B part of the public API.
 * Another case is if A didn't depend on B in earlier versions, and we
 * want to use B in A but we need to preserve backward compatibility with
 * configurations that explicitly activate MBEDTLS_xxx_A but not
 * MBEDTLS_xxx_B.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONFIG_ADJUST_LEGACY_CRYPTO_H
#define MBEDTLS_CONFIG_ADJUST_LEGACY_CRYPTO_H

#if !defined(TF_PSA_CRYPTO_CONFIG_FILES_READ)
#error "Do not include mbedtls/config_adjust_*.h manually! This can lead to problems, " \
    "up to and including runtime errors such as buffer overflows. " \
    "If you're trying to fix a complaint from check_config.h, just remove " \
    "it from your configuration file: since Mbed TLS 3.0, it is included " \
    "automatically at the right point."
#endif /* */

/* Ideally, we'd set those as defaults in mbedtls_config.h, but
 * putting an #ifdef _WIN32 in mbedtls_config.h would confuse config.py.
 *
 * So, adjust it here.
 * Not related to crypto, but this is the bottom of the stack. */
#if defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER <= 1900)
#if !defined(MBEDTLS_PLATFORM_SNPRINTF_ALT) && \
    !defined(MBEDTLS_PLATFORM_SNPRINTF_MACRO)
#define MBEDTLS_PLATFORM_SNPRINTF_ALT
#endif
#if !defined(MBEDTLS_PLATFORM_VSNPRINTF_ALT) && \
    !defined(MBEDTLS_PLATFORM_VSNPRINTF_MACRO)
#define MBEDTLS_PLATFORM_VSNPRINTF_ALT
#endif
#endif /* _MINGW32__ || (_MSC_VER && (_MSC_VER <= 1900)) */

/* If MBEDTLS_PSA_CRYPTO_C is defined, make sure MBEDTLS_PSA_CRYPTO_CLIENT
 * is defined as well to include all PSA code.
 */
#if defined(MBEDTLS_PSA_CRYPTO_C)
#define MBEDTLS_PSA_CRYPTO_CLIENT
/* Enable  MBEDTLS_ENTROPY_C in not client-only builds without an
 * external entropy source. */
#if !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
#define MBEDTLS_ENTROPY_C
#endif
#endif /* MBEDTLS_PSA_CRYPTO_C */

/**
 * \def MBEDTLS_USE_PSA_CRYPTO
 *
 * Make the X.509 and TLS libraries use PSA for cryptographic operations as
 * much as possible, and enable new APIs for using keys handled by PSA Crypto.
 *
 * \note This is a legacy symbol which still exists for backward compatibility.
 *       Up to Mbed TLS 3.x, it was not enabled by default. Now it is always
 *       enabled, and it will eventually disappear from the code base. This
 *       is not part of the public API of TF-PSA-Crypto or of Mbed TLS >=4.0.
 */
#define MBEDTLS_USE_PSA_CRYPTO

/* Auto-enable CIPHER_C when any of the unauthenticated ciphers is builtin
 * in PSA. */
#if defined(MBEDTLS_PSA_CRYPTO_C) && \
    (defined(MBEDTLS_PSA_BUILTIN_ALG_STREAM_CIPHER) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CTR) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CFB) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_OFB) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_NO_PADDING) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_PKCS7) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CCM_STAR_NO_TAG) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC))
#define MBEDTLS_CIPHER_C
#endif

/* Auto-enable MBEDTLS_MD_LIGHT based on MBEDTLS_MD_C.
 * This allows checking for MD_LIGHT rather than MD_LIGHT || MD_C.
 */
#if defined(MBEDTLS_MD_C)
#define MBEDTLS_MD_LIGHT
#endif

/* Auto-enable MBEDTLS_MD_LIGHT if needed by a module that didn't require it
 * in a previous release, to ensure backwards compatibility.
 */
#if defined(MBEDTLS_ECJPAKE_C) || \
    defined(MBEDTLS_PEM_PARSE_C) || \
    defined(MBEDTLS_ENTROPY_C) || \
    defined(MBEDTLS_PK_C) || \
    defined(MBEDTLS_RSA_C)
#define MBEDTLS_MD_LIGHT
#endif

#if defined(MBEDTLS_MD_LIGHT)
/*
 * - MBEDTLS_MD_xxx_VIA_PSA is defined if the md module may perform xxx via PSA
 *   (see below).
 * - MBEDTLS_MD_SOME_PSA is defined if at least one algorithm may be performed
 *   via PSA (see below).
 * - MBEDTLS_MD_SOME_LEGACY is defined if at least one algorithm may be performed
 *   via a direct legacy call (see below).
 *
 * The md module performs an algorithm via PSA if there is a PSA hash
 * accelerator and the PSA driver subsytem is initialized at the time the
 * operation is started, and makes a direct legacy call otherwise.
 */

/* PSA accelerated implementations */
#if defined(MBEDTLS_PSA_CRYPTO_C)

#if defined(MBEDTLS_PSA_ACCEL_ALG_MD5)
#define MBEDTLS_MD_MD5_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA_1)
#define MBEDTLS_MD_SHA1_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA_224)
#define MBEDTLS_MD_SHA224_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA_256)
#define MBEDTLS_MD_SHA256_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA_384)
#define MBEDTLS_MD_SHA384_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA_512)
#define MBEDTLS_MD_SHA512_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_RIPEMD160)
#define MBEDTLS_MD_RIPEMD160_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA3_224)
#define MBEDTLS_MD_SHA3_224_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA3_256)
#define MBEDTLS_MD_SHA3_256_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA3_384)
#define MBEDTLS_MD_SHA3_384_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_ALG_SHA3_512)
#define MBEDTLS_MD_SHA3_512_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif

#elif defined(MBEDTLS_PSA_CRYPTO_CLIENT)

#if defined(PSA_WANT_ALG_MD5)
#define MBEDTLS_MD_MD5_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA_1)
#define MBEDTLS_MD_SHA1_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA_224)
#define MBEDTLS_MD_SHA224_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA_256)
#define MBEDTLS_MD_SHA256_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA_384)
#define MBEDTLS_MD_SHA384_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA_512)
#define MBEDTLS_MD_SHA512_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_RIPEMD160)
#define MBEDTLS_MD_RIPEMD160_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA3_224)
#define MBEDTLS_MD_SHA3_224_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA3_256)
#define MBEDTLS_MD_SHA3_256_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA3_384)
#define MBEDTLS_MD_SHA3_384_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif
#if defined(PSA_WANT_ALG_SHA3_512)
#define MBEDTLS_MD_SHA3_512_VIA_PSA
#define MBEDTLS_MD_SOME_PSA
#endif

#endif /* !MBEDTLS_PSA_CRYPTO_CLIENT && !MBEDTLS_PSA_CRYPTO_C */

/* Built-in implementations */
#if defined(MBEDTLS_MD5_C) || \
    defined(MBEDTLS_SHA1_C) || \
    defined(MBEDTLS_SHA224_C) || \
    defined(MBEDTLS_SHA256_C) || \
    defined(MBEDTLS_SHA384_C) || \
    defined(MBEDTLS_SHA512_C) || \
    defined(MBEDTLS_RIPEMD160_C)
#define MBEDTLS_MD_SOME_LEGACY
#endif

#endif /* MBEDTLS_MD_LIGHT */

/* BLOCK_CIPHER module can dispatch to PSA when:
 * - PSA is enabled and drivers have been initialized
 * - desired key type is supported on the PSA side
 * If the above conditions are not met, but the legacy support is enabled, then
 * BLOCK_CIPHER will dynamically fallback to it.
 *
 * In case BLOCK_CIPHER is defined (see below) the following symbols/helpers
 * can be used to define its capabilities:
 * - MBEDTLS_BLOCK_CIPHER_SOME_PSA: there is at least 1 key type between AES,
 *   ARIA and Camellia which is supported through a driver;
 * - MBEDTLS_BLOCK_CIPHER_xxx_VIA_PSA: xxx key type is supported through a
 *   driver;
 * - MBEDTLS_BLOCK_CIPHER_xxx_VIA_LEGACY: xxx key type is supported through
 *   a legacy module (i.e. MBEDTLS_xxx_C)
 */
#if defined(MBEDTLS_PSA_CRYPTO_C)
#if defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_AES)
#define MBEDTLS_BLOCK_CIPHER_AES_VIA_PSA
#define MBEDTLS_BLOCK_CIPHER_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ARIA)
#define MBEDTLS_BLOCK_CIPHER_ARIA_VIA_PSA
#define MBEDTLS_BLOCK_CIPHER_SOME_PSA
#endif
#if defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_CAMELLIA)
#define MBEDTLS_BLOCK_CIPHER_CAMELLIA_VIA_PSA
#define MBEDTLS_BLOCK_CIPHER_SOME_PSA
#endif
#endif /* MBEDTLS_PSA_CRYPTO_C */

#if defined(MBEDTLS_AES_C)
#define MBEDTLS_BLOCK_CIPHER_AES_VIA_LEGACY
#endif
#if defined(MBEDTLS_ARIA_C)
#define MBEDTLS_BLOCK_CIPHER_ARIA_VIA_LEGACY
#endif
#if defined(MBEDTLS_CAMELLIA_C)
#define MBEDTLS_BLOCK_CIPHER_CAMELLIA_VIA_LEGACY
#endif

/* Helpers to state that BLOCK_CIPHER module supports AES, ARIA and/or Camellia
 * block ciphers via either PSA or legacy. */
#if defined(MBEDTLS_BLOCK_CIPHER_AES_VIA_PSA) || \
    defined(MBEDTLS_BLOCK_CIPHER_AES_VIA_LEGACY)
#define MBEDTLS_BLOCK_CIPHER_CAN_AES
#endif
#if defined(MBEDTLS_BLOCK_CIPHER_ARIA_VIA_PSA) || \
    defined(MBEDTLS_BLOCK_CIPHER_ARIA_VIA_LEGACY)
#define MBEDTLS_BLOCK_CIPHER_CAN_ARIA
#endif
#if defined(MBEDTLS_BLOCK_CIPHER_CAMELLIA_VIA_PSA) || \
    defined(MBEDTLS_BLOCK_CIPHER_CAMELLIA_VIA_LEGACY)
#define MBEDTLS_BLOCK_CIPHER_CAN_CAMELLIA
#endif

/* GCM_C and CCM_C can either depend on (in order of preference) BLOCK_CIPHER_C
 * or CIPHER_C. The former is auto-enabled when:
 * - CIPHER_C is not defined, which is also the legacy solution;
 * - BLOCK_CIPHER_SOME_PSA because in this case BLOCK_CIPHER can take advantage
 *   of the driver's acceleration.
 */
#if (defined(MBEDTLS_GCM_C) || defined(MBEDTLS_CCM_C)) && \
    (!defined(MBEDTLS_CIPHER_C) || defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA))
#define MBEDTLS_BLOCK_CIPHER_C
#endif

/* Helpers for GCM/CCM capabilities */
#if (defined(MBEDTLS_CIPHER_C) && defined(MBEDTLS_AES_C)) || \
    (defined(MBEDTLS_BLOCK_CIPHER_C) && defined(MBEDTLS_BLOCK_CIPHER_CAN_AES))
#define MBEDTLS_CCM_GCM_CAN_AES
#endif

#if (defined(MBEDTLS_CIPHER_C) && defined(MBEDTLS_ARIA_C)) || \
    (defined(MBEDTLS_BLOCK_CIPHER_C) && defined(MBEDTLS_BLOCK_CIPHER_CAN_ARIA))
#define MBEDTLS_CCM_GCM_CAN_ARIA
#endif

#if (defined(MBEDTLS_CIPHER_C) && defined(MBEDTLS_CAMELLIA_C)) || \
    (defined(MBEDTLS_BLOCK_CIPHER_C) && defined(MBEDTLS_BLOCK_CIPHER_CAN_CAMELLIA))
#define MBEDTLS_CCM_GCM_CAN_CAMELLIA
#endif

/* MBEDTLS_ECP_LIGHT is auto-enabled by the following symbols:
 * - MBEDTLS_ECP_C because now it consists of MBEDTLS_ECP_LIGHT plus functions
 *   for curve arithmetic. As a consequence if MBEDTLS_ECP_C is required for
 *   some reason, then MBEDTLS_ECP_LIGHT should be enabled as well.
 * - MBEDTLS_PK_PARSE_EC_EXTENDED and MBEDTLS_PK_PARSE_EC_COMPRESSED because
 *   these features are not supported in PSA so the only way to have them is
 *   to enable the built-in solution.
 *   Both of them are temporary dependencies:
 *   - PK_PARSE_EC_EXTENDED will be removed after #7779 and #7789
 *   - support for compressed points should also be added to PSA, but in this
 *     case there is no associated issue to track it yet.
 * - PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_DERIVE because Weierstrass key derivation
 *   still depends on ECP_LIGHT.
 */
#if defined(MBEDTLS_ECP_C) || \
    defined(MBEDTLS_PK_PARSE_EC_EXTENDED) || \
    defined(MBEDTLS_PK_PARSE_EC_COMPRESSED) || \
    defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR_DERIVE)
#define MBEDTLS_ECP_LIGHT
#endif

/* Backward compatibility: after #8740 the RSA module offers functions to parse
 * and write RSA private/public keys without relying on the PK one. Of course
 * this needs ASN1 support to do so, so we enable it here. */
#if defined(MBEDTLS_RSA_C)
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#endif

/* MBEDTLS_PK_PARSE_EC_COMPRESSED is introduced in Mbed TLS version 3.5, while
 * in previous version compressed points were automatically supported as long
 * as PK_PARSE_C and ECP_C were enabled. As a consequence, for backward
 * compatibility, we auto-enable PK_PARSE_EC_COMPRESSED when these conditions
 * are met. */
#if defined(MBEDTLS_PK_PARSE_C) && defined(MBEDTLS_ECP_C)
#define MBEDTLS_PK_PARSE_EC_COMPRESSED
#endif

/* Helper symbol to state that there is support for ECDH, either through
 * library implementation (ECDH_C) or through PSA. */
#if defined(PSA_WANT_ALG_ECDH)
#define MBEDTLS_CAN_ECDH
#endif

/* Historically pkparse did not check the CBC padding when decrypting
 * a key. This was a bug, which is now fixed. As a consequence, pkparse
 * now needs PKCS7 padding support, but existing configurations might not
 * enable it, so we enable it here. */
#if defined(MBEDTLS_PK_PARSE_C) && defined(MBEDTLS_PKCS5_C) && defined(MBEDTLS_CIPHER_MODE_CBC)
#define MBEDTLS_CIPHER_PADDING_PKCS7
#endif

/* Some internal helpers to determine which operation modes are available. */

#if defined(PSA_WANT_ALG_GCM) || defined(PSA_WANT_ALG_CCM) || \
    defined(PSA_WANT_ALG_CHACHA20_POLY1305)
#define MBEDTLS_SSL_HAVE_AEAD
#endif

#endif /* MBEDTLS_CONFIG_ADJUST_LEGACY_CRYPTO_H */
