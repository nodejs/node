/**
 * \file psa/crypto.h
 * \brief Platform Security Architecture cryptography module
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_H
#define PSA_CRYPTO_H

#if defined(MBEDTLS_PSA_CRYPTO_PLATFORM_FILE)
#include MBEDTLS_PSA_CRYPTO_PLATFORM_FILE
#else
#include "crypto_platform.h"
#endif

#include <stddef.h>

#ifdef __DOXYGEN_ONLY__
/* This __DOXYGEN_ONLY__ block contains mock definitions for things that
 * must be defined in the crypto_platform.h header. These mock definitions
 * are present in this file as a convenience to generate pretty-printed
 * documentation that includes those definitions. */

/** \defgroup platform Implementation-specific definitions
 * @{
 */

/**@}*/
#endif /* __DOXYGEN_ONLY__ */

#ifdef __cplusplus
extern "C" {
#endif

/* The file "crypto_types.h" declares types that encode errors,
 * algorithms, key types, policies, etc. */
#include "crypto_types.h"

/** \defgroup version API version
 * @{
 */

/**
 * The major version of this implementation of the PSA Crypto API
 */
#define PSA_CRYPTO_API_VERSION_MAJOR 1

/**
 * The minor version of this implementation of the PSA Crypto API
 */
#define PSA_CRYPTO_API_VERSION_MINOR 0

/**@}*/

/* The file "crypto_values.h" declares macros to build and analyze values
 * of integral types defined in "crypto_types.h". */
#include "crypto_values.h"

/* The file "crypto_sizes.h" contains definitions for size calculation
 * macros whose definitions are implementation-specific. */
#include "crypto_sizes.h"

/* The file "crypto_struct.h" contains definitions for
 * implementation-specific structs that are declared above. */
#if defined(MBEDTLS_PSA_CRYPTO_STRUCT_FILE)
#include MBEDTLS_PSA_CRYPTO_STRUCT_FILE
#else
#include "crypto_struct.h"
#endif

/** \defgroup initialization Library initialization
 * @{
 */

/**
 * \brief Library initialization.
 *
 * Applications must call this function before calling any other
 * function in this module.
 *
 * Applications may call this function more than once. Once a call
 * succeeds, subsequent calls are guaranteed to succeed.
 *
 * If the application calls other functions before calling psa_crypto_init(),
 * the behavior is undefined. Implementations are encouraged to either perform
 * the operation as if the library had been initialized or to return
 * #PSA_ERROR_BAD_STATE or some other applicable error. In particular,
 * implementations should not return a success status if the lack of
 * initialization may have security implications, for example due to improper
 * seeding of the random number generator.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 */
psa_status_t psa_crypto_init(void);

/**@}*/

/** \addtogroup attributes
 * @{
 */

/** \def PSA_KEY_ATTRIBUTES_INIT
 *
 * This macro returns a suitable initializer for a key attribute structure
 * of type #psa_key_attributes_t.
 */

/** Return an initial value for a key attributes structure.
 */
static psa_key_attributes_t psa_key_attributes_init(void);

/** Declare a key as persistent and set its key identifier.
 *
 * If the attribute structure currently declares the key as volatile (which
 * is the default content of an attribute structure), this function sets
 * the lifetime attribute to #PSA_KEY_LIFETIME_PERSISTENT.
 *
 * This function does not access storage, it merely stores the given
 * value in the structure.
 * The persistent key will be written to storage when the attribute
 * structure is passed to a key creation function such as
 * psa_import_key(), psa_generate_key(), psa_generate_key_custom(),
 * psa_key_derivation_output_key(), psa_key_derivation_output_key_custom()
 * or psa_copy_key().
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes  The attribute structure to write to.
 * \param key              The persistent identifier for the key.
 *                         This can be any value in the range from
 *                         #PSA_KEY_ID_USER_MIN to #PSA_KEY_ID_USER_MAX
 *                         inclusive.
 */
static void psa_set_key_id(psa_key_attributes_t *attributes,
                           mbedtls_svc_key_id_t key);

#ifdef MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER
/** Set the owner identifier of a key.
 *
 * When key identifiers encode key owner identifiers, psa_set_key_id() does
 * not allow to define in key attributes the owner of volatile keys as
 * psa_set_key_id() enforces the key to be persistent.
 *
 * This function allows to set in key attributes the owner identifier of a
 * key. It is intended to be used for volatile keys. For persistent keys,
 * it is recommended to use the PSA Cryptography API psa_set_key_id() to define
 * the owner of a key.
 *
 * \param[out] attributes  The attribute structure to write to.
 * \param owner            The key owner identifier.
 */
static void mbedtls_set_key_owner_id(psa_key_attributes_t *attributes,
                                     mbedtls_key_owner_id_t owner);
#endif

/** Set the location of a persistent key.
 *
 * To make a key persistent, you must give it a persistent key identifier
 * with psa_set_key_id(). By default, a key that has a persistent identifier
 * is stored in the default storage area identifier by
 * #PSA_KEY_LIFETIME_PERSISTENT. Call this function to choose a storage
 * area, or to explicitly declare the key as volatile.
 *
 * This function does not access storage, it merely stores the given
 * value in the structure.
 * The persistent key will be written to storage when the attribute
 * structure is passed to a key creation function such as
 * psa_import_key(), psa_generate_key(), psa_generate_key_custom(),
 * psa_key_derivation_output_key(), psa_key_derivation_output_key_custom()
 * or psa_copy_key().
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes       The attribute structure to write to.
 * \param lifetime              The lifetime for the key.
 *                              If this is #PSA_KEY_LIFETIME_VOLATILE, the
 *                              key will be volatile, and the key identifier
 *                              attribute is reset to 0.
 */
static void psa_set_key_lifetime(psa_key_attributes_t *attributes,
                                 psa_key_lifetime_t lifetime);

/** Retrieve the key identifier from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The persistent identifier stored in the attribute structure.
 *         This value is unspecified if the attribute structure declares
 *         the key as volatile.
 */
static mbedtls_svc_key_id_t psa_get_key_id(
    const psa_key_attributes_t *attributes);

/** Retrieve the lifetime from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The lifetime value stored in the attribute structure.
 */
static psa_key_lifetime_t psa_get_key_lifetime(
    const psa_key_attributes_t *attributes);

/** Declare usage flags for a key.
 *
 * Usage flags are part of a key's usage policy. They encode what
 * kind of operations are permitted on the key. For more details,
 * refer to the documentation of the type #psa_key_usage_t.
 *
 * This function overwrites any usage flags
 * previously set in \p attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes       The attribute structure to write to.
 * \param usage_flags           The usage flags to write.
 */
static void psa_set_key_usage_flags(psa_key_attributes_t *attributes,
                                    psa_key_usage_t usage_flags);

/** Retrieve the usage flags from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The usage flags stored in the attribute structure.
 */
static psa_key_usage_t psa_get_key_usage_flags(
    const psa_key_attributes_t *attributes);

/** Declare the permitted algorithm policy for a key.
 *
 * The permitted algorithm policy of a key encodes which algorithm or
 * algorithms are permitted to be used with this key. The following
 * algorithm policies are supported:
 * - 0 does not allow any cryptographic operation with the key. The key
 *   may be used for non-cryptographic actions such as exporting (if
 *   permitted by the usage flags).
 * - An algorithm value permits this particular algorithm.
 * - An algorithm wildcard built from #PSA_ALG_ANY_HASH allows the specified
 *   signature scheme with any hash algorithm.
 * - An algorithm built from #PSA_ALG_AT_LEAST_THIS_LENGTH_MAC allows
 *   any MAC algorithm from the same base class (e.g. CMAC) which
 *   generates/verifies a MAC length greater than or equal to the length
 *   encoded in the wildcard algorithm.
 * - An algorithm built from #PSA_ALG_AEAD_WITH_AT_LEAST_THIS_LENGTH_TAG
 *   allows any AEAD algorithm from the same base class (e.g. CCM) which
 *   generates/verifies a tag length greater than or equal to the length
 *   encoded in the wildcard algorithm.
 *
 * This function overwrites any algorithm policy
 * previously set in \p attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes       The attribute structure to write to.
 * \param alg                   The permitted algorithm policy to write.
 */
static void psa_set_key_algorithm(psa_key_attributes_t *attributes,
                                  psa_algorithm_t alg);


/** Retrieve the algorithm policy from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The algorithm stored in the attribute structure.
 */
static psa_algorithm_t psa_get_key_algorithm(
    const psa_key_attributes_t *attributes);

/** Declare the type of a key.
 *
 * This function overwrites any key type
 * previously set in \p attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes       The attribute structure to write to.
 * \param type                  The key type to write.
 *                              If this is 0, the key type in \p attributes
 *                              becomes unspecified.
 */
static void psa_set_key_type(psa_key_attributes_t *attributes,
                             psa_key_type_t type);


/** Declare the size of a key.
 *
 * This function overwrites any key size previously set in \p attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate each of its arguments exactly once.
 *
 * \param[out] attributes       The attribute structure to write to.
 * \param bits                  The key size in bits.
 *                              If this is 0, the key size in \p attributes
 *                              becomes unspecified. Keys of size 0 are
 *                              not supported.
 */
static void psa_set_key_bits(psa_key_attributes_t *attributes,
                             size_t bits);

/** Retrieve the key type from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The key type stored in the attribute structure.
 */
static psa_key_type_t psa_get_key_type(const psa_key_attributes_t *attributes);

/** Retrieve the key size from key attributes.
 *
 * This function may be declared as `static` (i.e. without external
 * linkage). This function may be provided as a function-like macro,
 * but in this case it must evaluate its argument exactly once.
 *
 * \param[in] attributes        The key attribute structure to query.
 *
 * \return The key size stored in the attribute structure, in bits.
 */
static size_t psa_get_key_bits(const psa_key_attributes_t *attributes);

/** Retrieve the attributes of a key.
 *
 * This function first resets the attribute structure as with
 * psa_reset_key_attributes(). It then copies the attributes of
 * the given key into the given attribute structure.
 *
 * \note This function may allocate memory or other resources.
 *       Once you have called this function on an attribute structure,
 *       you must call psa_reset_key_attributes() to free these resources.
 *
 * \param[in] key               Identifier of the key to query.
 * \param[in,out] attributes    On success, the attributes of the key.
 *                              On failure, equivalent to a
 *                              freshly-initialized structure.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_get_key_attributes(mbedtls_svc_key_id_t key,
                                    psa_key_attributes_t *attributes);

/** Reset a key attribute structure to a freshly initialized state.
 *
 * You must initialize the attribute structure as described in the
 * documentation of the type #psa_key_attributes_t before calling this
 * function. Once the structure has been initialized, you may call this
 * function at any time.
 *
 * This function frees any auxiliary resources that the structure
 * may contain.
 *
 * \param[in,out] attributes    The attribute structure to reset.
 */
void psa_reset_key_attributes(psa_key_attributes_t *attributes);

/**@}*/

/** \defgroup key_management Key management
 * @{
 */

/** Remove non-essential copies of key material from memory.
 *
 * If the key identifier designates a volatile key, this functions does not do
 * anything and returns successfully.
 *
 * If the key identifier designates a persistent key, then this function will
 * free all resources associated with the key in volatile memory. The key
 * data in persistent storage is not affected and the key can still be used.
 *
 * \param key Identifier of the key to purge.
 *
 * \retval #PSA_SUCCESS
 *         The key material will have been removed from memory if it is not
 *         currently required.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not a valid key identifier.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_purge_key(mbedtls_svc_key_id_t key);

/** Make a copy of a key.
 *
 * Copy key material from one location to another.
 *
 * This function is primarily useful to copy a key from one location
 * to another, since it populates a key using the material from
 * another key which may have a different lifetime.
 *
 * This function may be used to share a key with a different party,
 * subject to implementation-defined restrictions on key sharing.
 *
 * The policy on the source key must have the usage flag
 * #PSA_KEY_USAGE_COPY set.
 * This flag is sufficient to permit the copy if the key has the lifetime
 * #PSA_KEY_LIFETIME_VOLATILE or #PSA_KEY_LIFETIME_PERSISTENT.
 * Some secure elements do not provide a way to copy a key without
 * making it extractable from the secure element. If a key is located
 * in such a secure element, then the key must have both usage flags
 * #PSA_KEY_USAGE_COPY and #PSA_KEY_USAGE_EXPORT in order to make
 * a copy of the key outside the secure element.
 *
 * The resulting key may only be used in a way that conforms to
 * both the policy of the original key and the policy specified in
 * the \p attributes parameter:
 * - The usage flags on the resulting key are the bitwise-and of the
 *   usage flags on the source policy and the usage flags in \p attributes.
 * - If both allow the same algorithm or wildcard-based
 *   algorithm policy, the resulting key has the same algorithm policy.
 * - If either of the policies allows an algorithm and the other policy
 *   allows a wildcard-based algorithm policy that includes this algorithm,
 *   the resulting key allows the same algorithm.
 * - If the policies do not allow any algorithm in common, this function
 *   fails with the status #PSA_ERROR_INVALID_ARGUMENT.
 *
 * The effect of this function on implementation-defined attributes is
 * implementation-defined.
 *
 * \param source_key        The key to copy. It must allow the usage
 *                          #PSA_KEY_USAGE_COPY. If a private or secret key is
 *                          being copied outside of a secure element it must
 *                          also allow #PSA_KEY_USAGE_EXPORT.
 * \param[in] attributes    The attributes for the new key.
 *                          They are used as follows:
 *                          - The key type and size may be 0. If either is
 *                            nonzero, it must match the corresponding
 *                            attribute of the source key.
 *                          - The key location (the lifetime and, for
 *                            persistent keys, the key identifier) is
 *                            used directly.
 *                          - The policy constraints (usage flags and
 *                            algorithm policy) are combined from
 *                            the source key and \p attributes so that
 *                            both sets of restrictions apply, as
 *                            described in the documentation of this function.
 * \param[out] target_key   On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         \p source_key is invalid.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The lifetime or identifier in \p attributes are invalid, or
 *         the policy constraints on the source and specified in
 *         \p attributes are incompatible, or
 *         \p attributes specifies a key type or key size
 *         which does not match the attributes of the source key.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The source key does not have the #PSA_KEY_USAGE_COPY usage flag, or
 *         the source key is not exportable and its lifetime does not
 *         allow copying it to the target's lifetime.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_copy_key(mbedtls_svc_key_id_t source_key,
                          const psa_key_attributes_t *attributes,
                          mbedtls_svc_key_id_t *target_key);


/**
 * \brief Destroy a key.
 *
 * This function destroys a key from both volatile
 * memory and, if applicable, non-volatile storage. Implementations shall
 * make a best effort to ensure that the key material cannot be recovered.
 *
 * This function also erases any metadata such as policies and frees
 * resources associated with the key.
 *
 * If a key is currently in use in a multipart operation, then destroying the
 * key will cause the multipart operation to fail.
 *
 * \warning    We can only guarantee that the the key material will
 *             eventually be wiped from memory. With threading enabled
 *             and during concurrent execution, copies of the key material may
 *             still exist until all threads have finished using the key.
 *
 * \param key  Identifier of the key to erase. If this is \c 0, do nothing and
 *             return #PSA_SUCCESS.
 *
 * \retval #PSA_SUCCESS
 *         \p key was a valid identifier and the key material that it
 *         referred to has been erased. Alternatively, \p key is \c 0.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key cannot be erased because it is
 *         read-only, either due to a policy or due to physical restrictions.
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         \p key is not a valid identifier nor \c 0.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE
 *         There was a failure in communication with the cryptoprocessor.
 *         The key material may still be present in the cryptoprocessor.
 * \retval #PSA_ERROR_DATA_INVALID
 *         This error is typically a result of either storage corruption on a
 *         cleartext storage backend, or an attempt to read data that was
 *         written by an incompatible version of the library.
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The storage is corrupted. Implementations shall make a best effort
 *         to erase key material even in this stage, however applications
 *         should be aware that it may be impossible to guarantee that the
 *         key material is not recoverable in such cases.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *         An unexpected condition which is not a storage corruption or
 *         a communication failure occurred. The cryptoprocessor may have
 *         been compromised.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_destroy_key(mbedtls_svc_key_id_t key);

/**@}*/

/** \defgroup import_export Key import and export
 * @{
 */

/**
 * \brief Import a key in binary format.
 *
 * This function supports any output from psa_export_key(). Refer to the
 * documentation of psa_export_public_key() for the format of public keys
 * and to the documentation of psa_export_key() for the format for
 * other key types.
 *
 * The key data determines the key size. The attributes may optionally
 * specify a key size; in this case it must match the size determined
 * from the key data. A key size of 0 in \p attributes indicates that
 * the key size is solely determined by the key data.
 *
 * Implementations must reject an attempt to import a key of size 0.
 *
 * This specification supports a single format for each key type.
 * Implementations may support other formats as long as the standard
 * format is supported. Implementations that support other formats
 * should ensure that the formats are clearly unambiguous so as to
 * minimize the risk that an invalid input is accidentally interpreted
 * according to a different format.
 *
 * \param[in] attributes    The attributes for the new key.
 *                          The key size is always determined from the
 *                          \p data buffer.
 *                          If the key size in \p attributes is nonzero,
 *                          it must be equal to the size from \p data.
 * \param[out] key          On success, an identifier to the newly created key.
 *                          For persistent keys, this is the key identifier
 *                          defined in \p attributes.
 *                          \c 0 on failure.
 * \param[in] data    Buffer containing the key data. The content of this
 *                    buffer is interpreted according to the type declared
 *                    in \p attributes.
 *                    All implementations must support at least the format
 *                    described in the documentation
 *                    of psa_export_key() or psa_export_public_key() for
 *                    the chosen type. Implementations may allow other
 *                    formats, but should be conservative: implementations
 *                    should err on the side of rejecting content if it
 *                    may be erroneous (e.g. wrong type or truncated data).
 * \param data_length Size of the \p data buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The key type or key size is not supported, either by the
 *         implementation in general or in this particular persistent location.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key attributes, as a whole, are invalid, or
 *         the key data is not correctly formatted, or
 *         the size in \p attributes is nonzero and does not match the size
 *         of the key data.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_import_key(const psa_key_attributes_t *attributes,
                            const uint8_t *data,
                            size_t data_length,
                            mbedtls_svc_key_id_t *key);



/**
 * \brief Export a key in binary format.
 *
 * The output of this function can be passed to psa_import_key() to
 * create an equivalent object.
 *
 * If the implementation of psa_import_key() supports other formats
 * beyond the format specified here, the output from psa_export_key()
 * must use the representation specified here, not the original
 * representation.
 *
 * For standard key types, the output format is as follows:
 *
 * - For symmetric keys (including MAC keys), the format is the
 *   raw bytes of the key.
 * - For DES, the key data consists of 8 bytes. The parity bits must be
 *   correct.
 * - For Triple-DES, the format is the concatenation of the
 *   two or three DES keys.
 * - For RSA key pairs (#PSA_KEY_TYPE_RSA_KEY_PAIR), the format
 *   is the non-encrypted DER encoding of the representation defined by
 *   PKCS\#1 (RFC 8017) as `RSAPrivateKey`, version 0.
 *   ```
 *   RSAPrivateKey ::= SEQUENCE {
 *       version             INTEGER,  -- must be 0
 *       modulus             INTEGER,  -- n
 *       publicExponent      INTEGER,  -- e
 *       privateExponent     INTEGER,  -- d
 *       prime1              INTEGER,  -- p
 *       prime2              INTEGER,  -- q
 *       exponent1           INTEGER,  -- d mod (p-1)
 *       exponent2           INTEGER,  -- d mod (q-1)
 *       coefficient         INTEGER,  -- (inverse of q) mod p
 *   }
 *   ```
 * - For elliptic curve key pairs (key types for which
 *   #PSA_KEY_TYPE_IS_ECC_KEY_PAIR is true), the format is
 *   a representation of the private value as a `ceiling(m/8)`-byte string
 *   where `m` is the bit size associated with the curve, i.e. the bit size
 *   of the order of the curve's coordinate field. This byte string is
 *   in little-endian order for Montgomery curves (curve types
 *   `PSA_ECC_FAMILY_CURVEXXX`), and in big-endian order for Weierstrass
 *   curves (curve types `PSA_ECC_FAMILY_SECTXXX`, `PSA_ECC_FAMILY_SECPXXX`
 *   and `PSA_ECC_FAMILY_BRAINPOOL_PXXX`).
 *   For Weierstrass curves, this is the content of the `privateKey` field of
 *   the `ECPrivateKey` format defined by RFC 5915.  For Montgomery curves,
 *   the format is defined by RFC 7748, and output is masked according to §5.
 *   For twisted Edwards curves, the private key is as defined by RFC 8032
 *   (a 32-byte string for Edwards25519, a 57-byte string for Edwards448).
 * - For Diffie-Hellman key exchange key pairs (key types for which
 *   #PSA_KEY_TYPE_IS_DH_KEY_PAIR is true), the
 *   format is the representation of the private key `x` as a big-endian byte
 *   string. The length of the byte string is the private key size in bytes
 *   (leading zeroes are not stripped).
 * - For public keys (key types for which #PSA_KEY_TYPE_IS_PUBLIC_KEY is
 *   true), the format is the same as for psa_export_public_key().
 *
 * The policy on the key must have the usage flag #PSA_KEY_USAGE_EXPORT set.
 *
 * \param key               Identifier of the key to export. It must allow the
 *                          usage #PSA_KEY_USAGE_EXPORT, unless it is a public
 *                          key.
 * \param[out] data         Buffer where the key data is to be written.
 * \param data_size         Size of the \p data buffer in bytes.
 * \param[out] data_length  On success, the number of bytes
 *                          that make up the key data.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key does not have the #PSA_KEY_USAGE_EXPORT flag.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p data buffer is too small. You can determine a
 *         sufficient buffer size by calling
 *         #PSA_EXPORT_KEY_OUTPUT_SIZE(\c type, \c bits)
 *         where \c type is the key type
 *         and \c bits is the key size in bits.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_export_key(mbedtls_svc_key_id_t key,
                            uint8_t *data,
                            size_t data_size,
                            size_t *data_length);

/**
 * \brief Export a public key or the public part of a key pair in binary format.
 *
 * The output of this function can be passed to psa_import_key() to
 * create an object that is equivalent to the public key.
 *
 * This specification supports a single format for each key type.
 * Implementations may support other formats as long as the standard
 * format is supported. Implementations that support other formats
 * should ensure that the formats are clearly unambiguous so as to
 * minimize the risk that an invalid input is accidentally interpreted
 * according to a different format.
 *
 * For standard key types, the output format is as follows:
 * - For RSA public keys (#PSA_KEY_TYPE_RSA_PUBLIC_KEY), the DER encoding of
 *   the representation defined by RFC 3279 &sect;2.3.1 as `RSAPublicKey`.
 *   ```
 *   RSAPublicKey ::= SEQUENCE {
 *      modulus            INTEGER,    -- n
 *      publicExponent     INTEGER  }  -- e
 *   ```
 * - For elliptic curve keys on a twisted Edwards curve (key types for which
 *   #PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY is true and #PSA_KEY_TYPE_ECC_GET_FAMILY
 *   returns #PSA_ECC_FAMILY_TWISTED_EDWARDS), the public key is as defined
 *   by RFC 8032
 *   (a 32-byte string for Edwards25519, a 57-byte string for Edwards448).
 * - For other elliptic curve public keys (key types for which
 *   #PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY is true), the format is the uncompressed
 *   representation defined by SEC1 &sect;2.3.3 as the content of an ECPoint.
 *   Let `m` be the bit size associated with the curve, i.e. the bit size of
 *   `q` for a curve over `F_q`. The representation consists of:
 *      - The byte 0x04;
 *      - `x_P` as a `ceiling(m/8)`-byte string, big-endian;
 *      - `y_P` as a `ceiling(m/8)`-byte string, big-endian.
 * - For Diffie-Hellman key exchange public keys (key types for which
 *   #PSA_KEY_TYPE_IS_DH_PUBLIC_KEY is true),
 *   the format is the representation of the public key `y = g^x mod p` as a
 *   big-endian byte string. The length of the byte string is the length of the
 *   base prime `p` in bytes.
 *
 * Exporting a public key object or the public part of a key pair is
 * always permitted, regardless of the key's usage flags.
 *
 * \param key               Identifier of the key to export.
 * \param[out] data         Buffer where the key data is to be written.
 * \param data_size         Size of the \p data buffer in bytes.
 * \param[out] data_length  On success, the number of bytes
 *                          that make up the key data.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key is neither a public key nor a key pair.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p data buffer is too small. You can determine a
 *         sufficient buffer size by calling
 *         #PSA_EXPORT_KEY_OUTPUT_SIZE(#PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(\c type), \c bits)
 *         where \c type is the key type
 *         and \c bits is the key size in bits.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_export_public_key(mbedtls_svc_key_id_t key,
                                   uint8_t *data,
                                   size_t data_size,
                                   size_t *data_length);



/**@}*/

/** \defgroup hash Message digests
 * @{
 */

/** Calculate the hash (digest) of a message.
 *
 * \note To verify the hash of a message against an
 *       expected value, use psa_hash_compare() instead.
 *
 * \param alg               The hash algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_HASH(\p alg) is true).
 * \param[in] input         Buffer containing the message to hash.
 * \param input_length      Size of the \p input buffer in bytes.
 * \param[out] hash         Buffer where the hash is to be written.
 * \param hash_size         Size of the \p hash buffer in bytes.
 * \param[out] hash_length  On success, the number of bytes
 *                          that make up the hash value. This is always
 *                          #PSA_HASH_LENGTH(\p alg).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a hash algorithm.
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p hash_size is too small
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_compute(psa_algorithm_t alg,
                              const uint8_t *input,
                              size_t input_length,
                              uint8_t *hash,
                              size_t hash_size,
                              size_t *hash_length);

/** Calculate the hash (digest) of a message and compare it with a
 * reference value.
 *
 * \param alg               The hash algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_HASH(\p alg) is true).
 * \param[in] input         Buffer containing the message to hash.
 * \param input_length      Size of the \p input buffer in bytes.
 * \param[in] hash          Buffer containing the expected hash value.
 * \param hash_length       Size of the \p hash buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The expected hash is identical to the actual hash of the input.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The hash of the message was calculated successfully, but it
 *         differs from the expected hash.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a hash algorithm.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p input_length or \p hash_length do not match the hash size for \p alg
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_compare(psa_algorithm_t alg,
                              const uint8_t *input,
                              size_t input_length,
                              const uint8_t *hash,
                              size_t hash_length);

/** The type of the state data structure for multipart hash operations.
 *
 * Before calling any function on a hash operation object, the application must
 * initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_hash_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_hash_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_HASH_OPERATION_INIT,
 *   for example:
 *   \code
 *   psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function psa_hash_operation_init()
 *   to the structure, for example:
 *   \code
 *   psa_hash_operation_t operation;
 *   operation = psa_hash_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_hash_operation_s psa_hash_operation_t;

/** \def PSA_HASH_OPERATION_INIT
 *
 * This macro returns a suitable initializer for a hash operation object
 * of type #psa_hash_operation_t.
 */

/** Return an initial value for a hash operation object.
 */
static psa_hash_operation_t psa_hash_operation_init(void);

/** Set up a multipart hash operation.
 *
 * The sequence of operations to calculate a hash (message digest)
 * is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_hash_operation_t, e.g. #PSA_HASH_OPERATION_INIT.
 * -# Call psa_hash_setup() to specify the algorithm.
 * -# Call psa_hash_update() zero, one or more times, passing a fragment
 *    of the message each time. The hash that is calculated is the hash
 *    of the concatenation of these messages in order.
 * -# To calculate the hash, call psa_hash_finish().
 *    To compare the hash with an expected value, call psa_hash_verify().
 *
 * If an error occurs at any step after a call to psa_hash_setup(), the
 * operation will need to be reset by a call to psa_hash_abort(). The
 * application may call psa_hash_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_hash_setup(), the application must
 * eventually terminate the operation. The following events terminate an
 * operation:
 * - A successful call to psa_hash_finish() or psa_hash_verify().
 * - A call to psa_hash_abort().
 *
 * \param[in,out] operation The operation object to set up. It must have
 *                          been initialized as per the documentation for
 *                          #psa_hash_operation_t and not yet in use.
 * \param alg               The hash algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_HASH(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not a supported hash algorithm.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p alg is not a hash algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_setup(psa_hash_operation_t *operation,
                            psa_algorithm_t alg);

/** Add a message fragment to a multipart hash operation.
 *
 * The application must call psa_hash_setup() before calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_hash_abort().
 *
 * \param[in,out] operation Active hash operation.
 * \param[in] input         Buffer containing the message fragment to hash.
 * \param input_length      Size of the \p input buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_update(psa_hash_operation_t *operation,
                             const uint8_t *input,
                             size_t input_length);

/** Finish the calculation of the hash of a message.
 *
 * The application must call psa_hash_setup() before calling this function.
 * This function calculates the hash of the message formed by concatenating
 * the inputs passed to preceding calls to psa_hash_update().
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_hash_abort().
 *
 * \warning Applications should not call this function if they expect
 *          a specific value for the hash. Call psa_hash_verify() instead.
 *          Beware that comparing integrity or authenticity data such as
 *          hash values with a function such as \c memcmp is risky
 *          because the time taken by the comparison may leak information
 *          about the hashed data which could allow an attacker to guess
 *          a valid hash and thereby bypass security controls.
 *
 * \param[in,out] operation     Active hash operation.
 * \param[out] hash             Buffer where the hash is to be written.
 * \param hash_size             Size of the \p hash buffer in bytes.
 * \param[out] hash_length      On success, the number of bytes
 *                              that make up the hash value. This is always
 *                              #PSA_HASH_LENGTH(\c alg) where \c alg is the
 *                              hash algorithm that is calculated.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p hash buffer is too small. You can determine a
 *         sufficient buffer size by calling #PSA_HASH_LENGTH(\c alg)
 *         where \c alg is the hash algorithm that is calculated.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_finish(psa_hash_operation_t *operation,
                             uint8_t *hash,
                             size_t hash_size,
                             size_t *hash_length);

/** Finish the calculation of the hash of a message and compare it with
 * an expected value.
 *
 * The application must call psa_hash_setup() before calling this function.
 * This function calculates the hash of the message formed by concatenating
 * the inputs passed to preceding calls to psa_hash_update(). It then
 * compares the calculated hash with the expected hash passed as a
 * parameter to this function.
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_hash_abort().
 *
 * \note Implementations shall make the best effort to ensure that the
 * comparison between the actual hash and the expected hash is performed
 * in constant time.
 *
 * \param[in,out] operation     Active hash operation.
 * \param[in] hash              Buffer containing the expected hash value.
 * \param hash_length           Size of the \p hash buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The expected hash is identical to the actual hash of the message.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The hash of the message was calculated successfully, but it
 *         differs from the expected hash.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_verify(psa_hash_operation_t *operation,
                             const uint8_t *hash,
                             size_t hash_length);

/** Abort a hash operation.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation by calling
 * psa_hash_setup() again.
 *
 * You may call this function any time after the operation object has
 * been initialized by one of the methods described in #psa_hash_operation_t.
 *
 * In particular, calling psa_hash_abort() after the operation has been
 * terminated by a call to psa_hash_abort(), psa_hash_finish() or
 * psa_hash_verify() is safe and has no effect.
 *
 * \param[in,out] operation     Initialized hash operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_abort(psa_hash_operation_t *operation);

/** Clone a hash operation.
 *
 * This function copies the state of an ongoing hash operation to
 * a new operation object. In other words, this function is equivalent
 * to calling psa_hash_setup() on \p target_operation with the same
 * algorithm that \p source_operation was set up for, then
 * psa_hash_update() on \p target_operation with the same input that
 * that was passed to \p source_operation. After this function returns, the
 * two objects are independent, i.e. subsequent calls involving one of
 * the objects do not affect the other object.
 *
 * \param[in] source_operation      The active hash operation to clone.
 * \param[in,out] target_operation  The operation object to set up.
 *                                  It must be initialized but not active.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The \p source_operation state is not valid (it must be active), or
 *         the \p target_operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_hash_clone(const psa_hash_operation_t *source_operation,
                            psa_hash_operation_t *target_operation);

/**@}*/

/** \defgroup MAC Message authentication codes
 * @{
 */

/** Calculate the MAC (message authentication code) of a message.
 *
 * \note To verify the MAC of a message against an
 *       expected value, use psa_mac_verify() instead.
 *       Beware that comparing integrity or authenticity data such as
 *       MAC values with a function such as \c memcmp is risky
 *       because the time taken by the comparison may leak information
 *       about the MAC value which could allow an attacker to guess
 *       a valid MAC and thereby bypass security controls.
 *
 * \param key               Identifier of the key to use for the operation. It
 *                          must allow the usage PSA_KEY_USAGE_SIGN_MESSAGE.
 * \param alg               The MAC algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_MAC(\p alg) is true).
 * \param[in] input         Buffer containing the input message.
 * \param input_length      Size of the \p input buffer in bytes.
 * \param[out] mac          Buffer where the MAC value is to be written.
 * \param mac_size          Size of the \p mac buffer in bytes.
 * \param[out] mac_length   On success, the number of bytes
 *                          that make up the MAC value.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a MAC algorithm.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p mac_size is too small
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The key could not be retrieved from storage.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_compute(mbedtls_svc_key_id_t key,
                             psa_algorithm_t alg,
                             const uint8_t *input,
                             size_t input_length,
                             uint8_t *mac,
                             size_t mac_size,
                             size_t *mac_length);

/** Calculate the MAC of a message and compare it with a reference value.
 *
 * \param key               Identifier of the key to use for the operation. It
 *                          must allow the usage PSA_KEY_USAGE_VERIFY_MESSAGE.
 * \param alg               The MAC algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_MAC(\p alg) is true).
 * \param[in] input         Buffer containing the input message.
 * \param input_length      Size of the \p input buffer in bytes.
 * \param[in] mac           Buffer containing the expected MAC value.
 * \param mac_length        Size of the \p mac buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The expected MAC is identical to the actual MAC of the input.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The MAC of the message was calculated successfully, but it
 *         differs from the expected value.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a MAC algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The key could not be retrieved from storage.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_verify(mbedtls_svc_key_id_t key,
                            psa_algorithm_t alg,
                            const uint8_t *input,
                            size_t input_length,
                            const uint8_t *mac,
                            size_t mac_length);

/** The type of the state data structure for multipart MAC operations.
 *
 * Before calling any function on a MAC operation object, the application must
 * initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_mac_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_mac_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_MAC_OPERATION_INIT,
 *   for example:
 *   \code
 *   psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function psa_mac_operation_init()
 *   to the structure, for example:
 *   \code
 *   psa_mac_operation_t operation;
 *   operation = psa_mac_operation_init();
 *   \endcode
 *
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_mac_operation_s psa_mac_operation_t;

/** \def PSA_MAC_OPERATION_INIT
 *
 * This macro returns a suitable initializer for a MAC operation object of type
 * #psa_mac_operation_t.
 */

/** Return an initial value for a MAC operation object.
 */
static psa_mac_operation_t psa_mac_operation_init(void);

/** Set up a multipart MAC calculation operation.
 *
 * This function sets up the calculation of the MAC
 * (message authentication code) of a byte string.
 * To verify the MAC of a message against an
 * expected value, use psa_mac_verify_setup() instead.
 *
 * The sequence of operations to calculate a MAC is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_mac_operation_t, e.g. #PSA_MAC_OPERATION_INIT.
 * -# Call psa_mac_sign_setup() to specify the algorithm and key.
 * -# Call psa_mac_update() zero, one or more times, passing a fragment
 *    of the message each time. The MAC that is calculated is the MAC
 *    of the concatenation of these messages in order.
 * -# At the end of the message, call psa_mac_sign_finish() to finish
 *    calculating the MAC value and retrieve it.
 *
 * If an error occurs at any step after a call to psa_mac_sign_setup(), the
 * operation will need to be reset by a call to psa_mac_abort(). The
 * application may call psa_mac_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_mac_sign_setup(), the application must
 * eventually terminate the operation through one of the following methods:
 * - A successful call to psa_mac_sign_finish().
 * - A call to psa_mac_abort().
 *
 * \param[in,out] operation The operation object to set up. It must have
 *                          been initialized as per the documentation for
 *                          #psa_mac_operation_t and not yet in use.
 * \param key               Identifier of the key to use for the operation. It
 *                          must remain valid until the operation terminates.
 *                          It must allow the usage PSA_KEY_USAGE_SIGN_MESSAGE.
 * \param alg               The MAC algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_MAC(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a MAC algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The key could not be retrieved from storage.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_sign_setup(psa_mac_operation_t *operation,
                                mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg);

/** Set up a multipart MAC verification operation.
 *
 * This function sets up the verification of the MAC
 * (message authentication code) of a byte string against an expected value.
 *
 * The sequence of operations to verify a MAC is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_mac_operation_t, e.g. #PSA_MAC_OPERATION_INIT.
 * -# Call psa_mac_verify_setup() to specify the algorithm and key.
 * -# Call psa_mac_update() zero, one or more times, passing a fragment
 *    of the message each time. The MAC that is calculated is the MAC
 *    of the concatenation of these messages in order.
 * -# At the end of the message, call psa_mac_verify_finish() to finish
 *    calculating the actual MAC of the message and verify it against
 *    the expected value.
 *
 * If an error occurs at any step after a call to psa_mac_verify_setup(), the
 * operation will need to be reset by a call to psa_mac_abort(). The
 * application may call psa_mac_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_mac_verify_setup(), the application must
 * eventually terminate the operation through one of the following methods:
 * - A successful call to psa_mac_verify_finish().
 * - A call to psa_mac_abort().
 *
 * \param[in,out] operation The operation object to set up. It must have
 *                          been initialized as per the documentation for
 *                          #psa_mac_operation_t and not yet in use.
 * \param key               Identifier of the key to use for the operation. It
 *                          must remain valid until the operation terminates.
 *                          It must allow the usage
 *                          PSA_KEY_USAGE_VERIFY_MESSAGE.
 * \param alg               The MAC algorithm to compute (\c PSA_ALG_XXX value
 *                          such that #PSA_ALG_IS_MAC(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c key is not compatible with \c alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \c alg is not supported or is not a MAC algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The key could not be retrieved from storage.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_verify_setup(psa_mac_operation_t *operation,
                                  mbedtls_svc_key_id_t key,
                                  psa_algorithm_t alg);

/** Add a message fragment to a multipart MAC operation.
 *
 * The application must call psa_mac_sign_setup() or psa_mac_verify_setup()
 * before calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_mac_abort().
 *
 * \param[in,out] operation Active MAC operation.
 * \param[in] input         Buffer containing the message fragment to add to
 *                          the MAC calculation.
 * \param input_length      Size of the \p input buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_update(psa_mac_operation_t *operation,
                            const uint8_t *input,
                            size_t input_length);

/** Finish the calculation of the MAC of a message.
 *
 * The application must call psa_mac_sign_setup() before calling this function.
 * This function calculates the MAC of the message formed by concatenating
 * the inputs passed to preceding calls to psa_mac_update().
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_mac_abort().
 *
 * \warning Applications should not call this function if they expect
 *          a specific value for the MAC. Call psa_mac_verify_finish() instead.
 *          Beware that comparing integrity or authenticity data such as
 *          MAC values with a function such as \c memcmp is risky
 *          because the time taken by the comparison may leak information
 *          about the MAC value which could allow an attacker to guess
 *          a valid MAC and thereby bypass security controls.
 *
 * \param[in,out] operation Active MAC operation.
 * \param[out] mac          Buffer where the MAC value is to be written.
 * \param mac_size          Size of the \p mac buffer in bytes.
 * \param[out] mac_length   On success, the number of bytes
 *                          that make up the MAC value. This is always
 *                          #PSA_MAC_LENGTH(\c key_type, \c key_bits, \c alg)
 *                          where \c key_type and \c key_bits are the type and
 *                          bit-size respectively of the key and \c alg is the
 *                          MAC algorithm that is calculated.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p mac buffer is too small. You can determine a
 *         sufficient buffer size by calling PSA_MAC_LENGTH().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active mac sign
 *         operation), or the library has not been previously initialized
 *         by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_sign_finish(psa_mac_operation_t *operation,
                                 uint8_t *mac,
                                 size_t mac_size,
                                 size_t *mac_length);

/** Finish the calculation of the MAC of a message and compare it with
 * an expected value.
 *
 * The application must call psa_mac_verify_setup() before calling this function.
 * This function calculates the MAC of the message formed by concatenating
 * the inputs passed to preceding calls to psa_mac_update(). It then
 * compares the calculated MAC with the expected MAC passed as a
 * parameter to this function.
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_mac_abort().
 *
 * \note Implementations shall make the best effort to ensure that the
 * comparison between the actual MAC and the expected MAC is performed
 * in constant time.
 *
 * \param[in,out] operation Active MAC operation.
 * \param[in] mac           Buffer containing the expected MAC value.
 * \param mac_length        Size of the \p mac buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The expected MAC is identical to the actual MAC of the message.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The MAC of the message was calculated successfully, but it
 *         differs from the expected MAC.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active mac verify
 *         operation), or the library has not been previously initialized
 *         by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_verify_finish(psa_mac_operation_t *operation,
                                   const uint8_t *mac,
                                   size_t mac_length);

/** Abort a MAC operation.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation by calling
 * psa_mac_sign_setup() or psa_mac_verify_setup() again.
 *
 * You may call this function any time after the operation object has
 * been initialized by one of the methods described in #psa_mac_operation_t.
 *
 * In particular, calling psa_mac_abort() after the operation has been
 * terminated by a call to psa_mac_abort(), psa_mac_sign_finish() or
 * psa_mac_verify_finish() is safe and has no effect.
 *
 * \param[in,out] operation Initialized MAC operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_mac_abort(psa_mac_operation_t *operation);

/**@}*/

/** \defgroup cipher Symmetric ciphers
 * @{
 */

/** Encrypt a message using a symmetric cipher.
 *
 * This function encrypts a message with a random IV (initialization
 * vector). Use the multipart operation interface with a
 * #psa_cipher_operation_t object to provide other forms of IV.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              It must allow the usage #PSA_KEY_USAGE_ENCRYPT.
 * \param alg                   The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 * \param[in] input             Buffer containing the message to encrypt.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the output is to be written.
 *                              The output contains the IV followed by
 *                              the ciphertext proper.
 * \param output_size           Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a cipher algorithm.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_encrypt(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input,
                                size_t input_length,
                                uint8_t *output,
                                size_t output_size,
                                size_t *output_length);

/** Decrypt a message using a symmetric cipher.
 *
 * This function decrypts a message encrypted with a symmetric cipher.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              It must remain valid until the operation
 *                              terminates. It must allow the usage
 *                              #PSA_KEY_USAGE_DECRYPT.
 * \param alg                   The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 * \param[in] input             Buffer containing the message to decrypt.
 *                              This consists of the IV followed by the
 *                              ciphertext proper.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the plaintext is to be written.
 * \param output_size           Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a cipher algorithm.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_decrypt(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input,
                                size_t input_length,
                                uint8_t *output,
                                size_t output_size,
                                size_t *output_length);

/** The type of the state data structure for multipart cipher operations.
 *
 * Before calling any function on a cipher operation object, the application
 * must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_cipher_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_cipher_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_CIPHER_OPERATION_INIT,
 *   for example:
 *   \code
 *   psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function psa_cipher_operation_init()
 *   to the structure, for example:
 *   \code
 *   psa_cipher_operation_t operation;
 *   operation = psa_cipher_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_cipher_operation_s psa_cipher_operation_t;

/** \def PSA_CIPHER_OPERATION_INIT
 *
 * This macro returns a suitable initializer for a cipher operation object of
 * type #psa_cipher_operation_t.
 */

/** Return an initial value for a cipher operation object.
 */
static psa_cipher_operation_t psa_cipher_operation_init(void);

/** Set the key for a multipart symmetric encryption operation.
 *
 * The sequence of operations to encrypt a message with a symmetric cipher
 * is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_cipher_operation_t, e.g.
 *    #PSA_CIPHER_OPERATION_INIT.
 * -# Call psa_cipher_encrypt_setup() to specify the algorithm and key.
 * -# Call either psa_cipher_generate_iv() or psa_cipher_set_iv() to
 *    generate or set the IV (initialization vector). You should use
 *    psa_cipher_generate_iv() unless the protocol you are implementing
 *    requires a specific IV value.
 * -# Call psa_cipher_update() zero, one or more times, passing a fragment
 *    of the message each time.
 * -# Call psa_cipher_finish().
 *
 * If an error occurs at any step after a call to psa_cipher_encrypt_setup(),
 * the operation will need to be reset by a call to psa_cipher_abort(). The
 * application may call psa_cipher_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_cipher_encrypt_setup(), the application must
 * eventually terminate the operation. The following events terminate an
 * operation:
 * - A successful call to psa_cipher_finish().
 * - A call to psa_cipher_abort().
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized as per the documentation for
 *                              #psa_cipher_operation_t and not yet in use.
 * \param key                   Identifier of the key to use for the operation.
 *                              It must remain valid until the operation
 *                              terminates. It must allow the usage
 *                              #PSA_KEY_USAGE_ENCRYPT.
 * \param alg                   The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a cipher algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_encrypt_setup(psa_cipher_operation_t *operation,
                                      mbedtls_svc_key_id_t key,
                                      psa_algorithm_t alg);

/** Set the key for a multipart symmetric decryption operation.
 *
 * The sequence of operations to decrypt a message with a symmetric cipher
 * is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_cipher_operation_t, e.g.
 *    #PSA_CIPHER_OPERATION_INIT.
 * -# Call psa_cipher_decrypt_setup() to specify the algorithm and key.
 * -# Call psa_cipher_set_iv() with the IV (initialization vector) for the
 *    decryption. If the IV is prepended to the ciphertext, you can call
 *    psa_cipher_update() on a buffer containing the IV followed by the
 *    beginning of the message.
 * -# Call psa_cipher_update() zero, one or more times, passing a fragment
 *    of the message each time.
 * -# Call psa_cipher_finish().
 *
 * If an error occurs at any step after a call to psa_cipher_decrypt_setup(),
 * the operation will need to be reset by a call to psa_cipher_abort(). The
 * application may call psa_cipher_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_cipher_decrypt_setup(), the application must
 * eventually terminate the operation. The following events terminate an
 * operation:
 * - A successful call to psa_cipher_finish().
 * - A call to psa_cipher_abort().
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized as per the documentation for
 *                              #psa_cipher_operation_t and not yet in use.
 * \param key                   Identifier of the key to use for the operation.
 *                              It must remain valid until the operation
 *                              terminates. It must allow the usage
 *                              #PSA_KEY_USAGE_DECRYPT.
 * \param alg                   The cipher algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_CIPHER(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not a cipher algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_decrypt_setup(psa_cipher_operation_t *operation,
                                      mbedtls_svc_key_id_t key,
                                      psa_algorithm_t alg);

/** Generate an IV for a symmetric encryption operation.
 *
 * This function generates a random IV (initialization vector), nonce
 * or initial counter value for the encryption operation as appropriate
 * for the chosen algorithm, key type and key size.
 *
 * The application must call psa_cipher_encrypt_setup() before
 * calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_cipher_abort().
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[out] iv               Buffer where the generated IV is to be written.
 * \param iv_size               Size of the \p iv buffer in bytes.
 * \param[out] iv_length        On success, the number of bytes of the
 *                              generated IV.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p iv buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, with no IV set),
 *         or the library has not been previously initialized
 *         by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_generate_iv(psa_cipher_operation_t *operation,
                                    uint8_t *iv,
                                    size_t iv_size,
                                    size_t *iv_length);

/** Set the IV for a symmetric encryption or decryption operation.
 *
 * This function sets the IV (initialization vector), nonce
 * or initial counter value for the encryption or decryption operation.
 *
 * The application must call psa_cipher_encrypt_setup() before
 * calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_cipher_abort().
 *
 * \note When encrypting, applications should use psa_cipher_generate_iv()
 * instead of this function, unless implementing a protocol that requires
 * a non-random IV.
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[in] iv                Buffer containing the IV to use.
 * \param iv_length             Size of the IV in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size of \p iv is not acceptable for the chosen algorithm,
 *         or the chosen algorithm does not use an IV.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active cipher
 *         encrypt operation, with no IV set), or the library has not been
 *         previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_set_iv(psa_cipher_operation_t *operation,
                               const uint8_t *iv,
                               size_t iv_length);

/** Encrypt or decrypt a message fragment in an active cipher operation.
 *
 * Before calling this function, you must:
 * 1. Call either psa_cipher_encrypt_setup() or psa_cipher_decrypt_setup().
 *    The choice of setup function determines whether this function
 *    encrypts or decrypts its input.
 * 2. If the algorithm requires an IV, call psa_cipher_generate_iv()
 *    (recommended when encrypting) or psa_cipher_set_iv().
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_cipher_abort().
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[in] input             Buffer containing the message fragment to
 *                              encrypt or decrypt.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the output is to be written.
 * \param output_size           Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, with an IV set
 *         if required for the algorithm), or the library has not been
 *         previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_update(psa_cipher_operation_t *operation,
                               const uint8_t *input,
                               size_t input_length,
                               uint8_t *output,
                               size_t output_size,
                               size_t *output_length);

/** Finish encrypting or decrypting a message in a cipher operation.
 *
 * The application must call psa_cipher_encrypt_setup() or
 * psa_cipher_decrypt_setup() before calling this function. The choice
 * of setup function determines whether this function encrypts or
 * decrypts its input.
 *
 * This function finishes the encryption or decryption of the message
 * formed by concatenating the inputs passed to preceding calls to
 * psa_cipher_update().
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_cipher_abort().
 *
 * \param[in,out] operation     Active cipher operation.
 * \param[out] output           Buffer where the output is to be written.
 * \param output_size           Size of the \p output buffer in bytes.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
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
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, with an IV set
 *         if required for the algorithm), or the library has not been
 *         previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_finish(psa_cipher_operation_t *operation,
                               uint8_t *output,
                               size_t output_size,
                               size_t *output_length);

/** Abort a cipher operation.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation by calling
 * psa_cipher_encrypt_setup() or psa_cipher_decrypt_setup() again.
 *
 * You may call this function any time after the operation object has
 * been initialized as described in #psa_cipher_operation_t.
 *
 * In particular, calling psa_cipher_abort() after the operation has been
 * terminated by a call to psa_cipher_abort() or psa_cipher_finish()
 * is safe and has no effect.
 *
 * \param[in,out] operation     Initialized cipher operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_cipher_abort(psa_cipher_operation_t *operation);

/**@}*/

/** \defgroup aead Authenticated encryption with associated data (AEAD)
 * @{
 */

/** Process an authenticated encryption operation.
 *
 * \param key                     Identifier of the key to use for the
 *                                operation. It must allow the usage
 *                                #PSA_KEY_USAGE_ENCRYPT.
 * \param alg                     The AEAD algorithm to compute
 *                                (\c PSA_ALG_XXX value such that
 *                                #PSA_ALG_IS_AEAD(\p alg) is true).
 * \param[in] nonce               Nonce or IV to use.
 * \param nonce_length            Size of the \p nonce buffer in bytes.
 * \param[in] additional_data     Additional data that will be authenticated
 *                                but not encrypted.
 * \param additional_data_length  Size of \p additional_data in bytes.
 * \param[in] plaintext           Data that will be authenticated and
 *                                encrypted.
 * \param plaintext_length        Size of \p plaintext in bytes.
 * \param[out] ciphertext         Output buffer for the authenticated and
 *                                encrypted data. The additional data is not
 *                                part of this output. For algorithms where the
 *                                encrypted data and the authentication tag
 *                                are defined as separate outputs, the
 *                                authentication tag is appended to the
 *                                encrypted data.
 * \param ciphertext_size         Size of the \p ciphertext buffer in bytes.
 *                                This must be appropriate for the selected
 *                                algorithm and key:
 *                                - A sufficient output size is
 *                                  #PSA_AEAD_ENCRYPT_OUTPUT_SIZE(\c key_type,
 *                                  \p alg, \p plaintext_length) where
 *                                  \c key_type is the type of \p key.
 *                                - #PSA_AEAD_ENCRYPT_OUTPUT_MAX_SIZE(\p
 *                                  plaintext_length) evaluates to the maximum
 *                                  ciphertext size of any supported AEAD
 *                                  encryption.
 * \param[out] ciphertext_length  On success, the size of the output
 *                                in the \p ciphertext buffer.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not an AEAD algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p ciphertext_size is too small.
 *         #PSA_AEAD_ENCRYPT_OUTPUT_SIZE(\c key_type, \p alg,
 *         \p plaintext_length) or
 *         #PSA_AEAD_ENCRYPT_OUTPUT_MAX_SIZE(\p plaintext_length) can be used to
 *         determine the required buffer size.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_encrypt(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *nonce,
                              size_t nonce_length,
                              const uint8_t *additional_data,
                              size_t additional_data_length,
                              const uint8_t *plaintext,
                              size_t plaintext_length,
                              uint8_t *ciphertext,
                              size_t ciphertext_size,
                              size_t *ciphertext_length);

/** Process an authenticated decryption operation.
 *
 * \param key                     Identifier of the key to use for the
 *                                operation. It must allow the usage
 *                                #PSA_KEY_USAGE_DECRYPT.
 * \param alg                     The AEAD algorithm to compute
 *                                (\c PSA_ALG_XXX value such that
 *                                #PSA_ALG_IS_AEAD(\p alg) is true).
 * \param[in] nonce               Nonce or IV to use.
 * \param nonce_length            Size of the \p nonce buffer in bytes.
 * \param[in] additional_data     Additional data that has been authenticated
 *                                but not encrypted.
 * \param additional_data_length  Size of \p additional_data in bytes.
 * \param[in] ciphertext          Data that has been authenticated and
 *                                encrypted. For algorithms where the
 *                                encrypted data and the authentication tag
 *                                are defined as separate inputs, the buffer
 *                                must contain the encrypted data followed
 *                                by the authentication tag.
 * \param ciphertext_length       Size of \p ciphertext in bytes.
 * \param[out] plaintext          Output buffer for the decrypted data.
 * \param plaintext_size          Size of the \p plaintext buffer in bytes.
 *                                This must be appropriate for the selected
 *                                algorithm and key:
 *                                - A sufficient output size is
 *                                  #PSA_AEAD_DECRYPT_OUTPUT_SIZE(\c key_type,
 *                                  \p alg, \p ciphertext_length) where
 *                                  \c key_type is the type of \p key.
 *                                - #PSA_AEAD_DECRYPT_OUTPUT_MAX_SIZE(\p
 *                                  ciphertext_length) evaluates to the maximum
 *                                  plaintext size of any supported AEAD
 *                                  decryption.
 * \param[out] plaintext_length   On success, the size of the output
 *                                in the \p plaintext buffer.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The ciphertext is not authentic.
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not an AEAD algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p plaintext_size is too small.
 *         #PSA_AEAD_DECRYPT_OUTPUT_SIZE(\c key_type, \p alg,
 *         \p ciphertext_length) or
 *         #PSA_AEAD_DECRYPT_OUTPUT_MAX_SIZE(\p ciphertext_length) can be used
 *         to determine the required buffer size.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_decrypt(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *nonce,
                              size_t nonce_length,
                              const uint8_t *additional_data,
                              size_t additional_data_length,
                              const uint8_t *ciphertext,
                              size_t ciphertext_length,
                              uint8_t *plaintext,
                              size_t plaintext_size,
                              size_t *plaintext_length);

/** The type of the state data structure for multipart AEAD operations.
 *
 * Before calling any function on an AEAD operation object, the application
 * must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_aead_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_aead_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_AEAD_OPERATION_INIT,
 *   for example:
 *   \code
 *   psa_aead_operation_t operation = PSA_AEAD_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function psa_aead_operation_init()
 *   to the structure, for example:
 *   \code
 *   psa_aead_operation_t operation;
 *   operation = psa_aead_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_aead_operation_s psa_aead_operation_t;

/** \def PSA_AEAD_OPERATION_INIT
 *
 * This macro returns a suitable initializer for an AEAD operation object of
 * type #psa_aead_operation_t.
 */

/** Return an initial value for an AEAD operation object.
 */
static psa_aead_operation_t psa_aead_operation_init(void);

/** Set the key for a multipart authenticated encryption operation.
 *
 * The sequence of operations to encrypt a message with authentication
 * is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_aead_operation_t, e.g.
 *    #PSA_AEAD_OPERATION_INIT.
 * -# Call psa_aead_encrypt_setup() to specify the algorithm and key.
 * -# If needed, call psa_aead_set_lengths() to specify the length of the
 *    inputs to the subsequent calls to psa_aead_update_ad() and
 *    psa_aead_update(). See the documentation of psa_aead_set_lengths()
 *    for details.
 * -# Call either psa_aead_generate_nonce() or psa_aead_set_nonce() to
 *    generate or set the nonce. You should use
 *    psa_aead_generate_nonce() unless the protocol you are implementing
 *    requires a specific nonce value.
 * -# Call psa_aead_update_ad() zero, one or more times, passing a fragment
 *    of the non-encrypted additional authenticated data each time.
 * -# Call psa_aead_update() zero, one or more times, passing a fragment
 *    of the message to encrypt each time.
 * -# Call psa_aead_finish().
 *
 * If an error occurs at any step after a call to psa_aead_encrypt_setup(),
 * the operation will need to be reset by a call to psa_aead_abort(). The
 * application may call psa_aead_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_aead_encrypt_setup(), the application must
 * eventually terminate the operation. The following events terminate an
 * operation:
 * - A successful call to psa_aead_finish().
 * - A call to psa_aead_abort().
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized as per the documentation for
 *                              #psa_aead_operation_t and not yet in use.
 * \param key                   Identifier of the key to use for the operation.
 *                              It must remain valid until the operation
 *                              terminates. It must allow the usage
 *                              #PSA_KEY_USAGE_ENCRYPT.
 * \param alg                   The AEAD algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_AEAD(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not an AEAD algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_encrypt_setup(psa_aead_operation_t *operation,
                                    mbedtls_svc_key_id_t key,
                                    psa_algorithm_t alg);

/** Set the key for a multipart authenticated decryption operation.
 *
 * The sequence of operations to decrypt a message with authentication
 * is as follows:
 * -# Allocate an operation object which will be passed to all the functions
 *    listed here.
 * -# Initialize the operation object with one of the methods described in the
 *    documentation for #psa_aead_operation_t, e.g.
 *    #PSA_AEAD_OPERATION_INIT.
 * -# Call psa_aead_decrypt_setup() to specify the algorithm and key.
 * -# If needed, call psa_aead_set_lengths() to specify the length of the
 *    inputs to the subsequent calls to psa_aead_update_ad() and
 *    psa_aead_update(). See the documentation of psa_aead_set_lengths()
 *    for details.
 * -# Call psa_aead_set_nonce() with the nonce for the decryption.
 * -# Call psa_aead_update_ad() zero, one or more times, passing a fragment
 *    of the non-encrypted additional authenticated data each time.
 * -# Call psa_aead_update() zero, one or more times, passing a fragment
 *    of the ciphertext to decrypt each time.
 * -# Call psa_aead_verify().
 *
 * If an error occurs at any step after a call to psa_aead_decrypt_setup(),
 * the operation will need to be reset by a call to psa_aead_abort(). The
 * application may call psa_aead_abort() at any time after the operation
 * has been initialized.
 *
 * After a successful call to psa_aead_decrypt_setup(), the application must
 * eventually terminate the operation. The following events terminate an
 * operation:
 * - A successful call to psa_aead_verify().
 * - A call to psa_aead_abort().
 *
 * \param[in,out] operation     The operation object to set up. It must have
 *                              been initialized as per the documentation for
 *                              #psa_aead_operation_t and not yet in use.
 * \param key                   Identifier of the key to use for the operation.
 *                              It must remain valid until the operation
 *                              terminates. It must allow the usage
 *                              #PSA_KEY_USAGE_DECRYPT.
 * \param alg                   The AEAD algorithm to compute
 *                              (\c PSA_ALG_XXX value such that
 *                              #PSA_ALG_IS_AEAD(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p key is not compatible with \p alg.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not supported or is not an AEAD algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or the
 *         library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_decrypt_setup(psa_aead_operation_t *operation,
                                    mbedtls_svc_key_id_t key,
                                    psa_algorithm_t alg);

/** Generate a random nonce for an authenticated encryption operation.
 *
 * This function generates a random nonce for the authenticated encryption
 * operation with an appropriate size for the chosen algorithm, key type
 * and key size.
 *
 * The application must call psa_aead_encrypt_setup() before
 * calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[out] nonce            Buffer where the generated nonce is to be
 *                              written.
 * \param nonce_size            Size of the \p nonce buffer in bytes.
 * \param[out] nonce_length     On success, the number of bytes of the
 *                              generated nonce.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p nonce buffer is too small.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active aead encrypt
 *         operation, with no nonce set), or the library has not been
 *         previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_generate_nonce(psa_aead_operation_t *operation,
                                     uint8_t *nonce,
                                     size_t nonce_size,
                                     size_t *nonce_length);

/** Set the nonce for an authenticated encryption or decryption operation.
 *
 * This function sets the nonce for the authenticated
 * encryption or decryption operation.
 *
 * The application must call psa_aead_encrypt_setup() or
 * psa_aead_decrypt_setup() before calling this function.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \note When encrypting, applications should use psa_aead_generate_nonce()
 * instead of this function, unless implementing a protocol that requires
 * a non-random IV.
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[in] nonce             Buffer containing the nonce to use.
 * \param nonce_length          Size of the nonce in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size of \p nonce is not acceptable for the chosen algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, with no nonce
 *         set), or the library has not been previously initialized
 *         by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_set_nonce(psa_aead_operation_t *operation,
                                const uint8_t *nonce,
                                size_t nonce_length);

/** Declare the lengths of the message and additional data for AEAD.
 *
 * The application must call this function before calling
 * psa_aead_update_ad() or psa_aead_update() if the algorithm for
 * the operation requires it. If the algorithm does not require it,
 * calling this function is optional, but if this function is called
 * then the implementation must enforce the lengths.
 *
 * You may call this function before or after setting the nonce with
 * psa_aead_set_nonce() or psa_aead_generate_nonce().
 *
 * - For #PSA_ALG_CCM, calling this function is required.
 * - For the other AEAD algorithms defined in this specification, calling
 *   this function is not required.
 * - For vendor-defined algorithm, refer to the vendor documentation.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param ad_length             Size of the non-encrypted additional
 *                              authenticated data in bytes.
 * \param plaintext_length      Size of the plaintext to encrypt in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         At least one of the lengths is not acceptable for the chosen
 *         algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, and
 *         psa_aead_update_ad() and psa_aead_update() must not have been
 *         called yet), or the library has not been previously initialized
 *         by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_set_lengths(psa_aead_operation_t *operation,
                                  size_t ad_length,
                                  size_t plaintext_length);

/** Pass additional data to an active AEAD operation.
 *
 * Additional data is authenticated, but not encrypted.
 *
 * You may call this function multiple times to pass successive fragments
 * of the additional data. You may not call this function after passing
 * data to encrypt or decrypt with psa_aead_update().
 *
 * Before calling this function, you must:
 * 1. Call either psa_aead_encrypt_setup() or psa_aead_decrypt_setup().
 * 2. Set the nonce with psa_aead_generate_nonce() or psa_aead_set_nonce().
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \warning When decrypting, until psa_aead_verify() has returned #PSA_SUCCESS,
 *          there is no guarantee that the input is valid. Therefore, until
 *          you have called psa_aead_verify() and it has returned #PSA_SUCCESS,
 *          treat the input as untrusted and prepare to undo any action that
 *          depends on the input if psa_aead_verify() returns an error status.
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[in] input             Buffer containing the fragment of
 *                              additional data.
 * \param input_length          Size of the \p input buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The total input length overflows the additional data length that
 *         was previously specified with psa_aead_set_lengths().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, have a nonce
 *         set, have lengths set if required by the algorithm, and
 *         psa_aead_update() must not have been called yet), or the library
 *         has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_update_ad(psa_aead_operation_t *operation,
                                const uint8_t *input,
                                size_t input_length);

/** Encrypt or decrypt a message fragment in an active AEAD operation.
 *
 * Before calling this function, you must:
 * 1. Call either psa_aead_encrypt_setup() or psa_aead_decrypt_setup().
 *    The choice of setup function determines whether this function
 *    encrypts or decrypts its input.
 * 2. Set the nonce with psa_aead_generate_nonce() or psa_aead_set_nonce().
 * 3. Call psa_aead_update_ad() to pass all the additional data.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \warning When decrypting, until psa_aead_verify() has returned #PSA_SUCCESS,
 *          there is no guarantee that the input is valid. Therefore, until
 *          you have called psa_aead_verify() and it has returned #PSA_SUCCESS:
 *          - Do not use the output in any way other than storing it in a
 *            confidential location. If you take any action that depends
 *            on the tentative decrypted data, this action will need to be
 *            undone if the input turns out not to be valid. Furthermore,
 *            if an adversary can observe that this action took place
 *            (for example through timing), they may be able to use this
 *            fact as an oracle to decrypt any message encrypted with the
 *            same key.
 *          - In particular, do not copy the output anywhere but to a
 *            memory or storage space that you have exclusive access to.
 *
 * This function does not require the input to be aligned to any
 * particular block boundary. If the implementation can only process
 * a whole block at a time, it must consume all the input provided, but
 * it may delay the end of the corresponding output until a subsequent
 * call to psa_aead_update(), psa_aead_finish() or psa_aead_verify()
 * provides sufficient input. The amount of data that can be delayed
 * in this way is bounded by #PSA_AEAD_UPDATE_OUTPUT_SIZE.
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[in] input             Buffer containing the message fragment to
 *                              encrypt or decrypt.
 * \param input_length          Size of the \p input buffer in bytes.
 * \param[out] output           Buffer where the output is to be written.
 * \param output_size           Size of the \p output buffer in bytes.
 *                              This must be appropriate for the selected
 *                                algorithm and key:
 *                                - A sufficient output size is
 *                                  #PSA_AEAD_UPDATE_OUTPUT_SIZE(\c key_type,
 *                                  \c alg, \p input_length) where
 *                                  \c key_type is the type of key and \c alg is
 *                                  the algorithm that were used to set up the
 *                                  operation.
 *                                - #PSA_AEAD_UPDATE_OUTPUT_MAX_SIZE(\p
 *                                  input_length) evaluates to the maximum
 *                                  output size of any supported AEAD
 *                                  algorithm.
 * \param[out] output_length    On success, the number of bytes
 *                              that make up the returned output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p output buffer is too small.
 *         #PSA_AEAD_UPDATE_OUTPUT_SIZE(\c key_type, \c alg, \p input_length) or
 *         #PSA_AEAD_UPDATE_OUTPUT_MAX_SIZE(\p input_length) can be used to
 *         determine the required buffer size.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The total length of input to psa_aead_update_ad() so far is
 *         less than the additional data length that was previously
 *         specified with psa_aead_set_lengths(), or
 *         the total input length overflows the plaintext length that
 *         was previously specified with psa_aead_set_lengths().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active, have a nonce
 *         set, and have lengths set if required by the algorithm), or the
 *         library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_update(psa_aead_operation_t *operation,
                             const uint8_t *input,
                             size_t input_length,
                             uint8_t *output,
                             size_t output_size,
                             size_t *output_length);

/** Finish encrypting a message in an AEAD operation.
 *
 * The operation must have been set up with psa_aead_encrypt_setup().
 *
 * This function finishes the authentication of the additional data
 * formed by concatenating the inputs passed to preceding calls to
 * psa_aead_update_ad() with the plaintext formed by concatenating the
 * inputs passed to preceding calls to psa_aead_update().
 *
 * This function has two output buffers:
 * - \p ciphertext contains trailing ciphertext that was buffered from
 *   preceding calls to psa_aead_update().
 * - \p tag contains the authentication tag.
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[out] ciphertext       Buffer where the last part of the ciphertext
 *                              is to be written.
 * \param ciphertext_size       Size of the \p ciphertext buffer in bytes.
 *                              This must be appropriate for the selected
 *                              algorithm and key:
 *                              - A sufficient output size is
 *                                #PSA_AEAD_FINISH_OUTPUT_SIZE(\c key_type,
 *                                \c alg) where \c key_type is the type of key
 *                                and \c alg is the algorithm that were used to
 *                                set up the operation.
 *                              - #PSA_AEAD_FINISH_OUTPUT_MAX_SIZE evaluates to
 *                                the maximum output size of any supported AEAD
 *                                algorithm.
 * \param[out] ciphertext_length On success, the number of bytes of
 *                              returned ciphertext.
 * \param[out] tag              Buffer where the authentication tag is
 *                              to be written.
 * \param tag_size              Size of the \p tag buffer in bytes.
 *                              This must be appropriate for the selected
 *                              algorithm and key:
 *                              - The exact tag size is #PSA_AEAD_TAG_LENGTH(\c
 *                                key_type, \c key_bits, \c alg) where
 *                                \c key_type and \c key_bits are the type and
 *                                bit-size of the key, and \c alg is the
 *                                algorithm that were used in the call to
 *                                psa_aead_encrypt_setup().
 *                              - #PSA_AEAD_TAG_MAX_SIZE evaluates to the
 *                                maximum tag size of any supported AEAD
 *                                algorithm.
 * \param[out] tag_length       On success, the number of bytes
 *                              that make up the returned tag.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p ciphertext or \p tag buffer is too small.
 *         #PSA_AEAD_FINISH_OUTPUT_SIZE(\c key_type, \c alg) or
 *         #PSA_AEAD_FINISH_OUTPUT_MAX_SIZE can be used to determine the
 *         required \p ciphertext buffer size. #PSA_AEAD_TAG_LENGTH(\c key_type,
 *         \c key_bits, \c alg) or #PSA_AEAD_TAG_MAX_SIZE can be used to
 *         determine the required \p tag buffer size.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The total length of input to psa_aead_update_ad() so far is
 *         less than the additional data length that was previously
 *         specified with psa_aead_set_lengths(), or
 *         the total length of input to psa_aead_update() so far is
 *         less than the plaintext length that was previously
 *         specified with psa_aead_set_lengths().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active encryption
 *         operation with a nonce set), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_finish(psa_aead_operation_t *operation,
                             uint8_t *ciphertext,
                             size_t ciphertext_size,
                             size_t *ciphertext_length,
                             uint8_t *tag,
                             size_t tag_size,
                             size_t *tag_length);

/** Finish authenticating and decrypting a message in an AEAD operation.
 *
 * The operation must have been set up with psa_aead_decrypt_setup().
 *
 * This function finishes the authenticated decryption of the message
 * components:
 *
 * -  The additional data consisting of the concatenation of the inputs
 *    passed to preceding calls to psa_aead_update_ad().
 * -  The ciphertext consisting of the concatenation of the inputs passed to
 *    preceding calls to psa_aead_update().
 * -  The tag passed to this function call.
 *
 * If the authentication tag is correct, this function outputs any remaining
 * plaintext and reports success. If the authentication tag is not correct,
 * this function returns #PSA_ERROR_INVALID_SIGNATURE.
 *
 * When this function returns successfully, the operation becomes inactive.
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_aead_abort().
 *
 * \note Implementations shall make the best effort to ensure that the
 * comparison between the actual tag and the expected tag is performed
 * in constant time.
 *
 * \param[in,out] operation     Active AEAD operation.
 * \param[out] plaintext        Buffer where the last part of the plaintext
 *                              is to be written. This is the remaining data
 *                              from previous calls to psa_aead_update()
 *                              that could not be processed until the end
 *                              of the input.
 * \param plaintext_size        Size of the \p plaintext buffer in bytes.
 *                              This must be appropriate for the selected algorithm and key:
 *                              - A sufficient output size is
 *                                #PSA_AEAD_VERIFY_OUTPUT_SIZE(\c key_type,
 *                                \c alg) where \c key_type is the type of key
 *                                and \c alg is the algorithm that were used to
 *                                set up the operation.
 *                              - #PSA_AEAD_VERIFY_OUTPUT_MAX_SIZE evaluates to
 *                                the maximum output size of any supported AEAD
 *                                algorithm.
 * \param[out] plaintext_length On success, the number of bytes of
 *                              returned plaintext.
 * \param[in] tag               Buffer containing the authentication tag.
 * \param tag_length            Size of the \p tag buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculations were successful, but the authentication tag is
 *         not correct.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p plaintext buffer is too small.
 *         #PSA_AEAD_VERIFY_OUTPUT_SIZE(\c key_type, \c alg) or
 *         #PSA_AEAD_VERIFY_OUTPUT_MAX_SIZE can be used to determine the
 *         required buffer size.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The total length of input to psa_aead_update_ad() so far is
 *         less than the additional data length that was previously
 *         specified with psa_aead_set_lengths(), or
 *         the total length of input to psa_aead_update() so far is
 *         less than the plaintext length that was previously
 *         specified with psa_aead_set_lengths().
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be an active decryption
 *         operation with a nonce set), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_verify(psa_aead_operation_t *operation,
                             uint8_t *plaintext,
                             size_t plaintext_size,
                             size_t *plaintext_length,
                             const uint8_t *tag,
                             size_t tag_length);

/** Abort an AEAD operation.
 *
 * Aborting an operation frees all associated resources except for the
 * \p operation structure itself. Once aborted, the operation object
 * can be reused for another operation by calling
 * psa_aead_encrypt_setup() or psa_aead_decrypt_setup() again.
 *
 * You may call this function any time after the operation object has
 * been initialized as described in #psa_aead_operation_t.
 *
 * In particular, calling psa_aead_abort() after the operation has been
 * terminated by a call to psa_aead_abort(), psa_aead_finish() or
 * psa_aead_verify() is safe and has no effect.
 *
 * \param[in,out] operation     Initialized AEAD operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_aead_abort(psa_aead_operation_t *operation);

/**@}*/

/** \defgroup asymmetric Asymmetric cryptography
 * @{
 */

/**
 * \brief Sign a message with a private key. For hash-and-sign algorithms,
 *        this includes the hashing step.
 *
 * \note To perform a multi-part hash-and-sign signature algorithm, first use
 *       a multi-part hash operation and then pass the resulting hash to
 *       psa_sign_hash(). PSA_ALG_GET_HASH(\p alg) can be used to determine the
 *       hash algorithm to use.
 *
 * \param[in]  key              Identifier of the key to use for the operation.
 *                              It must be an asymmetric key pair. The key must
 *                              allow the usage #PSA_KEY_USAGE_SIGN_MESSAGE.
 * \param[in]  alg              An asymmetric signature algorithm (PSA_ALG_XXX
 *                              value such that #PSA_ALG_IS_SIGN_MESSAGE(\p alg)
 *                              is true), that is compatible with the type of
 *                              \p key.
 * \param[in]  input            The input message to sign.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param[in]  signature_size   Size of the \p signature buffer in bytes. This
 *                              must be appropriate for the selected
 *                              algorithm and key:
 *                              - The required signature size is
 *                                #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *                                where \c key_type and \c key_bits are the type and
 *                                bit-size respectively of key.
 *                              - #PSA_SIGNATURE_MAX_SIZE evaluates to the
 *                                maximum signature size of any supported
 *                                signature algorithm.
 * \param[out] signature_length On success, the number of bytes that make up
 *                              the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key does not have the #PSA_KEY_USAGE_SIGN_MESSAGE flag,
 *         or it does not permit the requested algorithm.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \p key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_sign_message(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *input,
                              size_t input_length,
                              uint8_t *signature,
                              size_t signature_size,
                              size_t *signature_length);

/** \brief Verify the signature of a message with a public key, using
 *         a hash-and-sign verification algorithm.
 *
 * \note To perform a multi-part hash-and-sign signature verification
 *       algorithm, first use a multi-part hash operation to hash the message
 *       and then pass the resulting hash to psa_verify_hash().
 *       PSA_ALG_GET_HASH(\p alg) can be used to determine the hash algorithm
 *       to use.
 *
 * \param[in]  key              Identifier of the key to use for the operation.
 *                              It must be a public key or an asymmetric key
 *                              pair. The key must allow the usage
 *                              #PSA_KEY_USAGE_VERIFY_MESSAGE.
 * \param[in]  alg              An asymmetric signature algorithm (PSA_ALG_XXX
 *                              value such that #PSA_ALG_IS_SIGN_MESSAGE(\p alg)
 *                              is true), that is compatible with the type of
 *                              \p key.
 * \param[in]  input            The message whose signature is to be verified.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[in] signature         Buffer containing the signature to verify.
 * \param[in]  signature_length Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key does not have the #PSA_KEY_USAGE_SIGN_MESSAGE flag,
 *         or it does not permit the requested algorithm.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed signature
 *         is not a valid signature.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_verify_message(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input,
                                size_t input_length,
                                const uint8_t *signature,
                                size_t signature_length);

/**
 * \brief Sign a hash or short message with a private key.
 *
 * Note that to perform a hash-and-sign signature algorithm, you must
 * first calculate the hash by calling psa_hash_setup(), psa_hash_update()
 * and psa_hash_finish(), or alternatively by calling psa_hash_compute().
 * Then pass the resulting hash as the \p hash
 * parameter to this function. You can use #PSA_ALG_SIGN_GET_HASH(\p alg)
 * to determine the hash algorithm to use.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              It must be an asymmetric key pair. The key must
 *                              allow the usage #PSA_KEY_USAGE_SIGN_HASH.
 * \param alg                   A signature algorithm (PSA_ALG_XXX
 *                              value such that #PSA_ALG_IS_SIGN_HASH(\p alg)
 *                              is true), that is compatible with
 *                              the type of \p key.
 * \param[in] hash              The hash or message to sign.
 * \param hash_length           Size of the \p hash buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param signature_size        Size of the \p signature buffer in bytes.
 * \param[out] signature_length On success, the number of bytes
 *                              that make up the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \p key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_sign_hash(mbedtls_svc_key_id_t key,
                           psa_algorithm_t alg,
                           const uint8_t *hash,
                           size_t hash_length,
                           uint8_t *signature,
                           size_t signature_size,
                           size_t *signature_length);

/**
 * \brief Verify the signature of a hash or short message using a public key.
 *
 * Note that to perform a hash-and-sign signature algorithm, you must
 * first calculate the hash by calling psa_hash_setup(), psa_hash_update()
 * and psa_hash_finish(), or alternatively by calling psa_hash_compute().
 * Then pass the resulting hash as the \p hash
 * parameter to this function. You can use #PSA_ALG_SIGN_GET_HASH(\p alg)
 * to determine the hash algorithm to use.
 *
 * \param key               Identifier of the key to use for the operation. It
 *                          must be a public key or an asymmetric key pair. The
 *                          key must allow the usage
 *                          #PSA_KEY_USAGE_VERIFY_HASH.
 * \param alg               A signature algorithm (PSA_ALG_XXX
 *                          value such that #PSA_ALG_IS_SIGN_HASH(\p alg)
 *                          is true), that is compatible with
 *                          the type of \p key.
 * \param[in] hash          The hash or message whose signature is to be
 *                          verified.
 * \param hash_length       Size of the \p hash buffer in bytes.
 * \param[in] signature     Buffer containing the signature to verify.
 * \param signature_length  Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The signature is valid.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_verify_hash(mbedtls_svc_key_id_t key,
                             psa_algorithm_t alg,
                             const uint8_t *hash,
                             size_t hash_length,
                             const uint8_t *signature,
                             size_t signature_length);

/**
 * \brief Encrypt a short message with a public key.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              It must be a public key or an asymmetric key
 *                              pair. It must allow the usage
 *                              #PSA_KEY_USAGE_ENCRYPT.
 * \param alg                   An asymmetric encryption algorithm that is
 *                              compatible with the type of \p key.
 * \param[in] input             The message to encrypt.
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
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
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
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_asymmetric_encrypt(mbedtls_svc_key_id_t key,
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
 * \param key                   Identifier of the key to use for the operation.
 *                              It must be an asymmetric key pair. It must
 *                              allow the usage #PSA_KEY_USAGE_DECRYPT.
 * \param alg                   An asymmetric encryption algorithm that is
 *                              compatible with the type of \p key.
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
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
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
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_INVALID_PADDING \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_asymmetric_decrypt(mbedtls_svc_key_id_t key,
                                    psa_algorithm_t alg,
                                    const uint8_t *input,
                                    size_t input_length,
                                    const uint8_t *salt,
                                    size_t salt_length,
                                    uint8_t *output,
                                    size_t output_size,
                                    size_t *output_length);

/**@}*/

/** \defgroup key_derivation Key derivation and pseudorandom generation
 * @{
 */

/** The type of the state data structure for key derivation operations.
 *
 * Before calling any function on a key derivation operation object, the
 * application must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_key_derivation_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_key_derivation_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_KEY_DERIVATION_OPERATION_INIT,
 *   for example:
 *   \code
 *   psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function psa_key_derivation_operation_init()
 *   to the structure, for example:
 *   \code
 *   psa_key_derivation_operation_t operation;
 *   operation = psa_key_derivation_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice.
 */
typedef struct psa_key_derivation_s psa_key_derivation_operation_t;

/** \def PSA_KEY_DERIVATION_OPERATION_INIT
 *
 * This macro returns a suitable initializer for a key derivation operation
 * object of type #psa_key_derivation_operation_t.
 */

/** Return an initial value for a key derivation operation object.
 */
static psa_key_derivation_operation_t psa_key_derivation_operation_init(void);

/** Set up a key derivation operation.
 *
 * A key derivation algorithm takes some inputs and uses them to generate
 * a byte stream in a deterministic way.
 * This byte stream can be used to produce keys and other
 * cryptographic material.
 *
 * To derive a key:
 * -# Start with an initialized object of type #psa_key_derivation_operation_t.
 * -# Call psa_key_derivation_setup() to select the algorithm.
 * -# Provide the inputs for the key derivation by calling
 *    psa_key_derivation_input_bytes() or psa_key_derivation_input_key()
 *    as appropriate. Which inputs are needed, in what order, and whether
 *    they may be keys and if so of what type depends on the algorithm.
 * -# Optionally set the operation's maximum capacity with
 *    psa_key_derivation_set_capacity(). You may do this before, in the middle
 *    of or after providing inputs. For some algorithms, this step is mandatory
 *    because the output depends on the maximum capacity.
 * -# To derive a key, call psa_key_derivation_output_key() or
 *    psa_key_derivation_output_key_custom().
 *    To derive a byte string for a different purpose, call
 *    psa_key_derivation_output_bytes().
 *    Successive calls to these functions use successive output bytes
 *    calculated by the key derivation algorithm.
 * -# Clean up the key derivation operation object with
 *    psa_key_derivation_abort().
 *
 * If this function returns an error, the key derivation operation object is
 * not changed.
 *
 * If an error occurs at any step after a call to psa_key_derivation_setup(),
 * the operation will need to be reset by a call to psa_key_derivation_abort().
 *
 * Implementations must reject an attempt to derive a key of size 0.
 *
 * \param[in,out] operation       The key derivation operation object
 *                                to set up. It must
 *                                have been initialized but not set up yet.
 * \param alg                     The key derivation algorithm to compute
 *                                (\c PSA_ALG_XXX value such that
 *                                #PSA_ALG_IS_KEY_DERIVATION(\p alg) is true).
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c alg is not a key derivation algorithm.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \c alg is not supported or is not a key derivation algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be inactive), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_setup(
    psa_key_derivation_operation_t *operation,
    psa_algorithm_t alg);

/** Retrieve the current capacity of a key derivation operation.
 *
 * The capacity of a key derivation is the maximum number of bytes that it can
 * return. When you get *N* bytes of output from a key derivation operation,
 * this reduces its capacity by *N*.
 *
 * \param[in] operation     The operation to query.
 * \param[out] capacity     On success, the capacity of the operation.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_get_capacity(
    const psa_key_derivation_operation_t *operation,
    size_t *capacity);

/** Set the maximum capacity of a key derivation operation.
 *
 * The capacity of a key derivation operation is the maximum number of bytes
 * that the key derivation operation can return from this point onwards.
 *
 * \param[in,out] operation The key derivation operation object to modify.
 * \param capacity          The new capacity of the operation.
 *                          It must be less or equal to the operation's
 *                          current capacity.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p capacity is larger than the operation's current capacity.
 *         In this case, the operation object remains valid and its capacity
 *         remains unchanged.
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active), or the
 *         library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_set_capacity(
    psa_key_derivation_operation_t *operation,
    size_t capacity);

/** Use the maximum possible capacity for a key derivation operation.
 *
 * Use this value as the capacity argument when setting up a key derivation
 * to indicate that the operation should have the maximum possible capacity.
 * The value of the maximum possible capacity depends on the key derivation
 * algorithm.
 */
#define PSA_KEY_DERIVATION_UNLIMITED_CAPACITY ((size_t) (-1))

/** Provide an input for key derivation or key agreement.
 *
 * Which inputs are required and in what order depends on the algorithm.
 * Refer to the documentation of each key derivation or key agreement
 * algorithm for information.
 *
 * This function passes direct inputs, which is usually correct for
 * non-secret inputs. To pass a secret input, which should be in a key
 * object, call psa_key_derivation_input_key() instead of this function.
 * Refer to the documentation of individual step types
 * (`PSA_KEY_DERIVATION_INPUT_xxx` values of type ::psa_key_derivation_step_t)
 * for more information.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * \param[in,out] operation       The key derivation operation object to use.
 *                                It must have been set up with
 *                                psa_key_derivation_setup() and must not
 *                                have produced any output yet.
 * \param step                    Which step the input data is for.
 * \param[in] data                Input data to use.
 * \param data_length             Size of the \p data buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c step is not compatible with the operation's algorithm, or
 *         \c step does not allow direct inputs.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid for this input \p step, or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_input_bytes(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    const uint8_t *data,
    size_t data_length);

/** Provide a numeric input for key derivation or key agreement.
 *
 * Which inputs are required and in what order depends on the algorithm.
 * However, when an algorithm requires a particular order, numeric inputs
 * usually come first as they tend to be configuration parameters.
 * Refer to the documentation of each key derivation or key agreement
 * algorithm for information.
 *
 * This function is used for inputs which are fixed-size non-negative
 * integers.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * \param[in,out] operation       The key derivation operation object to use.
 *                                It must have been set up with
 *                                psa_key_derivation_setup() and must not
 *                                have produced any output yet.
 * \param step                    Which step the input data is for.
 * \param[in] value               The value of the numeric input.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c step is not compatible with the operation's algorithm, or
 *         \c step does not allow numeric inputs.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid for this input \p step, or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_input_integer(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    uint64_t value);

/** Provide an input for key derivation in the form of a key.
 *
 * Which inputs are required and in what order depends on the algorithm.
 * Refer to the documentation of each key derivation or key agreement
 * algorithm for information.
 *
 * This function obtains input from a key object, which is usually correct for
 * secret inputs or for non-secret personalization strings kept in the key
 * store. To pass a non-secret parameter which is not in the key store,
 * call psa_key_derivation_input_bytes() instead of this function.
 * Refer to the documentation of individual step types
 * (`PSA_KEY_DERIVATION_INPUT_xxx` values of type ::psa_key_derivation_step_t)
 * for more information.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * \param[in,out] operation       The key derivation operation object to use.
 *                                It must have been set up with
 *                                psa_key_derivation_setup() and must not
 *                                have produced any output yet.
 * \param step                    Which step the input data is for.
 * \param key                     Identifier of the key. It must have an
 *                                appropriate type for step and must allow the
 *                                usage #PSA_KEY_USAGE_DERIVE or
 *                                #PSA_KEY_USAGE_VERIFY_DERIVATION (see note)
 *                                and the algorithm used by the operation.
 *
 * \note Once all inputs steps are completed, the operations will allow:
 * - psa_key_derivation_output_bytes() if each input was either a direct input
 *   or  a key with #PSA_KEY_USAGE_DERIVE set;
 * - psa_key_derivation_output_key() or psa_key_derivation_output_key_custom()
 *   if the input for step
 *   #PSA_KEY_DERIVATION_INPUT_SECRET or #PSA_KEY_DERIVATION_INPUT_PASSWORD
 *   was from a key slot with #PSA_KEY_USAGE_DERIVE and each other input was
 *   either a direct input or a key with #PSA_KEY_USAGE_DERIVE set;
 * - psa_key_derivation_verify_bytes() if each input was either a direct input
 *   or  a key with #PSA_KEY_USAGE_VERIFY_DERIVATION set;
 * - psa_key_derivation_verify_key() under the same conditions as
 *   psa_key_derivation_verify_bytes().
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key allows neither #PSA_KEY_USAGE_DERIVE nor
 *         #PSA_KEY_USAGE_VERIFY_DERIVATION, or it doesn't allow this
 *         algorithm.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c step is not compatible with the operation's algorithm, or
 *         \c step does not allow key inputs of the given type
 *         or does not allow key inputs at all.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid for this input \p step, or
 *         the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_input_key(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    mbedtls_svc_key_id_t key);

/** Perform a key agreement and use the shared secret as input to a key
 * derivation.
 *
 * A key agreement algorithm takes two inputs: a private key \p private_key
 * a public key \p peer_key.
 * The result of this function is passed as input to a key derivation.
 * The output of this key derivation can be extracted by reading from the
 * resulting operation to produce keys and other cryptographic material.
 *
 * If this function returns an error status, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * \param[in,out] operation       The key derivation operation object to use.
 *                                It must have been set up with
 *                                psa_key_derivation_setup() with a
 *                                key agreement and derivation algorithm
 *                                \c alg (\c PSA_ALG_XXX value such that
 *                                #PSA_ALG_IS_KEY_AGREEMENT(\c alg) is true
 *                                and #PSA_ALG_IS_RAW_KEY_AGREEMENT(\c alg)
 *                                is false).
 *                                The operation must be ready for an
 *                                input of the type given by \p step.
 * \param step                    Which step the input data is for.
 * \param private_key             Identifier of the private key to use. It must
 *                                allow the usage #PSA_KEY_USAGE_DERIVE.
 * \param[in] peer_key      Public key of the peer. The peer key must be in the
 *                          same format that psa_import_key() accepts for the
 *                          public key type corresponding to the type of
 *                          private_key. That is, this function performs the
 *                          equivalent of
 *                          #psa_import_key(...,
 *                          `peer_key`, `peer_key_length`) where
 *                          with key attributes indicating the public key
 *                          type corresponding to the type of `private_key`.
 *                          For example, for EC keys, this means that peer_key
 *                          is interpreted as a point on the curve that the
 *                          private key is on. The standard formats for public
 *                          keys are documented in the documentation of
 *                          psa_export_public_key().
 * \param peer_key_length         Size of \p peer_key in bytes.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c private_key is not compatible with \c alg,
 *         or \p peer_key is not valid for \c alg or not compatible with
 *         \c private_key, or \c step does not allow an input resulting
 *         from a key agreement.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \c alg is not supported or is not a key derivation algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid for this key agreement \p step,
 *         or the library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_key_agreement(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    mbedtls_svc_key_id_t private_key,
    const uint8_t *peer_key,
    size_t peer_key_length);

/** Read some data from a key derivation operation.
 *
 * This function calculates output bytes from a key derivation algorithm and
 * return those bytes.
 * If you view the key derivation's output as a stream of bytes, this
 * function destructively reads the requested number of bytes from the
 * stream.
 * The operation's capacity decreases by the number of bytes read.
 *
 * If this function returns an error status other than
 * #PSA_ERROR_INSUFFICIENT_DATA, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[out] output       Buffer where the output will be written.
 * \param output_length     Number of bytes to output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         One of the inputs was a key whose policy didn't allow
 *         #PSA_KEY_USAGE_DERIVE.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *                          The operation's capacity was less than
 *                          \p output_length bytes. Note that in this case,
 *                          no output is written to the output buffer.
 *                          The operation's capacity is set to 0, thus
 *                          subsequent calls to this function will not
 *                          succeed, even with a smaller output buffer.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_output_bytes(
    psa_key_derivation_operation_t *operation,
    uint8_t *output,
    size_t output_length);

/** Derive a key from an ongoing key derivation operation.
 *
 * This function calculates output bytes from a key derivation algorithm
 * and uses those bytes to generate a key deterministically.
 * The key's location, usage policy, type and size are taken from
 * \p attributes.
 *
 * If you view the key derivation's output as a stream of bytes, this
 * function destructively reads as many bytes as required from the
 * stream.
 * The operation's capacity decreases by the number of bytes read.
 *
 * If this function returns an error status other than
 * #PSA_ERROR_INSUFFICIENT_DATA, the operation enters an error
 * state and must be aborted by calling psa_key_derivation_abort().
 *
 * How much output is produced and consumed from the operation, and how
 * the key is derived, depends on the key type and on the key size
 * (denoted \c bits below):
 *
 * - For key types for which the key is an arbitrary sequence of bytes
 *   of a given size, this function is functionally equivalent to
 *   calling #psa_key_derivation_output_bytes
 *   and passing the resulting output to #psa_import_key.
 *   However, this function has a security benefit:
 *   if the implementation provides an isolation boundary then
 *   the key material is not exposed outside the isolation boundary.
 *   As a consequence, for these key types, this function always consumes
 *   exactly (\c bits / 8) bytes from the operation.
 *   The following key types defined in this specification follow this scheme:
 *
 *     - #PSA_KEY_TYPE_AES;
 *     - #PSA_KEY_TYPE_ARIA;
 *     - #PSA_KEY_TYPE_CAMELLIA;
 *     - #PSA_KEY_TYPE_DERIVE;
 *     - #PSA_KEY_TYPE_HMAC;
 *     - #PSA_KEY_TYPE_PASSWORD_HASH.
 *
 * - For ECC keys on a Montgomery elliptic curve
 *   (#PSA_KEY_TYPE_ECC_KEY_PAIR(\c curve) where \c curve designates a
 *   Montgomery curve), this function always draws a byte string whose
 *   length is determined by the curve, and sets the mandatory bits
 *   accordingly. That is:
 *
 *     - Curve25519 (#PSA_ECC_FAMILY_MONTGOMERY, 255 bits): draw a 32-byte
 *       string and process it as specified in RFC 7748 &sect;5.
 *     - Curve448 (#PSA_ECC_FAMILY_MONTGOMERY, 448 bits): draw a 56-byte
 *       string and process it as specified in RFC 7748 &sect;5.
 *
 * - For key types for which the key is represented by a single sequence of
 *   \c bits bits with constraints as to which bit sequences are acceptable,
 *   this function draws a byte string of length (\c bits / 8) bytes rounded
 *   up to the nearest whole number of bytes. If the resulting byte string
 *   is acceptable, it becomes the key, otherwise the drawn bytes are discarded.
 *   This process is repeated until an acceptable byte string is drawn.
 *   The byte string drawn from the operation is interpreted as specified
 *   for the output produced by psa_export_key().
 *   The following key types defined in this specification follow this scheme:
 *
 *     - #PSA_KEY_TYPE_DES.
 *       Force-set the parity bits, but discard forbidden weak keys.
 *       For 2-key and 3-key triple-DES, the three keys are generated
 *       successively (for example, for 3-key triple-DES,
 *       if the first 8 bytes specify a weak key and the next 8 bytes do not,
 *       discard the first 8 bytes, use the next 8 bytes as the first key,
 *       and continue reading output from the operation to derive the other
 *       two keys).
 *     - Finite-field Diffie-Hellman keys (#PSA_KEY_TYPE_DH_KEY_PAIR(\c group)
 *       where \c group designates any Diffie-Hellman group) and
 *       ECC keys on a Weierstrass elliptic curve
 *       (#PSA_KEY_TYPE_ECC_KEY_PAIR(\c curve) where \c curve designates a
 *       Weierstrass curve).
 *       For these key types, interpret the byte string as integer
 *       in big-endian order. Discard it if it is not in the range
 *       [0, *N* - 2] where *N* is the boundary of the private key domain
 *       (the prime *p* for Diffie-Hellman, the subprime *q* for DSA,
 *       or the order of the curve's base point for ECC).
 *       Add 1 to the resulting integer and use this as the private key *x*.
 *       This method allows compliance to NIST standards, specifically
 *       the methods titled "key-pair generation by testing candidates"
 *       in NIST SP 800-56A &sect;5.6.1.1.4 for Diffie-Hellman,
 *       in FIPS 186-4 &sect;B.1.2 for DSA, and
 *       in NIST SP 800-56A &sect;5.6.1.2.2 or
 *       FIPS 186-4 &sect;B.4.2 for elliptic curve keys.
 *
 * - For other key types, including #PSA_KEY_TYPE_RSA_KEY_PAIR,
 *   the way in which the operation output is consumed is
 *   implementation-defined.
 *
 * In all cases, the data that is read is discarded from the operation.
 * The operation's capacity is decreased by the number of bytes read.
 *
 * For algorithms that take an input step #PSA_KEY_DERIVATION_INPUT_SECRET,
 * the input to that step must be provided with psa_key_derivation_input_key().
 * Future versions of this specification may include additional restrictions
 * on the derived key based on the attributes and strength of the secret key.
 *
 * \note This function is equivalent to calling
 *       psa_key_derivation_output_key_custom()
 *       with the custom production parameters #PSA_CUSTOM_KEY_PARAMETERS_INIT
 *       and `custom_data_length == 0` (i.e. `custom_data` is empty).
 *
 * \param[in] attributes    The attributes for the new key.
 *                          If the key type to be created is
 *                          #PSA_KEY_TYPE_PASSWORD_HASH then the algorithm in
 *                          the policy must be the same as in the current
 *                          operation.
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *         There was not enough data to create the desired key.
 *         Note that in this case, no output is written to the output buffer.
 *         The operation's capacity is set to 0, thus subsequent calls to
 *         this function will not succeed, even with a smaller output buffer.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The key type or key size is not supported, either by the
 *         implementation in general or in this particular location.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The provided key attributes are not valid for the operation.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The #PSA_KEY_DERIVATION_INPUT_SECRET or
 *         #PSA_KEY_DERIVATION_INPUT_PASSWORD input was not provided through a
 *         key; or one of the inputs was a key whose policy didn't allow
 *         #PSA_KEY_USAGE_DERIVE.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_output_key(
    const psa_key_attributes_t *attributes,
    psa_key_derivation_operation_t *operation,
    mbedtls_svc_key_id_t *key);

/** Derive a key from an ongoing key derivation operation with custom
 *  production parameters.
 *
 * See the description of psa_key_derivation_out_key() for the operation of
 * this function with the default production parameters.
 * Mbed TLS currently does not currently support any non-default production
 * parameters.
 *
 * \note This function is experimental and may change in future minor
 *       versions of Mbed TLS.
 *
 * \param[in] attributes    The attributes for the new key.
 *                          If the key type to be created is
 *                          #PSA_KEY_TYPE_PASSWORD_HASH then the algorithm in
 *                          the policy must be the same as in the current
 *                          operation.
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[in] custom        Customization parameters for the key generation.
 *                          When this is #PSA_CUSTOM_KEY_PARAMETERS_INIT
 *                          with \p custom_data_length = 0,
 *                          this function is equivalent to
 *                          psa_key_derivation_output_key().
 * \param[in] custom_data   Variable-length data associated with \c custom.
 * \param custom_data_length
 *                          Length of `custom_data` in bytes.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *         There was not enough data to create the desired key.
 *         Note that in this case, no output is written to the output buffer.
 *         The operation's capacity is set to 0, thus subsequent calls to
 *         this function will not succeed, even with a smaller output buffer.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The key type or key size is not supported, either by the
 *         implementation in general or in this particular location.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The provided key attributes are not valid for the operation.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The #PSA_KEY_DERIVATION_INPUT_SECRET or
 *         #PSA_KEY_DERIVATION_INPUT_PASSWORD input was not provided through a
 *         key; or one of the inputs was a key whose policy didn't allow
 *         #PSA_KEY_USAGE_DERIVE.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_output_key_custom(
    const psa_key_attributes_t *attributes,
    psa_key_derivation_operation_t *operation,
    const psa_custom_key_parameters_t *custom,
    const uint8_t *custom_data,
    size_t custom_data_length,
    mbedtls_svc_key_id_t *key);

#ifndef __cplusplus
/* Omitted when compiling in C++, because one of the parameters is a
 * pointer to a struct with a flexible array member, and that is not
 * standard C++.
 * https://github.com/Mbed-TLS/mbedtls/issues/9020
 */
/** Derive a key from an ongoing key derivation operation with custom
 *  production parameters.
 *
 * \note
 * This is a deprecated variant of psa_key_derivation_output_key_custom().
 * It is equivalent except that the associated variable-length data
 * is passed in `params->data` instead of a separate parameter.
 * This function will be removed in a future version of Mbed TLS.
 *
 * \param[in] attributes    The attributes for the new key.
 *                          If the key type to be created is
 *                          #PSA_KEY_TYPE_PASSWORD_HASH then the algorithm in
 *                          the policy must be the same as in the current
 *                          operation.
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[in] params        Customization parameters for the key derivation.
 *                          When this is #PSA_KEY_PRODUCTION_PARAMETERS_INIT
 *                          with \p params_data_length = 0,
 *                          this function is equivalent to
 *                          psa_key_derivation_output_key().
 *                          Mbed TLS currently only supports the default
 *                          production parameters, i.e.
 *                          #PSA_KEY_PRODUCTION_PARAMETERS_INIT,
 *                          for all key types.
 * \param params_data_length
 *                          Length of `params->data` in bytes.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *         There was not enough data to create the desired key.
 *         Note that in this case, no output is written to the output buffer.
 *         The operation's capacity is set to 0, thus subsequent calls to
 *         this function will not succeed, even with a smaller output buffer.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The key type or key size is not supported, either by the
 *         implementation in general or in this particular location.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The provided key attributes are not valid for the operation.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The #PSA_KEY_DERIVATION_INPUT_SECRET or
 *         #PSA_KEY_DERIVATION_INPUT_PASSWORD input was not provided through a
 *         key; or one of the inputs was a key whose policy didn't allow
 *         #PSA_KEY_USAGE_DERIVE.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_output_key_ext(
    const psa_key_attributes_t *attributes,
    psa_key_derivation_operation_t *operation,
    const psa_key_production_parameters_t *params,
    size_t params_data_length,
    mbedtls_svc_key_id_t *key);
#endif /* !__cplusplus */

/** Compare output data from a key derivation operation to an expected value.
 *
 * This function calculates output bytes from a key derivation algorithm and
 * compares those bytes to an expected value in constant time.
 * If you view the key derivation's output as a stream of bytes, this
 * function destructively reads the expected number of bytes from the
 * stream before comparing them.
 * The operation's capacity decreases by the number of bytes read.
 *
 * This is functionally equivalent to the following code:
 * \code
 * psa_key_derivation_output_bytes(operation, tmp, output_length);
 * if (memcmp(output, tmp, output_length) != 0)
 *     return PSA_ERROR_INVALID_SIGNATURE;
 * \endcode
 * except (1) it works even if the key's policy does not allow outputting the
 * bytes, and (2) the comparison will be done in constant time.
 *
 * If this function returns an error status other than
 * #PSA_ERROR_INSUFFICIENT_DATA or #PSA_ERROR_INVALID_SIGNATURE,
 * the operation enters an error state and must be aborted by calling
 * psa_key_derivation_abort().
 *
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[in] expected      Buffer containing the expected derivation output.
 * \param expected_length   Length of the expected output; this is also the
 *                          number of bytes that will be read.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The output was read successfully, but it differs from the expected
 *         output.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         One of the inputs was a key whose policy didn't allow
 *         #PSA_KEY_USAGE_VERIFY_DERIVATION.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *                          The operation's capacity was less than
 *                          \p output_length bytes. Note that in this case,
 *                          the operation's capacity is set to 0, thus
 *                          subsequent calls to this function will not
 *                          succeed, even with a smaller expected output.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_verify_bytes(
    psa_key_derivation_operation_t *operation,
    const uint8_t *expected,
    size_t expected_length);

/** Compare output data from a key derivation operation to an expected value
 * stored in a key object.
 *
 * This function calculates output bytes from a key derivation algorithm and
 * compares those bytes to an expected value, provided as key of type
 * #PSA_KEY_TYPE_PASSWORD_HASH.
 * If you view the key derivation's output as a stream of bytes, this
 * function destructively reads the number of bytes corresponding to the
 * length of the expected value from the stream before comparing them.
 * The operation's capacity decreases by the number of bytes read.
 *
 * This is functionally equivalent to exporting the key and calling
 * psa_key_derivation_verify_bytes() on the result, except that it
 * works even if the key cannot be exported.
 *
 * If this function returns an error status other than
 * #PSA_ERROR_INSUFFICIENT_DATA or #PSA_ERROR_INVALID_SIGNATURE,
 * the operation enters an error state and must be aborted by calling
 * psa_key_derivation_abort().
 *
 * \param[in,out] operation The key derivation operation object to read from.
 * \param[in] expected      A key of type #PSA_KEY_TYPE_PASSWORD_HASH
 *                          containing the expected output. Its policy must
 *                          include the #PSA_KEY_USAGE_VERIFY_DERIVATION flag
 *                          and the permitted algorithm must match the
 *                          operation. The value of this key was likely
 *                          computed by a previous call to
 *                          psa_key_derivation_output_key() or
 *                          psa_key_derivation_output_key_custom().
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The output was read successfully, but if differs from the expected
 *         output.
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         The key passed as the expected value does not exist.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key passed as the expected value has an invalid type.
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key passed as the expected value does not allow this usage or
 *         this algorithm; or one of the inputs was a key whose policy didn't
 *         allow #PSA_KEY_USAGE_VERIFY_DERIVATION.
 * \retval #PSA_ERROR_INSUFFICIENT_DATA
 *                          The operation's capacity was less than
 *                          the length of the expected value. In this case,
 *                          the operation's capacity is set to 0, thus
 *                          subsequent calls to this function will not
 *                          succeed, even with a smaller expected output.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The operation state is not valid (it must be active and completed
 *         all required input steps), or the library has not been previously
 *         initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_verify_key(
    psa_key_derivation_operation_t *operation,
    psa_key_id_t expected);

/** Abort a key derivation operation.
 *
 * Aborting an operation frees all associated resources except for the \c
 * operation structure itself. Once aborted, the operation object can be reused
 * for another operation by calling psa_key_derivation_setup() again.
 *
 * This function may be called at any time after the operation
 * object has been initialized as described in #psa_key_derivation_operation_t.
 *
 * In particular, it is valid to call psa_key_derivation_abort() twice, or to
 * call psa_key_derivation_abort() on an operation that has not been set up.
 *
 * \param[in,out] operation    The operation to abort.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_key_derivation_abort(
    psa_key_derivation_operation_t *operation);

/** Perform a key agreement and return the raw shared secret.
 *
 * \warning The raw result of a key agreement algorithm such as finite-field
 * Diffie-Hellman or elliptic curve Diffie-Hellman has biases and should
 * not be used directly as key material. It should instead be passed as
 * input to a key derivation algorithm. To chain a key agreement with
 * a key derivation, use psa_key_derivation_key_agreement() and other
 * functions from the key derivation interface.
 *
 * \param alg                     The key agreement algorithm to compute
 *                                (\c PSA_ALG_XXX value such that
 *                                #PSA_ALG_IS_RAW_KEY_AGREEMENT(\p alg)
 *                                is true).
 * \param private_key             Identifier of the private key to use. It must
 *                                allow the usage #PSA_KEY_USAGE_DERIVE.
 * \param[in] peer_key            Public key of the peer. It must be
 *                                in the same format that psa_import_key()
 *                                accepts. The standard formats for public
 *                                keys are documented in the documentation
 *                                of psa_export_public_key().
 * \param peer_key_length         Size of \p peer_key in bytes.
 * \param[out] output             Buffer where the decrypted message is to
 *                                be written.
 * \param output_size             Size of the \c output buffer in bytes.
 * \param[out] output_length      On success, the number of bytes
 *                                that make up the returned output.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \p alg is not a key agreement algorithm, or
 *         \p private_key is not compatible with \p alg,
 *         or \p peer_key is not valid for \p alg or not compatible with
 *         \p private_key.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         \p output_size is too small
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         \p alg is not a supported key agreement algorithm.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_raw_key_agreement(psa_algorithm_t alg,
                                   mbedtls_svc_key_id_t private_key,
                                   const uint8_t *peer_key,
                                   size_t peer_key_length,
                                   uint8_t *output,
                                   size_t output_size,
                                   size_t *output_length);

/**@}*/

/** \defgroup random Random generation
 * @{
 */

/**
 * \brief Generate random bytes.
 *
 * \warning This function **can** fail! Callers MUST check the return status
 *          and MUST NOT use the content of the output buffer if the return
 *          status is not #PSA_SUCCESS.
 *
 * \note    To generate a key, use psa_generate_key() instead.
 *
 * \param[out] output       Output buffer for the generated data.
 * \param output_size       Number of bytes to generate and output.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_generate_random(uint8_t *output,
                                 size_t output_size);

/**
 * \brief Generate a key or key pair.
 *
 * The key is generated randomly.
 * Its location, usage policy, type and size are taken from \p attributes.
 *
 * Implementations must reject an attempt to generate a key of size 0.
 *
 * The following type-specific considerations apply:
 * - For RSA keys (#PSA_KEY_TYPE_RSA_KEY_PAIR),
 *   the public exponent is 65537.
 *   The modulus is a product of two probabilistic primes
 *   between 2^{n-1} and 2^n where n is the bit size specified in the
 *   attributes.
 *
 * \note This function is equivalent to calling psa_generate_key_custom()
 *       with the custom production parameters #PSA_CUSTOM_KEY_PARAMETERS_INIT
 *       and `custom_data_length == 0` (i.e. `custom_data` is empty).
 *
 * \param[in] attributes    The attributes for the new key.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_generate_key(const psa_key_attributes_t *attributes,
                              mbedtls_svc_key_id_t *key);

/**
 * \brief Generate a key or key pair using custom production parameters.
 *
 * See the description of psa_generate_key() for the operation of this
 * function with the default production parameters. In addition, this function
 * supports the following production customizations, described in more detail
 * in the documentation of ::psa_custom_key_parameters_t:
 *
 * - RSA keys: generation with a custom public exponent.
 *
 * \note This function is experimental and may change in future minor
 *       versions of Mbed TLS.
 *
 * \param[in] attributes    The attributes for the new key.
 * \param[in] custom        Customization parameters for the key generation.
 *                          When this is #PSA_CUSTOM_KEY_PARAMETERS_INIT
 *                          with \p custom_data_length = 0,
 *                          this function is equivalent to
 *                          psa_generate_key().
 * \param[in] custom_data   Variable-length data associated with \c custom.
 * \param custom_data_length
 *                          Length of `custom_data` in bytes.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_generate_key_custom(const psa_key_attributes_t *attributes,
                                     const psa_custom_key_parameters_t *custom,
                                     const uint8_t *custom_data,
                                     size_t custom_data_length,
                                     mbedtls_svc_key_id_t *key);

#ifndef __cplusplus
/* Omitted when compiling in C++, because one of the parameters is a
 * pointer to a struct with a flexible array member, and that is not
 * standard C++.
 * https://github.com/Mbed-TLS/mbedtls/issues/9020
 */
/**
 * \brief Generate a key or key pair using custom production parameters.
 *
 * \note
 * This is a deprecated variant of psa_key_derivation_output_key_custom().
 * It is equivalent except that the associated variable-length data
 * is passed in `params->data` instead of a separate parameter.
 * This function will be removed in a future version of Mbed TLS.
 *
 * \param[in] attributes    The attributes for the new key.
 * \param[in] params        Customization parameters for the key generation.
 *                          When this is #PSA_KEY_PRODUCTION_PARAMETERS_INIT
 *                          with \p params_data_length = 0,
 *                          this function is equivalent to
 *                          psa_generate_key().
 * \param params_data_length
 *                          Length of `params->data` in bytes.
 * \param[out] key          On success, an identifier for the newly created
 *                          key. For persistent keys, this is the key
 *                          identifier defined in \p attributes.
 *                          \c 0 on failure.
 *
 * \retval #PSA_SUCCESS
 *         Success.
 *         If the key is persistent, the key material and the key's metadata
 *         have been saved to persistent storage.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         This is an attempt to create a persistent key, and there is
 *         already a persistent key with the given identifier.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_generate_key_ext(const psa_key_attributes_t *attributes,
                                  const psa_key_production_parameters_t *params,
                                  size_t params_data_length,
                                  mbedtls_svc_key_id_t *key);
#endif /* !__cplusplus */

/**@}*/

/** \defgroup interruptible_hash Interruptible sign/verify hash
 * @{
 */

/** The type of the state data structure for interruptible hash
 *  signing operations.
 *
 * Before calling any function on a sign hash operation object, the
 * application must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_sign_hash_interruptible_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_sign_hash_interruptible_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer
 *   #PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT, for example:
 *   \code
 *   psa_sign_hash_interruptible_operation_t operation =
 *   PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function
 *   psa_sign_hash_interruptible_operation_init() to the structure, for
 *   example:
 *   \code
 *   psa_sign_hash_interruptible_operation_t operation;
 *   operation = psa_sign_hash_interruptible_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_sign_hash_interruptible_operation_s psa_sign_hash_interruptible_operation_t;

/** The type of the state data structure for interruptible hash
 *  verification operations.
 *
 * Before calling any function on a sign hash operation object, the
 * application must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_verify_hash_interruptible_operation_t operation;
 *   memset(&operation, 0, sizeof(operation));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_verify_hash_interruptible_operation_t operation = {0};
 *   \endcode
 * - Initialize the structure to the initializer
 *   #PSA_VERIFY_HASH_INTERRUPTIBLE_OPERATION_INIT, for example:
 *   \code
 *   psa_verify_hash_interruptible_operation_t operation =
 *   PSA_VERIFY_HASH_INTERRUPTIBLE_OPERATION_INIT;
 *   \endcode
 * - Assign the result of the function
 *   psa_verify_hash_interruptible_operation_init() to the structure, for
 *   example:
 *   \code
 *   psa_verify_hash_interruptible_operation_t operation;
 *   operation = psa_verify_hash_interruptible_operation_init();
 *   \endcode
 *
 * This is an implementation-defined \c struct. Applications should not
 * make any assumptions about the content of this structure.
 * Implementation details can change in future versions without notice. */
typedef struct psa_verify_hash_interruptible_operation_s psa_verify_hash_interruptible_operation_t;

/**
 * \brief                       Set the maximum number of ops allowed to be
 *                              executed by an interruptible function in a
 *                              single call.
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        The time taken to execute a single op is
 *                              implementation specific and depends on
 *                              software, hardware, the algorithm, key type and
 *                              curve chosen. Even within a single operation,
 *                              successive ops can take differing amounts of
 *                              time. The only guarantee is that lower values
 *                              for \p max_ops means functions will block for a
 *                              lesser maximum amount of time. The functions
 *                              \c psa_sign_interruptible_get_num_ops() and
 *                              \c psa_verify_interruptible_get_num_ops() are
 *                              provided to help with tuning this value.
 *
 * \note                        This value defaults to
 *                              #PSA_INTERRUPTIBLE_MAX_OPS_UNLIMITED, which
 *                              means the whole operation will be done in one
 *                              go, regardless of the number of ops required.
 *
 * \note                        If more ops are needed to complete a
 *                              computation, #PSA_OPERATION_INCOMPLETE will be
 *                              returned by the function performing the
 *                              computation. It is then the caller's
 *                              responsibility to either call again with the
 *                              same operation context until it returns 0 or an
 *                              error code; or to call the relevant abort
 *                              function if the answer is no longer required.
 *
 * \note                        The interpretation of \p max_ops is also
 *                              implementation defined. On a hard real time
 *                              system, this can indicate a hard deadline, as a
 *                              real-time system needs a guarantee of not
 *                              spending more than X time, however care must be
 *                              taken in such an implementation to avoid the
 *                              situation whereby calls just return, not being
 *                              able to do any actual work within the allotted
 *                              time.  On a non-real-time system, the
 *                              implementation can be more relaxed, but again
 *                              whether this number should be interpreted as as
 *                              hard or soft limit or even whether a less than
 *                              or equals as regards to ops executed in a
 *                              single call is implementation defined.
 *
 * \note                        For keys in local storage when no accelerator
 *                              driver applies, please see also the
 *                              documentation for \c mbedtls_ecp_set_max_ops(),
 *                              which is the internal implementation in these
 *                              cases.
 *
 * \warning                     With implementations that interpret this number
 *                              as a hard limit, setting this number too small
 *                              may result in an infinite loop, whereby each
 *                              call results in immediate return with no ops
 *                              done (as there is not enough time to execute
 *                              any), and thus no result will ever be achieved.
 *
 * \note                        This only applies to functions whose
 *                              documentation mentions they may return
 *                              #PSA_OPERATION_INCOMPLETE.
 *
 * \param max_ops               The maximum number of ops to be executed in a
 *                              single call. This can be a number from 0 to
 *                              #PSA_INTERRUPTIBLE_MAX_OPS_UNLIMITED, where 0
 *                              is the least amount of work done per call.
 */
void psa_interruptible_set_max_ops(uint32_t max_ops);

/**
 * \brief                       Get the maximum number of ops allowed to be
 *                              executed by an interruptible function in a
 *                              single call. This will return the last
 *                              value set by
 *                              \c psa_interruptible_set_max_ops() or
 *                              #PSA_INTERRUPTIBLE_MAX_OPS_UNLIMITED if
 *                              that function has never been called.
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \return                      Maximum number of ops allowed to be
 *                              executed by an interruptible function in a
 *                              single call.
 */
uint32_t psa_interruptible_get_max_ops(void);

/**
 * \brief                       Get the number of ops that a hash signing
 *                              operation has taken so far. If the operation
 *                              has completed, then this will represent the
 *                              number of ops required for the entire
 *                              operation. After initialization or calling
 *                              \c psa_sign_hash_interruptible_abort() on
 *                              the operation, a value of 0 will be returned.
 *
 * \note                        This interface is guaranteed re-entrant and
 *                              thus may be called from driver code.
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 *                              This is a helper provided to help you tune the
 *                              value passed to \c
 *                              psa_interruptible_set_max_ops().
 *
 * \param operation             The \c psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \return                      Number of ops that the operation has taken so
 *                              far.
 */
uint32_t psa_sign_hash_get_num_ops(
    const psa_sign_hash_interruptible_operation_t *operation);

/**
 * \brief                       Get the number of ops that a hash verification
 *                              operation has taken so far. If the operation
 *                              has completed, then this will represent the
 *                              number of ops required for the entire
 *                              operation. After initialization or calling \c
 *                              psa_verify_hash_interruptible_abort() on the
 *                              operation, a value of 0 will be returned.
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 *                              This is a helper provided to help you tune the
 *                              value passed to \c
 *                              psa_interruptible_set_max_ops().
 *
 * \param operation             The \c
 *                              psa_verify_hash_interruptible_operation_t to
 *                              use. This must be initialized first.
 *
 * \return                      Number of ops that the operation has taken so
 *                              far.
 */
uint32_t psa_verify_hash_get_num_ops(
    const psa_verify_hash_interruptible_operation_t *operation);

/**
 * \brief                       Start signing a hash or short message with a
 *                              private key, in an interruptible manner.
 *
 * \see                         \c psa_sign_hash_complete()
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        This function combined with \c
 *                              psa_sign_hash_complete() is equivalent to
 *                              \c psa_sign_hash() but
 *                              \c psa_sign_hash_complete() can return early and
 *                              resume according to the limit set with \c
 *                              psa_interruptible_set_max_ops() to reduce the
 *                              maximum time spent in a function call.
 *
 * \note                        Users should call \c psa_sign_hash_complete()
 *                              repeatedly on the same context after a
 *                              successful call to this function until \c
 *                              psa_sign_hash_complete() either returns 0 or an
 *                              error. \c psa_sign_hash_complete() will return
 *                              #PSA_OPERATION_INCOMPLETE if there is more work
 *                              to do. Alternatively users can call
 *                              \c psa_sign_hash_abort() at any point if they no
 *                              longer want the result.
 *
 * \note                        If this function returns an error status, the
 *                              operation enters an error state and must be
 *                              aborted by calling \c psa_sign_hash_abort().
 *
 * \param[in, out] operation    The \c psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              It must be an asymmetric key pair. The key must
 *                              allow the usage #PSA_KEY_USAGE_SIGN_HASH.
 * \param alg                   A signature algorithm (\c PSA_ALG_XXX
 *                              value such that #PSA_ALG_IS_SIGN_HASH(\p alg)
 *                              is true), that is compatible with
 *                              the type of \p key.
 * \param[in] hash              The hash or message to sign.
 * \param hash_length           Size of the \p hash buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The operation started successfully - call \c psa_sign_hash_complete()
 *         with the same context to complete the operation
 *
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key does not have the #PSA_KEY_USAGE_SIGN_HASH flag, or it does
 *         not permit the requested algorithm.
 * \retval #PSA_ERROR_BAD_STATE
 *         An operation has previously been started on this context, and is
 *         still in progress.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_sign_hash_start(
    psa_sign_hash_interruptible_operation_t *operation,
    mbedtls_svc_key_id_t key, psa_algorithm_t alg,
    const uint8_t *hash, size_t hash_length);

/**
 * \brief                       Continue and eventually complete the action of
 *                              signing a hash or short message with a private
 *                              key, in an interruptible manner.
 *
 * \see                         \c psa_sign_hash_start()
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        This function combined with \c
 *                              psa_sign_hash_start() is equivalent to
 *                              \c psa_sign_hash() but this function can return
 *                              early and resume according to the limit set with
 *                              \c psa_interruptible_set_max_ops() to reduce the
 *                              maximum time spent in a function call.
 *
 * \note                        Users should call this function on the same
 *                              operation object repeatedly until it either
 *                              returns 0 or an error. This function will return
 *                              #PSA_OPERATION_INCOMPLETE if there is more work
 *                              to do. Alternatively users can call
 *                              \c psa_sign_hash_abort() at any point if they no
 *                              longer want the result.
 *
 * \note                        When this function returns successfully, the
 *                              operation becomes inactive. If this function
 *                              returns an error status, the operation enters an
 *                              error state and must be aborted by calling
 *                              \c psa_sign_hash_abort().
 *
 * \param[in, out] operation    The \c psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first, and have
 *                              had \c psa_sign_hash_start() called with it
 *                              first.
 *
 * \param[out] signature        Buffer where the signature is to be written.
 * \param signature_size        Size of the \p signature buffer in bytes. This
 *                              must be appropriate for the selected
 *                              algorithm and key:
 *                              - The required signature size is
 *                                #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c
 *                                key_bits, \c alg) where \c key_type and \c
 *                                key_bits are the type and bit-size
 *                                respectively of key.
 *                              - #PSA_SIGNATURE_MAX_SIZE evaluates to the
 *                                maximum signature size of any supported
 *                                signature algorithm.
 * \param[out] signature_length On success, the number of bytes that make up
 *                              the returned signature value.
 *
 * \retval #PSA_SUCCESS
 *         Operation completed successfully
 *
 * \retval #PSA_OPERATION_INCOMPLETE
 *         Operation was interrupted due to the setting of \c
 *         psa_interruptible_set_max_ops(). There is still work to be done.
 *         Call this function again with the same operation object.
 *
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \c alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \c key.
 *
 * \retval #PSA_ERROR_BAD_STATE
 *         An operation was not previously started on this context via
 *         \c psa_sign_hash_start().
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has either not been previously initialized by
 *         psa_crypto_init() or you did not previously call
 *         psa_sign_hash_start() with this operation object. It is
 *         implementation-dependent whether a failure to initialize results in
 *         this error code.
 */
psa_status_t psa_sign_hash_complete(
    psa_sign_hash_interruptible_operation_t *operation,
    uint8_t *signature, size_t signature_size,
    size_t *signature_length);

/**
 * \brief                       Abort a sign hash operation.
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        This function is the only function that clears
 *                              the number of ops completed as part of the
 *                              operation. Please ensure you copy this value via
 *                              \c psa_sign_hash_get_num_ops() if required
 *                              before calling.
 *
 * \note                        Aborting an operation frees all associated
 *                              resources except for the \p operation structure
 *                              itself. Once aborted, the operation object can
 *                              be reused for another operation by calling \c
 *                              psa_sign_hash_start() again.
 *
 * \note                        You may call this function any time after the
 *                              operation object has been initialized. In
 *                              particular, calling \c psa_sign_hash_abort()
 *                              after the operation has already been terminated
 *                              by a call to \c psa_sign_hash_abort() or
 *                              psa_sign_hash_complete() is safe.
 *
 * \param[in,out] operation     Initialized sign hash operation.
 *
 * \retval #PSA_SUCCESS
 *         The operation was aborted successfully.
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_sign_hash_abort(
    psa_sign_hash_interruptible_operation_t *operation);

/**
 * \brief                       Start reading and verifying a hash or short
 *                              message, in an interruptible manner.
 *
 * \see                         \c psa_verify_hash_complete()
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        This function combined with \c
 *                              psa_verify_hash_complete() is equivalent to
 *                              \c psa_verify_hash() but \c
 *                              psa_verify_hash_complete() can return early and
 *                              resume according to the limit set with \c
 *                              psa_interruptible_set_max_ops() to reduce the
 *                              maximum time spent in a function.
 *
 * \note                        Users should call \c psa_verify_hash_complete()
 *                              repeatedly on the same operation object after a
 *                              successful call to this function until \c
 *                              psa_verify_hash_complete() either returns 0 or
 *                              an error. \c psa_verify_hash_complete() will
 *                              return #PSA_OPERATION_INCOMPLETE if there is
 *                              more work to do. Alternatively users can call
 *                              \c psa_verify_hash_abort() at any point if they
 *                              no longer want the result.
 *
 * \note                        If this function returns an error status, the
 *                              operation enters an error state and must be
 *                              aborted by calling \c psa_verify_hash_abort().
 *
 * \param[in, out] operation    The \c psa_verify_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \param key                   Identifier of the key to use for the operation.
 *                              The key must allow the usage
 *                              #PSA_KEY_USAGE_VERIFY_HASH.
 * \param alg                   A signature algorithm (\c PSA_ALG_XXX
 *                              value such that #PSA_ALG_IS_SIGN_HASH(\p alg)
 *                              is true), that is compatible with
 *                              the type of \p key.
 * \param[in] hash              The hash whose signature is to be verified.
 * \param hash_length           Size of the \p hash buffer in bytes.
 * \param[in] signature         Buffer containing the signature to verify.
 * \param signature_length      Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The operation started successfully - please call \c
 *         psa_verify_hash_complete() with the same context to complete the
 *         operation.
 *
 * \retval #PSA_ERROR_BAD_STATE
 *         Another operation has already been started on this context, and is
 *         still in progress.
 *
 * \retval #PSA_ERROR_NOT_PERMITTED
 *         The key does not have the #PSA_KEY_USAGE_VERIFY_HASH flag, or it does
 *         not permit the requested algorithm.
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_verify_hash_start(
    psa_verify_hash_interruptible_operation_t *operation,
    mbedtls_svc_key_id_t key, psa_algorithm_t alg,
    const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);

/**
 * \brief                       Continue and eventually complete the action of
 *                              reading and verifying a hash or short message
 *                              signed with a private key, in an interruptible
 *                              manner.
 *
 * \see                         \c psa_verify_hash_start()
 *
 * \warning                     This is a beta API, and thus subject to change
 *                              at any point. It is not bound by the usual
 *                              interface stability promises.
 *
 * \note                        This function combined with \c
 *                              psa_verify_hash_start() is equivalent to
 *                              \c psa_verify_hash() but this function can
 *                              return early and resume according to the limit
 *                              set with \c psa_interruptible_set_max_ops() to
 *                              reduce the maximum time spent in a function
 *                              call.
 *
 * \note                        Users should call this function on the same
 *                              operation object repeatedly until it either
 *                              returns 0 or an error. This function will return
 *                              #PSA_OPERATION_INCOMPLETE if there is more work
 *                              to do. Alternatively users can call
 *                              \c psa_verify_hash_abort() at any point if they
 *                              no longer want the result.
 *
 * \note                        When this function returns successfully, the
 *                              operation becomes inactive. If this function
 *                              returns an error status, the operation enters an
 *                              error state and must be aborted by calling
 *                              \c psa_verify_hash_abort().
 *
 * \param[in, out] operation    The \c psa_verify_hash_interruptible_operation_t
 *                              to use. This must be initialized first, and have
 *                              had \c psa_verify_hash_start() called with it
 *                              first.
 *
 * \retval #PSA_SUCCESS
 *         Operation completed successfully, and the passed signature is valid.
 *
 * \retval #PSA_OPERATION_INCOMPLETE
 *         Operation was interrupted due to the setting of \c
 *         psa_interruptible_set_max_ops(). There is still work to be done.
 *         Call this function again with the same operation object.
 *
 * \retval #PSA_ERROR_INVALID_HANDLE \emptydescription
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 * \retval #PSA_ERROR_BAD_STATE
 *         An operation was not previously started on this context via
 *         \c psa_verify_hash_start().
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has either not been previously initialized by
 *         psa_crypto_init() or you did not previously call
 *         psa_verify_hash_start() on this object. It is
 *         implementation-dependent whether a failure to initialize results in
 *         this error code.
 */
psa_status_t psa_verify_hash_complete(
    psa_verify_hash_interruptible_operation_t *operation);

/**
 * \brief                     Abort a verify hash operation.
 *
 * \warning                   This is a beta API, and thus subject to change at
 *                            any point. It is not bound by the usual interface
 *                            stability promises.
 *
 * \note                      This function is the only function that clears the
 *                            number of ops completed as part of the operation.
 *                            Please ensure you copy this value via
 *                            \c psa_verify_hash_get_num_ops() if required
 *                            before calling.
 *
 * \note                      Aborting an operation frees all associated
 *                            resources except for the operation structure
 *                            itself. Once aborted, the operation object can be
 *                            reused for another operation by calling \c
 *                            psa_verify_hash_start() again.
 *
 * \note                      You may call this function any time after the
 *                            operation object has been initialized.
 *                            In particular, calling \c psa_verify_hash_abort()
 *                            after the operation has already been terminated by
 *                            a call to \c psa_verify_hash_abort() or
 *                            psa_verify_hash_complete() is safe.
 *
 * \param[in,out] operation   Initialized verify hash operation.
 *
 * \retval #PSA_SUCCESS
 *         The operation was aborted successfully.
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been previously initialized by psa_crypto_init().
 *         It is implementation-dependent whether a failure to initialize
 *         results in this error code.
 */
psa_status_t psa_verify_hash_abort(
    psa_verify_hash_interruptible_operation_t *operation);


/**@}*/

#ifdef __cplusplus
}
#endif

/* The file "crypto_extra.h" contains vendor-specific definitions. This
 * can include vendor-defined algorithms, extra functions, etc. */
#include "crypto_extra.h"

#endif /* PSA_CRYPTO_H */
