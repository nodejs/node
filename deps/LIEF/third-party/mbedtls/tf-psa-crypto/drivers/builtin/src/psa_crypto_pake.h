/*
 *  PSA PAKE layer on top of Mbed TLS software crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_PAKE_H
#define PSA_CRYPTO_PAKE_H

#include <psa/crypto.h>

/** Set the session information for a password-authenticated key exchange.
 *
 * \note The signature of this function is that of a PSA driver
 *       pake_setup entry point. This function behaves as a pake_setup
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized but not set up yet.
 * \param[in] inputs            Inputs required for PAKE operation (role, password,
 *                              key lifetime, cipher suite)
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The algorithm in \p cipher_suite is not a supported PAKE algorithm,
 *         or the PAKE primitive in \p cipher_suite is not supported or not
 *         compatible with the PAKE algorithm, or the hash algorithm in
 *         \p cipher_suite is not supported or not compatible with the PAKE
 *         algorithm and primitive.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 */
psa_status_t mbedtls_psa_pake_setup(mbedtls_psa_pake_operation_t *operation,
                                    const psa_crypto_driver_pake_inputs_t *inputs);


/** Get output for a step of a password-authenticated key exchange.
 *
 * \note The signature of this function is that of a PSA driver
 *       pake_output entry point. This function behaves as a pake_output
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in,out] operation    Active PAKE operation.
 * \param step                 The step of the algorithm for which the output is
 *                             requested.
 * \param[out] output          Buffer where the output is to be written in the
 *                             format appropriate for this driver \p step. Refer to
 *                             the documentation of psa_crypto_driver_pake_step_t for
 *                             more information.
 * \param output_size          Size of the \p output buffer in bytes. This must
 *                             be at least #PSA_PAKE_OUTPUT_SIZE(\p alg, \p
 *                             primitive, \p step) where \p alg and
 *                             \p primitive are the PAKE algorithm and primitive
 *                             in the operation's cipher suite, and \p step is
 *                             the output step.
 *
 * \param[out] output_length   On success, the number of bytes of the returned
 *                             output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY  \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT          \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID          \emptydescription
 */
psa_status_t mbedtls_psa_pake_output(mbedtls_psa_pake_operation_t *operation,
                                     psa_crypto_driver_pake_step_t step,
                                     uint8_t *output,
                                     size_t output_size,
                                     size_t *output_length);

/** Provide input for a step of a password-authenticated key exchange.
 *
 * \note The signature of this function is that of a PSA driver
 *       pake_input entry point. This function behaves as a pake_input
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \note The core checks that input_length is smaller than PSA_PAKE_INPUT_MAX_SIZE.
 *
 * \param[in,out] operation    Active PAKE operation.
 * \param step                 The driver step for which the input is provided.
 * \param[in] input            Buffer containing the input in the format
 *                             appropriate for this \p step. Refer to the
 *                             documentation of psa_crypto_driver_pake_step_t
 *                             for more information.
 * \param input_length         Size of the \p input buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The verification fails for a zero-knowledge input step.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         the \p input is not valid for the \p operation's algorithm, cipher suite
 *         or \p step.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         the \p input is not supported for the \p operation's algorithm, cipher
 *         suite or \p step.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT          \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID          \emptydescription
 */
psa_status_t mbedtls_psa_pake_input(mbedtls_psa_pake_operation_t *operation,
                                    psa_crypto_driver_pake_step_t step,
                                    const uint8_t *input,
                                    size_t input_length);

/** Get implicitly confirmed shared secret from a PAKE.
 *
 * \note The signature of this function is that of a PSA driver
 *       pake_get_implicit_key entry point. This function behaves as a
 *       pake_get_implicit_key entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation    Active PAKE operation.
 * \param[out] output          Output buffer for implicit key.
 * \param      output_size     Size of the output buffer in bytes.
 * \param[out] output_length   On success, the number of bytes of the implicit key.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         Input from a PAKE is not supported by the algorithm in the \p output
 *         key derivation operation.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT          \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID          \emptydescription
 */
psa_status_t mbedtls_psa_pake_get_implicit_key(
    mbedtls_psa_pake_operation_t *operation,
    uint8_t *output, size_t output_size,
    size_t *output_length);

/** Abort a PAKE operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       pake_abort entry point. This function behaves as a pake_abort
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in,out] operation    The operation to abort.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 */
psa_status_t mbedtls_psa_pake_abort(mbedtls_psa_pake_operation_t *operation);

#endif /* PSA_CRYPTO_PAKE_H */
