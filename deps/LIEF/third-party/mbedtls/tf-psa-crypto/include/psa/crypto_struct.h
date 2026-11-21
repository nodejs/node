/**
 * \file psa/crypto_struct.h
 *
 * \brief PSA cryptography module: Mbed TLS structured type implementations
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * This file contains the definitions of some data structures with
 * implementation-specific definitions.
 *
 * In implementations with isolation between the application and the
 * cryptography module, it is expected that the front-end and the back-end
 * would have different versions of this file.
 *
 * <h3>Design notes about multipart operation structures</h3>
 *
 * For multipart operations without driver delegation support, each multipart
 * operation structure contains a `psa_algorithm_t alg` field which indicates
 * which specific algorithm the structure is for. When the structure is not in
 * use, `alg` is 0. Most of the structure consists of a union which is
 * discriminated by `alg`.
 *
 * For multipart operations with driver delegation support, each multipart
 * operation structure contains an `unsigned int id` field indicating which
 * driver got assigned to do the operation. When the structure is not in use,
 * 'id' is 0. The structure contains also a driver context which is the union
 * of the contexts of all drivers able to handle the type of multipart
 * operation.
 *
 * Note that when `alg` or `id` is 0, the content of other fields is undefined.
 * In particular, it is not guaranteed that a freshly-initialized structure
 * is all-zero: we initialize structures to something like `{0, 0}`, which
 * is only guaranteed to initializes the first member of the union;
 * GCC and Clang initialize the whole structure to 0 (at the time of writing),
 * but MSVC and CompCert don't.
 *
 * In Mbed TLS, multipart operation structures live independently from
 * the key. This allows Mbed TLS to free the key objects when destroying
 * a key slot. If a multipart operation needs to remember the key after
 * the setup function returns, the operation structure needs to contain a
 * copy of the key.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_STRUCT_H
#define PSA_CRYPTO_STRUCT_H
#include "mbedtls/private_access.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "tf-psa-crypto/build_info.h"

/* Include the context definition for the compiled-in drivers for the primitive
 * algorithms. */
#include "psa/crypto_driver_contexts_primitives.h"

struct psa_hash_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_driver_wrappers.h.
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. the driver context is not active, in use). */
    unsigned int MBEDTLS_PRIVATE(id);
    psa_driver_hash_context_t MBEDTLS_PRIVATE(ctx);
#endif
};
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_HASH_OPERATION_INIT { 0 }
#else
#define PSA_HASH_OPERATION_INIT { 0, { 0 } }
#endif
static inline struct psa_hash_operation_s psa_hash_operation_init(void)
{
    const struct psa_hash_operation_s v = PSA_HASH_OPERATION_INIT;
    return v;
}

struct psa_cipher_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active). */
    unsigned int MBEDTLS_PRIVATE(id);

    unsigned int MBEDTLS_PRIVATE(iv_required) : 1;
    unsigned int MBEDTLS_PRIVATE(iv_set) : 1;

    uint8_t MBEDTLS_PRIVATE(default_iv_length);

    psa_driver_cipher_context_t MBEDTLS_PRIVATE(ctx);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_CIPHER_OPERATION_INIT { 0 }
#else
#define PSA_CIPHER_OPERATION_INIT { 0, 0, 0, 0, { 0 } }
#endif
static inline struct psa_cipher_operation_s psa_cipher_operation_init(void)
{
    const struct psa_cipher_operation_s v = PSA_CIPHER_OPERATION_INIT;
    return v;
}

/* Include the context definition for the compiled-in drivers for the composite
 * algorithms. */
#include "psa/crypto_driver_contexts_composites.h"

struct psa_mac_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active). */
    unsigned int MBEDTLS_PRIVATE(id);
    uint8_t MBEDTLS_PRIVATE(mac_size);
    unsigned int MBEDTLS_PRIVATE(is_sign) : 1;
    psa_driver_mac_context_t MBEDTLS_PRIVATE(ctx);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_MAC_OPERATION_INIT { 0 }
#else
#define PSA_MAC_OPERATION_INIT { 0, 0, 0, { 0 } }
#endif
static inline struct psa_mac_operation_s psa_mac_operation_init(void)
{
    const struct psa_mac_operation_s v = PSA_MAC_OPERATION_INIT;
    return v;
}

