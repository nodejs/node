/**
 * \file psa/crypto_adjust_config_synonyms.h
 * \brief Adjust PSA configuration: enable quasi-synonyms
 *
 * This is an internal header. Do not include it directly.
 *
 * When two features require almost the same code, we automatically enable
 * both when either one is requested, to reduce the combinatorics of
 * possible configurations.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_ADJUST_CONFIG_SYNONYMS_H
#define PSA_CRYPTO_ADJUST_CONFIG_SYNONYMS_H

#if !defined(MBEDTLS_CONFIG_FILES_READ)
#error "Do not include psa/crypto_adjust_*.h manually! This can lead to problems, " \
    "up to and including runtime errors such as buffer overflows. " \
    "If you're trying to fix a complaint from check_config.h, just remove " \
    "it from your configuration file: since Mbed TLS 3.0, it is included " \
    "automatically at the right point."
#endif /* */

/****************************************************************/
/* De facto synonyms */
/****************************************************************/

#if defined(PSA_WANT_ALG_ECDSA_ANY) && !defined(PSA_WANT_ALG_ECDSA)
#define PSA_WANT_ALG_ECDSA PSA_WANT_ALG_ECDSA_ANY
#elif !defined(PSA_WANT_ALG_ECDSA_ANY) && defined(PSA_WANT_ALG_ECDSA)
#define PSA_WANT_ALG_ECDSA_ANY PSA_WANT_ALG_ECDSA
#endif

#if defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN_RAW) && !defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN)
#define PSA_WANT_ALG_RSA_PKCS1V15_SIGN PSA_WANT_ALG_RSA_PKCS1V15_SIGN_RAW
#elif !defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN_RAW) && defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN)
#define PSA_WANT_ALG_RSA_PKCS1V15_SIGN_RAW PSA_WANT_ALG_RSA_PKCS1V15_SIGN
#endif

#if defined(PSA_WANT_ALG_RSA_PSS_ANY_SALT) && !defined(PSA_WANT_ALG_RSA_PSS)
#define PSA_WANT_ALG_RSA_PSS PSA_WANT_ALG_RSA_PSS_ANY_SALT
#elif !defined(PSA_WANT_ALG_RSA_PSS_ANY_SALT) && defined(PSA_WANT_ALG_RSA_PSS)
#define PSA_WANT_ALG_RSA_PSS_ANY_SALT PSA_WANT_ALG_RSA_PSS
#endif

#endif /* PSA_CRYPTO_ADJUST_CONFIG_SYNONYMS_H */
