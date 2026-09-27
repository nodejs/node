/** \file psa_crypto_its.h
 * \brief Interface of trusted storage that crypto is built on.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_ITS_H
#define PSA_CRYPTO_ITS_H

#include <stddef.h>
#include <stdint.h>

#include <psa/crypto_types.h>
#include <psa/crypto_values.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Flags used when creating a data entry
 */
typedef uint32_t psa_storage_create_flags_t;

/** \brief A type for UIDs used for identifying data
 */
typedef uint64_t psa_storage_uid_t;

#define PSA_STORAGE_FLAG_NONE        0         /**< No flags to pass */
#define PSA_STORAGE_FLAG_WRITE_ONCE (1 << 0) /**< The data associated with the uid will not be able to be modified or deleted. Intended to be used to set bits in `psa_storage_create_flags_t`*/

/**
 * \brief A container for metadata associated with a specific uid
 */
struct psa_storage_info_t {
    uint32_t size;                  /**< The size of the data associated with a uid **/
    psa_storage_create_flags_t flags;    /**< The flags set when the uid was created **/
};

/** Flag indicating that \ref psa_storage_create and \ref psa_storage_set_extended are supported */
#define PSA_STORAGE_SUPPORT_SET_EXTENDED (1 << 0)

#define PSA_ITS_API_VERSION_MAJOR  1  /**< The major version number of the PSA ITS API. It will be incremented on significant updates that may include breaking changes */
#define PSA_ITS_API_VERSION_MINOR  1  /**< The minor version number of the PSA ITS API. It will be incremented in small updates that are unlikely to include breaking changes */

/**
 * \brief create a new or modify an existing uid/value pair
 *
 * \param[in] uid           the identifier for the data
 * \param[in] data_length   The size in bytes of the data in `p_data`
 * \param[in] p_data        A buffer containing the data
 * \param[in] create_flags  The flags that the data will be stored with
 *
 * \return      A status indicating the success/failure of the operation
 *
 * \retval      #PSA_SUCCESS                     The operation completed successfully
 * \retval      #PSA_ERROR_NOT_PERMITTED         The operation failed because the provided `uid` value was already created with PSA_STORAGE_FLAG_WRITE_ONCE
 * \retval      #PSA_ERROR_NOT_SUPPORTED         The operation failed because one or more of the flags provided in `create_flags` is not supported or is not valid
 * \retval      #PSA_ERROR_INSUFFICIENT_STORAGE  The operation failed because there was insufficient space on the storage medium
 * \retval      #PSA_ERROR_STORAGE_FAILURE       The operation failed because the physical storage has failed (Fatal error)
 * \retval      #PSA_ERROR_INVALID_ARGUMENT      The operation failed because one of the provided pointers(`p_data`)
 *                                               is invalid, for example is `NULL` or references memory the caller cannot access
 */
psa_status_t psa_its_set(psa_storage_uid_t uid,
                         uint32_t data_length,
                         const void *p_data,
                         psa_storage_create_flags_t create_flags);

/**
 * \brief Retrieve the value associated with a provided uid
 *
 * \param[in] uid               The uid value
 * \param[in] data_offset       The starting offset of the data requested
 * \param[in] data_length       the amount of data requested (and the minimum allocated size of the `p_data` buffer)
 * \param[out] p_data           The buffer where the data will be placed upon successful completion
 * \param[out] p_data_length    The amount of data returned in the p_data buffer
 *
 *
 * \return      A status indicating the success/failure of the operation
 *
 * \retval      #PSA_SUCCESS                 The operation completed successfully
 * \retval      #PSA_ERROR_DOES_NOT_EXIST    The operation failed because the provided `uid` value was not found in the storage
 * \retval      #PSA_ERROR_STORAGE_FAILURE   The operation failed because the physical storage has failed (Fatal error)
 * \retval      #PSA_ERROR_DATA_CORRUPT      The operation failed because stored data has been corrupted
 * \retval      #PSA_ERROR_INVALID_ARGUMENT  The operation failed because one of the provided pointers(`p_data`, `p_data_length`)
 *                                           is invalid. For example is `NULL` or references memory the caller cannot access.
 *                                           In addition, this can also happen if an invalid offset was provided.
 */
psa_status_t psa_its_get(psa_storage_uid_t uid,
                         uint32_t data_offset,
                         uint32_t data_length,
                         void *p_data,
                         size_t *p_data_length);

/**
 * \brief Retrieve the metadata about the provided uid
 *
 * \param[in] uid           The uid value
 * \param[out] p_info       A pointer to the `psa_storage_info_t` struct that will be populated with the metadata
 *
 * \return      A status indicating the success/failure of the operation
 *
 * \retval      #PSA_SUCCESS                 The operation completed successfully
 * \retval      #PSA_ERROR_DOES_NOT_EXIST    The operation failed because the provided uid value was not found in the storage
 * \retval      #PSA_ERROR_DATA_CORRUPT      The operation failed because stored data has been corrupted
 * \retval      #PSA_ERROR_INVALID_ARGUMENT  The operation failed because one of the provided pointers(`p_info`)
 *                                           is invalid, for example is `NULL` or references memory the caller cannot access
 */
psa_status_t psa_its_get_info(psa_storage_uid_t uid,
                              struct psa_storage_info_t *p_info);

/**
 * \brief Remove the provided key and its associated data from the storage
 *
 * \param[in] uid   The uid value
 *
 * \return  A status indicating the success/failure of the operation
 *
 * \retval      #PSA_SUCCESS                  The operation completed successfully
 * \retval      #PSA_ERROR_DOES_NOT_EXIST     The operation failed because the provided key value was not found in the storage
 * \retval      #PSA_ERROR_NOT_PERMITTED      The operation failed because the provided key value was created with PSA_STORAGE_FLAG_WRITE_ONCE
 * \retval      #PSA_ERROR_STORAGE_FAILURE    The operation failed because the physical storage has failed (Fatal error)
 */
psa_status_t psa_its_remove(psa_storage_uid_t uid);

#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_ITS_H */
