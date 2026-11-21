/*
 *  PSA FFDH layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_FFDH_H
#define PSA_CRYPTO_FFDH_H

#include <psa/crypto.h>

/** Perform a key agreement and return the FFDH shared secret.
 *
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  peer_key             The buffer containing the key context
 *                                  of the peer's public key.
 * \param[in]  peer_key_length      Size of the \p peer_key buffer in
 *                                  bytes.
 * \param[in]  key_buffer           The buffer containing the private key
 *                                  context.
 * \param[in]  key_buffer_size      Size of the \p key_buffer buffer in
 *                                  bytes.
 * \param[out] shared_secret        The buffer to which the shared secret
 *                                  is to be written.
 * \param[in]  shared_secret_size   Size of the \p shared_secret buffer in
 *                                  bytes.
 * \param[out] shared_secret_length On success, the number of bytes that make
 *                                  up the returned shared secret.
 * \retval #PSA_SUCCESS
 *         Success. Shared secret successfully calculated.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key_buffer_size, \p peer_key_length, \p shared_secret_size
 *         do not match
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 */
psa_status_t mbedtls_psa_ffdh_key_agreement(
    const psa_key_attributes_t *attributes,
    const uint8_t *peer_key,
    size_t peer_key_length,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    uint8_t *shared_secret,
    size_t shared_secret_size,
    size_t *shared_secret_length);

/** Export a public key or the public part of a DH key pair in binary format.
 *
 * \param[in]  attributes       The attributes for the key to export.
 * \param[in]  key_buffer       Material or context of the key to export.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[out] data             Buffer where the key data is to be written.
 * \param[in]  data_size        Size of the \p data buffer in bytes.
 * \param[out] data_length      On success, the number of bytes written in
 *                              \p data
 *
 * \retval #PSA_SUCCESS  The public key was exported successfully.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 * \retval #PSA_ERROR_NOT_PERMITTED         \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 */
psa_status_t mbedtls_psa_ffdh_export_public_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    uint8_t *data,
    size_t data_size,
    size_t *data_length);

/**
 * \brief Generate DH key.
 *
 * \note The signature of the function is that of a PSA driver generate_key
 *       entry point.
 *
 * \param[in]  attributes         The attributes for the key to generate.
 * \param[out] key_buffer         Buffer where the key data is to be written.
 * \param[in]  key_buffer_size    Size of \p key_buffer in bytes.
 * \param[out] key_buffer_length  On success, the number of bytes written in
 *                                \p key_buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key was generated successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         Key size in bits is invalid.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY   \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED   \emptydescription
 */
psa_status_t mbedtls_psa_ffdh_generate_key(
    const psa_key_attributes_t *attributes,
    uint8_t *key_buffer,
    size_t key_buffer_size,
    size_t *key_buffer_length);

/**
 * \brief Import DH key.
 *
 * \note The signature of the function is that of a PSA driver import_key
 *       entry point.
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
 * \retval #PSA_SUCCESS
 *         The key was generated successfully.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 */
psa_status_t mbedtls_psa_ffdh_import_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits);

#endif /* PSA_CRYPTO_FFDH_H */
