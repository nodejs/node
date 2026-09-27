/**
 * \file threading.h
 *
 * \brief Threading abstraction layer
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_THREADING_H
#define MBEDTLS_THREADING_H
#include "mbedtls/private_access.h"

#include "tf-psa-crypto/build_info.h"
#include "mbedtls/compat-3-crypto.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Detected error in mutex or condition variable usage.
 *
 * Note that depending on the platform, many usage errors of
 * synchronization primitives have undefined behavior. But where
 * it is practical to detect usage errors at runtime, mutex and
 * condition primitives can return this error code.
 */
#define MBEDTLS_ERR_THREADING_USAGE_ERROR                 -0x001E

/** A historical alias for #MBEDTLS_ERR_THREADING_USAGE_ERROR. */
#define MBEDTLS_ERR_THREADING_MUTEX_ERROR MBEDTLS_ERR_THREADING_USAGE_ERROR

#if defined(MBEDTLS_THREADING_C)

#if defined(MBEDTLS_THREADING_PTHREAD)
#include <pthread.h>
typedef pthread_mutex_t mbedtls_platform_mutex_t;
typedef pthread_cond_t mbedtls_platform_condition_variable_t;
#endif

#if defined(MBEDTLS_THREADING_ALT)
/* You should define the types mbedtls_platform_mutex_t and
 * mbedtls_platform_condition_variable_t in your header. */
#include "threading_alt.h"

/**
 * \brief           Set your alternate threading implementation function
 *                  pointers and initialize global mutexes. If used, this
 *                  function must be called once in the main thread before any
 *                  other Mbed TLS function is called, and
 *                  mbedtls_threading_free_alt() must be called once in the main
 *                  thread after all other Mbed TLS functions.
 *
 * \note            Functions should return #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  if a mutex usage error is detected. However, it is
 *                  acceptable for usage errors to result in undefined behavior
 *                  (including deadlocks and crashes) if detecting usage errors
 *                  is not practical on your platform.
 *
 * \note            The library will always unlock a mutex from the same
 *                  thread that locked it, and will never lock a mutex
 *                  in a thread that has already locked it.
 *
 * \note            Spurious wakeups on condition variables are permitted.
 *
 * \param mutex_init    The mutex init function implementation. <br>
 *                      The behavior is undefined if the mutex is already
 *                      initialized and has not been destroyed, or if this
 *                      function is called concurrently from multiple threads.
 * \param mutex_destroy The mutex destroy function implementation. <br>
 *                      This function must free any resources associated
 *                      with the mutex object. <br>
 *                      The behavior is undefined if the mutex was not
 *                      initialized, if it has already been destroyed,
 *                      if it is currently locked, or if this function
 *                      is called concurrently from multiple threads.
 * \param mutex_lock    The mutex lock function implementation. <br>
 *                      The behavior is undefined if the mutex was not
 *                      initialized, if it has already been destroyed, or if
 *                      it is currently locked by the calling thread.
 * \param mutex_unlock  The mutex unlock function implementation. <br>
 *                      The behavior is undefined if the mutex is not
 *                      currently locked by the calling thread.
 * \param cond_init     The condition variable initialization implementation. <br>
 *                      The behavior is undefined if the variable is already
 *                      initialized, if it has been destroyed, or if this
 *                      function is called concurrently from multiple threads.
 * \param cond_destroy  The condition variable destroy implementation. <br>
 *                      This function must free any resources associated
 *                      with the condition variable object. <br>
 *                      The behavior is undefined if the condition variable
 *                      was not initialized, if it has already been destroyed,
 *                      if a thread is waiting on it, or if this function
 *                      is called concurrently from multiple threads.
 * \param cond_signal   The condition variable signal implementation. <br>
 *                      The behavior is undefined if the condition variable
 *                      was not initialized or if it has already been destroyed.
 * \param cond_broadcast The condition variable broadcast implementation. <br>
 *                      The behavior is undefined if the condition variable
 *                      was not initialized or if it has already been destroyed.
 * \param cond_wait     The condition variable wait implementation. <br>
 *                      The behavior is undefined if the mutex and the
 *                      condition variable have not both been initialized,
 *                      if one of them has already been destroyed, or if the
 *                      mutex is not currently locked by the calling thread.
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
                     mbedtls_platform_mutex_t *));

/**
 * \brief               Free global mutexes.
 */
void mbedtls_threading_free_alt(void);
#endif /* MBEDTLS_THREADING_ALT */

