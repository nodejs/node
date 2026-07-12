/*
 *  PSA crypto support for secure element drivers
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)

#include <stdint.h>
#include <string.h>

#include "psa/crypto_se_driver.h"

#include "psa_crypto_se.h"

#if defined(MBEDTLS_PSA_ITS_FILE_C)
#include "psa_crypto_its.h"
#else /* Native ITS implementation */
#include "psa/error.h"
#include "psa/internal_trusted_storage.h"
#endif

#include "mbedtls/platform.h"



/****************************************************************/
/* Driver lookup */
/****************************************************************/

/* This structure is identical to psa_drv_se_context_t declared in
 * `crypto_se_driver.h`, except that some parts are writable here
 * (non-const, or pointer to non-const). */
typedef struct {
    void *persistent_data;
    size_t persistent_data_size;
    uintptr_t transient_data;
} psa_drv_se_internal_context_t;

struct psa_se_drv_table_entry_s {
    psa_key_location_t location;
    const psa_drv_se_t *methods;
    union {
        psa_drv_se_internal_context_t internal;
        psa_drv_se_context_t context;
    } u;
};

static psa_se_drv_table_entry_t driver_table[PSA_MAX_SE_DRIVERS];

psa_se_drv_table_entry_t *psa_get_se_driver_entry(
    psa_key_lifetime_t lifetime)
{
    size_t i;
    psa_key_location_t location = PSA_KEY_LIFETIME_GET_LOCATION(lifetime);
    /* In the driver table, location=0 means an entry that isn't used.
     * No driver has a location of 0 because it's a reserved value
     * (which designates transparent keys). Make sure we never return
     * a driver entry for location 0. */
    if (location == 0) {
        return NULL;
    }
    for (i = 0; i < PSA_MAX_SE_DRIVERS; i++) {
        if (driver_table[i].location == location) {
            return &driver_table[i];
        }
    }
    return NULL;
}

const psa_drv_se_t *psa_get_se_driver_methods(
    const psa_se_drv_table_entry_t *driver)
{
    return driver->methods;
}

psa_drv_se_context_t *psa_get_se_driver_context(
    psa_se_drv_table_entry_t *driver)
{
    return &driver->u.context;
}

int psa_get_se_driver(psa_key_lifetime_t lifetime,
                      const psa_drv_se_t **p_methods,
                      psa_drv_se_context_t **p_drv_context)
{
    psa_se_drv_table_entry_t *driver = psa_get_se_driver_entry(lifetime);
    if (p_methods != NULL) {
        *p_methods = (driver ? driver->methods : NULL);
    }
    if (p_drv_context != NULL) {
        *p_drv_context = (driver ? &driver->u.context : NULL);
    }
    return driver != NULL;
}



/****************************************************************/
/* Persistent data management */
/****************************************************************/

static psa_status_t psa_get_se_driver_its_file_uid(
    const psa_se_drv_table_entry_t *driver,
    psa_storage_uid_t *uid)
{
    if (driver->location > PSA_MAX_SE_LOCATION) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* ITS file sizes are limited to 32 bits. */
    if (driver->u.internal.persistent_data_size > UINT32_MAX) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* See the documentation of PSA_CRYPTO_SE_DRIVER_ITS_UID_BASE. */
    *uid = PSA_CRYPTO_SE_DRIVER_ITS_UID_BASE + driver->location;
    return PSA_SUCCESS;
}

psa_status_t psa_load_se_persistent_data(
    const psa_se_drv_table_entry_t *driver)
{
    psa_status_t status;
    psa_storage_uid_t uid;
    size_t length;

    status = psa_get_se_driver_its_file_uid(driver, &uid);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /* Read the amount of persistent data that the driver requests.
     * If the data in storage is larger, it is truncated. If the data
     * in storage is smaller, silently keep what is already at the end
     * of the output buffer. */
    /* psa_get_se_driver_its_file_uid ensures that the size_t
     * persistent_data_size is in range, but compilers don't know that,
     * so cast to reassure them. */
    return psa_its_get(uid, 0,
                       (uint32_t) driver->u.internal.persistent_data_size,
                       driver->u.internal.persistent_data,
                       &length);
}

psa_status_t psa_save_se_persistent_data(
    const psa_se_drv_table_entry_t *driver)
{
    psa_status_t status;
    psa_storage_uid_t uid;

    status = psa_get_se_driver_its_file_uid(driver, &uid);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /* psa_get_se_driver_its_file_uid ensures that the size_t
     * persistent_data_size is in range, but compilers don't know that,
     * so cast to reassure them. */
    return psa_its_set(uid,
                       (uint32_t) driver->u.internal.persistent_data_size,
                       driver->u.internal.persistent_data,
                       0);
}

psa_status_t psa_destroy_se_persistent_data(psa_key_location_t location)
{
    psa_storage_uid_t uid;
    if (location > PSA_MAX_SE_LOCATION) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    uid = PSA_CRYPTO_SE_DRIVER_ITS_UID_BASE + location;
    return psa_its_remove(uid);
}

