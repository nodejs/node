/**
 * \file psa/crypto_adjust_config_derived.h
 * \brief Adjust PSA configuration by defining internal symbols
 *
 * This is an internal header. Do not include it directly.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_ADJUST_CONFIG_DERIVED_H
#define PSA_CRYPTO_ADJUST_CONFIG_DERIVED_H

#if !defined(TF_PSA_CRYPTO_CONFIG_FILES_READ)
#error "Do not include psa/crypto_adjust_*.h manually! This can lead to problems, " \
    "up to and including runtime errors such as buffer overflows. " \
    "If you're trying to fix a complaint from check_config.h, just remove " \
    "it from your configuration file: since Mbed TLS 3.0, it is included " \
    "automatically at the right point."
#endif /* */

/* The number of "true" entropy sources (excluding NV seed).
 * This must be consistent with mbedtls_entropy_init() in entropy.c.
 */
/* Define auxiliary macros, because in standard C, defined(xxx) is only
 * allowed directly on an #if or #elif line, not in recursive expansion. */
#if defined(MBEDTLS_PSA_BUILTIN_GET_ENTROPY)
#define MBEDTLS_PSA_BUILTIN_GET_ENTROPY_DEFINED 1
#else
#define MBEDTLS_PSA_BUILTIN_GET_ENTROPY_DEFINED 0
#endif
#if defined(MBEDTLS_PSA_DRIVER_GET_ENTROPY)
#define MBEDTLS_PSA_DRIVER_GET_ENTROPY_DEFINED 1
#else
#define MBEDTLS_PSA_DRIVER_GET_ENTROPY_DEFINED 0
#endif

#define MBEDTLS_ENTROPY_TRUE_SOURCES ( \
        MBEDTLS_PSA_BUILTIN_GET_ENTROPY_DEFINED + \
        MBEDTLS_PSA_DRIVER_GET_ENTROPY_DEFINED + \
        0)

/* Whether there is at least one entropy source for the entropy module.
 *
 * Note that when MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG is enabled, the entropy
 * module is unused and the configuration will typically not include any
 * entropy source, so this macro will typically remain undefined.
 */
#if defined(MBEDTLS_ENTROPY_NV_SEED)
#define MBEDTLS_ENTROPY_HAVE_SOURCES (MBEDTLS_ENTROPY_TRUE_SOURCES + 1)
#elif MBEDTLS_ENTROPY_TRUE_SOURCES != 0
#define MBEDTLS_ENTROPY_HAVE_SOURCES MBEDTLS_ENTROPY_TRUE_SOURCES
#else
#undef MBEDTLS_ENTROPY_HAVE_SOURCES
#endif

#if defined(PSA_WANT_ALG_ECDSA) || defined(PSA_WANT_ALG_DETERMINISTIC_ECDSA)
#define PSA_HAVE_ALG_SOME_ECDSA
#endif

#if defined(PSA_HAVE_ALG_SOME_ECDSA) && defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC)
#define PSA_HAVE_ALG_ECDSA_SIGN
#endif

#if defined(PSA_HAVE_ALG_SOME_ECDSA) && defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
#define PSA_HAVE_ALG_ECDSA_VERIFY
#endif

#if defined(PSA_WANT_ALG_JPAKE)
#define PSA_WANT_ALG_SOME_PAKE 1
#endif

/*
 * If the RNG strength is not explicitly defined in the configuration, define
 * it here to its default value. This ensures it is available for use in
 * adjusting the configuration of RNG internal modules in
 * config_adjust_legacy_crypto.h.
 */
#if !defined(MBEDTLS_PSA_CRYPTO_RNG_STRENGTH)
#define MBEDTLS_PSA_CRYPTO_RNG_STRENGTH 256
#endif

#if !defined(MBEDTLS_PSA_CRYPTO_RNG_HASH)

#if defined(PSA_WANT_ALG_SHA_256)
#define MBEDTLS_PSA_CRYPTO_RNG_HASH PSA_ALG_SHA_256
#elif defined(PSA_WANT_ALG_SHA_512)
#define MBEDTLS_PSA_CRYPTO_RNG_HASH PSA_ALG_SHA_512
#else
#if (defined(MBEDTLS_PSA_CRYPTO_C) && !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG))
#error "Not able to define MBEDTLS_PSA_CRYPTO_RNG_HASH for the entropy module."
#endif
#if defined(MBEDTLS_HMAC_DRBG_C)
#error "Not able to define MBEDTLS_PSA_CRYPTO_RNG_HASH for HMAC_DRBG."
#endif
#endif /* !PSA_WANT_ALG_SHA_256, !PSA_WANT_ALG_SHA_512 */

#endif /* !MBEDTLS_PSA_CRYPTO_RNG_HASH */

#endif /* PSA_CRYPTO_ADJUST_CONFIG_DERIVED_H */
