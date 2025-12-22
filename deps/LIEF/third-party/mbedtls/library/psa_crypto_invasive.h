/**
 * \file psa_crypto_invasive.h
 *
 * \brief PSA cryptography module: invasive interfaces for test only.
 *
 * The interfaces in this file are intended for testing purposes only.
 * They MUST NOT be made available to clients over IPC in integrations
 * with isolation, and they SHOULD NOT be made available in library
 * integrations except when building the library for testing.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_INVASIVE_H
#define PSA_CRYPTO_INVASIVE_H

/*
 * Include the build-time configuration information header. Here, we do not
 * include `"mbedtls/build_info.h"` directly but `"psa/build_info.h"`, which
 * is basically just an alias to it. This is to ease the maintenance of the
 * TF-PSA-Crypto repository which has a different build system and
 * configuration.
 */
#include "psa/build_info.h"

#include "psa/crypto.h"
#include "common.h"

#include "mbedtls/entropy.h"

#if !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
/** \brief Configure entropy sources.
 *
 * This function may only be called before a call to psa_crypto_init(),
 * or after a call to mbedtls_psa_crypto_free() and before any
 * subsequent call to psa_crypto_init().
 *
 * This function is only intended for test purposes. The functionality
 * it provides is also useful for system integrators, but
 * system integrators should configure entropy drivers instead of
 * breaking through to the Mbed TLS API.
 *
 * \param entropy_init  Function to initialize the entropy context
 *                      and set up the desired entropy sources.
 *                      It is called by psa_crypto_init().
 *                      By default this is mbedtls_entropy_init().
 *                      This function cannot report failures directly.
 *                      To indicate a failure, set the entropy context
 *                      to a state where mbedtls_entropy_func() will
 *                      return an error.
 * \param entropy_free  Function to free the entropy context
 *                      and associated resources.
 *                      It is called by mbedtls_psa_crypto_free().
 *                      By default this is mbedtls_entropy_free().
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The caller does not have the permission to configure
 *         entropy sources.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has already been initialized.
 */
psa_status_t mbedtls_psa_crypto_configure_entropy_sources(
    void (* entropy_init)(mbedtls_entropy_context *ctx),
    void (* entropy_free)(mbedtls_entropy_context *ctx));
#endif /* !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) */

#if defined(MBEDTLS_TEST_HOOKS) && defined(MBEDTLS_PSA_CRYPTO_C)
psa_status_t psa_mac_key_can_do(
    psa_algorithm_t algorithm,
    psa_key_type_t key_type);

psa_status_t psa_crypto_copy_input(const uint8_t *input, size_t input_len,
                                   uint8_t *input_copy, size_t input_copy_len);

psa_status_t psa_crypto_copy_output(const uint8_t *output_copy, size_t output_copy_len,
                                    uint8_t *output, size_t output_len);

/*
 * Test hooks to use for memory unpoisoning/poisoning in copy functions.
 */
extern void (*psa_input_pre_copy_hook)(const uint8_t *input, size_t input_len);
extern void (*psa_input_post_copy_hook)(const uint8_t *input, size_t input_len);
extern void (*psa_output_pre_copy_hook)(const uint8_t *output, size_t output_len);
extern void (*psa_output_post_copy_hook)(const uint8_t *output, size_t output_len);

#endif /* MBEDTLS_TEST_HOOKS && MBEDTLS_PSA_CRYPTO_C */

#endif /* PSA_CRYPTO_INVASIVE_H */