psa_status_t psa_find_se_slot_for_key(
    const psa_key_attributes_t *attributes,
    psa_key_creation_method_t method,
    psa_se_drv_table_entry_t *driver,
    psa_key_slot_number_t *slot_number)
{
    psa_status_t status;
    psa_key_location_t key_location =
        PSA_KEY_LIFETIME_GET_LOCATION(psa_get_key_lifetime(attributes));

    /* If the location is wrong, it's a bug in the library. */
    if (driver->location != key_location) {
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    /* If the driver doesn't support key creation in any way, give up now. */
    if (driver->methods->key_management == NULL) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    if (psa_get_key_slot_number(attributes, slot_number) == PSA_SUCCESS) {
        /* The application wants to use a specific slot. Allow it if
         * the driver supports it. On a system with isolation,
         * the crypto service must check that the application is
         * permitted to request this slot. */
        psa_drv_se_validate_slot_number_t p_validate_slot_number =
            driver->methods->key_management->p_validate_slot_number;
        if (p_validate_slot_number == NULL) {
            return PSA_ERROR_NOT_SUPPORTED;
        }
        status = p_validate_slot_number(&driver->u.context,
                                        driver->u.internal.persistent_data,
                                        attributes, method,
                                        *slot_number);
    } else if (method == PSA_KEY_CREATION_REGISTER) {
        /* The application didn't specify a slot number. This doesn't
         * make sense when registering a slot. */
        return PSA_ERROR_INVALID_ARGUMENT;
    } else {
        /* The application didn't tell us which slot to use. Let the driver
         * choose. This is the normal case. */
        psa_drv_se_allocate_key_t p_allocate =
            driver->methods->key_management->p_allocate;
        if (p_allocate == NULL) {
            return PSA_ERROR_NOT_SUPPORTED;
        }
        status = p_allocate(&driver->u.context,
                            driver->u.internal.persistent_data,
                            attributes, method,
                            slot_number);
    }
    return status;
}

psa_status_t psa_destroy_se_key(psa_se_drv_table_entry_t *driver,
                                psa_key_slot_number_t slot_number)
{
    psa_status_t status;
    psa_status_t storage_status;
    /* Normally a missing method would mean that the action is not
     * supported. But psa_destroy_key() is not supposed to return
     * PSA_ERROR_NOT_SUPPORTED: if you can create a key, you should
     * be able to destroy it. The only use case for a driver that
     * does not have a way to destroy keys at all is if the keys are
     * locked in a read-only state: we can use the keys but not
     * destroy them. Hence, if the driver doesn't support destroying
     * keys, it's really a lack of permission. */
    if (driver->methods->key_management == NULL ||
        driver->methods->key_management->p_destroy == NULL) {
        return PSA_ERROR_NOT_PERMITTED;
    }
    status = driver->methods->key_management->p_destroy(
        &driver->u.context,
        driver->u.internal.persistent_data,
        slot_number);
    storage_status = psa_save_se_persistent_data(driver);
    return status == PSA_SUCCESS ? storage_status : status;
}

psa_status_t psa_init_all_se_drivers(void)
{
    size_t i;
    for (i = 0; i < PSA_MAX_SE_DRIVERS; i++) {
        psa_se_drv_table_entry_t *driver = &driver_table[i];
        if (driver->location == 0) {
            continue; /* skipping unused entry */
        }
        const psa_drv_se_t *methods = psa_get_se_driver_methods(driver);
        if (methods->p_init != NULL) {
            psa_status_t status = methods->p_init(
                &driver->u.context,
                driver->u.internal.persistent_data,
                driver->location);
            if (status != PSA_SUCCESS) {
                return status;
            }
            status = psa_save_se_persistent_data(driver);
            if (status != PSA_SUCCESS) {
                return status;
            }
        }
    }
    return PSA_SUCCESS;
}



/****************************************************************/
/* Driver registration */
/****************************************************************/

psa_status_t psa_register_se_driver(
    psa_key_location_t location,
    const psa_drv_se_t *methods)
{
    size_t i;
    psa_status_t status;

    if (methods->hal_version != PSA_DRV_SE_HAL_VERSION) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    /* Driver table entries are 0-initialized. 0 is not a valid driver
     * location because it means a transparent key. */
    MBEDTLS_STATIC_ASSERT(PSA_KEY_LOCATION_LOCAL_STORAGE == 0,
                          "Secure element support requires 0 to mean a local key");

    if (location == PSA_KEY_LOCATION_LOCAL_STORAGE) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    if (location > PSA_MAX_SE_LOCATION) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    for (i = 0; i < PSA_MAX_SE_DRIVERS; i++) {
        if (driver_table[i].location == 0) {
            break;
        }
        /* Check that location isn't already in use up to the first free
         * entry. Since entries are created in order and never deleted,
         * there can't be a used entry after the first free entry. */
        if (driver_table[i].location == location) {
            return PSA_ERROR_ALREADY_EXISTS;
        }
    }
    if (i == PSA_MAX_SE_DRIVERS) {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    driver_table[i].location = location;
    driver_table[i].methods = methods;
    driver_table[i].u.internal.persistent_data_size =
        methods->persistent_data_size;

    if (methods->persistent_data_size != 0) {
        driver_table[i].u.internal.persistent_data =
            mbedtls_calloc(1, methods->persistent_data_size);
        if (driver_table[i].u.internal.persistent_data == NULL) {
            status = PSA_ERROR_INSUFFICIENT_MEMORY;
            goto error;
        }
        /* Load the driver's persistent data. On first use, the persistent
         * data does not exist in storage, and is initialized to
         * all-bits-zero by the calloc call just above. */
        status = psa_load_se_persistent_data(&driver_table[i]);
        if (status != PSA_SUCCESS && status != PSA_ERROR_DOES_NOT_EXIST) {
            goto error;
        }
    }

    return PSA_SUCCESS;

error:
    memset(&driver_table[i], 0, sizeof(driver_table[i]));
    return status;
}

void psa_unregister_all_se_drivers(void)
{
    size_t i;
    for (i = 0; i < PSA_MAX_SE_DRIVERS; i++) {
        if (driver_table[i].u.internal.persistent_data != NULL) {
            mbedtls_free(driver_table[i].u.internal.persistent_data);
        }
    }
    memset(driver_table, 0, sizeof(driver_table));
}



/****************************************************************/
/* The end */
/****************************************************************/

#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
