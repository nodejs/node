/*
 *  Mbed TLS configuration checks
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/* Apply the TF-PSA-Crypto configuration first. We need to do this
 * before <mbedtls/build_info.h>, because "mbedtls_config_check_before.h"
 * needs to run after the crypto config (including derived macros) is
 * finalized, but before the user's mbedtls config is applied. This way
 * it is possible to differentiate macros set by the user's mbedtls config
 * from macros set or derived by the crypto config. */
#include <tf-psa-crypto/build_info.h>

/* Consistency checks on the user's configuration.
 * Check that it doesn't define macros that we assume are under full
 * control of the library, or options from past major versions that
 * no longer have any effect.
 * These headers are automatically generated. See
 * framework/scripts/mbedtls_framework/config_checks_generator.py
 */
#include "mbedtls_config_check_before.h"
#define MBEDTLS_INCLUDE_AFTER_RAW_CONFIG "mbedtls_config_check_user.h"

#include <mbedtls/build_info.h>

/* Consistency checks in the configuration: check for incompatible options,
 * missing options when at least one of a set needs to be enabled, etc. */
/* Manually written checks */
#include "mbedtls_check_config.h"
/* Automatically generated checks */
#include "mbedtls_config_check_final.h"
