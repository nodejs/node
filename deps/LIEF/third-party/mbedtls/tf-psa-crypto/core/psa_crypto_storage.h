/**
 * \file psa_crypto_storage.h
 *
 * \brief PSA cryptography module: Mbed TLS key storage
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_STORAGE_H
#define PSA_CRYPTO_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "psa/crypto.h"

#include <stdint.h>
#include <string.h>

/* Limit the maximum key size in storage. */
#if defined(MBEDTLS_PSA_STATIC_KEY_SLOTS)
/* Reflect the maximum size for the key buffer. */
#define PSA_CRYPTO_MAX_STORAGE_SIZE (MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE)
#else
/* Just set an upper boundary but it should have no effect since the key size
 * is limited in memory. */
#define PSA_CRYPTO_MAX_STORAGE_SIZE (PSA_BITS_TO_BYTES(PSA_MAX_KEY_BITS))
#endif

/* Sanity check: a file size must fit in 32 bits. Allow a generous
 * 64kB of metadata. */
#if PSA_CRYPTO_MAX_STORAGE_SIZE > 0xffff0000
#error "PSA_CRYPTO_MAX_STORAGE_SIZE > 0xffff0000"
#endif

/** The maximum permitted persistent slot number.
 *
 * In Mbed Crypto 0.1.0b:
 * - Using the file backend, all key ids are ok except 0.
 * - Using the ITS backend, all key ids are ok except 0xFFFFFF52
 *   (#PSA_CRYPTO_ITS_RANDOM_SEED_UID) for which the file contains the
 *   device's random seed (if this feature is enabled).
 * - Only key ids from 1 to #MBEDTLS_PSA_KEY_SLOT_COUNT are actually used.
 *
 * Since we need to preserve the random seed, avoid using that key slot.
 * Reserve a whole range of key slots just in case something else comes up.
 *
 * This limitation will probably become moot when we implement client
 * separation for key storage.
 */
#define PSA_MAX_PERSISTENT_KEY_IDENTIFIER PSA_KEY_ID_VENDOR_MAX

/**
 * \brief Checks if persistent data is stored for the given key slot number
 *
 * This function checks if any key data or metadata exists for the key slot in
 * the persistent storage.
 *
 * \param key           Persistent identifier to check.
 *
 * \retval 0
 *         No persistent data present for slot number
 * \retval 1
 *         Persistent data present for slot number
 */
int psa_is_key_present_in_storage(const mbedtls_svc_key_id_t key);

/**
 * \brief Format key data and metadata and save to a location for given key
 *        slot.
 *
 * This function formats the key data and metadata and saves it to a
 * persistent storage backend. The storage location corresponding to the
 * key slot must be empty, otherwise this function will fail. This function
 * should be called after loading the key into an internal slot to ensure the
 * persistent key is not saved into a storage location corresponding to an
 * already occupied non-persistent key, as well as ensuring the key data is
 * validated.
 *
 * Note: This function will only succeed for key buffers which are not
 * empty. If passed a NULL pointer or zero-length, the function will fail
 * with #PSA_ERROR_INVALID_ARGUMENT.
 *
 * \param[in] attr          The attributes of the key to save.
 *                          The key identifier field in the attributes
 *                          determines the key's location.
 * \param[in] data          Buffer containing the key data.
 * \param data_length       The number of bytes that make up the key data.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_ALREADY_EXISTS \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 */
psa_status_t psa_save_persistent_key(const psa_key_attributes_t *attr,
                                     const uint8_t *data,
                                     const size_t data_length);

/**
 * \brief Parses key data and metadata and load persistent key for given
 * key slot number.
 *
 * This function reads from a storage backend, parses the key data and
 * metadata and writes them to the appropriate output parameters.
 *
 * Note: This function allocates a buffer and returns a pointer to it through
 * the data parameter. On successful return, the pointer is guaranteed to be
 * valid and the buffer contains at least one byte of data.
 * psa_free_persistent_key_data() must be called on the data buffer
 * afterwards to zeroize and free this buffer.
 *
 * \param[in,out] attr      On input, the key identifier field identifies
 *                          the key to load. Other fields are ignored.
 *                          On success, the attribute structure contains
 *                          the key metadata that was loaded from storage.
 * \param[out] data         Pointer to an allocated key data buffer on return.
 * \param[out] data_length  The number of bytes that make up the key data.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DOES_NOT_EXIST \emptydescription
 */
psa_status_t psa_load_persistent_key(psa_key_attributes_t *attr,
                                     uint8_t **data,
                                     size_t *data_length);

/**
 * \brief Remove persistent data for the given key slot number.
 *
 * \param key           Persistent identifier of the key to remove
 *                      from persistent storage.
 *
 * \retval #PSA_SUCCESS
 *         The key was successfully removed,
 *         or the key did not exist.
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 */
psa_status_t psa_destroy_persistent_key(const mbedtls_svc_key_id_t key);

/**
 * \brief Free the temporary buffer allocated by psa_load_persistent_key().
 *
 * This function must be called at some point after psa_load_persistent_key()
 * to zeroize and free the memory allocated to the buffer in that function.
 *
 * \param key_data        Buffer for the key data.
 * \param key_data_length Size of the key data buffer.
 *
 */
void psa_free_persistent_key_data(uint8_t *key_data, size_t key_data_length);

/**
 * \brief Formats key data and metadata for persistent storage
 *
 * \param[in] data          Buffer containing the key data.
 * \param data_length       Length of the key data buffer.
 * \param[in] attr          The core attributes of the key.
 * \param[out] storage_data Output buffer for the formatted data.
 *
 */
void psa_format_key_data_for_storage(const uint8_t *data,
                                     const size_t data_length,
                                     const psa_key_attributes_t *attr,
                                     uint8_t *storage_data);

/**
 * \brief Parses persistent storage data into key data and metadata
 *
 * \param[in] storage_data     Buffer for the storage data.
 * \param storage_data_length  Length of the storage data buffer
 * \param[out] key_data        On output, pointer to a newly allocated buffer
 *                             containing the key data. This must be freed
 *                             using psa_free_persistent_key_data()
 * \param[out] key_data_length Length of the key data buffer
 * \param[out] attr            On success, the attribute structure is filled
 *                             with the loaded key metadata.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 */
psa_status_t psa_parse_key_data_from_storage(const uint8_t *storage_data,
                                             size_t storage_data_length,
                                             uint8_t **key_data,
                                             size_t *key_data_length,
                                             psa_key_attributes_t *attr);

#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_STORAGE_H */
