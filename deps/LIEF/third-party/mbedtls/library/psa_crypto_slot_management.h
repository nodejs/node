/*
 *  PSA crypto layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_SLOT_MANAGEMENT_H
#define PSA_CRYPTO_SLOT_MANAGEMENT_H

#include "psa/crypto.h"
#include "psa_crypto_core.h"
#include "psa_crypto_se.h"

/** Range of volatile key identifiers.
 *
 *  The first #MBEDTLS_PSA_KEY_SLOT_COUNT identifiers of the implementation
 *  range of key identifiers are reserved for volatile key identifiers.
 *
 *  If \c id is a a volatile key identifier, #PSA_KEY_ID_VOLATILE_MIN - \c id
 *  indicates the key slot containing the volatile key definition. See
 *  psa_crypto_slot_management.c for details.
 */

/** The minimum value for a volatile key identifier.
 */
#define PSA_KEY_ID_VOLATILE_MIN  PSA_KEY_ID_VENDOR_MIN

/** The maximum value for a volatile key identifier.
 */
#if defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC)
#define PSA_KEY_ID_VOLATILE_MAX (MBEDTLS_PSA_KEY_ID_BUILTIN_MIN - 1)
#else /* MBEDTLS_PSA_KEY_STORE_DYNAMIC */
#define PSA_KEY_ID_VOLATILE_MAX                                 \
    (PSA_KEY_ID_VOLATILE_MIN + MBEDTLS_PSA_KEY_SLOT_COUNT - 1)
#endif /* MBEDTLS_PSA_KEY_STORE_DYNAMIC */

/** Test whether a key identifier is a volatile key identifier.
 *
 * \param key_id  Key identifier to test.
 *
 * \retval 1
 *         The key identifier is a volatile key identifier.
 * \retval 0
 *         The key identifier is not a volatile key identifier.
 */
static inline int psa_key_id_is_volatile(psa_key_id_t key_id)
{
    return (key_id >= PSA_KEY_ID_VOLATILE_MIN) &&
           (key_id <= PSA_KEY_ID_VOLATILE_MAX);
}

/** Get the description of a key given its identifier and lock it.
 *
 * The descriptions of volatile keys and loaded persistent keys are stored in
 * key slots. This function returns a pointer to the key slot containing the
 * description of a key given its identifier.
 *
 * In case of a persistent key, the function loads the description of the key
 * into a key slot if not already done.
 *
 * On success, the returned key slot has been registered for reading.
 * It is the responsibility of the caller to call psa_unregister_read(slot)
 * when they have finished reading the contents of the slot.
 *
 * On failure, `*p_slot` is set to NULL. This ensures that it is always valid
 * to call psa_unregister_read on the returned slot.
 *
 * \param key           Key identifier to query.
 * \param[out] p_slot   On success, `*p_slot` contains a pointer to the
 *                      key slot containing the description of the key
 *                      identified by \p key.
 *
 * \retval #PSA_SUCCESS
 *         \p *p_slot contains a pointer to the key slot containing the
 *         description of the key identified by \p key.
 *         The key slot counter has been incremented.
 * \retval #PSA_ERROR_BAD_STATE
 *         The library has not been initialized.
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         \p key is not a valid key identifier.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         \p key is a persistent key identifier. The implementation does not
 *         have sufficient resources to load the persistent key. This can be
 *         due to a lack of empty key slot, or available memory.
 * \retval #PSA_ERROR_DOES_NOT_EXIST
 *         There is no key with key identifier \p key.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 */
psa_status_t psa_get_and_lock_key_slot(mbedtls_svc_key_id_t key,
                                       psa_key_slot_t **p_slot);

/** Initialize the key slot structures.
 *
 * \retval #PSA_SUCCESS
 *         Currently this function always succeeds.
 */
psa_status_t psa_initialize_key_slots(void);

#if defined(MBEDTLS_TEST_HOOKS) && defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC)
/* Allow test code to customize the key slice length. We use this in tests
 * that exhaust the key store to reach a full key store in reasonable time
 * and memory.
 *
 * The length of each slice must be between 1 and
 * (1 << KEY_ID_SLOT_INDEX_WIDTH) inclusive.
 *
 * The length for a given slice index must not change while
 * the key store is initialized.
 */
