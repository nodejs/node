/*
 *  PSA MAC layer on top of Mbed TLS software crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_MAC_H
#define PSA_CRYPTO_MAC_H

#include <psa/crypto.h>

/** Calculate the MAC (message authentication code) of a message using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver mac_compute
 *       entry point. This function behaves as a mac_compute entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key to use for
 *                              computing the MAC. This buffer contains the key
 *                              in export representation as defined by
 *                              psa_export_key() (i.e. the raw key bytes).
 * \param key_buffer_size       Size of the \p key_buffer buffer in bytes.
 * \param alg                   The MAC algorithm to use (\c PSA_ALG_XXX value
 *                              such that #PSA_ALG_IS_MAC(\p alg) is true).
 * \param[in] input             Buffer containing the input message.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[out] mac              Buffer where the MAC value is to be written.
 * \param mac_size              Size of the \p mac buffer in bytes.
 * \param[out] mac_length       On success, the number of bytes
 *                              that make up the MAC value.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p mac_size is too small
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_mac_compute(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *input,
    size_t input_length,
    uint8_t *mac,
    size_t mac_size,
    size_t *mac_length);

/** Set up a multipart MAC calculation operation using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver mac_sign_setup
 *       entry point. This function behaves as a mac_sign_setup entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized and not yet in use.
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key to use for
 *                              computing the MAC. This buffer contains the key
 *                              in export representation as defined by
 *                              psa_export_key() (i.e. the raw key bytes).
 * \param key_buffer_size       Size of the \p key_buffer buffer in bytes.
 * \param alg                   The MAC algorithm to use (\c PSA_ALG_XXX value
 *                              such that #PSA_ALG_IS_MAC(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive).
 */
psa_status_t mbedtls_psa_mac_sign_setup(
    mbedtls_psa_mac_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg);

/** Set up a multipart MAC verification operation using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver mac_verify_setup
 *       entry point. This function behaves as a mac_verify_setup entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized and not yet in use.
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key to use for
 *                              computing the MAC. This buffer contains the key
 *                              in export representation as defined by
 *                              psa_export_key() (i.e. the raw key bytes).
 * \param key_buffer_size       Size of the \p key_buffer buffer in bytes.
 * \param alg                   The MAC algorithm to use (\c PSA_ALG_XXX value
 *                              such that #PSA_ALG_IS_MAC(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive).
 */
psa_status_t mbedtls_psa_mac_verify_setup(
    mbedtls_psa_mac_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg);

/** Add a message fragment to a multipart MAC operation using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver mac_update
 *       entry point. This function behaves as a mac_update entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * The PSA core calls mbedtls_psa_mac_sign_setup() or
 * mbedtls_psa_mac_verify_setup() before calling this function.
 *
 * If this function returns an error status, the PSA core aborts the
 * operation by calling mbedtls_psa_mac_abort().
 *
 * \param[in,out] operation Active MAC operation.
 * \param[in] input         Buffer containing the message fragment to add to
 *                          the MAC calculation.
 * \param input_length      Size of the \p input buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active).
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_mac_update(
    mbedtls_psa_mac_operation_t *operation,
    const uint8_t *input,
    size_t input_length);

/** Finish the calculation of the MAC of a message using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver mac_sign_finish
 *       entry point. This function behaves as a mac_sign_finish entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * The PSA core calls mbedtls_psa_mac_sign_setup() before calling this function.
 * This function calculates the MAC of the message formed by concatenating
 * the inputs passed to preceding calls to mbedtls_psa_mac_update().
 *
 * Whether this function returns successfully or not, the PSA core subsequently
 * aborts the operation by calling mbedtls_psa_mac_abort().
 *
 * \param[in,out] operation Active MAC operation.
 * \param[out] mac          Buffer where the MAC value is to be written.
 * \param mac_size          Output size requested for the MAC algorithm. The PSA
 *                          core guarantees this is a valid MAC length for the
 *                          algorithm and key combination passed to
 *                          mbedtls_psa_mac_sign_setup(). It also guarantees the
 *                          \p mac buffer is large enough to contain the
 *                          requested output size.
 * \param[out] mac_length   On success, the number of bytes output to buffer
 *                          \p mac, which will be equal to the requested length
 *                          \p mac_size.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active mac sign
 *         operation).
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p mac buffer is too small. A sufficient buffer size
 *         can be determined by calling PSA_MAC_LENGTH().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_mac_sign_finish(
    mbedtls_psa_mac_operation_t *operation,
    uint8_t *mac,
    size_t mac_size,
    size_t *mac_length);

/** Finish the calculation of the MAC of a message and compare it with
 * an expected value using Mbed TLS.
 *
 * \note The signature of this function is that of a PSA driver
 *       mac_verify_finish entry point. This function behaves as a
 *       mac_verify_finish entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * The PSA core calls mbedtls_psa_mac_verify_setup() before calling this
 * function. This function calculates the MAC of the message formed by
 * concatenating the inputs passed to preceding calls to
 * mbedtls_psa_mac_update(). It then compares the calculated MAC with the
 * expected MAC passed as a parameter to this function.
 *
 * Whether this function returns successfully or not, the PSA core subsequently
 * aborts the operation by calling mbedtls_psa_mac_abort().
 *
 * \param[in,out] operation Active MAC operation.
 * \param[in] mac           Buffer containing the expected MAC value.
 * \param mac_length        Length in bytes of the expected MAC value. The PSA
 *                          core guarantees that this length is a valid MAC
 *                          length for the algorithm and key combination passed
 *                          to mbedtls_psa_mac_verify_setup().
 *
 * \retval #PSA_SUCCESS
 *         The expected MAC is identical to the actual MAC of the message.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The MAC of the message was calculated successfully, but it
 *         differs from the expected MAC.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active mac verify
 *         operation).
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_mac_verify_finish(
    mbedtls_psa_mac_operation_t *operation,
    const uint8_t *mac,
    size_t mac_length);

/** Abort a MAC operation using Mbed TLS.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation by calling
 * mbedtls_psa_mac_sign_setup() or mbedtls_psa_mac_verify_setup() again.
 *
 * The PSA core may call this function any time after the operation object has
 * been initialized by one of the methods described in
 * #mbedtls_psa_mac_operation_t.
 *
 * In particular, calling mbedtls_psa_mac_abort() after the operation has been
 * terminated by a call to mbedtls_psa_mac_abort(),
 * mbedtls_psa_mac_sign_finish() or mbedtls_psa_mac_verify_finish() is safe and
 * has no effect.
 *
 * \param[in,out] operation Initialized MAC operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_mac_abort(
    mbedtls_psa_mac_operation_t *operation);

#endif /* PSA_CRYPTO_MAC_H */
