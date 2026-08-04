/*
 *  PSA crypto support for secure element drivers
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_SE_H
#define PSA_CRYPTO_SE_H

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

/** The maximum location value that this implementation supports
 * for a secure element.
 *
 * This is not a characteristic that each PSA implementation has, but a
 * limitation of the current implementation due to the constraints imposed
 * by storage. See #PSA_CRYPTO_SE_DRIVER_ITS_UID_BASE.
 *
 * The minimum location value for a secure element is 1, like on any
 * PSA implementation (0 means a transparent key).
 */
#define PSA_MAX_SE_LOCATION 255

/** The base of the range of ITS file identifiers for secure element
 * driver persistent data.
 *
 * We use a slice of the implementation reserved range 0xffff0000..0xffffffff,
 * specifically the range 0xfffffe00..0xfffffeff. The length of this range
 * drives the value of #PSA_MAX_SE_LOCATION. The identifier 0xfffffe00 is
 * actually not used since it corresponds to #PSA_KEY_LOCATION_LOCAL_STORAGE
 * which doesn't have a driver.
 */
#define PSA_CRYPTO_SE_DRIVER_ITS_UID_BASE ((psa_key_id_t) 0xfffffe00)

/** The maximum number of registered secure element driver locations. */
#define PSA_MAX_SE_DRIVERS 4

/** Unregister all secure element drivers.
 *
 * \warning Do not call this function while the library is in the initialized
 *          state. This function is only intended to be called at the end
 *          of mbedtls_psa_crypto_free().
 */
void psa_unregister_all_se_drivers(void);

/** Initialize all secure element drivers.
 *
 * Called from psa_crypto_init().
 */
psa_status_t psa_init_all_se_drivers(void);

/** A structure that describes a registered secure element driver.
 *
 * A secure element driver table entry contains a pointer to the
 * driver's method table as well as the driver context structure.
 */
typedef struct psa_se_drv_table_entry_s psa_se_drv_table_entry_t;

/** Return the secure element driver information for a lifetime value.
 *
 * \param lifetime              The lifetime value to query.
 * \param[out] p_methods        On output, if there is a driver,
 *                              \c *methods points to its method table.
 *                              Otherwise \c *methods is \c NULL.
 * \param[out] p_drv_context    On output, if there is a driver,
 *                              \c *drv_context points to its context
 *                              structure.
 *                              Otherwise \c *drv_context is \c NULL.
 *
 * \retval 1
 *         \p lifetime corresponds to a registered driver.
 * \retval 0
 *         \p lifetime does not correspond to a registered driver.
 */
int psa_get_se_driver(psa_key_lifetime_t lifetime,
                      const psa_drv_se_t **p_methods,
                      psa_drv_se_context_t **p_drv_context);

/** Return the secure element driver table entry for a lifetime value.
 *
 * \param lifetime      The lifetime value to query.
 *
 * \return The driver table entry for \p lifetime, or
 *         \p NULL if \p lifetime does not correspond to a registered driver.
 */
psa_se_drv_table_entry_t *psa_get_se_driver_entry(
    psa_key_lifetime_t lifetime);

/** Return the method table for a secure element driver.
 *
 * \param[in] driver    The driver table entry to access, or \c NULL.
 *
 * \return The driver's method table.
 *         \c NULL if \p driver is \c NULL.
 */
const psa_drv_se_t *psa_get_se_driver_methods(
    const psa_se_drv_table_entry_t *driver);

/** Return the context of a secure element driver.
 *
 * \param[in] driver    The driver table entry to access, or \c NULL.
 *
 * \return A pointer to the driver context.
 *         \c NULL if \p driver is \c NULL.
 */
psa_drv_se_context_t *psa_get_se_driver_context(
    psa_se_drv_table_entry_t *driver);

/** Find a free slot for a key that is to be created.
 *
 * This function calls the relevant method in the driver to find a suitable
 * slot for a key with the given attributes.
 *
 * \param[in] attributes    Metadata about the key that is about to be created.
 * \param[in] driver        The driver table entry to query.
 * \param[out] slot_number  On success, a slot number that is free in this
 *                          secure element.
 */
psa_status_t psa_find_se_slot_for_key(
    const psa_key_attributes_t *attributes,
    psa_key_creation_method_t method,
    psa_se_drv_table_entry_t *driver,
    psa_key_slot_number_t *slot_number);

/** Destroy a key in a secure element.
 *
 * This function calls the relevant driver method to destroy a key
 * and updates the driver's persistent data.
 */
psa_status_t psa_destroy_se_key(psa_se_drv_table_entry_t *driver,
                                psa_key_slot_number_t slot_number);

/** Load the persistent data of a secure element driver.
 *
 * \param driver        The driver table entry containing the persistent
 *                      data to load from storage.
 *
 * \return #PSA_SUCCESS
 * \return #PSA_ERROR_NOT_SUPPORTED
 * \return #PSA_ERROR_DOES_NOT_EXIST
 * \return #PSA_ERROR_STORAGE_FAILURE
 * \return #PSA_ERROR_DATA_CORRUPT
 * \return #PSA_ERROR_INVALID_ARGUMENT
 */
psa_status_t psa_load_se_persistent_data(
    const psa_se_drv_table_entry_t *driver);

/** Save the persistent data of a secure element driver.
 *
 * \param[in] driver    The driver table entry containing the persistent
 *                      data to save to storage.
 *
 * \return #PSA_SUCCESS
 * \return #PSA_ERROR_NOT_SUPPORTED
 * \return #PSA_ERROR_NOT_PERMITTED
 * \return #PSA_ERROR_NOT_SUPPORTED
 * \return #PSA_ERROR_INSUFFICIENT_STORAGE
 * \return #PSA_ERROR_STORAGE_FAILURE
 * \return #PSA_ERROR_INVALID_ARGUMENT
 */
psa_status_t psa_save_se_persistent_data(
    const psa_se_drv_table_entry_t *driver);

/** Destroy the persistent data of a secure element driver.
 *
 * This is currently only used for testing.
 *
 * \param[in] location  The location identifier for the driver whose
 *                      persistent data is to be erased.
 */
psa_status_t psa_destroy_se_persistent_data(psa_key_location_t location);


/** The storage representation of a key whose data is in a secure element.
 */
typedef struct {
    uint8_t slot_number[sizeof(psa_key_slot_number_t)];
} psa_se_key_data_storage_t;

#endif /* PSA_CRYPTO_SE_H */
