/*
 *  Threading abstraction layer
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * Ensure gmtime_r is available even with -std=c99; must be defined before
 * mbedtls_config.h, which pulls in glibc's features.h. Harmless on other platforms.
 */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_THREADING_C)

#include "threading_internal.h"

#include <psa/crypto_values.h>

#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_PLATFORM_GMTIME_R_ALT)

#if !defined(_WIN32) && (defined(unix) || \
    defined(__unix) || defined(__unix__) || (defined(__APPLE__) && \
    defined(__MACH__)))
#include <unistd.h>
#endif /* !_WIN32 && (unix || __unix || __unix__ ||
        * (__APPLE__ && __MACH__)) */

#if !((defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L) ||     \
    (defined(_POSIX_THREAD_SAFE_FUNCTIONS) &&                     \
    _POSIX_THREAD_SAFE_FUNCTIONS >= 200112L))
/*
 * This is a convenience shorthand macro to avoid checking the long
 * preprocessor conditions above. Ideally, we could expose this macro in
 * platform_util.h and simply use it in platform_util.c, threading.c and
 * threading.h. However, this macro is not part of the Mbed TLS public API, so
 * we keep it private by only defining it in this file
 */

#if !(defined(_WIN32) && !defined(EFIX64) && !defined(EFI32))
#define THREADING_USE_GMTIME
#endif /* ! ( defined(_WIN32) && !defined(EFIX64) && !defined(EFI32) ) */

#endif /* !( ( defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L ) || \
             ( defined(_POSIX_THREAD_SAFE_FUNCTIONS ) && \
                _POSIX_THREAD_SAFE_FUNCTIONS >= 200112L ) ) */

#endif /* MBEDTLS_HAVE_TIME_DATE && !MBEDTLS_PLATFORM_GMTIME_R_ALT */

