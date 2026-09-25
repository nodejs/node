/**
 * \file psa/crypto_types.h
 *
 * \brief PSA cryptography module: type aliases.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h. Drivers must include the appropriate driver
 * header file.
 *
 * This file contains portable definitions of integral types for properties
 * of cryptographic keys, designations of cryptographic algorithms, and
 * error codes returned by the library.
 *
 * This header file does not declare any function.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_TYPES_H
#define PSA_CRYPTO_TYPES_H

/*
 * Include the build-time configuration information header. Here, we do not
 * include `"mbedtls/build_info.h"` directly but `"psa/build_info.h"`, which
 * is basically just an alias to it. This is to ease the maintenance of the
 * TF-PSA-Crypto repository which has a different build system and
 * configuration.
 */
#include "psa/build_info.h"

/* Define the MBEDTLS_PRIVATE macro. */
#include "mbedtls/private_access.h"

#if defined(MBEDTLS_PSA_CRYPTO_PLATFORM_FILE)
#include MBEDTLS_PSA_CRYPTO_PLATFORM_FILE
#else
#include "crypto_platform.h"
#endif

#include <stdint.h>

/** \defgroup error Error codes
 * @{
 */

/**
 * \brief Function return status.
 *
 * This is either #PSA_SUCCESS (which is zero), indicating success,
 * or a small negative value indicating that an error occurred. Errors are
 * encoded as one of the \c PSA_ERROR_xxx values defined here. */
/* If #PSA_SUCCESS is already defined, it means that #psa_status_t
 * is also defined in an external header, so prevent its multiple
 * definition.
 */
#ifndef PSA_SUCCESS
typedef int32_t psa_status_t;
#endif

/**@}*/

/** \defgroup crypto_types Key and algorithm types
 * @{
 */

/** \brief Encoding of a key type.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_KEY_TYPE_xxx`.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint16_t psa_key_type_t;

/** The type of PSA elliptic curve family identifiers.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_ECC_FAMILY_xxx`.
 *
 * The curve identifier is required to create an ECC key using the
 * PSA_KEY_TYPE_ECC_KEY_PAIR() or PSA_KEY_TYPE_ECC_PUBLIC_KEY()
 * macros.
 *
 * Values defined by this standard will never be in the range 0x80-0xff.
 * Vendors who define additional families must use an encoding in this range.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint8_t psa_ecc_family_t;

/** The type of PSA Diffie-Hellman group family identifiers.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_DH_FAMILY_xxx`.
 *
 * The group identifier is required to create a Diffie-Hellman key using the
 * PSA_KEY_TYPE_DH_KEY_PAIR() or PSA_KEY_TYPE_DH_PUBLIC_KEY()
 * macros.
 *
 * Values defined by this standard will never be in the range 0x80-0xff.
 * Vendors who define additional families must use an encoding in this range.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint8_t psa_dh_family_t;

/** \brief Encoding of a cryptographic algorithm.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_ALG_xxx`.
 *
 * For algorithms that can be applied to multiple key types, this type
 * does not encode the key type. For example, for symmetric ciphers
 * based on a block cipher, #psa_algorithm_t encodes the block cipher
 * mode and the padding mode while the block cipher itself is encoded
 * via #psa_key_type_t.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint32_t psa_algorithm_t;

/**@}*/

/** \defgroup key_lifetimes Key lifetimes
 * @{
 */

