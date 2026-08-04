/*
 *  PSA RSA layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_RSA_H
#define PSA_CRYPTO_RSA_H

#include <psa/crypto.h>
#include <mbedtls/rsa.h>

/** Load the contents of a key buffer into an internal RSA representation
 *
 * \param[in] type          The type of key contained in \p data.
 * \param[in] data          The buffer from which to load the representation.
 * \param[in] data_length   The size in bytes of \p data.
 * \param[out] p_rsa        Returns a pointer to an RSA context on success.
 *                          The caller is responsible for freeing both the
 *                          contents of the context and the context itself
 *                          when done.
 */
psa_status_t mbedtls_psa_rsa_load_representation(psa_key_type_t type,
                                                 const uint8_t *data,
                                                 size_t data_length,
                                                 mbedtls_rsa_context **p_rsa);

/** Import an RSA key in binary format.
 *
 * \note The signature of this function is that of a PSA driver
 *       import_key entry point. This function behaves as an import_key
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes for the key to import.
 * \param[in]  data             The buffer containing the key data in import
 *                              format.
 * \param[in]  data_length      Size of the \p data buffer in bytes.
 * \param[out] key_buffer       The buffer containing the key data in output
 *                              format.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes. This
 *                              size is greater or equal to \p data_length.
 * \param[out] key_buffer_length  The length of the data written in \p
 *                                key_buffer in bytes.
 * \param[out] bits             The key size in number of bits.
 *
 * \retval #PSA_SUCCESS  The RSA key was imported successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key data is not correctly formatted.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_rsa_import_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits);

/** Export an RSA key to export representation
 *
 * \param[in] type          The type of key (public/private) to export
 * \param[in] rsa           The internal RSA representation from which to export
 * \param[out] data         The buffer to export to
 * \param[in] data_size     The length of the buffer to export to
 * \param[out] data_length  The amount of bytes written to \p data
 */
psa_status_t mbedtls_psa_rsa_export_key(psa_key_type_t type,
                                        mbedtls_rsa_context *rsa,
                                        uint8_t *data,
                                        size_t data_size,
                                        size_t *data_length);

/** Export a public RSA key or the public part of an RSA key pair in binary
 *  format.
 *
 * \note The signature of this function is that of a PSA driver
 *       export_public_key entry point. This function behaves as an
 *       export_public_key entry point as defined in the PSA driver interface
 *       specification.
 *
 * \param[in]  attributes       The attributes for the key to export.
 * \param[in]  key_buffer       Material or context of the key to export.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[out] data             Buffer where the key data is to be written.
 * \param[in]  data_size        Size of the \p data buffer in bytes.
 * \param[out] data_length      On success, the number of bytes written in
 *                              \p data.
 *
 * \retval #PSA_SUCCESS  The RSA public key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_rsa_export_public_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/**
 * \brief Generate an RSA key.
 *
 * \param[in]  attributes         The attributes for the RSA key to generate.
 * \param[in]  custom_data        The public exponent to use.
 *                                This can be a null pointer if
 *                                \c params_data_length is 0.
 * \param custom_data_length      Length of \p custom_data in bytes.
 *                                This can be 0, in which case the
 *                                public exponent will be 65537.
 * \param[out] key_buffer         Buffer where the key data is to be written.
 * \param[in]  key_buffer_size    Size of \p key_buffer in bytes.
 * \param[out] key_buffer_length  On success, the number of bytes written in
 *                                \p key_buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key was successfully generated.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         Key length or type not supported.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 */
psa_status_t mbedtls_psa_rsa_generate_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *custom_data, size_t custom_data_length,
    uint8_t *key_buffer, size_t key_buffer_size, size_t *key_buffer_length);

/** Sign an already-calculated hash with an RSA private key.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash entry point. This function behaves as a sign_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the RSA key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the RSA key context.
 *                              format.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              an RSA key.
 * \param[in]  hash             The hash or message to sign.
 * \param[in]  hash_length      Size of the \p hash buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param[in]  signature_size   Size of the \p signature buffer in bytes.
 * \param[out] signature_length On success, the number of bytes
 *                              that make up the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c PSA_KEY_TYPE_RSA_KEY_PAIR, \c key_bits,
 *         \p alg) where \c key_bits is the bit-size of the RSA key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t mbedtls_psa_rsa_sign_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/**
 * \brief Verify the signature a hash or short message using a public RSA key.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash entry point. This function behaves as a verify_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the RSA key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the RSA key context.
 *                              format.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              an RSA key.
 * \param[in]  hash             The hash or message whose signature is to be
 *                              verified.
 * \param[in]  hash_length      Size of the \p hash buffer in bytes.
 * \param[in]  signature        Buffer containing the signature to verify.
 * \param[in]  signature_length Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The signature is valid.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_rsa_verify_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);

/**
 * \brief Encrypt a short message with a public key.
 *
 * \param attributes            The attributes for the key to import.
 * \param key_buffer            Buffer where the key data is to be written.
 * \param key_buffer_size       Size of the \p key_buffer buffer in bytes.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[in] salt              A salt or label, if supported by the
 *                              encryption algorithm.
 *                              If the algorithm does not support a
 *                              salt, pass \c NULL.
 *                              If the algorithm supports an optional
 *                              salt and you do not want to pass a salt,
 *                              pass \c NULL.
 *
 *                              - For #PSA_ALG_RSA_PKCS1V15_CRYPT, no salt is
 *                                supported.
 * \param salt_length           Size of the \p salt buffer in bytes.
 *                              If \p salt is \c NULL, pass 0.
 * \param[out] output           Buffer where the encrypted message is to
 *                              be written.
 * \param output_size           Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_ASYMMETRIC_ENCRYPT_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \p key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t mbedtls_psa_asymmetric_encrypt(const psa_key_attributes_t *attributes,
                                            const uint8_t *key_buffer,
                                            size_t key_buffer_size,
                                            psa_algorithm_t alg,
                                            const uint8_t *input,
                                            size_t input_length,
                                            const uint8_t *salt,
                                            size_t salt_length,
                                            uint8_t *output,
                                            size_t output_size,
                                            size_t *output_length);

/**
 * \brief Decrypt a short message with a private key.
 *
 * \param attributes            The attributes for the key to import.
 * \param key_buffer            Buffer where the key data is to be written.
 * \param key_buffer_size       Size of the \p key_buffer buffer in bytes.
 * \param[in] input             The message to decrypt.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[in] salt              A salt or label, if supported by the
 *                              encryption algorithm.
 *                              If the algorithm does not support a
 *                              salt, pass \c NULL.
 *                              If the algorithm supports an optional
 *                              salt and you do not want to pass a salt,
 *                              pass \c NULL.
 *
 *                              - For #PSA_ALG_RSA_PKCS1V15_CRYPT, no salt is
 *                                supported.
 * \param salt_length           Size of the \p salt buffer in bytes.
 *                              If \p salt is \c NULL, pass 0.
 * \param[out] output           Buffer where the decrypted message is to
 *                              be written.
 * \param output_size           Size of the \c output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_ASYMMETRIC_DECRYPT_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \p key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_INVALID_PADDING \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t mbedtls_psa_asymmetric_decrypt(const psa_key_attributes_t *attributes,
                                            const uint8_t *key_buffer,
                                            size_t key_buffer_size,
                                            psa_algorithm_t alg,
                                            const uint8_t *input,
                                            size_t input_length,
                                            const uint8_t *salt,
                                            size_t salt_length,
                                            uint8_t *output,
                                            size_t output_size,
                                            size_t *output_length);

#endif /* PSA_CRYPTO_RSA_H */