extern size_t (*mbedtls_test_hook_psa_volatile_key_slice_length)(
    size_t slice_idx);

/* The number of volatile key slices. */
size_t psa_key_slot_volatile_slice_count(void);
#endif

/** Delete all data from key slots in memory.
 * This function is not thread safe, it wipes every key slot regardless of
 * state and reader count. It should only be called when no slot is in use.
 *
 * This does not affect persistent storage. */
void psa_wipe_all_key_slots(void);

/** Find a free key slot and reserve it to be filled with a key.
 *
 * This function finds a key slot that is free,
 * sets its state to PSA_SLOT_FILLING and then returns the slot.
 *
 * On success, the key slot's state is PSA_SLOT_FILLING.
 * It is the responsibility of the caller to change the slot's state to
 * PSA_SLOT_EMPTY/FULL once key creation has finished.
 *
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param[out] volatile_key_id   - If null, reserve a cache slot for
 *                                 a persistent or built-in key.
 *                               - If non-null, allocate a slot for
 *                                 a volatile key. On success,
 *                                 \p *volatile_key_id is the
 *                                 identifier corresponding to the
 *                                 returned slot. It is the caller's
 *                                 responsibility to set this key identifier
 *                                 in the attributes.
 * \param[out] p_slot            On success, a pointer to the slot.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         There were no free key slots.
 *         When #MBEDTLS_PSA_KEY_STORE_DYNAMIC is enabled, there was not
 *         enough memory to allocate more slots.
 * \retval #PSA_ERROR_BAD_STATE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *         This function attempted to operate on a key slot which was in an
 *         unexpected state.
 */
psa_status_t psa_reserve_free_key_slot(psa_key_id_t *volatile_key_id,
                                       psa_key_slot_t **p_slot);

#if defined(MBEDTLS_PSA_KEY_STORE_DYNAMIC)
/** Return a key slot to the free list.
 *
 * Call this function when a slot obtained from psa_reserve_free_key_slot()
 * is no longer in use.
 *
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param slice_idx             The slice containing the slot.
 *                              This is `slot->slice_index` when the slot
 *                              is obtained from psa_reserve_free_key_slot().
 * \param slot                  The key slot.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *         This function attempted to operate on a key slot which was in an
 *         unexpected state.
 */
psa_status_t psa_free_key_slot(size_t slice_idx,
                               psa_key_slot_t *slot);
#endif /* MBEDTLS_PSA_KEY_STORE_DYNAMIC */

/** Change the state of a key slot.
 *
 * This function changes the state of the key slot from expected_state to
 * new state. If the state of the slot was not expected_state, the state is
 * unchanged.
 *
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param[in] slot            The key slot.
 * \param[in] expected_state  The current state of the slot.
 * \param[in] new_state       The new state of the slot.
 *
 * \retval #PSA_SUCCESS
               The key slot's state variable is new_state.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *             The slot's state was not expected_state.
 */
static inline psa_status_t psa_key_slot_state_transition(
    psa_key_slot_t *slot, psa_key_slot_state_t expected_state,
    psa_key_slot_state_t new_state)
{
    if (slot->state != expected_state) {
        return PSA_ERROR_CORRUPTION_DETECTED;
    }
    slot->state = new_state;
    return PSA_SUCCESS;
}

/** Register as a reader of a key slot.
 *
 * This function increments the key slot registered reader counter by one.
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \param[in] slot  The key slot.
 *
 * \retval #PSA_SUCCESS
               The key slot registered reader counter was incremented.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *             The reader counter already reached its maximum value and was not
 *             increased, or the slot's state was not PSA_SLOT_FULL.
 */
static inline psa_status_t psa_register_read(psa_key_slot_t *slot)
{
    if ((slot->state != PSA_SLOT_FULL) ||
        (slot->var.occupied.registered_readers >= SIZE_MAX)) {
        return PSA_ERROR_CORRUPTION_DETECTED;
    }
    slot->var.occupied.registered_readers++;

    return PSA_SUCCESS;
}