struct psa_aead_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active). */
    unsigned int MBEDTLS_PRIVATE(id);

    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    psa_key_type_t MBEDTLS_PRIVATE(key_type);

    size_t MBEDTLS_PRIVATE(ad_remaining);
    size_t MBEDTLS_PRIVATE(body_remaining);

    unsigned int MBEDTLS_PRIVATE(nonce_set) : 1;
    unsigned int MBEDTLS_PRIVATE(lengths_set) : 1;
    unsigned int MBEDTLS_PRIVATE(ad_started) : 1;
    unsigned int MBEDTLS_PRIVATE(body_started) : 1;
    unsigned int MBEDTLS_PRIVATE(is_encrypt) : 1;

    psa_driver_aead_context_t MBEDTLS_PRIVATE(ctx);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_AEAD_OPERATION_INIT { 0 }
#else
#define PSA_AEAD_OPERATION_INIT { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 } }
#endif
static inline struct psa_aead_operation_s psa_aead_operation_init(void)
{
    const struct psa_aead_operation_s v = PSA_AEAD_OPERATION_INIT;
    return v;
}

/* Include the context definition for the compiled-in drivers for the key
 * derivation algorithms. */
#include "psa/crypto_driver_contexts_key_derivation.h"

struct psa_key_derivation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    unsigned int MBEDTLS_PRIVATE(can_output_key) : 1;
    size_t MBEDTLS_PRIVATE(capacity);
    psa_driver_key_derivation_context_t MBEDTLS_PRIVATE(ctx);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_KEY_DERIVATION_OPERATION_INIT { 0 }
#else
/* This only zeroes out the first byte in the union, the rest is unspecified. */
#define PSA_KEY_DERIVATION_OPERATION_INIT { 0, 0, 0, { 0 } }
#endif
static inline struct psa_key_derivation_s psa_key_derivation_operation_init(
    void)
{
    const struct psa_key_derivation_s v = PSA_KEY_DERIVATION_OPERATION_INIT;
    return v;
}

struct psa_custom_key_parameters_s {
    /* Future versions may add other fields in this structure. */
    uint32_t flags;
};

/** The default production parameters for key generation or key derivation.
 *
 * Calling psa_generate_key_custom() or psa_key_derivation_output_key_custom()
 * with `custom=PSA_CUSTOM_KEY_PARAMETERS_INIT` and `custom_data_length=0` is
 * equivalent to calling psa_generate_key() or psa_key_derivation_output_key()
 * respectively.
 */
#define PSA_CUSTOM_KEY_PARAMETERS_INIT { 0 }

struct psa_key_policy_s {
    psa_key_usage_t MBEDTLS_PRIVATE(usage);
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    psa_algorithm_t MBEDTLS_PRIVATE(alg2);
};
typedef struct psa_key_policy_s psa_key_policy_t;

#define PSA_KEY_POLICY_INIT { 0, 0, 0 }
static inline struct psa_key_policy_s psa_key_policy_init(void)
{
    const struct psa_key_policy_s v = PSA_KEY_POLICY_INIT;
    return v;
}

/* The type used internally for key sizes.
 * Public interfaces use size_t, but internally we use a smaller type. */
typedef uint16_t psa_key_bits_t;
/* The maximum value of the type used to represent bit-sizes.
 * This is used to mark an invalid key size. */
#define PSA_KEY_BITS_TOO_LARGE          ((psa_key_bits_t) -1)
/* The maximum size of a key in bits.
 * Currently defined as the maximum that can be represented, rounded down
 * to a whole number of bytes.
 * This is an uncast value so that it can be used in preprocessor
 * conditionals. */
#define PSA_MAX_KEY_BITS 0xfff8

struct psa_key_attributes_s {
    psa_key_type_t MBEDTLS_PRIVATE(type);
    psa_key_bits_t MBEDTLS_PRIVATE(bits);
    psa_key_lifetime_t MBEDTLS_PRIVATE(lifetime);
    psa_key_policy_t MBEDTLS_PRIVATE(policy);
    /* This type has a different layout in the client view wrt the
     * service view of the key id, i.e. in service view usually is
     * expected to have MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER defined
     * thus adding an owner field to the standard psa_key_id_t. For
     * implementations with client/service separation, this means the
     * object will be marshalled through a transport channel and
     * interpreted differently at each side of the transport. Placing
     * it at the end of structures allows to interpret the structure
     * at the client without reorganizing the memory layout of the
     * struct
     */
    mbedtls_svc_key_id_t MBEDTLS_PRIVATE(id);
};

#define PSA_KEY_ATTRIBUTES_INIT { PSA_KEY_TYPE_NONE, 0,            \
                                  PSA_KEY_LIFETIME_VOLATILE,       \
                                  PSA_KEY_POLICY_INIT,             \
                                  MBEDTLS_SVC_KEY_ID_INIT }

