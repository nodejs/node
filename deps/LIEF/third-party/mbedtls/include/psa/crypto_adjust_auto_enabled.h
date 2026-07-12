/**
 * \file psa/crypto_adjust_auto_enabled.h
 * \brief Adjust PSA configuration: enable always-on features
 *
 * This is an internal header. Do not include it directly.
 *
 * Always enable certain features which require a negligible amount of code
 * to implement, to avoid some edge cases in the configuration combinatorics.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_ADJUST_AUTO_ENABLED_H
#define PSA_CRYPTO_ADJUST_AUTO_ENABLED_H

#if !defined(MBEDTLS_CONFIG_FILES_READ)
#error "Do not include psa/crypto_adjust_*.h manually! This can lead to problems, " \
    "up to and including runtime errors such as buffer overflows. " \
    "If you're trying to fix a complaint from check_config.h, just remove " \
    "it from your configuration file: since Mbed TLS 3.0, it is included " \
    "automatically at the right point."
#endif /* */

#define PSA_WANT_KEY_TYPE_DERIVE 1
#define PSA_WANT_KEY_TYPE_PASSWORD 1
#define PSA_WANT_KEY_TYPE_PASSWORD_HASH 1
#define PSA_WANT_KEY_TYPE_RAW_DATA 1

#endif /* PSA_CRYPTO_ADJUST_AUTO_ENABLED_H */