/** Unregister from reading a key slot.
 *
 * This function decrements the key slot registered reader counter by one.
 * If the state of the slot is PSA_SLOT_PENDING_DELETION,
 * and there is only one registered reader (the caller),
 * this function will call psa_wipe_key_slot().
 * If multi-threading is enabled, the caller must hold the
 * global key slot mutex.
 *
 * \note To ease the handling of errors in retrieving a key slot
 *       a NULL input pointer is valid, and the function returns
 *       successfully without doing anything in that case.
 *
 * \param[in] slot  The key slot.
 * \retval #PSA_SUCCESS
 *             \p slot is NULL or the key slot reader counter has been
 *             decremented (and potentially wiped) successfully.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *             The slot's state was neither PSA_SLOT_FULL nor
 *             PSA_SLOT_PENDING_DELETION.
 *             Or a wipe was attempted and the slot's state was not
 *             PSA_SLOT_PENDING_DELETION.
 *             Or registered_readers was equal to 0.
 */
psa_status_t psa_unregister_read(psa_key_slot_t *slot);

/** Wrap a call to psa_unregister_read in the global key slot mutex.
 *
 * If threading is disabled, this simply calls psa_unregister_read.
 *
 * \note To ease the handling of errors in retrieving a key slot
 *       a NULL input pointer is valid, and the function returns
 *       successfully without doing anything in that case.
 *
 * \param[in] slot  The key slot.
 * \retval #PSA_SUCCESS
 *             \p slot is NULL or the key slot reader counter has been
 *             decremented (and potentially wiped) successfully.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED
 *             The slot's state was neither PSA_SLOT_FULL nor
 *             PSA_SLOT_PENDING_DELETION.
 *             Or a wipe was attempted and the slot's state was not
 *             PSA_SLOT_PENDING_DELETION.
 *             Or registered_readers was equal to 0.
 */
psa_status_t psa_unregister_read_under_mutex(psa_key_slot_t *slot);

/** Test whether a lifetime designates a key in an external cryptoprocessor.
 *
 * \param lifetime      The lifetime to test.
 *
 * \retval 1
 *         The lifetime designates an external key. There should be a
 *         registered driver for this lifetime, otherwise the key cannot
 *         be created or manipulated.
 * \retval 0
 *         The lifetime designates a key that is volatile or in internal
 *         storage.
 */
static inline int psa_key_lifetime_is_external(psa_key_lifetime_t lifetime)
{
    return PSA_KEY_LIFETIME_GET_LOCATION(lifetime)
           != PSA_KEY_LOCATION_LOCAL_STORAGE;
}

/** Validate a key's location.
 *
 * This function checks whether the key's attributes point to a location that
 * is known to the PSA Core, and returns the driver function table if the key
 * is to be found in an external location.
 *
 * \param[in] lifetime      The key lifetime attribute.
 * \param[out] p_drv        On success, when a key is located in external
 *                          storage, returns a pointer to the driver table
 *                          associated with the key's storage location.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 */
psa_status_t psa_validate_key_location(psa_key_lifetime_t lifetime,
                                       psa_se_drv_table_entry_t **p_drv);

/** Validate the persistence of a key.
 *
 * \param[in] lifetime  The key lifetime attribute.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED The key is persistent but persistent keys
 *             are not supported.
 */
psa_status_t psa_validate_key_persistence(psa_key_lifetime_t lifetime);

/** Validate a key identifier.
 *
 * \param[in] key           The key identifier.
 * \param[in] vendor_ok     Non-zero to indicate that key identifiers in the
 *                          vendor range are allowed, volatile key identifiers
 *                          excepted \c 0 otherwise.
 *
 * \retval <> 0 if the key identifier is valid, 0 otherwise.
 */
int psa_is_valid_key_id(mbedtls_svc_key_id_t key, int vendor_ok);

#endif /* PSA_CRYPTO_SLOT_MANAGEMENT_H */
