/**
 * \file check_config.h
 *
 * \brief Consistency checks for configuration options
 *
 * This is an internal header. Do not include it directly.
 *
 * This header is included automatically by all public Mbed TLS headers
 * (via mbedtls/build_info.h). Do not include it directly in a configuration
 * file such as mbedtls/mbedtls_config.h or #MBEDTLS_USER_CONFIG_FILE!
 * It would run at the wrong time due to missing derived symbols.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CHECK_CONFIG_H
#define MBEDTLS_CHECK_CONFIG_H

/* *INDENT-OFF* */

#if !defined(MBEDTLS_CONFIG_IS_FINALIZED)
#warning "Do not include mbedtls/check_config.h manually! " \
         "This may cause spurious errors. " \
         "It is included automatically at the right point since Mbed TLS 3.0."
#endif /* !MBEDTLS_CONFIG_IS_FINALIZED */

/*
 * We assume CHAR_BIT is 8 in many places. In practice, this is true on our
 * target platforms, so not an issue, but let's just be extra sure.
 */
#include <limits.h>
#if CHAR_BIT != 8
#error "Mbed TLS requires a platform with 8-bit chars"
#endif

#include <stdint.h>

#if defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER <= 1900)
#if !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_C is required on Windows"
#endif
/* See auto-enabling SNPRINTF_ALT and VSNPRINTF_ALT
 * in * config_adjust_legacy_crypto.h */
#endif /* _MINGW32__ || (_MSC_VER && (_MSC_VER <= 1900)) */

#if defined(TARGET_LIKE_MBED) && defined(MBEDTLS_NET_C)
#error "The NET module is not available for mbed OS - please use the network functions provided by Mbed OS"
#endif

#if defined(MBEDTLS_DEPRECATED_WARNING) && \
    !defined(__GNUC__) && !defined(__clang__)
#error "MBEDTLS_DEPRECATED_WARNING only works with GCC and Clang"
#endif

#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_HAVE_TIME)
#error "MBEDTLS_HAVE_TIME_DATE without MBEDTLS_HAVE_TIME does not make sense"
#endif

/* Limitations on ECC key types acceleration: if we have any of `PUBLIC_KEY`,
 * `KEY_PAIR_BASIC`, `KEY_PAIR_IMPORT`, `KEY_PAIR_EXPORT` then we must have
 * all 4 of them.
 */
#if defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY) || \
    defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC) || \
    defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT) || \
    defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT)
#if !defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY) || \
    !defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC) || \
    !defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT) || \
    !defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT)
#error "Unsupported partial support for ECC key type acceleration, see docs/driver-only-builds.md"
#endif /* not all of public, basic, import, export */
#endif /* one of public, basic, import, export */

/* Limitations on ECC curves acceleration: partial curve acceleration is only
 * supported with crypto excluding PK, X.509 or TLS.
 * Note: no need to check X.509 as it depends on PK. */
#if defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_384) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_BRAINPOOL_P_R1_512) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_255) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_448) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_K1_192) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_K1_224) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_K1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_192) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_224) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_256) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_384) || \
    defined(MBEDTLS_PSA_ACCEL_ECC_SECP_R1_521)
#if defined(MBEDTLS_PSA_ECC_ACCEL_INCOMPLETE_CURVES)
#if defined(MBEDTLS_PK_C) || \
    defined(MBEDTLS_SSL_TLS_C)
#error "Unsupported partial support for ECC curves acceleration, see docs/driver-only-builds.md"
#endif /* modules beyond what's supported */
#endif /* not all curves accelerated */
#endif /* some curve accelerated */

#if defined(MBEDTLS_CTR_DRBG_C) && !(defined(MBEDTLS_AES_C) || \
    (defined(MBEDTLS_PSA_CRYPTO_CLIENT) && defined(PSA_WANT_KEY_TYPE_AES) && \
    defined(PSA_WANT_ALG_ECB_NO_PADDING)))
#error "MBEDTLS_CTR_DRBG_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_DHM_C) && !defined(MBEDTLS_BIGNUM_C)
#error "MBEDTLS_DHM_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_CMAC_C) && \
    ( !defined(MBEDTLS_CIPHER_C ) || ( !defined(MBEDTLS_AES_C) && !defined(MBEDTLS_DES_C) ) )
#error "MBEDTLS_CMAC_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_NIST_KW_C) && \
    ( !defined(MBEDTLS_AES_C) || !defined(MBEDTLS_CIPHER_C) )
#error "MBEDTLS_NIST_KW_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT) && defined(MBEDTLS_PSA_CRYPTO_CONFIG)
#if defined(PSA_WANT_ALG_CBC_NO_PADDING)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and PSA_WANT_ALG_CBC_NO_PADDING cannot be defined simultaneously"
#endif
#if defined(PSA_WANT_ALG_CBC_PKCS7)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and PSA_WANT_ALG_CBC_PKCS7 cannot be defined simultaneously"
#endif
#if defined(PSA_WANT_ALG_ECB_NO_PADDING)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and PSA_WANT_ALG_ECB_NO_PADDING cannot be defined simultaneously"
#endif
#if defined(PSA_WANT_KEY_TYPE_DES)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and PSA_WANT_KEY_TYPE_DES cannot be defined simultaneously"
#endif
#endif

