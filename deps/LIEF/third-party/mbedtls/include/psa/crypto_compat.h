/**
 * \file psa/crypto_compat.h
 *
 * \brief PSA cryptography module: Backward compatibility aliases
 *
 * This header declares alternative names for macro and functions.
 * New application code should not use these names.
 * These names may be removed in a future version of Mbed TLS.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_COMPAT_H
#define PSA_CRYPTO_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * To support both openless APIs and psa_open_key() temporarily, define
 * psa_key_handle_t to be equal to mbedtls_svc_key_id_t. Do not mark the
 * type and its utility macros and functions deprecated yet. This will be done
 * in a subsequent phase.
 */
typedef mbedtls_svc_key_id_t psa_key_handle_t;

#define PSA_KEY_HANDLE_INIT MBEDTLS_SVC_KEY_ID_INIT

/** Check whether a handle is null.
 *
 * \param handle  Handle
 *
 * \return Non-zero if the handle is null, zero otherwise.
 */
static inline int psa_key_handle_is_null(psa_key_handle_t handle)
{
    return mbedtls_svc_key_id_is_null(handle);
}

/** Open a handle to an existing persistent key.
 *
 * Open a handle to a persistent key. A key is persistent if it was created
 * with a lifetime other than #PSA_KEY_LIFETIME_VOLATILE. A persistent key
 * always has a nonzero key identifier, set with psa_set_key_id() when
 * creating the key. Implementations may provide additional pre-provisioned
 * keys that can be opened with psa_open_key(). Such keys have an application
 * key identifier in the vendor range, as documented in the description of
 * #psa_key_id_t.
 *
 * The application must eventually close the handle with psa_close_key() or
 * psa_destroy_key() to release associated resources. If the application dies
 * without calling one of these functions, the implementation should perform
 * the equivalent of a call to psa_close_key().
 *
 * Some implementations permit an application to open the same key multiple
 * times. If this is successful, each call to psa_open_key() will return a
 * different key handle.
 *
 * \note This API is not part of the PSA Cryptography API Release 1.0.0
 * specification. It was defined in the 1.0 Beta 3 version of the
 * specification but was removed in the 1.0.0 released version. This API is
 * kept for the time being to not break applications relying on it. It is not
 * deprecated yet but will be in the near future.
 *
 * \note Applications that rely on opening a key multiple times will not be
 * portable to implementations that only permit a single key handle to be
 * opened. See also :ref:\`key-handles\`.
 *
 *
 * \param key           The persistent identifier of the key.
 * \param[out] handle   On success, a handle to the key.
 *
 * \retval #PSA_SUCCESS
 *         Success. The application can now use the value of `*handle`
 *         to access the key.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         The implementation does not have sufficient resources to open the
 *         key. This can be due to reaching an implementation limit on the
 *         number of open keys, the number of open key handles, or available
 *         memory.
 * \retval #PSA_ERROR_DOES_NOT_EXIST
 *         There is no persistent key with key identifier \p key.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not a valid persistent key identifier.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The specified key exists, but the application does not have the
 *         permission to access it. Note that this specification does not
 *         define any way to create such a key, but it may be possible
 *         through implementation-specific means.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_open_key(mbedtls_svc_key_id_t key,
                          psa_key_handle_t *handle);

/** Close a key handle.
 *
 * If the handle designates a volatile key, this will destroy the key material
 * and free all associated resources, just like psa_destroy_key().
 *
 * If this is the last open handle to a persistent key, then closing the handle
 * will free all resources associated with the key in volatile memory. The key
 * data in persistent storage is not affected and can be opened again later
 * with a call to psa_open_key().
 *
 * Closing the key handle makes the handle invalid, and the key handle
 * must not be used again by the application.
 *
 * \note This API is not part of the PSA Cryptography API Release 1.0.0
 * specification. It was defined in the 1.0 Beta 3 version of the
 * specification but was removed in the 1.0.0 released version. This API is
 * kept for the time being to not break applications relying on it. It is not
 * deprecated yet but will be in the near future.
 *
 * \note If the key handle was used to set up an active
 * :ref:\`multipart operation <multipart-operations>\`, then closing the
 * key handle can cause the multipart operation to fail. Applications should
 * maintain the key handle until after the multipart operation has finished.
 *
 * \param handle        The key handle to close.
 *                      If this is \c 0, do nothing and return \c PSA_SUCCESS.
 *
 * \retval #PSA_SUCCESS
 *         \p handle was a valid handle or \c 0. It is now closed.
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         \p handle is not a valid handle nor \c 0.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_close_key(psa_key_handle_t handle);

/** \addtogroup attributes
 * @{
 */

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/** Custom Diffie-Hellman group.
 *
 * Mbed TLS does not support custom DH groups.
 *
 * \deprecated This value is not useful, so this macro will be removed in
 *             a future version of the library.
 */
#define PSA_DH_FAMILY_CUSTOM                                            \
    ((psa_dh_family_t) MBEDTLS_DEPRECATED_NUMERIC_CONSTANT(0x7e))

/**
 * \brief Set domain parameters for a key.
 *
 * \deprecated  Mbed TLS no longer supports any domain parameters.
 *              This function only does the equivalent of
 *              psa_set_key_type() and will be removed in a future version
 *              of the library.
 *
 * \param[in,out] attributes    Attribute structure where \p type will be set.
 * \param type                  Key type (a \c PSA_KEY_TYPE_XXX value).
 * \param[in] data              Ignored.
 * \param data_length           Must be 0.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 */
static inline psa_status_t MBEDTLS_DEPRECATED psa_set_key_domain_parameters(
    psa_key_attributes_t *attributes,
    psa_key_type_t type, const uint8_t *data, size_t data_length)
{
    (void) data;
    if (data_length != 0) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    psa_set_key_type(attributes, type);
    return PSA_SUCCESS;
}

/**
 * \brief Get domain parameters for a key.
 *
 * \deprecated  Mbed TLS no longer supports any domain parameters.
 *              This function alwaya has an empty output and will be
 *              removed in a future version of the library.

 * \param[in] attributes        Ignored.
 * \param[out] data             Ignored.
 * \param data_size             Ignored.
 * \param[out] data_length      Set to 0.
 *
 * \retval #PSA_SUCCESS \emptydescription
 */
static inline psa_status_t MBEDTLS_DEPRECATED psa_get_key_domain_parameters(
    const psa_key_attributes_t *attributes,
    uint8_t *data, size_t data_size, size_t *data_length)
{
    (void) attributes;
    (void) data;
    (void) data_size;
    *data_length = 0;
    return PSA_SUCCESS;
}

/** Safe output buffer size for psa_get_key_domain_parameters().
 *
 */
#define PSA_KEY_DOMAIN_PARAMETERS_SIZE(key_type, key_bits)      \
    MBEDTLS_DEPRECATED_NUMERIC_CONSTANT(1u)
#endif /* MBEDTLS_DEPRECATED_REMOVED */

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_COMPAT_H */
