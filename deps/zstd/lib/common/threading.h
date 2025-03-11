/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef THREADING_H_938743
#define THREADING_H_938743

#include "debug.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(ZSTD_MULTITHREAD) && defined(_WIN32)

/**
 * Windows minimalist Pthread Wrapper
 */
#ifdef WINVER
#  undef WINVER
#endif
#define WINVER       0x0600

#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#include <windows.h>
#undef ERROR
#define ERROR(name) ZSTD_ERROR(name)


/* mutex */
#define ZSTD_pthread_mutex_t           CRITICAL_SECTION
#define ZSTD_pthread_mutex_init(a, b)  ((void)(b), InitializeCriticalSection((a)), 0)
#define ZSTD_pthread_mutex_destroy(a)  DeleteCriticalSection((a))
#define ZSTD_pthread_mutex_lock(a)     EnterCriticalSection((a))
#define ZSTD_pthread_mutex_unlock(a)   LeaveCriticalSection((a))

/* condition variable */
#define ZSTD_pthread_cond_t             CONDITION_VARIABLE
#define ZSTD_pthread_cond_init(a, b)    ((void)(b), InitializeConditionVariable((a)), 0)
#define ZSTD_pthread_cond_destroy(a)    ((void)(a))
#define ZSTD_pthread_cond_wait(a, b)    SleepConditionVariableCS((a), (b), INFINITE)
#define ZSTD_pthread_cond_signal(a)     WakeConditionVariable((a))
#define ZSTD_pthread_cond_broadcast(a)  WakeAllConditionVariable((a))

/* ZSTD_pthread_create() and ZSTD_pthread_join() */
typedef HANDLE ZSTD_pthread_t;

int ZSTD_pthread_create(ZSTD_pthread_t* thread, const void* unused,
                   void* (*start_routine) (void*), void* arg);

int ZSTD_pthread_join(ZSTD_pthread_t thread);

/**
 * add here more wrappers as required
 */


#elif defined(ZSTD_MULTITHREAD)    /* posix assumed ; need a better detection method */
/* ===   POSIX Systems   === */
#  include <pthread.h>

#if DEBUGLEVEL < 1

#define ZSTD_pthread_mutex_t            pthread_mutex_t
#define ZSTD_pthread_mutex_init(a, b)   pthread_mutex_init((a), (b))
#define ZSTD_pthread_mutex_destroy(a)   pthread_mutex_destroy((a))
#define ZSTD_pthread_mutex_lock(a)      pthread_mutex_lock((a))
#define ZSTD_pthread_mutex_unlock(a)    pthread_mutex_unlock((a))

#define ZSTD_pthread_cond_t             pthread_cond_t
#define ZSTD_pthread_cond_init(a, b)    pthread_cond_init((a), (b))
#define ZSTD_pthread_cond_destroy(a)    pthread_cond_destroy((a))
#define ZSTD_pthread_cond_wait(a, b)    pthread_cond_wait((a), (b))
#define ZSTD_pthread_cond_signal(a)     pthread_cond_signal((a))
#define ZSTD_pthread_cond_broadcast(a)  pthread_cond_broadcast((a))

#define ZSTD_pthread_t                  pthread_t
#define ZSTD_pthread_create(a, b, c, d) pthread_create((a), (b), (c), (d))
#define ZSTD_pthread_join(a)         pthread_join((a),NULL)

#else /* DEBUGLEVEL >= 1 */

/* Debug implementation of threading.
 * In this implementation we use pointers for mutexes and condition variables.
 * This way, if we forget to init/destroy them the program will crash or ASAN
 * will report leaks.
 */

#define ZSTD_pthread_mutex_t            pthread_mutex_t*
int ZSTD_pthread_mutex_init(ZSTD_pthread_mutex_t* mutex, pthread_mutexattr_t const* attr);
int ZSTD_pthread_mutex_destroy(ZSTD_pthread_mutex_t* mutex);
#define ZSTD_pthread_mutex_lock(a)      pthread_mutex_lock(*(a))
#define ZSTD_pthread_mutex_unlock(a)    pthread_mutex_unlock(*(a))

#define ZSTD_pthread_cond_t             pthread_cond_t*
int ZSTD_pthread_cond_init(ZSTD_pthread_cond_t* cond, pthread_condattr_t const* attr);
int ZSTD_pthread_cond_destroy(ZSTD_pthread_cond_t* cond);
#define ZSTD_pthread_cond_wait(a, b)    pthread_cond_wait(*(a), *(b))
#define ZSTD_pthread_cond_signal(a)     pthread_cond_signal(*(a))
#define ZSTD_pthread_cond_broadcast(a)  pthread_cond_broadcast(*(a))

#define ZSTD_pthread_t                  pthread_t
#define ZSTD_pthread_create(a, b, c, d) pthread_create((a), (b), (c), (d))
#define ZSTD_pthread_join(a)         pthread_join((a),NULL)

#endif

#else  /* ZSTD_MULTITHREAD not defined */
/* No multithreading support */

typedef int ZSTD_pthread_mutex_t;
#define ZSTD_pthread_mutex_init(a, b)   ((void)(a), (void)(b), 0)
#define ZSTD_pthread_mutex_destroy(a)   ((void)(a))
#define ZSTD_pthread_mutex_lock(a)      ((void)(a))
#define ZSTD_pthread_mutex_unlock(a)    ((void)(a))

typedef int ZSTD_pthread_cond_t;
#define ZSTD_pthread_cond_init(a, b)    ((void)(a), (void)(b), 0)
#define ZSTD_pthread_cond_destroy(a)    ((void)(a))
#define ZSTD_pthread_cond_wait(a, b)    ((void)(a), (void)(b))
#define ZSTD_pthread_cond_signal(a)     ((void)(a))
#define ZSTD_pthread_cond_broadcast(a)  ((void)(a))

/* do not use ZSTD_pthread_t */

#endif /* ZSTD_MULTITHREAD */

#if defined (__cplusplus)
}
#endif

#endif /* THREADING_H_938743 */
