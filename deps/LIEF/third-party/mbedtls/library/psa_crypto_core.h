/*
 *  PSA crypto core internal interfaces
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_CORE_H
#define PSA_CRYPTO_CORE_H

/*
 * Include the build-time configuration information header. Here, we do not
 * include `"mbedtls/build_info.h"` directly but `"psa/build_info.h"`, which
 * is basically just an alias to it. This is to ease the maintenance of the
 * TF-PSA-Crypto repository which has a different build system and
 * configuration.
 */
#include "psa/build_info.h"

#include "psa/crypto.h"
#include "psa/crypto_se_driver.h"
#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

/**
 * Tell if PSA is ready for this cipher.
 *
 * \note            For now, only checks the state of the driver subsystem,
 *                  not the algorithm. Might do more in the future.
 *
 * \param cipher_alg  The cipher algorithm (ignored for now).
 *
 * \return 1 if the driver subsytem is ready, 0 otherwise.
 */
int psa_can_do_cipher(psa_key_type_t key_type, psa_algorithm_t cipher_alg);

typedef enum {
    PSA_SLOT_EMPTY = 0,
    PSA_SLOT_FILLING,
    PSA_SLOT_FULL,
    PSA_SLOT_PENDING_DELETION,
} psa_key_slot_state_t;

/** The data structure representing a key slot, containing key material
 * and metadata for one key.
 */
typedef struct {
    /* This field is accessed in a lot of places. Putting it first
     * reduces the code size. */
    psa_key_attributes_t attr;

    /*
     * The current state of the key slot, as described in
     * docs/architecture/psa-thread-safety/psa-thread-safety.md.
     *
     * Library functions can modify the state of a key slot by calling
     * psa_key_slot_state_transition.
     *
     * The state variable is used to help determine whether library functions
     * which operate on the slot succeed. For example, psa_finish_key_creation,
     * which transfers the state of a slot from PSA_SLOT_FILLING to
     * PSA_SLOT_FULL, must fail with error code PSA_ERROR_CORRUPTION_DETECTED
     * if the state of the slot is not PSA_SLOT_FILLING.
     *
     * Library functions which traverse the array of key slots only consider
     * slots that are in a suitable state for the function.
     * For example, psa_get_and_lock_key_slot_in_memory, which finds a slot
     * containing a given key ID, will only check slots whose state variable is
     * PSA_SLOT_FULL.
     */
    psa_key_slot_state_t state;

#if defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC)
    /* The index of the slice containing this slot.
     * This field must be filled if the slot contains a key
     * (including keys being created or destroyed), and can be either
     * filled or 0 when the slot is free.
     *
     * In most cases, the slice index can be deduced from the key identifer.
     * We keep it in a separate field for robustness (it reduces the chance
     * that a coding mistake in the key store will result in accessing the
     * wrong slice), and also so that it's available even on code paths
     * during creation or destruction where the key identifier might not be
     * filled in.
     * */
    uint8_t slice_index;
#endif /* MBEDTLS_PSA_KEY_STORE_DYNAMIC */

    union {
        struct {
            /* The index of the next slot in the free list for this
             * slice, relative * to the next array element.
             *
             * That is, 0 means the next slot, 1 means the next slot
             * but one, etc. -1 would mean the slot itself. -2 means
             * the previous slot, etc.
             *
             * If this is beyond the array length, the free list ends with the
             * current element.
             *
             * The reason for this strange encoding is that 0 means the next
             * element. This way, when we allocate a slice and initialize it
             * to all-zero, the slice is ready for use, with a free list that
             * consists of all the slots in order.
             */
            int32_t next_free_relative_to_next;
        } free;

        struct {
            /*
             * Number of functions registered as reading the material in the key slot.
             *
             * Library functions must not write directly to registered_readers
             *
             * A function must call psa_register_read(slot) before reading
             * the current contents of the slot for an operation.
             * They then must call psa_unregister_read(slot) once they have
             * finished reading the current contents of the slot. If the key
             * slot mutex is not held (when mutexes are enabled), this call
             * must be done via a call to
             * psa_unregister_read_under_mutex(slot).
             * A function must call psa_key_slot_has_readers(slot) to check if
             * the slot is in use for reading.
             *
             * This counter is used to prevent resetting the key slot while
             * the library may access it. For example, such control is needed
             * in the following scenarios:
             * . In case of key slot starvation, all key slots contain the
             *   description of a key, and the library asks for the
             *   description of a persistent key not present in the
             *   key slots, the key slots currently accessed by the
             *   library cannot be reclaimed to free a key slot to load
             *   the persistent key.
             * . In case of a multi-threaded application where one thread
             *   asks to close or purge or destroy a key while it is in use
             *   by the library through another thread. */
            size_t registered_readers;
        } occupied;
    } var;

    /* Dynamically allocated key data buffer.
     * Format as specified in psa_export_key(). */
    struct key_data {
#if defined(MBEDTLS_PSA_STATIC_KEY_SLOTS)
        uint8_t data[MBEDTLS_PSA_STATIC_KEY_SLOT_BUFFER_SIZE];
#else
        uint8_t *data;
#endif
        size_t bytes;
    } key;
} psa_key_slot_t;