/** Encoding of key lifetimes.
 *
 * The lifetime of a key indicates where it is stored and what system actions
 * may create and destroy it.
 *
 * Lifetime values have the following structure:
 * - Bits 0-7 (#PSA_KEY_LIFETIME_GET_PERSISTENCE(\c lifetime)):
 *   persistence level. This value indicates what device management
 *   actions can cause it to be destroyed. In particular, it indicates
 *   whether the key is _volatile_ or _persistent_.
 *   See ::psa_key_persistence_t for more information.
 * - Bits 8-31 (#PSA_KEY_LIFETIME_GET_LOCATION(\c lifetime)):
 *   location indicator. This value indicates which part of the system
 *   has access to the key material and can perform operations using the key.
 *   See ::psa_key_location_t for more information.
 *
 * Volatile keys are automatically destroyed when the application instance
 * terminates or on a power reset of the device. Persistent keys are
 * preserved until the application explicitly destroys them or until an
 * integration-specific device management event occurs (for example,
 * a factory reset).
 *
 * Persistent keys have a key identifier of type #mbedtls_svc_key_id_t.
 * This identifier remains valid throughout the lifetime of the key,
 * even if the application instance that created the key terminates.
 * The application can call psa_open_key() to open a persistent key that
 * it created previously.
 *
 * The default lifetime of a key is #PSA_KEY_LIFETIME_VOLATILE. The lifetime
 * #PSA_KEY_LIFETIME_PERSISTENT is supported if persistent storage is
 * available. Other lifetime values may be supported depending on the
 * library configuration.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_KEY_LIFETIME_xxx`.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint32_t psa_key_lifetime_t;

/** Encoding of key persistence levels.
 *
 * What distinguishes different persistence levels is what device management
 * events may cause keys to be destroyed. _Volatile_ keys are destroyed
 * by a power reset. Persistent keys may be destroyed by events such as
 * a transfer of ownership or a factory reset. What management events
 * actually affect persistent keys at different levels is outside the
 * scope of the PSA Cryptography specification.
 *
 * The PSA Cryptography specification defines the following values of
 * persistence levels:
 * - \c 0 = #PSA_KEY_PERSISTENCE_VOLATILE: volatile key.
 *   A volatile key is automatically destroyed by the implementation when
 *   the application instance terminates. In particular, a volatile key
 *   is automatically destroyed on a power reset of the device.
 * - \c 1 = #PSA_KEY_PERSISTENCE_DEFAULT:
 *   persistent key with a default lifetime.
 * - \c 2-254: currently not supported by Mbed TLS.
 * - \c 255 = #PSA_KEY_PERSISTENCE_READ_ONLY:
 *   read-only or write-once key.
 *   A key with this persistence level cannot be destroyed.
 *   Mbed TLS does not currently offer a way to create such keys, but
 *   integrations of Mbed TLS can use it for built-in keys that the
 *   application cannot modify (for example, a hardware unique key (HUK)).
 *
 * \note Key persistence levels are 8-bit values. Key management
 *       interfaces operate on lifetimes (type ::psa_key_lifetime_t) which
 *       encode the persistence as the lower 8 bits of a 32-bit value.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint8_t psa_key_persistence_t;

/** Encoding of key location indicators.
 *
 * If an integration of Mbed TLS can make calls to external
 * cryptoprocessors such as secure elements, the location of a key
 * indicates which secure element performs the operations on the key.
 * Depending on the design of the secure element, the key
 * material may be stored either in the secure element, or
 * in wrapped (encrypted) form alongside the key metadata in the
 * primary local storage.
 *
 * The PSA Cryptography API specification defines the following values of
 * location indicators:
 * - \c 0: primary local storage.
 *   This location is always available.
 *   The primary local storage is typically the same storage area that
 *   contains the key metadata.
 * - \c 1: primary secure element.
 *   Integrations of Mbed TLS should support this value if there is a secure
 *   element attached to the operating environment.
 *   As a guideline, secure elements may provide higher resistance against
 *   side channel and physical attacks than the primary local storage, but may
 *   have restrictions on supported key types, sizes, policies and operations
 *   and may have different performance characteristics.
 * - \c 2-0x7fffff: other locations defined by a PSA specification.
 *   The PSA Cryptography API does not currently assign any meaning to these
 *   locations, but future versions of that specification or other PSA
 *   specifications may do so.
 * - \c 0x800000-0xffffff: vendor-defined locations.
 *   No PSA specification will assign a meaning to locations in this range.
 *
 * \note Key location indicators are 24-bit values. Key management
 *       interfaces operate on lifetimes (type ::psa_key_lifetime_t) which
 *       encode the location as the upper 24 bits of a 32-bit value.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint32_t psa_key_location_t;

/** Encoding of identifiers of persistent keys.
 *
 * - Applications may freely choose key identifiers in the range
 *   #PSA_KEY_ID_USER_MIN to #PSA_KEY_ID_USER_MAX.
 * - The implementation may define additional key identifiers in the range
 *   #PSA_KEY_ID_VENDOR_MIN to #PSA_KEY_ID_VENDOR_MAX.
 * - 0 is reserved as an invalid key identifier.
 * - Key identifiers outside these ranges are reserved for future use.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to how values are allocated must require careful
 *       consideration to allow backward compatibility.
 */
