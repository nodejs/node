/*
 *  PSA ECP layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_ECP_H
#define PSA_CRYPTO_ECP_H

#include <psa/crypto.h>
#include <mbedtls/ecp.h>

/** Load the contents of a key buffer into an internal ECP representation
 *
 * \param[in] type          The type of key contained in \p data.
 * \param[in] curve_bits    The nominal bit-size of the curve.
 *                          It must be consistent with the representation
 *                          passed in \p data.
 *                          This can be 0, in which case the bit-size
 *                          is inferred from \p data_length (which is possible
 *                          for all key types and representation formats
 *                          formats that are currently supported or will
 *                          be in the foreseeable future).
 * \param[in] data          The buffer from which to load the representation.
 * \param[in] data_length   The size in bytes of \p data.
 * \param[out] p_ecp        Returns a pointer to an ECP context on success.
 *                          The caller is responsible for freeing both the
 *                          contents of the context and the context itself
 *                          when done.
 */
psa_status_t mbedtls_psa_ecp_load_representation(psa_key_type_t type,
                                                 size_t curve_bits,
                                                 const uint8_t *data,
                                                 size_t data_length,
                                                 mbedtls_ecp_keypair **p_ecp);

/** Load the public part of an internal ECP, if required.
 *
 * \param ecp               The ECP context to load the public part for.
 *
 * \return PSA_SUCCESS on success, otherwise an MPI error.
 */

psa_status_t mbedtls_psa_ecp_load_public_part(mbedtls_ecp_keypair *ecp);

/** Import an ECP key in binary format.
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
 * \retval #PSA_SUCCESS  The ECP key was imported successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key data is not correctly formatted.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_ecp_import_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits);

/** Export an ECP key to export representation
 *
 * \param[in] type          The type of key (public/private) to export
 * \param[in] ecp           The internal ECP representation from which to export
 * \param[out] data         The buffer to export to
 * \param[in] data_size     The length of the buffer to export to
 * \param[out] data_length  The amount of bytes written to \p data
 */
psa_status_t mbedtls_psa_ecp_export_key(psa_key_type_t type,
                                        mbedtls_ecp_keypair *ecp,
                                        uint8_t *data,
                                        size_t data_size,
                                        size_t *data_length);

/** Export an ECP public key or the public part of an ECP key pair in binary
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
 *                              \p data
 *
 * \retval #PSA_SUCCESS  The ECP public key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_ecp_export_public_key(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/**
 * \brief Generate an ECP key.
 *
 * \note The signature of the function is that of a PSA driver generate_key
 *       entry point.
 *
 * \param[in]  attributes         The attributes for the ECP key to generate.
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
psa_status_t mbedtls_psa_ecp_generate_key(
    const psa_key_attributes_t *attributes,
    uint8_t *key_buffer, size_t key_buffer_size, size_t *key_buffer_length);

/** Sign an already-calculated hash with ECDSA.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash entry point. This function behaves as a sign_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the ECC key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the ECC key context.
 *                              format.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              Randomized or deterministic ECDSA algorithm.
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
 *         #PSA_SIGN_OUTPUT_SIZE(\c PSA_KEY_TYPE_ECC_KEY_PAIR, \c key_bits,
 *         \p alg) where \c key_bits is the bit-size of the ECC key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t mbedtls_psa_ecdsa_sign_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/**
 * \brief Verify an ECDSA hash or short message signature.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash entry point. This function behaves as a verify_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the ECC key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the ECC key context.
 *                              format.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              Randomized or deterministic ECDSA algorithm.
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
psa_status_t mbedtls_psa_ecdsa_verify_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);


/** Perform a key agreement and return the raw ECDH shared secret.
 *
 * \note The signature of this function is that of a PSA driver
 *       key_agreement entry point. This function behaves as a key_agreement
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  key_buffer           The buffer containing the private key
 *                                  context.
 * \param[in]  key_buffer_size      Size of the \p key_buffer buffer in
 *                                  bytes.
 * \param[in]  alg                  A key agreement algorithm that is
 *                                  compatible with the type of the key.
 * \param[in]  peer_key             The buffer containing the key context
 *                                  of the peer's public key.
 * \param[in]  peer_key_length      Size of the \p peer_key buffer in
 *                                  bytes.
 * \param[out] shared_secret        The buffer to which the shared secret
 *                                  is to be written.
 * \param[in]  shared_secret_size   Size of the \p shared_secret buffer in
 *                                  bytes.
 * \param[out] shared_secret_length On success, the number of bytes that make
 *                                  up the returned shared secret.
 * \retval #PSA_SUCCESS
 *         Success. Shared secret successfully calculated.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p alg is not a key agreement algorithm, or
 *         \p private_key is not compatible with \p alg,
 *         or \p peer_key is not valid for \p alg or not compatible with
 *         \p private_key.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p shared_secret_size is too small
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not a supported key agreement algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t mbedtls_psa_key_agreement_ecdh(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *peer_key, size_t peer_key_length,
    uint8_t *shared_secret, size_t shared_secret_size,
    size_t *shared_secret_length);
#endif /* PSA_CRYPTO_ECP_H */