#if defined(MBEDTLS_THREADING_C)

/** Perform a mutex operation and return immediately upon failure.
 *
 * Returns PSA_ERROR_SERVICE_FAILURE if the operation fails
 * and status was PSA_SUCCESS.
 *
 * Assumptions:
 *  psa_status_t status exists.
 *  f is a mutex operation which returns 0 upon success.
 */
#define PSA_THREADING_CHK_RET(f)                       \
    do                                                 \
    {                                                  \
        if ((f) != 0) {                                \
            if (status == PSA_SUCCESS) {               \
                return PSA_ERROR_SERVICE_FAILURE;      \
            }                                          \
            return status;                             \
        }                                              \
    } while (0);

/** Perform a mutex operation and goto exit on failure.
 *
 * Sets status to PSA_ERROR_SERVICE_FAILURE if status was PSA_SUCCESS.
 *
 * Assumptions:
 *  psa_status_t status exists.
 *  Label exit: exists.
 *  f is a mutex operation which returns 0 upon success.
 */
#define PSA_THREADING_CHK_GOTO_EXIT(f)                 \
    do                                                 \
    {                                                  \
        if ((f) != 0) {                                \
            if (status == PSA_SUCCESS) {               \
                status = PSA_ERROR_SERVICE_FAILURE;    \
            }                                          \
            goto exit;                                 \
        }                                              \
    } while (0);
#endif

/** Test whether a key slot has any registered readers.
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param[in] slot      The key slot to test.
 *
 * \return 1 if the slot has any registered readers, 0 otherwise.
 */
static inline int psa_key_slot_has_readers(const psa_key_slot_t *slot)
{
    return slot->var.occupied.registered_readers > 0;
}

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
/** Get the SE slot number of a key from the key slot storing its description.
 *
 * \param[in]  slot  The key slot to query. This must be a key slot storing
 *                   the description of a key of a dynamically registered
 *                   secure element, otherwise the behaviour is undefined.
 */
static inline psa_key_slot_number_t psa_key_slot_get_slot_number(
    const psa_key_slot_t *slot)
{
    return *((psa_key_slot_number_t *) (slot->key.data));
}
#endif

/** Completely wipe a slot in memory, including its policy.
 *
 * Persistent storage is not affected.
 * Sets the slot's state to PSA_SLOT_EMPTY.
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param[in,out] slot  The key slot to wipe.
 *
 * \retval #PSA_SUCCESS
 *         The slot has been successfully wiped.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *         The slot's state was PSA_SLOT_FULL or PSA_SLOT_PENDING_DELETION, and
 *         the amount of registered readers was not equal to 1. Or,
 *         the slot's state was PSA_SLOT_EMPTY. Or,
 *         the slot's state was PSA_SLOT_FILLING, and the amount
 *         of registered readers was not equal to 0.
 */