#if defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
#if defined(MBEDTLS_CIPHER_MODE_CBC)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and MBEDTLS_CIPHER_MODE_CBC cannot be defined simultaneously"
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and MBEDTLS_CIPHER_MODE_XTS cannot be defined simultaneously"
#endif
#if defined(MBEDTLS_DES_C)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and MBEDTLS_DES_C cannot be defined simultaneously"
#endif
#if defined(MBEDTLS_NIST_KW_C)
#error "MBEDTLS_BLOCK_CIPHER_NO_DECRYPT and MBEDTLS_NIST_KW_C cannot be defined simultaneously"
#endif
#endif

#if defined(MBEDTLS_ECDH_C) && !defined(MBEDTLS_ECP_C)
#error "MBEDTLS_ECDH_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECDSA_C) &&            \
    ( !defined(MBEDTLS_ECP_C) ||           \
      !( defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED) || \
         defined(MBEDTLS_ECP_DP_BP256R1_ENABLED) ||   \
         defined(MBEDTLS_ECP_DP_BP384R1_ENABLED) ||   \
         defined(MBEDTLS_ECP_DP_BP512R1_ENABLED) ) || \
      !defined(MBEDTLS_ASN1_PARSE_C) ||    \
      !defined(MBEDTLS_ASN1_WRITE_C) )
#error "MBEDTLS_ECDSA_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_C) && defined(MBEDTLS_USE_PSA_CRYPTO)
#if defined(MBEDTLS_PK_CAN_ECDSA_SIGN) && !defined(MBEDTLS_ASN1_WRITE_C)
#error "MBEDTLS_PK_C with MBEDTLS_USE_PSA_CRYPTO needs MBEDTLS_ASN1_WRITE_C for ECDSA signature"
#endif
#if defined(MBEDTLS_PK_CAN_ECDSA_VERIFY) && !defined(MBEDTLS_ASN1_PARSE_C)
#error "MBEDTLS_PK_C with MBEDTLS_USE_PSA_CRYPTO needs MBEDTLS_ASN1_PARSE_C for ECDSA verification"
#endif
#endif /* MBEDTLS_PK_C && MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_ECJPAKE_C) && \
    !defined(MBEDTLS_ECP_C)
#error "MBEDTLS_ECJPAKE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)           && \
    ( defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT) || \
      defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT)     || \
      defined(MBEDTLS_ECDSA_SIGN_ALT)          || \
      defined(MBEDTLS_ECDSA_VERIFY_ALT)        || \
      defined(MBEDTLS_ECDSA_GENKEY_ALT)        || \
      defined(MBEDTLS_ECP_INTERNAL_ALT)        || \
      defined(MBEDTLS_ECP_ALT) )
#error "MBEDTLS_ECP_RESTARTABLE defined, but it cannot coexist with an alternative ECP implementation"
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)           && \
    !defined(MBEDTLS_ECP_C)
#error "MBEDTLS_ECP_RESTARTABLE defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECDSA_DETERMINISTIC) && !defined(MBEDTLS_HMAC_DRBG_C)
#error "MBEDTLS_ECDSA_DETERMINISTIC defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_LIGHT) && ( !defined(MBEDTLS_BIGNUM_C) || (    \
    !defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED) &&                 \
    !defined(MBEDTLS_ECP_DP_CURVE448_ENABLED) ) )
#error "MBEDTLS_ECP_C defined (or a subset enabled), but not all prerequisites"
#endif

#if defined(MBEDTLS_ENTROPY_C) && \
    !(defined(MBEDTLS_MD_CAN_SHA512) || defined(MBEDTLS_MD_CAN_SHA256))
#error "MBEDTLS_ENTROPY_C defined, but not all prerequisites"
#endif
#if defined(MBEDTLS_ENTROPY_C) && \
    defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN) && (MBEDTLS_CTR_DRBG_ENTROPY_LEN > 64)
#error "MBEDTLS_CTR_DRBG_ENTROPY_LEN value too high"
#endif
#if defined(MBEDTLS_ENTROPY_C) &&                                            \
    (defined(MBEDTLS_ENTROPY_FORCE_SHA256) || !defined(MBEDTLS_MD_CAN_SHA512)) \
    && defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN) && (MBEDTLS_CTR_DRBG_ENTROPY_LEN > 32)
#error "MBEDTLS_CTR_DRBG_ENTROPY_LEN value too high"
#endif
#if defined(MBEDTLS_ENTROPY_C) && \
    defined(MBEDTLS_ENTROPY_FORCE_SHA256) && !defined(MBEDTLS_MD_CAN_SHA256)
#error "MBEDTLS_ENTROPY_FORCE_SHA256 defined, but not all prerequisites"
#endif

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define MBEDTLS_HAS_MEMSAN // #undef at the end of this paragraph
#endif
#endif
#if defined(MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN) &&  !defined(MBEDTLS_HAS_MEMSAN)
#error "MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN requires building with MemorySanitizer"
#endif
#if defined(MBEDTLS_HAS_MEMSAN) && defined(MBEDTLS_HAVE_ASM)
#error "MemorySanitizer does not support assembly implementation"
#endif
#undef MBEDTLS_HAS_MEMSAN // temporary macro defined above