typedef uint32_t psa_key_id_t;

/** Encoding of key identifiers as seen inside the PSA Crypto implementation.
 *
 * When PSA Crypto is built as a library inside an application, this type
 * is identical to #psa_key_id_t. When PSA Crypto is built as a service
 * that can store keys on behalf of multiple clients, this type
 * encodes the #psa_key_id_t value seen by each client application as
 * well as extra information that identifies the client that owns
 * the key.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
#if !defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)
typedef psa_key_id_t mbedtls_svc_key_id_t;

#else /* MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */
/* Implementation-specific: The Mbed TLS library can be built as
 * part of a multi-client service that exposes the PSA Cryptography API in each
 * client and encodes the client identity in the key identifier argument of
 * functions such as psa_open_key().
 */
typedef struct {
    psa_key_id_t MBEDTLS_PRIVATE(key_id);
    mbedtls_key_owner_id_t MBEDTLS_PRIVATE(owner);
} mbedtls_svc_key_id_t;

#endif /* !MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */

/**@}*/

/** \defgroup policy Key policies
 * @{
 */

/** \brief Encoding of permitted usage on a key.
 *
 * Values of this type are generally constructed as bitwise-ors of macros
 * called `PSA_KEY_USAGE_xxx`.
 *
 * \note Values of this type are encoded in the persistent key store.
 *       Any changes to existing values will require bumping the storage
 *       format version and providing a translation when reading the old
 *       format.
 */
typedef uint32_t psa_key_usage_t;

/**@}*/

/** \defgroup attributes Key attributes
 * @{
 */

/** The type of a structure containing key attributes.
 *
 * This is an opaque structure that can represent the metadata of a key
 * object. Metadata that can be stored in attributes includes:
 * - The location of the key in storage, indicated by its key identifier
 *   and its lifetime.
 * - The key's policy, comprising usage flags and a specification of
 *   the permitted algorithm(s).
 * - Information about the key itself: the key type and its size.
 * - Additional implementation-defined attributes.
 *
 * The actual key material is not considered an attribute of a key.
 * Key attributes do not contain information that is generally considered
 * highly confidential.
 *
 * An attribute structure works like a simple data structure where each function
 * `psa_set_key_xxx` sets a field and the corresponding function
 * `psa_get_key_xxx` retrieves the value of the corresponding field.
 * However, a future version of the library  may report values that are
 * equivalent to the original one, but have a different encoding. Invalid
 * values may be mapped to different, also invalid values.
 *
 * An attribute structure may contain references to auxiliary resources,
 * for example pointers to allocated memory or indirect references to
 * pre-calculated values. In order to free such resources, the application
 * must call psa_reset_key_attributes(). As an exception, calling
 * psa_reset_key_attributes() on an attribute structure is optional if
 * the structure has only been modified by the following functions
 * since it was initialized or last reset with psa_reset_key_attributes():
 * - psa_set_key_id()
 * - psa_set_key_lifetime()
 * - psa_set_key_type()
 * - psa_set_key_bits()
 * - psa_set_key_usage_flags()
 * - psa_set_key_algorithm()
 *
 * Before calling any function on a key attribute structure, the application
 * must initialize it by any of the following means:
 * - Set the structure to all-bits-zero, for example:
 *   \code
 *   psa_key_attributes_t attributes;
 *   memset(&attributes, 0, sizeof(attributes));
 *   \endcode
 * - Initialize the structure to logical zero values, for example:
 *   \code
 *   psa_key_attributes_t attributes = {0};
 *   \endcode
 * - Initialize the structure to the initializer #PSA_KEY_ATTRIBUTES_INIT,
 *   for example:
 *   \code
 *   psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
 *   \endcode
 * - Assign the result of the function psa_key_attributes_init()
 *   to the structure, for example:
 *   \code
 *   psa_key_attributes_t attributes;
 *   attributes = psa_key_attributes_init();
 *   \endcode
 *
 * A freshly initialized attribute structure contains the following
 * values:
 *
 * - lifetime: #PSA_KEY_LIFETIME_VOLATILE.
 * - key identifier: 0 (which is not a valid key identifier).
 * - type: \c 0 (meaning that the type is unspecified).
 * - key size: \c 0 (meaning that the size is unspecified).
 * - usage flags: \c 0 (which allows no usage except exporting a public key).
 * - algorithm: \c 0 (which allows no cryptographic usage, but allows
 *   exporting).
 *
 * A typical sequence to create a key is as follows:
 * -# Create and initialize an attribute structure.
 * -# If the key is persistent, call psa_set_key_id().
 *    Also call psa_set_key_lifetime() to place the key in a non-default
 *    location.
 * -# Set the key policy with psa_set_key_usage_flags() and
 *    psa_set_key_algorithm().
 * -# Set the key type with psa_set_key_type().
 *    Skip this step if copying an existing key with psa_copy_key().
 * -# When generating a random key with psa_generate_key() or deriving a key
 *    with psa_key_derivation_output_key(), set the desired key size with
 *    psa_set_key_bits().
 * -# Call a key creation function: psa_import_key(), psa_generate_key(),
 *    psa_key_derivation_output_key() or psa_copy_key(). This function reads
 *    the attribute structure, creates a key with these attributes, and
 *    outputs a key identifier to the newly created key.
 * -# The attribute structure is now no longer necessary.
 *    You may call psa_reset_key_attributes(), although this is optional
 *    with the workflow presented here because the attributes currently
 *    defined in this specification do not require any additional resources
 *    beyond the structure itself.
 *
 * A typical sequence to query a key's attributes is as follows:
 * -# Call psa_get_key_attributes().
 * -# Call `psa_get_key_xxx` functions to retrieve the attribute(s) that
 *    you are interested in.
 * -# Call psa_reset_key_attributes() to free any resources that may be
 *    used by the attribute structure.
 *
 * Once a key has been created, it is impossible to change its attributes.
 */