psa_status_t psa_wipe_key_slot(psa_key_slot_t *slot);

/** Try to allocate a buffer to an empty key slot.
 *
 * \param[in,out] slot          Key slot to attach buffer to.
 * \param[in] buffer_length     Requested size of the buffer.
 *
 * \retval #PSA_SUCCESS
 *         The buffer has been successfully allocated.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         Not enough memory was available for allocation.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         Trying to allocate a buffer to a non-empty key slot.
 */
psa_status_t psa_allocate_buffer_to_slot(psa_key_slot_t *slot,
                                         size_t buffer_length);

/** Wipe key data from a slot. Preserves metadata such as the policy. */
psa_status_t psa_remove_key_data_from_memory(psa_key_slot_t *slot);

/** Copy key data (in export format) into an empty key slot.
 *
 * This function assumes that the slot does not contain
 * any key material yet. On failure, the slot content is unchanged.
 *
 * \param[in,out] slot          Key slot to copy the key into.
 * \param[in] data              Buffer containing the key material.
 * \param data_length           Size of the key buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key has been copied successfully.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         Not enough memory was available for allocation of the
 *         copy buffer.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         There was other key material already present in the slot.
 */
psa_status_t psa_copy_key_material_into_slot(psa_key_slot_t *slot,
                                             const uint8_t *data,
                                             size_t data_length);

/** Convert an Mbed TLS error code to a PSA error code
 *
 * \note This function is provided solely for the convenience of
 *       Mbed TLS and may be removed at any time without notice.
 *
 * \param ret           An Mbed TLS-thrown error code
 *
 * \return              The corresponding PSA error code
 */
psa_status_t mbedtls_to_psa_error(int ret);

/** Import a key in binary format.
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
 * \param[out] key_buffer       The buffer to contain the key data in output
 *                              format upon successful return.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes. This
 *                              size is greater or equal to \p data_length.
 * \param[out] key_buffer_length  The length of the data written in \p
 *                                key_buffer in bytes.
 * \param[out] bits             The key size in number of bits.
 *
 * \retval #PSA_SUCCESS  The key was imported successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key data is not correctly formatted.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t psa_import_key_into_slot(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits);

/** Export a key in binary format
 *
 * \note The signature of this function is that of a PSA driver export_key
 *       entry point. This function behaves as an export_key entry point as
 *       defined in the PSA driver interface specification.
 *
 * \param[in]  attributes       The attributes for the key to export.
 * \param[in]  key_buffer       Material or context of the key to export.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[out] data             Buffer where the key data is to be written.
 * \param[in]  data_size        Size of the \p data buffer in bytes.
 * \param[out] data_length      On success, the number of bytes written in
 *                              \p data
 *
 * \retval #PSA_SUCCESS  The key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_export_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/** Export a public key or the public part of a key pair in binary format.
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
 * \retval #PSA_SUCCESS  The public key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_export_public_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/** Whether a key custom production parameters structure is the default.
 *
 * Calls to a key generation driver with non-default custom production parameters
 * require a driver supporting custom production parameters.
 *
 * \param[in] custom            The key custom production parameters to check.
 * \param custom_data_length    Size of the associated variable-length data
 *                              in bytes.
 */
int psa_custom_key_parameters_are_default(
    const psa_custom_key_parameters_t *custom,
    size_t custom_data_length);

/**
 * \brief Generate a key.
 *
 * \note The signature of the function is that of a PSA driver generate_key
 *       entry point.
 *
 * \param[in]  attributes         The attributes for the key to generate.
 * \param[in] custom              Custom parameters for the key generation.
 * \param[in] custom_data         Variable-length data associated with \c custom.
 * \param custom_data_length      Length of `custom_data` in bytes.
 * \param[out] key_buffer         Buffer where the key data is to be written.
 * \param[in]  key_buffer_size    Size of \p key_buffer in bytes.
 * \param[out] key_buffer_length  On success, the number of bytes written in
 *                                \p key_buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key was generated successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         Key size in bits or type not supported.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 */