#if defined(MBEDTLS_CCM_C) && \
    !(defined(MBEDTLS_CCM_GCM_CAN_AES) || defined(MBEDTLS_CCM_GCM_CAN_ARIA) || \
    defined(MBEDTLS_CCM_GCM_CAN_CAMELLIA))
#error "MBEDTLS_CCM_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_GCM_C) && \
    !(defined(MBEDTLS_CCM_GCM_CAN_AES) || defined(MBEDTLS_CCM_GCM_CAN_ARIA) || \
    defined(MBEDTLS_CCM_GCM_CAN_CAMELLIA))
#error "MBEDTLS_GCM_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_CHACHAPOLY_C) && !defined(MBEDTLS_CHACHA20_C)
#error "MBEDTLS_CHACHAPOLY_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_CHACHAPOLY_C) && !defined(MBEDTLS_POLY1305_C)
#error "MBEDTLS_CHACHAPOLY_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_RANDOMIZE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_ADD_MIXED_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_ADD_MIXED_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_DOUBLE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_RANDOMIZE_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NO_FALLBACK) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NO_FALLBACK defined, but no alternative implementation enabled"
#endif

#if defined(MBEDTLS_HKDF_C) && !defined(MBEDTLS_MD_C)
#error "MBEDTLS_HKDF_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_HMAC_DRBG_C) && !defined(MBEDTLS_MD_C)
#error "MBEDTLS_HMAC_DRBG_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_CAN_ECDH) ||                                       \
      !defined(MBEDTLS_PK_CAN_ECDSA_SIGN) ||                                \
      !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_CAN_ECDH) || !defined(MBEDTLS_RSA_C) ||          \
      !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED) && !defined(MBEDTLS_DHM_C)
#error "MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) &&                     \
    !defined(MBEDTLS_CAN_ECDH)
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED) &&                   \
    ( !defined(MBEDTLS_DHM_C) || !defined(MBEDTLS_RSA_C) ||           \
      !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_CAN_ECDH) || !defined(MBEDTLS_RSA_C) ||          \
      !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) &&                \
    ( !defined(MBEDTLS_CAN_ECDH) ||                                       \
      !defined(MBEDTLS_PK_CAN_ECDSA_SIGN) ||                                \
      !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED) &&                   \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) || \
      !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED) &&                       \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) || \
      !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) &&    \
    ( !defined(PSA_WANT_ALG_JPAKE) ||                   \
      !defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC) || \
      !defined(PSA_WANT_ECC_SECP_R1_256) )
#error "MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED defined, but not all prerequisites"
#endif
#else /* MBEDTLS_USE_PSA_CRYPTO */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) &&    \
    ( !defined(MBEDTLS_ECJPAKE_C) ||                    \
      !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) )
#error "MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED defined, but not all prerequisites"
#endif
#endif /* MBEDTLS_USE_PSA_CRYPTO */

/* Use of EC J-PAKE in TLS requires SHA-256. */
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) &&                    \
    !defined(MBEDTLS_MD_CAN_SHA256)
#error "MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED) &&        \
    !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE) &&            \
    !defined(MBEDTLS_MD_CAN_SHA256) &&                        \
    !defined(MBEDTLS_MD_CAN_SHA512) &&                        \
    !defined(MBEDTLS_MD_CAN_SHA1)
#error "!MBEDTLS_SSL_KEEP_PEER_CERTIFICATE requires SHA-512, SHA-256 or SHA-1".
#endif

#if defined(MBEDTLS_MD_C) && \
    !defined(MBEDTLS_MD_CAN_MD5) && \
    !defined(MBEDTLS_MD_CAN_RIPEMD160) && \
    !defined(MBEDTLS_MD_CAN_SHA1) && \
    !defined(MBEDTLS_MD_CAN_SHA224) && \
    !defined(MBEDTLS_MD_CAN_SHA256) && \
    !defined(MBEDTLS_MD_CAN_SHA384) && \
    !defined(MBEDTLS_MD_CAN_SHA512) && \
    !defined(MBEDTLS_MD_CAN_SHA3_224) && \
    !defined(MBEDTLS_MD_CAN_SHA3_256) && \
    !defined(MBEDTLS_MD_CAN_SHA3_384) && \
    !defined(MBEDTLS_MD_CAN_SHA3_512)
#error "MBEDTLS_MD_C defined, but no hash algorithm"
#endif

#if defined(MBEDTLS_LMS_C) &&                                          \
    ! ( defined(MBEDTLS_PSA_CRYPTO_CLIENT) && defined(PSA_WANT_ALG_SHA_256) )
#error "MBEDTLS_LMS_C requires MBEDTLS_PSA_CRYPTO_C and PSA_WANT_ALG_SHA_256"
#endif

#if defined(MBEDTLS_LMS_PRIVATE) &&                                    \
    ( !defined(MBEDTLS_LMS_C) )
