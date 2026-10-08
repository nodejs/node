/*
 *   Driver entry points for p256-m
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef P256M_DRIVER_ENTRYPOINTS_H
#define P256M_DRIVER_ENTRYPOINTS_H

#if defined(MBEDTLS_PSA_P256M_DRIVER_ENABLED)
#ifndef PSA_CRYPTO_ACCELERATOR_DRIVER_PRESENT
#define PSA_CRYPTO_ACCELERATOR_DRIVER_PRESENT
#endif /* PSA_CRYPTO_ACCELERATOR_DRIVER_PRESENT */
#endif /* MBEDTLS_PSA_P256M_DRIVER_ENABLED */

#include "psa/crypto_types.h"

/** Import SECP256R1 key.
 *
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  data                 The raw key material. For private keys
 *                                  this must be a big-endian integer of 32
 *                                  bytes; for public key this must be an
 *                                  uncompressed ECPoint (65 bytes).
 * \param[in]  data_length          The size of the raw key material.
 * \param[out] key_buffer           The buffer to contain the key data in
 *                                  output format upon successful return.
 * \param[in]  key_buffer_size      Size of the \p key_buffer buffer in bytes.
 * \param[out] key_buffer_length    The length of the data written in \p
 *                                  key_buffer in bytes.
 * \param[out] bits                 The bitsize of the key.
 *
 * \retval  #PSA_SUCCESS
 *          Success. Keypair generated and stored in buffer.
 * \retval  #PSA_ERROR_NOT_SUPPORTED
 *          The input is not supported by this driver (not SECP256R1).
 * \retval  #PSA_ERROR_INVALID_ARGUMENT
 *          The input is invalid.
 * \retval  #PSA_ERROR_BUFFER_TOO_SMALL
 *          \p key_buffer_size is too small.
 */
psa_status_t p256_transparent_import_key(const psa_key_attributes_t *attributes,
                             const uint8_t *data,
                             size_t data_length,
                             uint8_t *key_buffer,
                             size_t key_buffer_size,
                             size_t *key_buffer_length,
                             size_t *bits);

/** Export SECP256R1 public key, from the private key.
 *
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  key_buffer           The private key in the export format.
 * \param[in]  key_buffer_size      The size of the private key in bytes.
 * \param[out] data                 The buffer to contain the public key in
 *                                  the export format upon successful return.
 * \param[in]  data_size            The size of the \p data buffer in bytes.
 * \param[out] data_length          The length written to \p data in bytes.
 *
 * \retval  #PSA_SUCCESS
 *          Success. Keypair generated and stored in buffer.
 * \retval  #PSA_ERROR_NOT_SUPPORTED
 *          The input is not supported by this driver (not SECP256R1).
 * \retval  #PSA_ERROR_INVALID_ARGUMENT
 *          The input is invalid.
 * \retval  #PSA_ERROR_BUFFER_TOO_SMALL
 *          \p key_buffer_size is too small.
 */
psa_status_t p256_transparent_export_public_key(const psa_key_attributes_t *attributes,
                                    const uint8_t *key_buffer,
                                    size_t key_buffer_size,
                                    uint8_t *data,
                                    size_t data_size,
                                    size_t *data_length);

/** Generate SECP256R1 ECC Key Pair.
 *  Interface function which calls the p256-m key generation function and
 *  places it in the key buffer provided by the caller (Mbed TLS) in the
 *  correct format. For a SECP256R1 curve this is the 32 bit private key.
 *
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[out]  key_buffer          The buffer to contain the key data in
 *                                  output format upon successful return.
 * \param[in]   key_buffer_size     Size of the \p key_buffer buffer in bytes.
 * \param[out]  key_buffer_length   The length of the data written in \p
 *                                  key_buffer in bytes.
 *
 * \retval  #PSA_SUCCESS
 *          Success. Keypair generated and stored in buffer.
 * \retval  #PSA_ERROR_BUFFER_TOO_SMALL
 *          \p key_buffer_size is too small.
 * \retval  #PSA_ERROR_GENERIC_ERROR
 *          The internal RNG failed.
 */