#if defined(MBEDTLS_THREADING_PTHREAD)
static int err_from_posix(int posix_ret)
{
    switch (posix_ret) {
        case 0:
            return 0;
        default:
            return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
}

static int threading_mutex_init_pthread(mbedtls_platform_mutex_t *mutex)
{
    int posix_ret = pthread_mutex_init(mutex, NULL);
    return err_from_posix(posix_ret);
}

static void threading_mutex_destroy_pthread(mbedtls_platform_mutex_t *mutex)
{
    (void) pthread_mutex_destroy(mutex);
}

static int threading_mutex_lock_pthread(mbedtls_platform_mutex_t *mutex)
{
    int posix_ret = pthread_mutex_lock(mutex);
    return err_from_posix(posix_ret);
}

static int threading_mutex_unlock_pthread(mbedtls_platform_mutex_t *mutex)
{
    int posix_ret = pthread_mutex_unlock(mutex);
    return err_from_posix(posix_ret);
}

int (*mbedtls_mutex_init_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_init_pthread;
void (*mbedtls_mutex_free_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_destroy_pthread;
int (*mbedtls_mutex_lock_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_lock_pthread;
int (*mbedtls_mutex_unlock_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_unlock_pthread;

/*
 * With pthreads we can statically initialize mutexes
 */
#define MUTEX_INIT  = { PTHREAD_MUTEX_INITIALIZER, 1, 1 }

int mbedtls_condition_variable_init(
    mbedtls_threading_condition_variable_t *cond)
{
    int posix_ret = pthread_cond_init(&cond->cond, NULL);
    return err_from_posix(posix_ret);
}

void mbedtls_condition_variable_free(
    mbedtls_threading_condition_variable_t *cond)
{
    (void) pthread_cond_destroy(&cond->cond);
}

int mbedtls_condition_variable_signal(
    mbedtls_threading_condition_variable_t *cond)
{
    int posix_ret = pthread_cond_signal(&cond->cond);
    return err_from_posix(posix_ret);
}

int mbedtls_condition_variable_broadcast(
    mbedtls_threading_condition_variable_t *cond)
{
    int posix_ret = pthread_cond_broadcast(&cond->cond);
    return err_from_posix(posix_ret);
}

int mbedtls_condition_variable_wait(
    mbedtls_threading_condition_variable_t *cond,
    mbedtls_threading_mutex_t *mutex)
{
    int posix_ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
    return err_from_posix(posix_ret);
}

#endif /* MBEDTLS_THREADING_PTHREAD */

#if defined(MBEDTLS_THREADING_ALT)
static int threading_mutex_fail(mbedtls_platform_mutex_t *mutex)
{
    ((void) mutex);
    return PSA_ERROR_BAD_STATE;
}
static void threading_mutex_dummy(mbedtls_platform_mutex_t *mutex)
{
    ((void) mutex);
    return;
}

int (*mbedtls_mutex_init_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_fail;
void (*mbedtls_mutex_free_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_dummy;
int (*mbedtls_mutex_lock_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_fail;
int (*mbedtls_mutex_unlock_ptr)(mbedtls_platform_mutex_t *) = threading_mutex_fail;

#endif /* MBEDTLS_THREADING_ALT */

void mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex)
{
    int ret = (*mbedtls_mutex_init_ptr)(&mutex->mutex);
    mutex->initialized = (ret == 0);
}

void mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex)
{
    if (!mutex->initialized) {
        return;
    }
    (*mbedtls_mutex_free_ptr)(&mutex->mutex);
    mutex->initialized = 0;
}

int mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex)
{
    if (!mutex->initialized) {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    return (*mbedtls_mutex_lock_ptr)(&mutex->mutex);
}

int mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex)
{
    if (!mutex->initialized) {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    return (*mbedtls_mutex_unlock_ptr)(&mutex->mutex);
}



#if defined(MBEDTLS_THREADING_ALT)

static int (*cond_init_ptr)(mbedtls_platform_condition_variable_t *) = NULL;

int mbedtls_condition_variable_init(
    mbedtls_threading_condition_variable_t *cond)
{
    if (*cond_init_ptr == NULL) {
        return PSA_ERROR_BAD_STATE;
    }
    return (*cond_init_ptr)(&cond->cond);
}

static void (*cond_destroy_ptr)(mbedtls_platform_condition_variable_t *) = NULL;

void mbedtls_condition_variable_free(
    mbedtls_threading_condition_variable_t *cond)
{
    if (*cond_destroy_ptr == NULL) {
        return;
    }
    (*cond_destroy_ptr)(&cond->cond);
}

static int (*cond_signal_ptr)(mbedtls_platform_condition_variable_t *) = NULL;

int mbedtls_condition_variable_signal(
    mbedtls_threading_condition_variable_t *cond)
{
    if (*cond_signal_ptr == NULL) {
        return PSA_ERROR_BAD_STATE;
    }
    return (*cond_signal_ptr)(&cond->cond);
}

static int (*cond_broadcast_ptr)(mbedtls_platform_condition_variable_t *) = NULL;

int mbedtls_condition_variable_broadcast(
    mbedtls_threading_condition_variable_t *cond)
{
    if (*cond_broadcast_ptr == NULL) {
        return PSA_ERROR_BAD_STATE;
    }
    return (*cond_broadcast_ptr)(&cond->cond);
}

static int (*cond_wait_ptr)(mbedtls_platform_condition_variable_t *,
                            mbedtls_platform_mutex_t *) = NULL;

int mbedtls_condition_variable_wait(
    mbedtls_threading_condition_variable_t *cond,
    mbedtls_threading_mutex_t *mutex)
{
    if (*cond_wait_ptr == NULL) {
        return PSA_ERROR_BAD_STATE;
    }
    return (*cond_wait_ptr)(&cond->cond, &mutex->mutex);
}

/*
 * Set functions pointers and initialize global mutexes
 */
void mbedtls_threading_set_alt(
    int (*mutex_init)(mbedtls_platform_mutex_t *),
    void (*mutex_destroy)(mbedtls_platform_mutex_t *),
    int (*mutex_lock)(mbedtls_platform_mutex_t *),
    int (*mutex_unlock)(mbedtls_platform_mutex_t *),
    int (*cond_init)(mbedtls_platform_condition_variable_t *),
    void (*cond_destroy)(mbedtls_platform_condition_variable_t *),
    int (*cond_signal)(mbedtls_platform_condition_variable_t *),
    int (*cond_broadcast)(mbedtls_platform_condition_variable_t *),
    int (*cond_wait)(mbedtls_platform_condition_variable_t *,
                     mbedtls_platform_mutex_t *))
{
    mbedtls_mutex_init_ptr = mutex_init;
    mbedtls_mutex_free_ptr = mutex_destroy;
    mbedtls_mutex_lock_ptr = mutex_lock;
    mbedtls_mutex_unlock_ptr = mutex_unlock;
    cond_init_ptr = cond_init;
    cond_destroy_ptr = cond_destroy;
    cond_signal_ptr = cond_signal;
    cond_broadcast_ptr = cond_broadcast;
    cond_wait_ptr = cond_wait;

#if defined(MBEDTLS_FS_IO)
    mbedtls_mutex_init(&mbedtls_threading_readdir_mutex);
#endif
#if defined(THREADING_USE_GMTIME)
    mbedtls_mutex_init(&mbedtls_threading_gmtime_mutex);
#endif
#if defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_mutex_init(&mbedtls_threading_key_slot_mutex);
    mbedtls_mutex_init(&mbedtls_threading_psa_globaldata_mutex);
    mbedtls_mutex_init(&mbedtls_threading_psa_rngdata_mutex);
#endif
}

/*
 * Free global mutexes
 */
void mbedtls_threading_free_alt(void)
{
#if defined(MBEDTLS_FS_IO)
    mbedtls_mutex_free(&mbedtls_threading_readdir_mutex);
#endif
#if defined(THREADING_USE_GMTIME)
    mbedtls_mutex_free(&mbedtls_threading_gmtime_mutex);
#endif
#if defined(MBEDTLS_PSA_CRYPTO_C)
    mbedtls_mutex_free(&mbedtls_threading_key_slot_mutex);
    mbedtls_mutex_free(&mbedtls_threading_psa_globaldata_mutex);
    mbedtls_mutex_free(&mbedtls_threading_psa_rngdata_mutex);
#endif
}
#endif /* MBEDTLS_THREADING_ALT */

/*
 * Define global mutexes
 */
#ifndef MUTEX_INIT
#define MUTEX_INIT
#endif
#if defined(MBEDTLS_FS_IO)
mbedtls_threading_mutex_t mbedtls_threading_readdir_mutex MUTEX_INIT;
#endif
#if defined(THREADING_USE_GMTIME)
mbedtls_threading_mutex_t mbedtls_threading_gmtime_mutex MUTEX_INIT;
#endif
#if defined(MBEDTLS_PSA_CRYPTO_C)
mbedtls_threading_mutex_t mbedtls_threading_key_slot_mutex MUTEX_INIT;
mbedtls_threading_mutex_t mbedtls_threading_psa_globaldata_mutex MUTEX_INIT;
mbedtls_threading_mutex_t mbedtls_threading_psa_rngdata_mutex MUTEX_INIT;
#endif

#endif /* MBEDTLS_THREADING_C */