#error "MBEDTLS_LMS_PRIVATE requires MBEDTLS_LMS_C"
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C) &&                          \
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_PLATFORM_MEMORY) )
#error "MBEDTLS_MEMORY_BUFFER_ALLOC_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_MEMORY_BACKTRACE) && !defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#error "MBEDTLS_MEMORY_BACKTRACE defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_MEMORY_DEBUG) && !defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#error "MBEDTLS_MEMORY_DEBUG defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PEM_PARSE_C) && !defined(MBEDTLS_BASE64_C)
#error "MBEDTLS_PEM_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PEM_WRITE_C) && !defined(MBEDTLS_BASE64_C)
#error "MBEDTLS_PEM_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_C) && \
    !defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#error "MBEDTLS_PK_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_PARSE_C) && \
    (!defined(MBEDTLS_ASN1_PARSE_C) || \
     !defined(MBEDTLS_OID_C)        || \
     !defined(MBEDTLS_PK_C))
#error "MBEDTLS_PK_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_WRITE_C) && \
    (!defined(MBEDTLS_ASN1_WRITE_C) || \
     !defined(MBEDTLS_OID_C)        || \
     !defined(MBEDTLS_PK_C))
#error "MBEDTLS_PK_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_EXIT_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_EXIT_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_EXIT_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_EXIT_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_EXIT_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_EXIT) ||\
        defined(MBEDTLS_PLATFORM_EXIT_ALT) )
#error "MBEDTLS_PLATFORM_EXIT_MACRO and MBEDTLS_PLATFORM_STD_EXIT/MBEDTLS_PLATFORM_EXIT_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_SETBUF_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_SETBUF_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_SETBUF_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_SETBUF_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_SETBUF_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_SETBUF) ||\
        defined(MBEDTLS_PLATFORM_SETBUF_ALT) )
#error "MBEDTLS_PLATFORM_SETBUF_MACRO and MBEDTLS_PLATFORM_STD_SETBUF/MBEDTLS_PLATFORM_SETBUF_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_TIME_ALT) &&\
    ( !defined(MBEDTLS_PLATFORM_C) ||\
        !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_TIME_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_TIME_MACRO) &&\
    ( !defined(MBEDTLS_PLATFORM_C) ||\
        !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_TIME_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_MS_TIME_TYPE_MACRO) &&\
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_MS_TIME_TYPE_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_MS_TIME_ALT)   && \
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_MS_TIME_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_TIME_TYPE_MACRO) &&\
    ( !defined(MBEDTLS_PLATFORM_C) ||\
        !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_TIME_TYPE_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_TIME_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_TIME) ||\
        defined(MBEDTLS_PLATFORM_TIME_ALT) )
#error "MBEDTLS_PLATFORM_TIME_MACRO and MBEDTLS_PLATFORM_STD_TIME/MBEDTLS_PLATFORM_TIME_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_TIME_TYPE_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_TIME) ||\
        defined(MBEDTLS_PLATFORM_TIME_ALT) )
#error "MBEDTLS_PLATFORM_TIME_TYPE_MACRO and MBEDTLS_PLATFORM_STD_TIME/MBEDTLS_PLATFORM_TIME_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_FPRINTF_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_FPRINTF_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_FPRINTF_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_FPRINTF_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_FPRINTF_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_FPRINTF) ||\
        defined(MBEDTLS_PLATFORM_FPRINTF_ALT) )
#error "MBEDTLS_PLATFORM_FPRINTF_MACRO and MBEDTLS_PLATFORM_STD_FPRINTF/MBEDTLS_PLATFORM_FPRINTF_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_FREE_MACRO) &&\
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_PLATFORM_MEMORY) )
#error "MBEDTLS_PLATFORM_FREE_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_FREE_MACRO) &&\
    defined(MBEDTLS_PLATFORM_STD_FREE)
#error "MBEDTLS_PLATFORM_FREE_MACRO and MBEDTLS_PLATFORM_STD_FREE cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_FREE_MACRO) && !defined(MBEDTLS_PLATFORM_CALLOC_MACRO)
#error "MBEDTLS_PLATFORM_CALLOC_MACRO must be defined if MBEDTLS_PLATFORM_FREE_MACRO is"
#endif

#if defined(MBEDTLS_PLATFORM_CALLOC_MACRO) &&\
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_PLATFORM_MEMORY) )
#error "MBEDTLS_PLATFORM_CALLOC_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_CALLOC_MACRO) &&\
    defined(MBEDTLS_PLATFORM_STD_CALLOC)
#error "MBEDTLS_PLATFORM_CALLOC_MACRO and MBEDTLS_PLATFORM_STD_CALLOC cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_CALLOC_MACRO) && !defined(MBEDTLS_PLATFORM_FREE_MACRO)
#error "MBEDTLS_PLATFORM_FREE_MACRO must be defined if MBEDTLS_PLATFORM_CALLOC_MACRO is"
#endif

#if defined(MBEDTLS_PLATFORM_MEMORY) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_MEMORY defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_PRINTF_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_PRINTF_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_PRINTF_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_PRINTF_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_PRINTF_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_PRINTF) ||\
        defined(MBEDTLS_PLATFORM_PRINTF_ALT) )
#error "MBEDTLS_PLATFORM_PRINTF_MACRO and MBEDTLS_PLATFORM_STD_PRINTF/MBEDTLS_PLATFORM_PRINTF_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_SNPRINTF_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_SNPRINTF_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_SNPRINTF_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_SNPRINTF_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_SNPRINTF_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_SNPRINTF) ||\
        defined(MBEDTLS_PLATFORM_SNPRINTF_ALT) )
