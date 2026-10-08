/**
 * \file tf-psa-crypto/build_info.h
 *
 * \brief Build-time configuration info
 *
 *  Include this file if you need to depend on the
 *  configuration options defined in crypto_config.h or TF_PSA_CRYPTO_CONFIG_FILE.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef TF_PSA_CRYPTO_BUILD_INFO_H
#define TF_PSA_CRYPTO_BUILD_INFO_H

/*
 * Version macros are defined in build_info.h rather than in version.h so that
 * the user config files have access to them. That way, for example, users who
 * deploy applications to multiple devices with different versions of
 * TF-PSA-Crypto can write configurations that depend on the version.
 */
/**
 * The version number x.y.z is split into three parts.
 * Major, Minor, Patchlevel
 */
#define TF_PSA_CRYPTO_VERSION_MAJOR  1
#define TF_PSA_CRYPTO_VERSION_MINOR  0
#define TF_PSA_CRYPTO_VERSION_PATCH  0

/**
 * The single version number has the following structure:
 *    MMNNPP00
 *    Major version | Minor version | Patch version
 */
#define TF_PSA_CRYPTO_VERSION_NUMBER        0x01000000
#define TF_PSA_CRYPTO_VERSION_STRING        "1.0.0"
#define TF_PSA_CRYPTO_VERSION_STRING_FULL   "TF-PSA-Crypto 1.0.0"

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

#if defined(TF_PSA_CRYPTO_CONFIG_FILES_READ)
#error \
    "Something went wrong: TF_PSA_CRYPTO_CONFIG_FILES_READ defined before reading the config files!"
#endif
#if defined(TF_PSA_CRYPTO_CONFIG_IS_FINALIZED)
#error \
    "Something went wrong: TF_PSA_CRYPTO_CONFIG_IS_FINALIZED defined before reading the config files!"
#endif

/* PSA crypto configuration */
#if defined(TF_PSA_CRYPTO_CONFIG_FILE)
#include TF_PSA_CRYPTO_CONFIG_FILE
#else
#include "psa/crypto_config.h"
#endif
#if defined(TF_PSA_CRYPTO_USER_CONFIG_FILE)
#include TF_PSA_CRYPTO_USER_CONFIG_FILE
#endif

/* For the sake of consistency checks in tf_psa_crypto_config.c */
#if defined(TF_PSA_CRYPTO_INCLUDE_AFTER_RAW_CONFIG)
#include TF_PSA_CRYPTO_INCLUDE_AFTER_RAW_CONFIG
#endif

/* Indicate that all configuration files have been read.
 * It is now time to adjust the configuration (follow through on dependencies,
 * make PSA and legacy crypto consistent, etc.).
 */
#define TF_PSA_CRYPTO_CONFIG_FILES_READ

#if defined(TF_PSA_CRYPTO_CONFIG_VERSION)
#if (TF_PSA_CRYPTO_CONFIG_VERSION < 0x01000000) ||                      \
    (TF_PSA_CRYPTO_CONFIG_VERSION > TF_PSA_CRYPTO_VERSION_NUMBER)
#error "Invalid config version, defined value of TF_PSA_CRYPTO_CONFIG_VERSION is unsupported"
#endif
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
#include "mbedtls/private/config_psa.h"

#include "mbedtls/config_adjust_legacy_crypto.h"

/* Indicate that all configuration symbols are set,
 * even the ones that are calculated programmatically.
 * It is now safe to query the configuration (to check it, to size buffers,
 * etc.).
 */
#define TF_PSA_CRYPTO_CONFIG_IS_FINALIZED

/* Up to Mbed TLS 3.6, build_info.h included check_config.h which included
 * <limits.h>. Keep including it until it's explicitly included everywhere
 * that uses definitions from <limits.h>.
 * https://github.com/Mbed-TLS/mbedtls/issues/10305
 */
#include <limits.h>

/*
 * Avoid warning from -pedantic. This is a convenient place for this
 * workaround since this is included by every single file before the
 * #if defined(MBEDTLS_xxx_C) that results in empty translation units.
 */
typedef int mbedtls_iso_c_forbids_empty_translation_units;

#endif /* TF_PSA_CRYPTO_BUILD_INFO_H */