psa_status_t psa_generate_key_internal(const psa_key_attributes_t *attributes,
                                       const psa_custom_key_parameters_t *custom,
                                       const uint8_t *custom_data,
                                       size_t custom_data_length,
                                       uint8_t *key_buffer,
                                       size_t key_buffer_size,
                                       size_t *key_buffer_length);

/** Sign a message with a private key. For hash-and-sign algorithms,
 *  this includes the hashing step.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_message entry point. This function behaves as a sign_message
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \note This function will call the driver for psa_sign_hash
 *       and go through driver dispatch again.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  input            The input message to sign.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param[in]  signature_size   Size of the \p signature buffer in bytes.
 * \param[out] signature_length On success, the number of bytes
 *                              that make up the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of the key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t psa_sign_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *input, size_t input_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/** Verify the signature of a message with a public key, using
 *  a hash-and-sign verification algorithm.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_message entry point. This function behaves as a verify_message
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \note This function will call the driver for psa_verify_hash
 *       and go through driver dispatch again.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  input            The message whose signature is to be verified.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
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
psa_status_t psa_verify_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *input, size_t input_length,
    const uint8_t *signature, size_t signature_length);

/** Sign an already-calculated hash with a private key.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash entry point. This function behaves as a sign_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
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
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of the key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t psa_sign_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/**
 * \brief Verify the signature a hash or short message using a public key.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash entry point. This function behaves as a verify_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
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
psa_status_t psa_verify_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);

/**
 * \brief Validate the key bit size for unstructured keys.
 *
 * \note  Check that the bit size is acceptable for a given key type for
 *        unstructured keys.
 *
 * \param[in]  type  The key type
 * \param[in]  bits  The number of bits of the key
 *
 * \retval #PSA_SUCCESS
 *         The key type and size are valid.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size in bits of the key is not valid.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The type and/or the size in bits of the key or the combination of
 *         the two is not supported.
 */
psa_status_t psa_validate_unstructured_key_bit_size(psa_key_type_t type,
                                                    size_t bits);

/** Perform a key agreement and return the raw shared secret, using
    built-in raw key agreement functions.
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
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_BAD_STATE \emptydescription
 */
psa_status_t psa_key_agreement_raw_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *peer_key,
    size_t peer_key_length,
    uint8_t *shared_secret,
    size_t shared_secret_size,
    size_t *shared_secret_length);

/**
 * \brief Set the maximum number of ops allowed to be executed by an
 *        interruptible function in a single call.
 *
 * \note The signature of this function is that of a PSA driver
 *       interruptible_set_max_ops entry point. This function behaves as an
 *       interruptible_set_max_ops entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param[in]  max_ops          The maximum number of ops to be executed in a
 *                              single call, this can be a number from 0 to
 *                              #PSA_INTERRUPTIBLE_MAX_OPS_UNLIMITED, where 0
 *                              is obviously the least amount of work done per
 *                              call.
 */
void mbedtls_psa_interruptible_set_max_ops(uint32_t max_ops);

/**
 * \brief Get the maximum number of ops allowed to be executed by an
 *        interruptible function in a single call.
 *
 * \note The signature of this function is that of a PSA driver
 *       interruptible_get_max_ops entry point. This function behaves as an
 *       interruptible_get_max_ops entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \return                      Maximum number of ops allowed to be executed
 *                              by an interruptible function in a single call.
 */
uint32_t mbedtls_psa_interruptible_get_max_ops(void);

/**
 * \brief Get the number of ops that a hash signing operation has taken for the
 *        previous call. If no call or work has taken place, this will return
 *        zero.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash_get_num_ops entry point. This function behaves as an
 *       sign_hash_get_num_ops entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param   operation           The \c
 *                              mbedtls_psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \return                      Number of ops that were completed
 *                              in the last call to \c
 *                              mbedtls_psa_sign_hash_complete().
 */
uint32_t mbedtls_psa_sign_hash_get_num_ops(
    const mbedtls_psa_sign_hash_interruptible_operation_t *operation);