#error "MBEDTLS_PLATFORM_SNPRINTF_MACRO and MBEDTLS_PLATFORM_STD_SNPRINTF/MBEDTLS_PLATFORM_SNPRINTF_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_VSNPRINTF_ALT) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_VSNPRINTF_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_VSNPRINTF_MACRO) && !defined(MBEDTLS_PLATFORM_C)
#error "MBEDTLS_PLATFORM_VSNPRINTF_MACRO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_VSNPRINTF_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_VSNPRINTF) ||\
        defined(MBEDTLS_PLATFORM_VSNPRINTF_ALT) )
#error "MBEDTLS_PLATFORM_VSNPRINTF_MACRO and MBEDTLS_PLATFORM_STD_VSNPRINTF/MBEDTLS_PLATFORM_VSNPRINTF_ALT cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_STD_MEM_HDR) &&\
    !defined(MBEDTLS_PLATFORM_NO_STD_FUNCTIONS)
#error "MBEDTLS_PLATFORM_STD_MEM_HDR defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_CALLOC) && !defined(MBEDTLS_PLATFORM_MEMORY)
#error "MBEDTLS_PLATFORM_STD_CALLOC defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_FREE) && !defined(MBEDTLS_PLATFORM_MEMORY)
#error "MBEDTLS_PLATFORM_STD_FREE defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_EXIT) &&\
    !defined(MBEDTLS_PLATFORM_EXIT_ALT)
#error "MBEDTLS_PLATFORM_STD_EXIT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_TIME) &&\
    ( !defined(MBEDTLS_PLATFORM_TIME_ALT) ||\
        !defined(MBEDTLS_HAVE_TIME) )
#error "MBEDTLS_PLATFORM_STD_TIME defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_FPRINTF) &&\
    !defined(MBEDTLS_PLATFORM_FPRINTF_ALT)
#error "MBEDTLS_PLATFORM_STD_FPRINTF defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_PRINTF) &&\
    !defined(MBEDTLS_PLATFORM_PRINTF_ALT)
#error "MBEDTLS_PLATFORM_STD_PRINTF defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_SNPRINTF) &&\
    !defined(MBEDTLS_PLATFORM_SNPRINTF_ALT)
#error "MBEDTLS_PLATFORM_STD_SNPRINTF defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ENTROPY_NV_SEED) &&\
    ( !defined(MBEDTLS_PLATFORM_C) || !defined(MBEDTLS_ENTROPY_C) )
#error "MBEDTLS_ENTROPY_NV_SEED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT) &&\
    !defined(MBEDTLS_ENTROPY_NV_SEED)
#error "MBEDTLS_PLATFORM_NV_SEED_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_NV_SEED_READ) &&\
    !defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
#error "MBEDTLS_PLATFORM_STD_NV_SEED_READ defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_STD_NV_SEED_WRITE) &&\
    !defined(MBEDTLS_PLATFORM_NV_SEED_ALT)
#error "MBEDTLS_PLATFORM_STD_NV_SEED_WRITE defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PLATFORM_NV_SEED_READ_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_NV_SEED_READ) ||\
      defined(MBEDTLS_PLATFORM_NV_SEED_ALT) )
#error "MBEDTLS_PLATFORM_NV_SEED_READ_MACRO and MBEDTLS_PLATFORM_STD_NV_SEED_READ cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PLATFORM_NV_SEED_WRITE_MACRO) &&\
    ( defined(MBEDTLS_PLATFORM_STD_NV_SEED_WRITE) ||\
      defined(MBEDTLS_PLATFORM_NV_SEED_ALT) )
#error "MBEDTLS_PLATFORM_NV_SEED_WRITE_MACRO and MBEDTLS_PLATFORM_STD_NV_SEED_WRITE cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_C) &&                                    \
    !( ( ( defined(MBEDTLS_CTR_DRBG_C) || defined(MBEDTLS_HMAC_DRBG_C) ) && \
         defined(MBEDTLS_ENTROPY_C) ) ||                                \
       defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) )
#error "MBEDTLS_PSA_CRYPTO_C defined, but not all prerequisites (missing RNG)"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_C) && defined(PSA_HAVE_SOFT_BLOCK_MODE) && \
    defined(PSA_HAVE_SOFT_BLOCK_CIPHER) && !defined(MBEDTLS_CIPHER_C)
#error "MBEDTLS_PSA_CRYPTO_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_SPM) && !defined(MBEDTLS_PSA_CRYPTO_C)
#error "MBEDTLS_PSA_CRYPTO_SPM defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_SE_C) &&    \
    ! ( defined(MBEDTLS_PSA_CRYPTO_C) && \
        defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) )
#error "MBEDTLS_PSA_CRYPTO_SE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
#if defined(MBEDTLS_DEPRECATED_REMOVED)
#error "MBEDTLS_PSA_CRYPTO_SE_C is deprecated and will be removed in a future version of Mbed TLS"
#elif defined(MBEDTLS_DEPRECATED_WARNING)
#warning "MBEDTLS_PSA_CRYPTO_SE_C is deprecated and will be removed in a future version of Mbed TLS"
#endif
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) &&            \
    ! defined(MBEDTLS_PSA_CRYPTO_C)
