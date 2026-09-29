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

#include "tf-psa-crypto/build_info.h"

/*
 * This set of compile-time defines can be used to determine the version number
 * of the Mbed TLS library used. Run-time variables for the same can be found in
 * version.h
 */

/**
 * The version number x.y.z is split into three parts.
 * Major, Minor, Patchlevel
 */
#define MBEDTLS_VERSION_MAJOR  4
#define MBEDTLS_VERSION_MINOR  0
#define MBEDTLS_VERSION_PATCH  0

/**
 * The single version number has the following structure:
 *    MMNNPP00
 *    Major version | Minor version | Patch version
 */
#define MBEDTLS_VERSION_NUMBER         0x04000000
#define MBEDTLS_VERSION_STRING         "4.0.0"
#define MBEDTLS_VERSION_STRING_FULL    "Mbed TLS 4.0.0"

#if defined(MBEDTLS_CONFIG_FILES_READ)
#error "Something went wrong: MBEDTLS_CONFIG_FILES_READ defined before reading the config files!"
#endif
#if defined(MBEDTLS_CONFIG_IS_FINALIZED)
#error "Something went wrong: MBEDTLS_CONFIG_IS_FINALIZED defined before reading the config files!"
#endif

/* X.509 and TLS configuration */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_CONFIG_VERSION) && ( \
    MBEDTLS_CONFIG_VERSION < 0x04000000 || \
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

/* For the sake of consistency checks in mbedtls_config.c */
#if defined(MBEDTLS_INCLUDE_AFTER_RAW_CONFIG)
#include MBEDTLS_INCLUDE_AFTER_RAW_CONFIG
#endif

/* Indicate that all configuration files have been read.
 * It is now time to adjust the configuration (follow through on dependencies,
 * make PSA and legacy crypto consistent, etc.).
 */
#define MBEDTLS_CONFIG_FILES_READ

#include "mbedtls/private/config_adjust_x509.h"

#include "mbedtls/private/config_adjust_ssl.h"

/* Indicate that all configuration symbols are set,
 * even the ones that are calculated programmatically.
 * It is now safe to query the configuration (to check it, to size buffers,
 * etc.).
 */
#define MBEDTLS_CONFIG_IS_FINALIZED

#endif /* MBEDTLS_BUILD_INFO_H */