typedef struct psa_key_attributes_s psa_key_attributes_t;


#ifndef __DOXYGEN_ONLY__
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
/* Mbed TLS defines this type in crypto_types.h because it is also
 * visible to applications through an implementation-specific extension.
 * For the PSA Cryptography specification, this type is only visible
 * via crypto_se_driver.h. */
typedef uint64_t psa_key_slot_number_t;
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
#endif /* !__DOXYGEN_ONLY__ */

/**@}*/

/** \defgroup derivation Key derivation
 * @{
 */

/** \brief Encoding of the step of a key derivation.
 *
 * Values of this type are generally constructed by macros called
 * `PSA_KEY_DERIVATION_INPUT_xxx`.
 */
typedef uint16_t psa_key_derivation_step_t;

/** \brief Custom parameters for key generation or key derivation.
 *
 * This is a structure type with at least the following field:
 *
 * - \c flags: an unsigned integer type. 0 for the default production parameters.
 *
 * Functions that take such a structure as input also take an associated
 * input buffer \c custom_data of length \c custom_data_length.
 *
 * The interpretation of this structure and the associated \c custom_data
 * parameter depend on the type of the created key.
 *
 * - #PSA_KEY_TYPE_RSA_KEY_PAIR:
 *     - \c flags: must be 0.
 *     - \c custom_data: the public exponent, in little-endian order.
 *       This must be an odd integer and must not be 1.
 *       Implementations must support 65537, should support 3 and may
 *       support other values.
 *       When not using a driver, Mbed TLS supports values up to \c INT_MAX.
 *       If this is empty, the default value 65537 is used.
 * - Other key types: reserved for future use. \c flags must be 0.
 */
typedef struct psa_custom_key_parameters_s psa_custom_key_parameters_t;

/** \brief Custom parameters for key generation or key derivation.
 *
 * This is a structure type with at least the following fields:
 *
 * - \c flags: an unsigned integer type. 0 for the default production parameters.
 * - \c data: a flexible array of bytes.
 *
 * The interpretation of this structure depend on the type of the
 * created key.
 *
 * - #PSA_KEY_TYPE_RSA_KEY_PAIR:
 *     - \c flags: must be 0.
 *     - \c data: the public exponent, in little-endian order.
 *       This must be an odd integer and must not be 1.
 *       Implementations must support 65537, should support 3 and may
 *       support other values.
 *       When not using a driver, Mbed TLS supports values up to \c INT_MAX.
 *       If this is empty or if the custom production parameters are omitted
 *       altogether, the default value 65537 is used.
 * - Other key types: reserved for future use. \c flags must be 0.
 *
 */
typedef struct psa_key_production_parameters_s psa_key_production_parameters_t;

/**@}*/

#endif /* PSA_CRYPTO_TYPES_H */