#error "MBEDTLS_PSA_CRYPTO_STORAGE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PSA_INJECT_ENTROPY) &&      \
    !( defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) && \
       defined(MBEDTLS_ENTROPY_NV_SEED) )
#error "MBEDTLS_PSA_INJECT_ENTROPY defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PSA_INJECT_ENTROPY) &&              \
    !defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES)
#error "MBEDTLS_PSA_INJECT_ENTROPY is not compatible with actual entropy sources"
#endif

#if defined(MBEDTLS_PSA_INJECT_ENTROPY) &&              \
    defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
#error "MBEDTLS_PSA_INJECT_ENTROPY is not compatible with MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG"
#endif

#if defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC) &&           \
    defined(MBEDTLS_PSA_STATIC_KEY_SLOTS)
#error "MBEDTLS_PSA_KEY_STORE_DYNAMIC and MBEDTLS_PSA_STATIC_KEY_SLOTS cannot be defined simultaneously"
#endif

#if defined(MBEDTLS_PSA_ITS_FILE_C) && \
    !defined(MBEDTLS_FS_IO)
#error "MBEDTLS_PSA_ITS_FILE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_RSA_C) && ( !defined(MBEDTLS_BIGNUM_C) ||         \
    !defined(MBEDTLS_OID_C) )
#error "MBEDTLS_RSA_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_RSA_C) && ( !defined(MBEDTLS_PKCS1_V21) &&         \
    !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_RSA_C defined, but none of the PKCS1 versions enabled"
#endif

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT) &&                        \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_PKCS1_V21) )
#error "MBEDTLS_X509_RSASSA_PSS_SUPPORT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT) && \
    defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)
#error "Must only define one of MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#endif

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)
#if !defined(MBEDTLS_SHA512_C)
#error "MBEDTLS_SHA512_USE_A64_CRYPTO_* defined without MBEDTLS_SHA512_C"
#endif
#if defined(MBEDTLS_SHA512_ALT) || defined(MBEDTLS_SHA512_PROCESS_ALT)
#error "MBEDTLS_SHA512_*ALT can't be used with MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#endif

#endif /* MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT || MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY */

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY) && !defined(__aarch64__)
#error "MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY defined on non-Aarch64 system"
#endif

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT) && \
    defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
#error "Must only define one of MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#endif

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
#if !defined(MBEDTLS_SHA256_C)
#error "MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_* defined without MBEDTLS_SHA256_C"
#endif
#if defined(MBEDTLS_SHA256_ALT) || defined(MBEDTLS_SHA256_PROCESS_ALT)
#error "MBEDTLS_SHA256_*ALT can't be used with MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#endif

#endif

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY) && !defined(MBEDTLS_ARCH_IS_ARMV8_A)
#error "MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY defined on non-Armv8-A system"
#endif

/* TLS 1.3 requires separate HKDF parts from PSA,
 * and at least one ciphersuite, so at least SHA-256 or SHA-384
 * from PSA to use with HKDF.
 *
 * Note: for dependencies common with TLS 1.2 (running handshake hash),
 * see MBEDTLS_SSL_TLS_C. */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && \
    !(defined(MBEDTLS_PSA_CRYPTO_CLIENT) && \
      defined(PSA_WANT_ALG_HKDF_EXTRACT) && \
      defined(PSA_WANT_ALG_HKDF_EXPAND) && \
      (defined(PSA_WANT_ALG_SHA_256) || defined(PSA_WANT_ALG_SHA_384)))
#error "MBEDTLS_SSL_PROTO_TLS1_3 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED)
#if !( (defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH)) && \
       defined(MBEDTLS_X509_CRT_PARSE_C) && \
       ( defined(MBEDTLS_PK_CAN_ECDSA_SIGN) || defined(MBEDTLS_PKCS1_V21) ) )
#error "MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED defined, but not all prerequisites"
#endif
#endif

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED)
#if !( defined(PSA_WANT_ALG_ECDH) || defined(PSA_WANT_ALG_FFDH) )
#error "MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED defined, but not all prerequisites"
#endif
#endif

/*
 * The current implementation of TLS 1.3 requires MBEDTLS_SSL_KEEP_PEER_CERTIFICATE.
 */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3) && !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
#error "MBEDTLS_SSL_PROTO_TLS1_3 defined without MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) &&                                    \
    !(defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED) ||                          \
      defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                      \
      defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                    \
      defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                  \
      defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||                     \
      defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED) ||                   \
      defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED) ||                          \
      defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED) ||                      \
      defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED) ||                      \
      defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||                    \
      defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) )
#error "One or more versions of the TLS protocol are enabled " \
        "but no key exchange methods defined with MBEDTLS_KEY_EXCHANGE_xxxx"
#endif

#if defined(MBEDTLS_SSL_EARLY_DATA) && \
    ( !defined(MBEDTLS_SSL_SESSION_TICKETS) || \
      ( !defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED) && \
        !defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED) ) )
#error "MBEDTLS_SSL_EARLY_DATA  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_SRV_C) && \
    defined(MBEDTLS_SSL_MAX_EARLY_DATA_SIZE) &&                      \
        ((MBEDTLS_SSL_MAX_EARLY_DATA_SIZE < 0) ||                    \
         (MBEDTLS_SSL_MAX_EARLY_DATA_SIZE > UINT32_MAX))