static inline struct psa_key_attributes_s psa_key_attributes_init(void)
{
    const struct psa_key_attributes_s v = PSA_KEY_ATTRIBUTES_INIT;
    return v;
}

static inline void psa_set_key_id(psa_key_attributes_t *attributes,
                                  mbedtls_svc_key_id_t key)
{
    psa_key_lifetime_t lifetime = attributes->MBEDTLS_PRIVATE(lifetime);

    attributes->MBEDTLS_PRIVATE(id) = key;

    if (PSA_KEY_LIFETIME_IS_VOLATILE(lifetime)) {
        attributes->MBEDTLS_PRIVATE(lifetime) =
            PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
                PSA_KEY_LIFETIME_PERSISTENT,
                PSA_KEY_LIFETIME_GET_LOCATION(lifetime));
    }
}

static inline mbedtls_svc_key_id_t psa_get_key_id(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(id);
}

#ifdef MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER
static inline void mbedtls_set_key_owner_id(psa_key_attributes_t *attributes,
                                            mbedtls_key_owner_id_t owner)
{
    attributes->MBEDTLS_PRIVATE(id).MBEDTLS_PRIVATE(owner) = owner;
}
#endif

static inline void psa_set_key_lifetime(psa_key_attributes_t *attributes,
                                        psa_key_lifetime_t lifetime)
{
    attributes->MBEDTLS_PRIVATE(lifetime) = lifetime;
    if (PSA_KEY_LIFETIME_IS_VOLATILE(lifetime)) {
#ifdef MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER
        attributes->MBEDTLS_PRIVATE(id).MBEDTLS_PRIVATE(key_id) = 0;
#else
        attributes->MBEDTLS_PRIVATE(id) = 0;
#endif
    }
}

static inline psa_key_lifetime_t psa_get_key_lifetime(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(lifetime);
}

static inline void psa_extend_key_usage_flags(psa_key_usage_t *usage_flags)
{
    if (*usage_flags & PSA_KEY_USAGE_SIGN_HASH) {
        *usage_flags |= PSA_KEY_USAGE_SIGN_MESSAGE;
    }

    if (*usage_flags & PSA_KEY_USAGE_VERIFY_HASH) {
        *usage_flags |= PSA_KEY_USAGE_VERIFY_MESSAGE;
    }
}

static inline void psa_set_key_usage_flags(psa_key_attributes_t *attributes,
                                           psa_key_usage_t usage_flags)
{
    psa_extend_key_usage_flags(&usage_flags);
    attributes->MBEDTLS_PRIVATE(policy).MBEDTLS_PRIVATE(usage) = usage_flags;
}

static inline psa_key_usage_t psa_get_key_usage_flags(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(policy).MBEDTLS_PRIVATE(usage);
}

static inline void psa_set_key_algorithm(psa_key_attributes_t *attributes,
                                         psa_algorithm_t alg)
{
    attributes->MBEDTLS_PRIVATE(policy).MBEDTLS_PRIVATE(alg) = alg;
}

static inline psa_algorithm_t psa_get_key_algorithm(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(policy).MBEDTLS_PRIVATE(alg);
}

static inline void psa_set_key_type(psa_key_attributes_t *attributes,
                                    psa_key_type_t type)
{
    attributes->MBEDTLS_PRIVATE(type) = type;
}

static inline psa_key_type_t psa_get_key_type(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(type);
}

static inline void psa_set_key_bits(psa_key_attributes_t *attributes,
                                    size_t bits)
{
    if (bits > PSA_MAX_KEY_BITS) {
        attributes->MBEDTLS_PRIVATE(bits) = PSA_KEY_BITS_TOO_LARGE;
    } else {
        attributes->MBEDTLS_PRIVATE(bits) = (psa_key_bits_t) bits;
    }
}

static inline size_t psa_get_key_bits(
    const psa_key_attributes_t *attributes)
{
    return attributes->MBEDTLS_PRIVATE(bits);
}

/**
 * \brief The context for PSA interruptible hash signing.
 */
struct psa_sign_hash_interruptible_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active). */
    unsigned int MBEDTLS_PRIVATE(id);

    psa_driver_sign_hash_interruptible_context_t MBEDTLS_PRIVATE(ctx);

    unsigned int MBEDTLS_PRIVATE(error_occurred) : 1;

    uint32_t MBEDTLS_PRIVATE(num_ops);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#else
#define PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0, { 0 }, 0, 0 }
#endif

static inline struct psa_sign_hash_interruptible_operation_s
psa_sign_hash_interruptible_operation_init(void)
{
    const struct psa_sign_hash_interruptible_operation_s v =
        PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT;

    return v;
}

