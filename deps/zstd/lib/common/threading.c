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

/**
 * This file will hold wrapper for systems, which do not support pthreads
 */

#include "threading.h"

/* create fake symbol to avoid empty translation unit warning */
int g_ZSTD_threading_useless_symbol;

#if defined(ZSTD_MULTITHREAD) && defined(_WIN32)

/**
 * Windows minimalist Pthread Wrapper
 */


/* ===  Dependencies  === */
#include <process.h>
#include <errno.h>


/* ===  Implementation  === */

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
    int initialized;
    ZSTD_pthread_cond_t initialized_cond;
    ZSTD_pthread_mutex_t initialized_mutex;
} ZSTD_thread_params_t;

static unsigned __stdcall worker(void *arg)
{
    void* (*start_routine)(void*);
    void* thread_arg;

    /* Initialized thread_arg and start_routine and signal main thread that we don't need it
     * to wait any longer.
     */
    {
        ZSTD_thread_params_t*  thread_param = (ZSTD_thread_params_t*)arg;
        thread_arg = thread_param->arg;
        start_routine = thread_param->start_routine;

        /* Signal main thread that we are running and do not depend on its memory anymore */
        ZSTD_pthread_mutex_lock(&thread_param->initialized_mutex);
        thread_param->initialized = 1;
        ZSTD_pthread_cond_signal(&thread_param->initialized_cond);
        ZSTD_pthread_mutex_unlock(&thread_param->initialized_mutex);
    }

    start_routine(thread_arg);

    return 0;
}

int ZSTD_pthread_create(ZSTD_pthread_t* thread, const void* unused,
            void* (*start_routine) (void*), void* arg)
{
    ZSTD_thread_params_t thread_param;
    (void)unused;

    thread_param.start_routine = start_routine;
    thread_param.arg = arg;
    thread_param.initialized = 0;
    *thread = NULL;

    /* Setup thread initialization synchronization */
    if(ZSTD_pthread_cond_init(&thread_param.initialized_cond, NULL)) {
        /* Should never happen on Windows */
        return -1;
    }
    if(ZSTD_pthread_mutex_init(&thread_param.initialized_mutex, NULL)) {
        /* Should never happen on Windows */
        ZSTD_pthread_cond_destroy(&thread_param.initialized_cond);
        return -1;
    }

    /* Spawn thread */
    *thread = (HANDLE)_beginthreadex(NULL, 0, worker, &thread_param, 0, NULL);
    if (!thread) {
        ZSTD_pthread_mutex_destroy(&thread_param.initialized_mutex);
        ZSTD_pthread_cond_destroy(&thread_param.initialized_cond);
        return errno;
    }

    /* Wait for thread to be initialized */
    ZSTD_pthread_mutex_lock(&thread_param.initialized_mutex);
    while(!thread_param.initialized) {
        ZSTD_pthread_cond_wait(&thread_param.initialized_cond, &thread_param.initialized_mutex);
    }
    ZSTD_pthread_mutex_unlock(&thread_param.initialized_mutex);
    ZSTD_pthread_mutex_destroy(&thread_param.initialized_mutex);
    ZSTD_pthread_cond_destroy(&thread_param.initialized_cond);

    return 0;
}

int ZSTD_pthread_join(ZSTD_pthread_t thread)
{
    DWORD result;

    if (!thread) return 0;

    result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    switch (result) {
    case WAIT_OBJECT_0:
        return 0;
    case WAIT_ABANDONED:
        return EINVAL;
    default:
        return GetLastError();
    }
}

#endif   /* ZSTD_MULTITHREAD */

#if defined(ZSTD_MULTITHREAD) && DEBUGLEVEL >= 1 && !defined(_WIN32)

#define ZSTD_DEPS_NEED_MALLOC
#include "zstd_deps.h"

int ZSTD_pthread_mutex_init(ZSTD_pthread_mutex_t* mutex, pthread_mutexattr_t const* attr)
{
    *mutex = (pthread_mutex_t*)ZSTD_malloc(sizeof(pthread_mutex_t));
    if (!*mutex)
        return 1;
    return pthread_mutex_init(*mutex, attr);
}

int ZSTD_pthread_mutex_destroy(ZSTD_pthread_mutex_t* mutex)
{
    if (!*mutex)
        return 0;
    {
        int const ret = pthread_mutex_destroy(*mutex);
        ZSTD_free(*mutex);
        return ret;
    }
}

int ZSTD_pthread_cond_init(ZSTD_pthread_cond_t* cond, pthread_condattr_t const* attr)
{
    *cond = (pthread_cond_t*)ZSTD_malloc(sizeof(pthread_cond_t));
    if (!*cond)
        return 1;
    return pthread_cond_init(*cond, attr);
}

int ZSTD_pthread_cond_destroy(ZSTD_pthread_cond_t* cond)
{
    if (!*cond)
        return 0;
    {
        int const ret = pthread_cond_destroy(*cond);
        ZSTD_free(*cond);
        return ret;
    }
}

#endif
