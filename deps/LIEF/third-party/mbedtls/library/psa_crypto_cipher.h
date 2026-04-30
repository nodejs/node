/*
 *  PSA cipher driver entry points and associated auxiliary functions
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_CIPHER_H
#define PSA_CRYPTO_CIPHER_H

#include <mbedtls/cipher.h>
#include <psa/crypto.h>

/** Get Mbed TLS cipher information given the cipher algorithm PSA identifier
 *  as well as the PSA type and size of the key to be used with the cipher
 *  algorithm.
 *
 * \param[in]      alg          PSA cipher algorithm identifier
 * \param[in]      key_type     PSA key type
 * \param[in,out]  key_bits     Size of the key in bits. The value provided in input
 *                              might be updated if necessary.
 * \param[out]     mode         Mbed TLS cipher mode
 * \param[out]     cipher_id    Mbed TLS cipher algorithm identifier
 *
 * \return  On success \c PSA_SUCCESS is returned and key_bits, mode and cipher_id
 *          are properly updated.
 *          \c PSA_ERROR_NOT_SUPPORTED is returned if the cipher algorithm is not
 *          supported.
 */

psa_status_t mbedtls_cipher_values_from_psa(psa_algorithm_t alg, psa_key_type_t key_type,
                                            size_t *key_bits, mbedtls_cipher_mode_t *mode,
                                            mbedtls_cipher_id_t *cipher_id);

#if defined(MBEDTLS_CIPHER_C)
/** Get Mbed TLS cipher information given the cipher algorithm PSA identifier
 *  as well as the PSA type and size of the key to be used with the cipher
 *  algorithm.
 *
 * \param       alg        PSA cipher algorithm identifier
 * \param       key_type   PSA key type
 * \param       key_bits   Size of the key in bits
 * \param[out]  cipher_id  Mbed TLS cipher algorithm identifier
 *
 * \return  The Mbed TLS cipher information of the cipher algorithm.
 *          \c NULL if the PSA cipher algorithm is not supported.
 */
const mbedtls_cipher_info_t *mbedtls_cipher_info_from_psa(
    psa_algorithm_t alg, psa_key_type_t key_type, size_t key_bits,
    mbedtls_cipher_id_t *cipher_id);
#endif /* MBEDTLS_CIPHER_C */

/**
 * \brief Set the key for a multipart symmetric encryption operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_encrypt_setup entry point. This function behaves as a
 *       cipher_encrypt_setup entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     The operation object to set up. It has been
 *                              initialized as per the documentation for
 *                              #psa_cipher_operation_t and not yet in use.
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key context.
 * \param[in] key_buffer_size   Size of the \p key_buffer buffer in bytes.
 * \param[in] alg               The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_cipher_encrypt_setup(
    mbedtls_psa_cipher_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg);

/**
 * \brief Set the key for a multipart symmetric decryption operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_decrypt_setup entry point. This function behaves as a
 *       cipher_decrypt_setup entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     The operation object to set up. It has been
 *                              initialized as per the documentation for
 *                              #psa_cipher_operation_t and not yet in use.
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key context.
 * \param[in] key_buffer_size   Size of the \p key_buffer buffer in bytes.
 * \param[in] alg               The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_cipher_decrypt_setup(
    mbedtls_psa_cipher_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg);

/** Set the IV for a symmetric encryption or decryption operation.
 *
 * This function sets the IV (initialization vector), nonce
 * or initial counter value for the encryption or decryption operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_set_iv entry point. This function behaves as a
 *       cipher_set_iv entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[in] iv                Buffer containing the IV to use.
 * \param[in] iv_length         Size of the IV in bytes. It is guaranteed by
 *                              the core to be less or equal to
 *                              PSA_CIPHER_IV_MAX_SIZE.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size of \p iv is not acceptable for the chosen algorithm,
 *         or the chosen algorithm does not use an IV.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_cipher_set_iv(
    mbedtls_psa_cipher_operation_t *operation,
    const uint8_t *iv, size_t iv_length);

/** Encrypt or decrypt a message fragment in an active cipher operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_update entry point. This function behaves as a
 *       cipher_update entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[in] input             Buffer containing the message fragment to
 *                              encrypt or decrypt.
 * \param[in] input_length      Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the output is to be written.
 * \param[in]  output_size      Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_cipher_update(
    mbedtls_psa_cipher_operation_t *operation,
    const uint8_t *input, size_t input_length,
    uint8_t *output, size_t output_size, size_t *output_length);

/** Finish encrypting or decrypting a message in a cipher operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_finish entry point. This function behaves as a
 *       cipher_finish entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[out] output           Buffer where the output is to be written.
 * \param[in]  output_size      Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The total input size passed to this operation is not valid for
 *         this particular algorithm. For example, the algorithm is a based
 *         on block cipher and requires a whole number of blocks, but the
 *         total input size is not a multiple of the block size.
 * \retval #PSA_ERROR_INVALID_PADDING
 *         This is a decryption operation for an algorithm that includes
 *         padding, and the ciphertext does not contain valid padding.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_cipher_finish(
    mbedtls_psa_cipher_operation_t *operation,
    uint8_t *output, size_t output_size, size_t *output_length);

/** Abort a cipher operation.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_abort entry point. This function behaves as a
 *       cipher_abort entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in,out] operation     Initialized cipher operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 */