typedef struct mbedtls_threading_mutex_t {
    mbedtls_platform_mutex_t MBEDTLS_PRIVATE(mutex);

    /* Whether the mutex has been initialized successfully.
     *
     * Attempting to lock or destroy a platform mutex that hasn't been
     * successfully initialized can cause a crash or other undefined
     * behavior on some platforms. Keeping track of a successful
     * initialization makes it possible to turn such misuse into
     * a predictable error. This is especially useful because
     * mbedtls_mutex_init() doesn't return an error code, for
     * historical reasons, so the application cannot handle such
     * failures by itself.
     */
    char MBEDTLS_PRIVATE(initialized);

    /* WARNING - state should only be accessed when holding the mutex lock in
     * framework/tests/src/threading_helpers.c, otherwise corruption can occur.
     * state will be 0 after a failed init or a free, and nonzero after a
     * successful init. This field is for testing only and thus not considered
     * part of the public API of Mbed TLS and may change without notice.*/
    char MBEDTLS_PRIVATE(state);

} mbedtls_threading_mutex_t;

typedef struct mbedtls_threading_condition_variable_t {
    mbedtls_platform_condition_variable_t MBEDTLS_PRIVATE(cond);
} mbedtls_threading_condition_variable_t;

/** Initialize a mutex (mutual exclusion lock).
 *
 * You must call this function on a mutex object before using it for any
 * purpose.
 *
 * \note            This function may fail internally, but for historical
 *                  reasons, it does not return a value. If the mutex
 *                  initialization fails internally, mbedtls_mutex_free()
 *                  will still work normally, and all other mutex functions
 *                  will fail safely with a nonzero return code.
 *
 * \note            The behavior is undefined if:
 *                  - \p mutex is already initialized;
 *                  - this function is called concurrently on the same
 *                    object from multiple threads.
 *
 * \param mutex     The mutex to initialize.
 */
void mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex);

/** Destroy a mutex.
 *
 * After this function returns, you may call mbedtls_mutex_init()
 * again on \p mutex.
 *
 * \note            The behavior is undefined if:
 *                  - any function is called concurrently on the same
 *                    object from another thread;
 *                  - mbedtls_mutex_init() has never been called on the
 *                    object, and it is not all-bits-zero or `{0}`;
 *                  - \p mutex is locked.
 *
 * \note            This function does nothing if:
 *                  - \p mutex is all-bits-zero or `{0}`.
 *                  - The last function called on \p mutex is
 *                    mbedtls_mutex_free() (i.e. a double free is safe).
 *
 * \param mutex     The mutex to destroy.
 */
void mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex);

/** Lock a mutex.
 *
 * It must not be already locked by the calling thread
 * (mutexes are not recursive).
 *
 * \note            The behavior is undefined if:
 *                  - \p mutex has not been initialized with
 *                    mbedtls_mutex_init(), or has already been freed
 *                    with mbedtls_mutex_free();
 *                  - \p mutex is already locked by the same thread.
 *
 * \param mutex     The mutex to lock.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  mbedtls_mutex_init() failed,
 *                  or a mutex usage error was detected.
 *                  Note that depending on the platform, a mutex usage
 *                  error may result in a deadlock, a crash or other
 *                  undesirable behavior instead of returning an error.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *                  There were insufficient resources to initialize or
 *                  lock the mutex.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex);

/** Unlock a mutex.
 *
 * It must be currently locked by the calling thread.
 *
 * \note            The behavior is undefined if:
 *                  - \p mutex has not been initialized with
 *                    mbedtls_mutex_init(), or has already been freed
 *                    with mbedtls_mutex_free();
 *                  - \p mutex is not locked;
 *                  - \p mutex was locked by a different thread.
 *
 * \param mutex     The mutex to unlock.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  mbedtls_mutex_init() failed,
 *                  or a mutex usage error was detected.
 *                  Note that depending on the platform, a mutex usage
 *                  error may result in a deadlock, a crash or other
 *                  undesirable behavior instead of returning an error.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex);

/** Initialize a condition variable.
 *
 * \note            The behavior is undefined if:
 *                  - \p cond is already initialized;
 *                  - this function is called concurrently on the same
 *                    object from multiple threads.
 *
 * \param cond      The condition variable to initialize.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  The condition variable is already initialized
 *                  (on platforms where this can be detected),
 *                  or an unpecified error occurred.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *                  There were insufficient resources to initialize the object.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_condition_variable_init(
    mbedtls_threading_condition_variable_t *cond);


/** Destroy a condition variable.
 *
 * After this function returns, you may call mbedtls_condition_variable_init()
 * again on \p cond.
 *
 * \note            The behavior is undefined if:
 *                  - \p cond has not been initialized with
 *                    mbedtls_condition_variable_init();
 *                  - any function is called concurrently on the same
 *                    object from another thread.
 *
 * \param cond      The condition variable to destroy.
 */
