/*
 *  TF-PSA-Crypto configuration checks
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/* Detect whether this is a normal build of the library, or a build of
 * libtestdriver1, where all identifiers starting with normal prefixes
 * have LIBTESTDRIVER1_ prepended. Do this without relying on
 * any headers, since this has to happen before we include
 * tf_psa_crypto/build_info.h.
 *
 * In the libtestdriver1 build, the next two code lines are
 * `#define LIBTESTDRIVER_FOO 1` and
 * `#if LIBTESTDRIVER_FOO == LIBTESTDRIVER_FOO` so the conditional is true.
 * In a normal build, `LIBTESTDRIVER1_TF_PSA_CRYPTO_MARKER` remains
 * undefined so the conditional is false.
 */
#define TF_PSA_CRYPTO_MARKER 1
#if LIBTESTDRIVER1_TF_PSA_CRYPTO_MARKER == TF_PSA_CRYPTO_MARKER
#define TF_PSA_CRYPTO_WE_ARE_IN_LIBTESTDRIVER1
#endif
#undef TF_PSA_CRYPTO_MARKER

/* Completely byass generated config checks in libtestdriver1, where
 * they aren't useful (when building test drivers, we can bypass
 * normal configuration mechanisms if we want).
 * This way we don't have to make them work. Since we bypass the
 * header inclusions, the build system doesn't even need to know how
 * to generate files with the right names and in the right locations.
 */
#if !defined(TF_PSA_CRYPTO_WE_ARE_IN_LIBTESTDRIVER1)
/* Consistency checks on the user's configuration.
 * Check that it doesn't define macros that we assume are under full
 * control of the library, or options from past major versions that
 * no longer have any effect.
 * These headers are automatically generated. See
 * framework/scripts/mbedtls_framework/config_checks_generator.py
 *
 * This here is the first stage, before including the user config.
 */
#include "tf_psa_crypto_config_check_before.h"
/* The second stage, after including the user config but before doing
 * any subsequent adjustment, will be included by build_info.h. */
#define TF_PSA_CRYPTO_INCLUDE_AFTER_RAW_CONFIG "tf_psa_crypto_config_check_user.h"
#endif /* !defined(TF_PSA_CRYPTO_WE_ARE_IN_LIBTESTDRIVER1) */

#include <tf-psa-crypto/build_info.h>

/* Consistency checks in the configuration: check for incompatible options,
 * missing options when at least one of a set needs to be enabled, etc. */
/* Manually written checks */
#include "tf_psa_crypto_check_config.h"

#if !defined(TF_PSA_CRYPTO_WE_ARE_IN_LIBTESTDRIVER1)
/* Automatically generated checks (final stage after config adjustment) */
#include "tf_psa_crypto_config_check_final.h"
#endif /* !defined(TF_PSA_CRYPTO_WE_ARE_IN_LIBTESTDRIVER1) */

/* For MBEDTLS_STATIC_ASSERT */
#include "tf_psa_crypto_common.h"
/* For PSA_HASH_LENGTH */
#include <psa/crypto_sizes.h>

/* Additional domain-specific checks */
#include "psa_crypto_random_impl.h"