psa_status_t mbedtls_psa_cipher_abort(mbedtls_psa_cipher_operation_t *operation);

/** Encrypt a message using a symmetric cipher.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_encrypt entry point. This function behaves as a
 *       cipher_encrypt entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in] attributes        The attributes of the key to use for the
 *                              operation.
 * \param[in] key_buffer        The buffer containing the key context.
 * \param[in] key_buffer_size   Size of the \p key_buffer buffer in bytes.
 * \param[in] alg               The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 * \param[in] iv                Buffer containing the IV for encryption. The
 *                              IV has been generated by the core.
 * \param[in] iv_length         Size of the \p iv in bytes.
 * \param[in] input             Buffer containing the message to encrypt.
 * \param[in] input_length      Size of the \p input buffer in bytes.
 * \param[in,out] output        Buffer where the output is to be written.
 * \param[in]  output_size      Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes that make up
 *                              the returned output. Initialized to zero
 *                              by the core.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size \p iv_length is not acceptable for the chosen algorithm,
 *         or the chosen algorithm does not use an IV.
 *         The total input size passed to this operation is not valid for
 *         this particular algorithm. For example, the algorithm is a based
 *         on block cipher and requires a whole number of blocks, but the
 *         total input size is not a multiple of the block size.
 * \retval #PSA_ERROR_INVALID_PADDING
 *         This is a decryption operation for an algorithm that includes
 *         padding, and the ciphertext does not contain valid padding.
 */
psa_status_t mbedtls_psa_cipher_encrypt(const psa_key_attributes_t *attributes,
                                        const uint8_t *key_buffer,
                                        size_t key_buffer_size,
                                        psa_algorithm_t alg,
                                        const uint8_t *iv,
                                        size_t iv_length,
                                        const uint8_t *input,
                                        size_t input_length,
                                        uint8_t *output,
                                        size_t output_size,
                                        size_t *output_length);

/** Decrypt a message using a symmetric cipher.
 *
 * \note The signature of this function is that of a PSA driver
 *       cipher_decrypt entry point. This function behaves as a
 *       cipher_decrypt entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 * \param[in]  input            Buffer containing the iv and the ciphertext.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the output is to be written.
 * \param[in]  output_size      Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes that make up
 *                              the returned output. Initialized to zero
 *                              by the core.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size of \p iv is not acceptable for the chosen algorithm,
 *         or the chosen algorithm does not use an IV.
 *         The total input size passed to this operation is not valid for
 *         this particular algorithm. For example, the algorithm is a based
 *         on block cipher and requires a whole number of blocks, but the
 *         total input size is not a multiple of the block size.
 * \retval #PSA_ERROR_INVALID_PADDING
 *         This is a decryption operation for an algorithm that includes
 *         padding, and the ciphertext does not contain valid padding.
 */
psa_status_t mbedtls_psa_cipher_decrypt(const psa_key_attributes_t *attributes,
                                        const uint8_t *key_buffer,
                                        size_t key_buffer_size,
                                        psa_algorithm_t alg,
                                        const uint8_t *input,
                                        size_t input_length,
                                        uint8_t *output,
                                        size_t output_size,
                                        size_t *output_length);

#endif /* PSA_CRYPTO_CIPHER_H */