void mbedtls_condition_variable_free(
    mbedtls_threading_condition_variable_t *cond);

/** Wake up one thread that is waiting on the given condition variable.
 *
 * Do nothing, successfully, if no thread is waiting.
 *
 * \note            The behavior is undefined if:
 *                  - \p cond has not been initialized with
 *                    mbedtls_condition_variable_init(), or has already been
 *                    freed with mbedtls_condition_variable_free().
 *
 * \param cond      The condition variable to signal.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  A usage error was detected.
 *                  Note that depending on the platform, a condition variable
 *                  usage error may result in a deadlock, a crash or other
 *                  undesirable behavior instead of returning an error.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_condition_variable_signal(
    mbedtls_threading_condition_variable_t *cond);

/** Wake up all threads that are waiting on the given condition variable.
 *
 * \note            The behavior is undefined if:
 *                  - \p cond has not been initialized with
 *                    mbedtls_condition_variable_init(), or has already been
 *                    freed with mbedtls_condition_variable_free().
 *
 * \param cond      The condition variable to signal.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  A usage error was detected.
 *                  Note that depending on the platform, a condition variable
 *                  usage error may result in a deadlock, a crash or other
 *                  undesirable behavior instead of returning an error.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_condition_variable_broadcast(
    mbedtls_threading_condition_variable_t *cond);

/** Wait for a wakeup signal on a condition variable.
 *
 * On entry, this function atomically unlocks \p mutex and blocks until
 * another thread calls mbedtls_condition_variable_signal() or
 * mbedtls_condition_variable_broadcast() on \p cond.
 *
 * Before returning, this function locks \p mutex.
 *
 * \note            On some platforms, it is possible for this function
 *                  to stop blocking even if no signal is raised on \p cond
 *                  (spurious wakeup).
 *
 * \note            The behavior is undefined if:
 *                  - \p mutex has not been initialized with
 *                    mbedtls_mutex_init(), or has already been
 *                    freed with mbedtls_mutex_free();
 *                  - \p cond has not been initialized with
 *                    mbedtls_condition_variable_init(), or has already been
 *                    freed with mbedtls_condition_variable_free();
 *                   - \p mutex is not currently locked by the calling thread.
 *
 * \param cond      The condition variable to wait on.
 * \param mutex     The mutex to unlock and re-lock.
 *                  It must currently be locked by the calling thread.
 *
 * \retval 0
 *                  Success.
 * \retval #MBEDTLS_ERR_THREADING_USAGE_ERROR
 *                  A usage error was detected.
 *                  Note that depending on the platform, a condition variable
 *                  usage error may result in a deadlock, a crash or other
 *                  undesirable behavior instead of returning an error.
 * \retval #PSA_ERROR_BAD_STATE
 *                  The compilation option #MBEDTLS_THREADING_ALT is
 *                  enabled, and mbedtls_threading_set_alt() has not
 *                  been called.
 */
int mbedtls_condition_variable_wait(
    mbedtls_threading_condition_variable_t *cond,
    mbedtls_threading_mutex_t *mutex);

/*
 * Global mutexes
 */
#if defined(MBEDTLS_FS_IO)
extern mbedtls_threading_mutex_t mbedtls_threading_readdir_mutex;
#endif

#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_PLATFORM_GMTIME_R_ALT)
/* This mutex may or may not be used in the default definition of
 * mbedtls_platform_gmtime_r(), but in order to determine that,
 * we need to check POSIX features, hence modify _POSIX_C_SOURCE.
 * With the current approach, this declaration is orphaned, lacking
 * an accompanying definition, in case mbedtls_platform_gmtime_r()
 * doesn't need it, but that's not a problem. */
extern mbedtls_threading_mutex_t mbedtls_threading_gmtime_mutex;
#endif /* MBEDTLS_HAVE_TIME_DATE && !MBEDTLS_PLATFORM_GMTIME_R_ALT */

#endif /* MBEDTLS_THREADING_C */

#ifdef __cplusplus
}
#endif

#endif /* threading.h */
