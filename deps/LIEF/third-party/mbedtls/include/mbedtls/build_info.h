/**
 * \file mbedtls/build_info.h
 *
 * \brief Build-time configuration info
 *
 *  Include this file if you need to depend on the
 *  configuration options defined in mbedtls_config.h or MBEDTLS_CONFIG_FILE
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BUILD_INFO_H
#define MBEDTLS_BUILD_INFO_H

/*
 * This set of compile-time defines can be used to determine the version number
 * of the Mbed TLS library used. Run-time variables for the same can be found in
 * version.h
 */

/**
 * The version number x.y.z is split into three parts.
 * Major, Minor, Patchlevel
 */
#define MBEDTLS_VERSION_MAJOR  3
#define MBEDTLS_VERSION_MINOR  6
#define MBEDTLS_VERSION_PATCH  4

/**
 * The single version number has the following structure:
 *    MMNNPP00
 *    Major version | Minor version | Patch version
 */
#define MBEDTLS_VERSION_NUMBER         0x03060400
#define MBEDTLS_VERSION_STRING         "3.6.4"
#define MBEDTLS_VERSION_STRING_FULL    "Mbed TLS 3.6.4"

/* Macros for build-time platform detection */

#if !defined(MBEDTLS_ARCH_IS_ARM64) && \
    (defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC))
#define MBEDTLS_ARCH_IS_ARM64
#endif

#if !defined(MBEDTLS_ARCH_IS_ARM32) && \
    (defined(__arm__) || defined(_M_ARM) || \
    defined(_M_ARMT) || defined(__thumb__) || defined(__thumb2__))
#define MBEDTLS_ARCH_IS_ARM32
#endif

#if !defined(MBEDTLS_ARCH_IS_X64) && \
    (defined(__amd64__) || defined(__x86_64__) || \
    ((defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64EC)))
#define MBEDTLS_ARCH_IS_X64
#endif

#if !defined(MBEDTLS_ARCH_IS_X86) && \
    (defined(__i386__) || defined(_X86_) || \
    (defined(_M_IX86) && !defined(_M_I86)))
#define MBEDTLS_ARCH_IS_X86
#endif

#if !defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64) && \
    (defined(_M_ARM64) || defined(_M_ARM64EC))
#define MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64
#endif

/* This is defined if the architecture is Armv8-A, or higher */
#if !defined(MBEDTLS_ARCH_IS_ARMV8_A)
#if defined(__ARM_ARCH) && defined(__ARM_ARCH_PROFILE)
#if (__ARM_ARCH >= 8) && (__ARM_ARCH_PROFILE == 'A')
/* GCC, clang, armclang and IAR */
#define MBEDTLS_ARCH_IS_ARMV8_A
#endif
#elif defined(__ARM_ARCH_8A)
/* Alternative defined by clang */
#define MBEDTLS_ARCH_IS_ARMV8_A
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
/* MSVC ARM64 is at least Armv8.0-A */
#define MBEDTLS_ARCH_IS_ARMV8_A
#endif
#endif

#if defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__clang__) \
    && !defined(__llvm__) && !defined(__INTEL_COMPILER)
/* Defined if the compiler really is gcc and not clang, etc */
#define MBEDTLS_COMPILER_IS_GCC
#define MBEDTLS_GCC_VERSION \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

/* Define `inline` on some non-C99-compliant compilers. */
#if (defined(__ARMCC_VERSION) || defined(_MSC_VER)) && \
    !defined(inline) && !defined(__cplusplus)
#define inline __inline
#endif

#if defined(MBEDTLS_CONFIG_FILES_READ)
#error "Something went wrong: MBEDTLS_CONFIG_FILES_READ defined before reading the config files!"
#endif
#if defined(MBEDTLS_CONFIG_IS_FINALIZED)
#error "Something went wrong: MBEDTLS_CONFIG_IS_FINALIZED defined before reading the config files!"
#endif

/* X.509, TLS and non-PSA crypto configuration */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_CONFIG_VERSION) && ( \
    MBEDTLS_CONFIG_VERSION < 0x03000000 || \
                             MBEDTLS_CONFIG_VERSION > MBEDTLS_VERSION_NUMBER)
#error "Invalid config version, defined value of MBEDTLS_CONFIG_VERSION is unsupported"
#endif

/* Target and application specific configurations
 *
 * Allow user to override any previous default.
 *
 */
#if defined(MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

/* PSA crypto configuration */
#if defined(MBEDTLS_PSA_CRYPTO_CONFIG)
#if defined(MBEDTLS_PSA_CRYPTO_CONFIG_FILE)
#include MBEDTLS_PSA_CRYPTO_CONFIG_FILE
#else
#include "psa/crypto_config.h"
#endif
#if defined(MBEDTLS_PSA_CRYPTO_USER_CONFIG_FILE)
#include MBEDTLS_PSA_CRYPTO_USER_CONFIG_FILE
#endif
#endif /* defined(MBEDTLS_PSA_CRYPTO_CONFIG) */

/* Indicate that all configuration files have been read.
 * It is now time to adjust the configuration (follow through on dependencies,
 * make PSA and legacy crypto consistent, etc.).
 */
#define MBEDTLS_CONFIG_FILES_READ

/* Auto-enable MBEDTLS_CTR_DRBG_USE_128_BIT_KEY if
 * MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH and MBEDTLS_CTR_DRBG_C defined
 * to ensure a 128-bit key size in CTR_DRBG.
 */
#if defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH) && defined(MBEDTLS_CTR_DRBG_C)
#define MBEDTLS_CTR_DRBG_USE_128_BIT_KEY
#endif

/* Auto-enable MBEDTLS_MD_C if needed by a module that didn't require it
 * in a previous release, to ensure backwards compatibility.
 */
#if defined(MBEDTLS_PKCS5_C)
#define MBEDTLS_MD_C
#endif

/* PSA crypto specific configuration options
 * - If config_psa.h reads a configuration option in preprocessor directive,
 *   this symbol should be set before its inclusion. (e.g. MBEDTLS_MD_C)
 * - If config_psa.h writes a configuration option in conditional directive,
 *   this symbol should be consulted after its inclusion.
 *   (e.g. MBEDTLS_MD_LIGHT)
 */
#if defined(MBEDTLS_PSA_CRYPTO_CONFIG) /* PSA_WANT_xxx influences MBEDTLS_xxx */ || \
    defined(MBEDTLS_PSA_CRYPTO_C) /* MBEDTLS_xxx influences PSA_WANT_xxx */ || \
    defined(MBEDTLS_PSA_CRYPTO_CLIENT) /* The same as the previous, but with separation only */
#include "mbedtls/config_psa.h"
#endif

#include "mbedtls/config_adjust_legacy_crypto.h"

#include "mbedtls/config_adjust_x509.h"

#include "mbedtls/config_adjust_ssl.h"

/* Indicate that all configuration symbols are set,
 * even the ones that are calculated programmatically.
 * It is now safe to query the configuration (to check it, to size buffers,
 * etc.).
 */
#define MBEDTLS_CONFIG_IS_FINALIZED

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_BUILD_INFO_H */