psa_status_t p256_transparent_generate_key(
    const psa_key_attributes_t *attributes,
    uint8_t *key_buffer,
    size_t key_buffer_size,
    size_t *key_buffer_length);

/** Perform raw key agreement using p256-m's ECDH implementation
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  key_buffer           The buffer containing the private key
 *                                  in the format specified by PSA.
 * \param[in]  key_buffer_size      Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg                  A key agreement algorithm that is
 *                                  compatible with the type of the key.
 * \param[in]  peer_key             The buffer containing the peer's public
 *                                  key in format specified by PSA.
 * \param[in]  peer_key_length      Size of the \p peer_key buffer in
 *                                  bytes.
 * \param[out] shared_secret        The buffer to which the shared secret
 *                                  is to be written.
 * \param[in]  shared_secret_size   Size of the \p shared_secret buffer in
 *                                  bytes.
 * \param[out] shared_secret_length On success, the number of bytes that
 *                                  make up the returned shared secret.
 * \retval  #PSA_SUCCESS
 *          Success. Shared secret successfully calculated.
 * \retval  #PSA_ERROR_INVALID_ARGUMENT
 *          The input is invalid.
 * \retval  #PSA_ERROR_BUFFER_TOO_SMALL
 *          \p shared_secret_size is too small.
 */
psa_status_t p256_transparent_key_agreement(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *peer_key,
    size_t peer_key_length,
    uint8_t *shared_secret,
    size_t shared_secret_size,
    size_t *shared_secret_length);

/** Sign an already-calculated hash with a private key using p256-m's ECDSA
 *  implementation
 * \param[in]  attributes           The attributes of the key to use for the
 *                                  operation.
 * \param[in]  key_buffer           The buffer containing the private key
 *                                  in the format specified by PSA.
 * \param[in]  key_buffer_size      Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg                  A signature algorithm that is compatible
 *                                  with the type of the key.
 * \param[in]  hash                 The hash to sign.
 * \param[in]  hash_length          Size of the \p hash buffer in bytes.
 * \param[out] signature            Buffer where signature is to be written.
 * \param[in]  signature_size       Size of the \p signature buffer in bytes.
 * \param[out] signature_length     On success, the number of bytes
 *                                  that make up the returned signature value.
 *
 * \retval  #PSA_SUCCESS
 *          Success. Hash was signed successfully.
 * \retval  #PSA_ERROR_INVALID_ARGUMENT
 *          The input is invalid.
 * \retval  #PSA_ERROR_BUFFER_TOO_SMALL
 *          \p signature_size is too small.
 * \retval  #PSA_ERROR_GENERIC_ERROR
 *          The internal RNG failed.
 */
psa_status_t p256_transparent_sign_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *hash,
    size_t hash_length,
    uint8_t *signature,
    size_t signature_size,
    size_t *signature_length);

/** Verify the signature of a hash using a SECP256R1 public key using p256-m's
 *  ECDSA implementation.
 *
 * \note p256-m expects a 64 byte public key, but the contents of the key
         buffer may be the 32 byte keypair representation or the 65 byte
         public key representation. As a result, this function calls
         psa_driver_wrapper_export_public_key() to ensure the public key
         can be passed to p256-m.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 *
 * \param[in]  key_buffer       The buffer containing the key
 *                              in the format specified by PSA.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  hash             The hash whose signature is to be
 *                              verified.
 * \param[in]  hash_length      Size of the \p hash buffer in bytes.
 * \param[in]  signature        Buffer containing the signature to verify.
 * \param[in]  signature_length Size of the \p signature buffer in bytes.
 *
 * \retval  #PSA_SUCCESS
 *          The signature is valid.
 * \retval  #PSA_ERROR_INVALID_SIGNATURE
 *          The calculation was performed successfully, but the passed
 *          signature is not a valid signature.
 * \retval  #PSA_ERROR_INVALID_ARGUMENT
 *          The input is invalid.
 */
psa_status_t p256_transparent_verify_hash(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *hash,
    size_t hash_length,
    const uint8_t *signature,
    size_t signature_length);

#endif /* P256M_DRIVER_ENTRYPOINTS_H */
