/**
 * \file version.h
 *
 * \brief Run-time version information
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 * This set of run-time variables can be used to determine the version number of
 * the Mbed TLS library used. Compile-time version defines for the same can be
 * found in build_info.h
 */
#ifndef MBEDTLS_VERSION_H
#define MBEDTLS_VERSION_H

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_VERSION_C)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the version number.
 *
 * \return          The constructed version number in the format
 *                  MMNNPP00 (Major, Minor, Patch).
 */
unsigned int mbedtls_version_get_number(void);

/**
 * Get the version string ("x.y.z").
 *
 * \param string    The string that will receive the value.
 *                  (Should be at least 9 bytes in size)
 */
void mbedtls_version_get_string(char *string);

/**
 * Get the full version string ("Mbed TLS x.y.z").
 *
 * \param string    The string that will receive the value. The Mbed TLS version
 *                  string will use 18 bytes AT MOST including a terminating
 *                  null byte.
 *                  (So the buffer should be at least 18 bytes to receive this
 *                  version string).
 */
void mbedtls_version_get_string_full(char *string);

/**
 * \brief           Check if support for a feature was compiled into this
 *                  Mbed TLS binary. This allows you to see at runtime if the
 *                  library was for instance compiled with or without
 *                  Multi-threading support.
 *
 * \note            only checks against defines in the sections "System
 *                  support", "Mbed TLS modules" and "Mbed TLS feature
 *                  support" in mbedtls_config.h
 *
 * \param feature   The string for the define to check (e.g. "MBEDTLS_AES_C")
 *
 * \return          0 if the feature is present,
 *                  -1 if the feature is not present and
 *                  -2 if support for feature checking as a whole was not
 *                  compiled in.
 */
int mbedtls_version_check_feature(const char *feature);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_VERSION_C */

#endif /* version.h */