#error "MBEDTLS_SSL_MAX_EARLY_DATA_SIZE must be in the range(0..UINT32_MAX)"
#endif

#if defined(MBEDTLS_SSL_PROTO_DTLS)     && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_PROTO_DTLS defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CLI_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_CLI_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_ASYNC_PRIVATE) && !defined(MBEDTLS_X509_CRT_PARSE_C)
#error "MBEDTLS_SSL_ASYNC_PRIVATE defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && !(defined(MBEDTLS_CIPHER_C) || \
    defined(MBEDTLS_USE_PSA_CRYPTO))
#error "MBEDTLS_SSL_TLS_C defined, but not all prerequisites"
#endif

/* TLS 1.2 and 1.3 require SHA-256 or SHA-384 (running handshake hash) */
#if defined(MBEDTLS_SSL_TLS_C)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#if !(defined(PSA_WANT_ALG_SHA_256) || defined(PSA_WANT_ALG_SHA_384))
#error "MBEDTLS_SSL_TLS_C defined, but not all prerequisites"
#endif
#else /* MBEDTLS_USE_PSA_CRYPTO */
#if !defined(MBEDTLS_MD_C) || \
    !(defined(MBEDTLS_MD_CAN_SHA256) || defined(MBEDTLS_MD_CAN_SHA384))
#error "MBEDTLS_SSL_TLS_C defined, but not all prerequisites"
#endif
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#endif /* MBEDTLS_SSL_TLS_C */

#if defined(MBEDTLS_SSL_SRV_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_SRV_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && \
    !( defined(MBEDTLS_SSL_PROTO_TLS1_2) || defined(MBEDTLS_SSL_PROTO_TLS1_3) )
#error "MBEDTLS_SSL_TLS_C defined, but no protocols are active"
#endif

#if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY) && !defined(MBEDTLS_SSL_PROTO_DTLS)
#error "MBEDTLS_SSL_DTLS_HELLO_VERIFY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && \
    !defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
#error "MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_ANTI_REPLAY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_CONNECTION_ID  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)            &&                 \
    defined(MBEDTLS_SSL_CID_IN_LEN_MAX) &&                 \
    MBEDTLS_SSL_CID_IN_LEN_MAX > 255
#error "MBEDTLS_SSL_CID_IN_LEN_MAX too large (max 255)"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)            &&                  \
    defined(MBEDTLS_SSL_CID_OUT_LEN_MAX) &&                 \
    MBEDTLS_SSL_CID_OUT_LEN_MAX > 255
#error "MBEDTLS_SSL_CID_OUT_LEN_MAX too large (max 255)"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT)     &&                 \
    !defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
#error "MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT) && MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT != 0
#if defined(MBEDTLS_DEPRECATED_REMOVED)
#error "MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT is deprecated and will be removed in a future version of Mbed TLS"
#elif defined(MBEDTLS_DEPRECATED_WARNING)
#warning "MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT is deprecated and will be removed in a future version of Mbed TLS"
#endif
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT && MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT != 0 */

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC) &&   \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_ENCRYPT_THEN_MAC defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_EXTENDED_MASTER_SECRET defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_RENEGOTIATION) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_RENEGOTIATION defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TICKET_C) && ( !defined(MBEDTLS_CIPHER_C) && \
                                       !defined(MBEDTLS_USE_PSA_CRYPTO) )
#error "MBEDTLS_SSL_TICKET_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TICKET_C) && \
    !( defined(MBEDTLS_SSL_HAVE_CCM) || defined(MBEDTLS_SSL_HAVE_GCM) || \
    defined(MBEDTLS_SSL_HAVE_CHACHAPOLY) )
#error "MBEDTLS_SSL_TICKET_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH) && \
    MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH >= 256
#error "MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH must be less than 256"
#endif

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION) && \
        !defined(MBEDTLS_X509_CRT_PARSE_C)
#error "MBEDTLS_SSL_SERVER_NAME_INDICATION defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_THREADING_PTHREAD)
#if !defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_PTHREAD defined, but not all prerequisites"
#endif
#define MBEDTLS_THREADING_IMPL // undef at the end of this paragraph
#endif
#if defined(MBEDTLS_THREADING_ALT)
#if !defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_ALT defined, but not all prerequisites"
#endif
#define MBEDTLS_THREADING_IMPL // undef at the end of this paragraph
#endif
#if defined(MBEDTLS_THREADING_C) && !defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_C defined, single threading implementation required"
#endif
#undef MBEDTLS_THREADING_IMPL // temporary macro defined above

#if defined(MBEDTLS_USE_PSA_CRYPTO) && !defined(MBEDTLS_PSA_CRYPTO_CLIENT)
#error "MBEDTLS_USE_PSA_CRYPTO defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_VERSION_FEATURES) && !defined(MBEDTLS_VERSION_C)
#error "MBEDTLS_VERSION_FEATURES defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_USE_C) && \
    (!defined(MBEDTLS_OID_C) || !defined(MBEDTLS_ASN1_PARSE_C) ||   \
    !defined(MBEDTLS_PK_PARSE_C) ||                                 \
    ( !defined(MBEDTLS_MD_C) && !defined(MBEDTLS_USE_PSA_CRYPTO) ) )