/**
 * \brief Get the number of ops that a hash verification operation has taken for
 *        the previous call. If no call or work has taken place, this will
 *        return zero.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash_get_num_ops entry point. This function behaves as an
 *       verify_hash_get_num_ops entry point as defined in the PSA driver
 *       interface specification for transparent drivers.
 *
 * \param   operation           The \c
 *                              mbedtls_psa_verify_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \return                      Number of ops that were completed
 *                              in the last call to \c
 *                              mbedtls_psa_verify_hash_complete().
 */
uint32_t mbedtls_psa_verify_hash_get_num_ops(
    const mbedtls_psa_verify_hash_interruptible_operation_t *operation);

/**
 * \brief  Start signing a hash or short message with a private key, in an
 *         interruptible manner.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash_start entry point. This function behaves as a
 *       sign_hash_start entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in] hash              The hash or message to sign.
 * \param hash_length           Size of the \p hash buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The operation started successfully - call \c psa_sign_hash_complete()
 *         with the same context to complete the operation
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         An unsupported, incorrectly formatted or incorrect type of key was
 *         used.
 * \retval #PSA_ERROR_NOT_SUPPORTED Either no internal interruptible operations
 *         are currently supported, or the key type is currently unsupported.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         There was insufficient memory to load the key representation.
 */
psa_status_t mbedtls_psa_sign_hash_start(
    mbedtls_psa_sign_hash_interruptible_operation_t *operation,
    const psa_key_attributes_t *attributes, const uint8_t *key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg,
    const uint8_t *hash, size_t hash_length);

/**
 * \brief Continue and eventually complete the action of signing a hash or
 *        short message with a private key, in an interruptible manner.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash_complete entry point. This function behaves as a
 *       sign_hash_complete entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \param[out] signature        Buffer where the signature is to be written.
 * \param signature_size        Size of the \p signature buffer in bytes. This
 *                              must be appropriate for the selected
 *                              algorithm and key.
 * \param[out] signature_length On success, the number of bytes that make up
 *                              the returned signature value.
 *
 * \retval #PSA_SUCCESS
 *         Operation completed successfully
 *
 * \retval #PSA_OPERATION_INCOMPLETE
 *         Operation was interrupted due to the setting of \c
 *         psa_interruptible_set_max_ops(), there is still work to be done,
 *         please call this function again with the same operation object.
 *
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of \p key.
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t mbedtls_psa_sign_hash_complete(
    mbedtls_psa_sign_hash_interruptible_operation_t *operation,
    uint8_t *signature, size_t signature_size,
    size_t *signature_length);

/**
 * \brief Abort a sign hash operation.
 *
 * \note The signature of this function is that of a PSA driver sign_hash_abort
 *       entry point. This function behaves as a sign_hash_abort entry point as
 *       defined in the PSA driver interface specification for transparent
 *       drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_sign_hash_interruptible_operation_t
 *                              to abort.
 *
 * \retval #PSA_SUCCESS
 *         The operation was aborted successfully.
 */
psa_status_t mbedtls_psa_sign_hash_abort(
    mbedtls_psa_sign_hash_interruptible_operation_t *operation);

/**
 * \brief  Start reading and verifying a hash or short message, in an
 *         interruptible manner.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash_start entry point. This function behaves as a
 *       verify_hash_start entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_verify_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in] hash              The hash whose signature is to be verified.
 * \param hash_length           Size of the \p hash buffer in bytes.
 * \param[in] signature         Buffer containing the signature to verify.
 * \param signature_length      Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The operation started successfully - call \c psa_sign_hash_complete()
 *         with the same context to complete the operation
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         An unsupported or incorrect type of key was used.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *        Either no internal interruptible operations are currently supported,
 *         or the key type is currently unsupported.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *        There was insufficient memory either to load the key representation,
 *        or to prepare the operation.
 */
psa_status_t mbedtls_psa_verify_hash_start(
    mbedtls_psa_verify_hash_interruptible_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);

