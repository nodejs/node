/**
 * \file mbedtls/config_adjust_test_accelerators.h
 * \brief Declare the transparent test drivers as accelerators
 *
 * This is an internal header for test purposes only. Do not include it directly.
 *
 * The purpose of this header is to keep executing as long as necessary some
 * driver-only related unit test cases when running the test_psa_crypto_drivers
 * all.sh component (namely test cases in test_suite_block_cipher and
 * test_suite_md.psa). It is expected that as the 4.x work progress these test
 * cases will not be necessary anymore and:
 * . test_psa_crypto_drivers scope is restricted to running the
 *   test_suite_psa_crypto_driver_wrappers test suite: test of the dispatch to
 *   drivers and fallbacks.
 * . this file can be removed.
 *
 * This header is used as part of a build containing all the built-in drivers
 * and all the transparent test drivers as wrappers around the built-in
 * drivers. All the built-in drivers and the transparent test drivers are
 * included in the build by starting from a full configuration (config.py full)
 * and defining PSA_CRYPTO_DRIVER_TEST when building
 * (make CFLAGS="-DPSA_CRYPTO_DRIVER_TEST ...").
 *
 * The purpose of this header is to declare the transparent test drivers as
 * accelerators just after infering the built-in drivers
 * (config_adjust_legacy_from_psa.h). Not before the inclusion
 * of config_adjust_legacy_from_psa.h in the build_info.h sequence of header
 * inclusions as this would remove the built-in drivers. Just after to set up
 * properly the internal macros introduced as part of the driver only work
 * (mainly if not only in config_adjust_legacy_crypto.h).
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONFIG_ADJUST_TEST_ACCELERATORS_H
#define MBEDTLS_CONFIG_ADJUST_TEST_ACCELERATORS_H

#if !defined(TF_PSA_CRYPTO_CONFIG_FILES_READ)
#error "Do not include mbedtls/config_adjust_*.h manually! This can lead to problems, " \
    "up to and including runtime errors such as buffer overflows. " \
    "If you're trying to fix a complaint from check_config.h, just remove " \
    "it from your configuration file: since Mbed TLS 3.0, it is included " \
    "automatically at the right point."
#endif

/* Declare the accelerator driver for all cryptographic mechanisms for which
 * the test driver is implemented. This is copied from psa/crypto_config.h
 * with the parts not implemented by the test driver commented out. */
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DERIVE //no-check-names
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_PASSWORD //no-check-names
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_PASSWORD_HASH //no-check-names
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_HMAC //no-check-names
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_AES
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ARIA
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_CAMELLIA
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_CHACHA20
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_GENERATE
//#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_DERIVE
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DH_PUBLIC_KEY
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DH_KEY_PAIR_BASIC
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DH_KEY_PAIR_IMPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DH_KEY_PAIR_EXPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_DH_KEY_PAIR_GENERATE
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RAW_DATA //no-check-names
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_KEY_PAIR_BASIC
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_KEY_PAIR_IMPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_KEY_PAIR_EXPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_KEY_PAIR_GENERATE
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_PUBLIC_KEY

#define MBEDTLS_PSA_ACCEL_ALG_CBC_NO_PADDING
#define MBEDTLS_PSA_ACCEL_ALG_CBC_PKCS7
#define MBEDTLS_PSA_ACCEL_ALG_CCM
#define MBEDTLS_PSA_ACCEL_ALG_CCM_STAR_NO_TAG
#define MBEDTLS_PSA_ACCEL_ALG_CMAC
#define MBEDTLS_PSA_ACCEL_ALG_CFB
#define MBEDTLS_PSA_ACCEL_ALG_CHACHA20_POLY1305
#define MBEDTLS_PSA_ACCEL_ALG_CTR
#define MBEDTLS_PSA_ACCEL_ALG_DETERMINISTIC_ECDSA
#define MBEDTLS_PSA_ACCEL_ALG_ECB_NO_PADDING
#define MBEDTLS_PSA_ACCEL_ALG_ECDH
#define MBEDTLS_PSA_ACCEL_ALG_FFDH
#define MBEDTLS_PSA_ACCEL_ALG_ECDSA
#define MBEDTLS_PSA_ACCEL_ALG_JPAKE
#define MBEDTLS_PSA_ACCEL_ALG_GCM
//#define MBEDTLS_PSA_ACCEL_ALG_HKDF
//#define MBEDTLS_PSA_ACCEL_ALG_HKDF_EXTRACT
//#define MBEDTLS_PSA_ACCEL_ALG_HKDF_EXPAND
#define MBEDTLS_PSA_ACCEL_ALG_HMAC
#define MBEDTLS_PSA_ACCEL_ALG_MD5
#define MBEDTLS_PSA_ACCEL_ALG_OFB
//#define MBEDTLS_PSA_ACCEL_ALG_PBKDF2_HMAC
//#define MBEDTLS_PSA_ACCEL_ALG_PBKDF2_AES_CMAC_PRF_128
#define MBEDTLS_PSA_ACCEL_ALG_RIPEMD160
#define MBEDTLS_PSA_ACCEL_ALG_RSA_OAEP
#define MBEDTLS_PSA_ACCEL_ALG_RSA_PKCS1V15_CRYPT
#define MBEDTLS_PSA_ACCEL_ALG_RSA_PKCS1V15_SIGN
#define MBEDTLS_PSA_ACCEL_ALG_RSA_PSS
#define MBEDTLS_PSA_ACCEL_ALG_SHA_1
#define MBEDTLS_PSA_ACCEL_ALG_SHA_224
#define MBEDTLS_PSA_ACCEL_ALG_SHA_256
#define MBEDTLS_PSA_ACCEL_ALG_SHA_384
#define MBEDTLS_PSA_ACCEL_ALG_SHA_512
#define MBEDTLS_PSA_ACCEL_ALG_SHA3_224
#define MBEDTLS_PSA_ACCEL_ALG_SHA3_256
#define MBEDTLS_PSA_ACCEL_ALG_SHA3_384
#define MBEDTLS_PSA_ACCEL_ALG_SHA3_512
#define MBEDTLS_PSA_ACCEL_ALG_STREAM_CIPHER
//#define MBEDTLS_PSA_ACCEL_ALG_TLS12_PRF
//#define MBEDTLS_PSA_ACCEL_ALG_TLS12_PSK_TO_MS
//#define MBEDTLS_PSA_ACCEL_ALG_TLS12_ECJPAKE_TO_PMS

#endif /* MBEDTLS_CONFIG_ADJUST_TEST_ACCELERATORS_H */