#error "MBEDTLS_X509_USE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CREATE_C) && \
    (!defined(MBEDTLS_OID_C) || !defined(MBEDTLS_ASN1_WRITE_C) ||      \
    !defined(MBEDTLS_PK_PARSE_C) ||                                    \
    ( !defined(MBEDTLS_MD_C) && !defined(MBEDTLS_USE_PSA_CRYPTO) ) )
#error "MBEDTLS_X509_CREATE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRT_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRL_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRL_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CSR_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CRT_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CSR_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK) && \
            ( !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_HAVE_INT32) && defined(MBEDTLS_HAVE_INT64)
#error "MBEDTLS_HAVE_INT32 and MBEDTLS_HAVE_INT64 cannot be defined simultaneously"
#endif /* MBEDTLS_HAVE_INT32 && MBEDTLS_HAVE_INT64 */

#if ( defined(MBEDTLS_HAVE_INT32) || defined(MBEDTLS_HAVE_INT64) ) && \
    defined(MBEDTLS_HAVE_ASM)
#error "MBEDTLS_HAVE_INT32/MBEDTLS_HAVE_INT64 and MBEDTLS_HAVE_ASM cannot be defined simultaneously"
#endif /* (MBEDTLS_HAVE_INT32 || MBEDTLS_HAVE_INT64) && MBEDTLS_HAVE_ASM */

#if defined(MBEDTLS_SSL_DTLS_SRTP) && ( !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_SRTP defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH) && ( !defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH) )
#error "MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_RECORD_SIZE_LIMIT) && ( !defined(MBEDTLS_SSL_PROTO_TLS1_3) )
#error "MBEDTLS_SSL_RECORD_SIZE_LIMIT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CONTEXT_SERIALIZATION) && \
    !( defined(MBEDTLS_SSL_HAVE_CCM) || defined(MBEDTLS_SSL_HAVE_GCM) || \
    defined(MBEDTLS_SSL_HAVE_CHACHAPOLY) )
#error "MBEDTLS_SSL_CONTEXT_SERIALIZATION defined, but not all prerequisites"
#endif

/* Reject attempts to enable options that have been removed and that could
 * cause a build to succeed but with features removed. */

#if defined(MBEDTLS_HAVEGE_C) //no-check-names
#error "MBEDTLS_HAVEGE_C was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/2599"
#endif

#if defined(MBEDTLS_SSL_HW_RECORD_ACCEL) //no-check-names
#error "MBEDTLS_SSL_HW_RECORD_ACCEL was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_PROTO_SSL3) //no-check-names
#error "MBEDTLS_SSL_PROTO_SSL3 (SSL v3.0 support) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO) //no-check-names
#error "MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO (SSL v2 ClientHello support) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_SSL_TRUNCATED_HMAC_COMPAT) //no-check-names
#error "MBEDTLS_SSL_TRUNCATED_HMAC_COMPAT (compatibility with the buggy implementation of truncated HMAC in Mbed TLS up to 2.7) was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES) //no-check-names
#error "MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES was removed in Mbed TLS 3.0. See the ChangeLog entry if you really need SHA-1-signed certificates."
#endif

#if defined(MBEDTLS_ZLIB_SUPPORT) //no-check-names
#error "MBEDTLS_ZLIB_SUPPORT was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4031"
#endif

#if defined(MBEDTLS_CHECK_PARAMS) //no-check-names
#error "MBEDTLS_CHECK_PARAMS was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4313"
#endif

#if defined(MBEDTLS_SSL_CID_PADDING_GRANULARITY) //no-check-names
#error "MBEDTLS_SSL_CID_PADDING_GRANULARITY was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4335"
#endif

#if defined(MBEDTLS_SSL_TLS1_3_PADDING_GRANULARITY) //no-check-names
#error "MBEDTLS_SSL_TLS1_3_PADDING_GRANULARITY was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4335"
#endif

#if defined(MBEDTLS_SSL_TRUNCATED_HMAC) //no-check-names
#error "MBEDTLS_SSL_TRUNCATED_HMAC was removed in Mbed TLS 3.0. See https://github.com/Mbed-TLS/mbedtls/issues/4341"
#endif

#if defined(MBEDTLS_PKCS7_C) && ( ( !defined(MBEDTLS_ASN1_PARSE_C) ) || \
    ( !defined(MBEDTLS_OID_C) ) || ( !defined(MBEDTLS_PK_PARSE_C) ) || \
    ( !defined(MBEDTLS_X509_CRT_PARSE_C) ) || \
    ( !defined(MBEDTLS_X509_CRL_PARSE_C) ) || \
    ( !defined(MBEDTLS_MD_C) ) )
#error  "MBEDTLS_PKCS7_C is defined, but not all prerequisites"
#endif

/*
 * Avoid warning from -pedantic. This is a convenient place for this
 * workaround since this is included by every single file before the
 * #if defined(MBEDTLS_xxx_C) that results in empty translation units.
 */
typedef int mbedtls_iso_c_forbids_empty_translation_units;

/* *INDENT-ON* */
#endif /* MBEDTLS_CHECK_CONFIG_H */