/**
 * \brief Continue and eventually complete the action of signing a hash or
 *        short message with a private key, in an interruptible manner.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash_complete entry point. This function behaves as a
 *       sign_hash_complete entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_sign_hash_interruptible_operation_t
 *                              to use. This must be initialized first.
 *
 * \retval #PSA_SUCCESS
 *         Operation completed successfully, and the passed signature is valid.
 *
 * \retval #PSA_OPERATION_INCOMPLETE
 *         Operation was interrupted due to the setting of \c
 *         psa_interruptible_set_max_ops(), there is still work to be done,
 *         please call this function again with the same operation object.
 *
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 *
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t mbedtls_psa_verify_hash_complete(
    mbedtls_psa_verify_hash_interruptible_operation_t *operation);

/**
 * \brief Abort a verify signed hash operation.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash_abort entry point. This function behaves as a
 *       verify_hash_abort entry point as defined in the PSA driver interface
 *       specification for transparent drivers.
 *
 * \param[in]  operation        The \c
 *                              mbedtls_psa_verify_hash_interruptible_operation_t
 *                              to abort.
 *
 * \retval #PSA_SUCCESS
 *         The operation was aborted successfully.
 */
psa_status_t mbedtls_psa_verify_hash_abort(
    mbedtls_psa_verify_hash_interruptible_operation_t *operation);

typedef struct psa_crypto_local_input_s {
    uint8_t *buffer;
    size_t length;
} psa_crypto_local_input_t;

#define PSA_CRYPTO_LOCAL_INPUT_INIT ((psa_crypto_local_input_t) { NULL, 0 })

/** Allocate a local copy of an input buffer and copy the contents into it.
 *
 * \param[in] input             Pointer to input buffer.
 * \param[in] input_len         Length of the input buffer.
 * \param[out] local_input      Pointer to a psa_crypto_local_input_t struct
 *                              containing a local input copy.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_INSUFFICIENT_MEMORY, if a copy of
 *                              the buffer cannot be allocated.
 */
psa_status_t psa_crypto_local_input_alloc(const uint8_t *input, size_t input_len,
                                          psa_crypto_local_input_t *local_input);

/** Free a local copy of an input buffer.
 *
 * \param[in] local_input       Pointer to a psa_crypto_local_input_t struct
 *                              populated by a previous call to
 *                              psa_crypto_local_input_alloc().
 */
void psa_crypto_local_input_free(psa_crypto_local_input_t *local_input);

typedef struct psa_crypto_local_output_s {
    uint8_t *original;
    uint8_t *buffer;
    size_t length;
} psa_crypto_local_output_t;

#define PSA_CRYPTO_LOCAL_OUTPUT_INIT ((psa_crypto_local_output_t) { NULL, NULL, 0 })

/** Allocate a local copy of an output buffer.
 *
 * \note                        This does not copy any data from the original
 *                              output buffer but only allocates a buffer
 *                              whose contents will be copied back to the
 *                              original in a future call to
 *                              psa_crypto_local_output_free().
 *
 * \param[in] output            Pointer to output buffer.
 * \param[in] output_len        Length of the output buffer.
 * \param[out] local_output     Pointer to a psa_crypto_local_output_t struct to
 *                              populate with the local output copy.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_INSUFFICIENT_MEMORY, if a copy of
 *                              the buffer cannot be allocated.
 */
psa_status_t psa_crypto_local_output_alloc(uint8_t *output, size_t output_len,
                                           psa_crypto_local_output_t *local_output);

/** Copy from a local copy of an output buffer back to the original, then
 *  free the local copy.
 *
 * \param[in] local_output      Pointer to a psa_crypto_local_output_t struct
 *                              populated by a previous call to
 *                              psa_crypto_local_output_alloc().
 * \return                      #PSA_SUCCESS, if the local output was
 *                              successfully copied back to the original.
 * \return                      #PSA_ERROR_CORRUPTION_DETECTED, if the output
 *                              could not be copied back to the original.
 */
psa_status_t psa_crypto_local_output_free(psa_crypto_local_output_t *local_output);

#endif /* PSA_CRYPTO_CORE_H */