/**
 * \brief The context for PSA interruptible hash verification.
 */
struct psa_verify_hash_interruptible_operation_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /** Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active). */
    unsigned int MBEDTLS_PRIVATE(id);

    psa_driver_verify_hash_interruptible_context_t MBEDTLS_PRIVATE(ctx);

    unsigned int MBEDTLS_PRIVATE(error_occurred) : 1;

    uint32_t MBEDTLS_PRIVATE(num_ops);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_VERIFY_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#else
#define PSA_VERIFY_HASH_INTERRUPTIBLE_OPERATION_INIT { 0, { 0 }, 0, 0 }
#endif

static inline struct psa_verify_hash_interruptible_operation_s
psa_verify_hash_interruptible_operation_init(void)
{
    const struct psa_verify_hash_interruptible_operation_s v =
        PSA_VERIFY_HASH_INTERRUPTIBLE_OPERATION_INIT;

    return v;
}

/**
 * \brief The context for PSA interruptible key agreement.
 */
struct psa_key_agreement_iop_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /**
     *  Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active).
     */
    unsigned int MBEDTLS_PRIVATE(id);
    mbedtls_psa_key_agreement_interruptible_operation_t MBEDTLS_PRIVATE(mbedtls_ctx);
    uint32_t MBEDTLS_PRIVATE(num_ops);
    psa_key_attributes_t MBEDTLS_PRIVATE(attributes);
    unsigned int MBEDTLS_PRIVATE(error_occurred) : 1;
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_KEY_AGREEMENT_IOP_INIT { 0 }
#else
#define PSA_KEY_AGREEMENT_IOP_INIT { 0, MBEDTLS_PSA_KEY_AGREEMENT_IOP_INIT, 0, \
                                     PSA_KEY_ATTRIBUTES_INIT, 0 }
#endif

static inline struct psa_key_agreement_iop_s
psa_key_agreement_iop_init(void)
{
    const struct psa_key_agreement_iop_s v = PSA_KEY_AGREEMENT_IOP_INIT;

    return v;
}

/**
 * \brief The context for PSA interruptible key generation.
 */
struct psa_generate_key_iop_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /**
     *  Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active).
     */
    unsigned int MBEDTLS_PRIVATE(id);
    mbedtls_psa_generate_key_iop_t MBEDTLS_PRIVATE(ctx);
    psa_key_attributes_t MBEDTLS_PRIVATE(attributes);
    unsigned int MBEDTLS_PRIVATE(error_occurred) : 1;
    uint32_t MBEDTLS_PRIVATE(num_ops);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_GENERATE_KEY_IOP_INIT { 0 }
#else
#define PSA_GENERATE_KEY_IOP_INIT { 0, MBEDTLS_PSA_GENERATE_KEY_IOP_INIT, PSA_KEY_ATTRIBUTES_INIT, \
                                    0, 0 }
#endif

static inline struct psa_generate_key_iop_s
psa_generate_key_iop_init(void)
{
    const struct psa_generate_key_iop_s v = PSA_GENERATE_KEY_IOP_INIT;

    return v;
}

/**
 * \brief The context for PSA interruptible export public-key.
 */
struct psa_export_public_key_iop_s {
#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_psa_client_handle_t handle;
#else
    /**
     *  Unique ID indicating which driver got assigned to do the
     * operation. Since driver contexts are driver-specific, swapping
     * drivers halfway through the operation is not supported.
     * ID values are auto-generated in psa_crypto_driver_wrappers.h
     * ID value zero means the context is not valid or not assigned to
     * any driver (i.e. none of the driver contexts are active).
     */
    unsigned int MBEDTLS_PRIVATE(id);
    mbedtls_psa_export_public_key_iop_t MBEDTLS_PRIVATE(ctx);
    unsigned int MBEDTLS_PRIVATE(error_occurred) : 1;
    uint32_t MBEDTLS_PRIVATE(num_ops);
#endif
};

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
#define PSA_EXPORT_PUBLIC_KEY_IOP_INIT { 0 }
#else
#define PSA_EXPORT_PUBLIC_KEY_IOP_INIT { 0, MBEDTLS_PSA_EXPORT_PUBLIC_KEY_IOP_INIT, 0, 0 }
#endif

static inline struct psa_export_public_key_iop_s
psa_export_public_key_iop_init(void)
{
    const struct psa_export_public_key_iop_s v = PSA_EXPORT_PUBLIC_KEY_IOP_INIT;

    return v;
}

#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_STRUCT_H */
