/**
 * \file threading_internal.h
 *
 * \brief Threading interfaces used internally in the library and
 *        by the test framework.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_THREADING_INTERNAL_H
#define MBEDTLS_THREADING_INTERNAL_H

#include "tf_psa_crypto_common.h"

#include <mbedtls/threading.h>

/* A version number for the internal threading interface.
 * This is meant to allow the framework to remain compatible with
 * multiple versions, to facilitate transitions.
 *
 * Conventionally, this is the Mbed TLS version number when the
 * threading interface was last changed in a way that may impact the
 * test framework, with the lower byte incremented as necessary
 * if multiple changes happened between releases. */
#define MBEDTLS_THREADING_INTERNAL_VERSION 0x04000001

#if defined(MBEDTLS_THREADING_C)

/*
 * The function pointers for mutex_init, mutex_free, mutex_ and mutex_unlock
 *
 * They are exposed for the sake of the mutex usage verification framework
 * (see framework/tests/src/threading_helpers.c).
 */
extern int (*mbedtls_mutex_init_ptr)(mbedtls_platform_mutex_t *mutex);
extern void (*mbedtls_mutex_free_ptr)(mbedtls_platform_mutex_t *mutex);
extern int (*mbedtls_mutex_lock_ptr)(mbedtls_platform_mutex_t *mutex);
extern int (*mbedtls_mutex_unlock_ptr)(mbedtls_platform_mutex_t *mutex);

/*
 * Global mutexes
 */
#if defined(MBEDTLS_PSA_CRYPTO_C)
/*
 * A mutex used to make the PSA subsystem thread safe.
 *
 * key_slot_mutex protects the registered_readers and
 * state variable for all key slots in &global_data.key_slots.
 *
 * This mutex must be held when any read from or write to a state or
 * registered_readers field is performed, i.e. when calling functions:
 * psa_key_slot_state_transition(), psa_register_read(), psa_unregister_read(),
 * psa_key_slot_has_readers() and psa_wipe_key_slot(). */
extern mbedtls_threading_mutex_t mbedtls_threading_key_slot_mutex;

/*
 * A mutex used to make the non-rng PSA global_data struct members thread safe.
 *
 * This mutex must be held when reading or writing to any of the PSA global_data
 * structure members, other than the rng_state or rng struct. */
extern mbedtls_threading_mutex_t mbedtls_threading_psa_globaldata_mutex;

/*
 * A mutex used to make the PSA global_data rng data thread safe.
 *
 * This mutex must be held when reading or writing to the PSA
 * global_data rng_state or rng struct members. */
extern mbedtls_threading_mutex_t mbedtls_threading_psa_rngdata_mutex;
#endif

#endif /* MBEDTLS_THREADING_C */

#endif /* MBEDTLS_THREADING_INTERNAL_H */
