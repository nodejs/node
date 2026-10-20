/**
 * \file mbedtls/config_psa.h
 * \brief PSA crypto configuration options (set of defines)
 *
 *  This set of compile-time options takes settings defined in
 *  include/mbedtls/mbedtls_config.h and include/psa/crypto_config.h and uses
 *  those definitions to define symbols used in the library code.
 *
 *  Users and integrators should not edit this file, please edit
 *  include/mbedtls/mbedtls_config.h for MBEDTLS_XXX settings or
 *  include/psa/crypto_config.h for PSA_WANT_XXX settings.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CONFIG_PSA_H
#define MBEDTLS_CONFIG_PSA_H

#include "psa/crypto_adjust_config_synonyms.h"

#include "psa/crypto_adjust_config_dependencies.h"

/* Require built-in implementations based on PSA requirements */

/* We need this to have a complete list of requirements
 * before we deduce what built-ins are required. */
#include "psa/crypto_adjust_config_key_pair_types.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)
/* If we are implementing PSA crypto ourselves, then we want to enable the
 * required built-ins. Otherwise, PSA features will be provided by the server. */
#include "mbedtls/private/config_adjust_legacy_from_psa.h"
#if defined(MBEDTLS_CONFIG_ADJUST_TEST_ACCELERATORS) //no-check-names
#include "mbedtls/private/config_adjust_test_accelerators.h"
#endif
#endif /* MBEDTLS_PSA_CRYPTO_C */

#include "psa/crypto_adjust_config_derived.h"

#include "psa/crypto_adjust_auto_enabled.h"

#endif /* MBEDTLS_CONFIG_PSA_H */
