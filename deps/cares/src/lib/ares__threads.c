/* MIT License
 *
 * Copyright (c) 2023 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"

#ifdef CARES_THREADS
#  ifdef _WIN32

struct ares__thread_mutex {
  CRITICAL_SECTION mutex;
};

ares__thread_mutex_t *ares__thread_mutex_create(void)
{
  ares__thread_mutex_t *mut = ares_malloc_zero(sizeof(*mut));
  if (mut == NULL) {
    return NULL;
  }

  InitializeCriticalSection(&mut->mutex);
  return mut;
}

void ares__thread_mutex_destroy(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  DeleteCriticalSection(&mut->mutex);
  ares_free(mut);
}

void ares__thread_mutex_lock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  EnterCriticalSection(&mut->mutex);
}

void ares__thread_mutex_unlock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  LeaveCriticalSection(&mut->mutex);
}

struct ares__thread {
  HANDLE thread;
  DWORD  id;

  void *(*func)(void *arg);
  void   *arg;
  void   *rv;
};

/* Wrap for pthread compatibility */
static DWORD WINAPI ares__thread_func(LPVOID lpParameter)
{
  ares__thread_t *thread = lpParameter;

  thread->rv = thread->func(thread->arg);
  return 0;
}

ares_status_t ares__thread_create(ares__thread_t    **thread,
                                  ares__thread_func_t func, void *arg)
{
  ares__thread_t *thr = NULL;

  if (func == NULL || thread == NULL) {
    return ARES_EFORMERR;
  }

  thr = ares_malloc_zero(sizeof(*thr));
  if (thr == NULL) {
    return ARES_ENOMEM;
  }

  thr->func   = func;
  thr->arg    = arg;
  thr->thread = CreateThread(NULL, 0, ares__thread_func, thr, 0, &thr->id);
  if (thr->thread == NULL) {
    ares_free(thr);
    return ARES_ESERVFAIL;
  }

  *thread = thr;
  return ARES_SUCCESS;
}

ares_status_t ares__thread_join(ares__thread_t *thread, void **rv)
{
  ares_status_t status = ARES_SUCCESS;

  if (thread == NULL) {
    return ARES_EFORMERR;
  }

  if (WaitForSingleObject(thread->thread, INFINITE) != WAIT_OBJECT_0) {
    status = ARES_ENOTFOUND;
  } else {
    CloseHandle(thread->thread);
  }

  if (status == ARES_SUCCESS && rv != NULL) {
    *rv = thread->rv;
  }
  ares_free(thread);

  return status;
}

#  else /* !WIN32 == PTHREAD */
#    include <pthread.h>

struct ares__thread_mutex {
  pthread_mutex_t mutex;
};

ares__thread_mutex_t *ares__thread_mutex_create(void)
{
  pthread_mutexattr_t   attr;
  ares__thread_mutex_t *mut = ares_malloc_zero(sizeof(*mut));
  if (mut == NULL) {
    return NULL;
  }

  if (pthread_mutexattr_init(&attr) != 0) {
    ares_free(mut);
    return NULL;
  }

  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
    goto fail;
  }

  if (pthread_mutex_init(&mut->mutex, &attr) != 0) {
    goto fail;
  }

  pthread_mutexattr_destroy(&attr);
  return mut;

fail:
  pthread_mutexattr_destroy(&attr);
  ares_free(mut);
  return NULL;
}

void ares__thread_mutex_destroy(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_destroy(&mut->mutex);
  ares_free(mut);
}

void ares__thread_mutex_lock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_lock(&mut->mutex);
}

void ares__thread_mutex_unlock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_unlock(&mut->mutex);
}

struct ares__thread {
  pthread_t thread;
};

ares_status_t ares__thread_create(ares__thread_t    **thread,
                                  ares__thread_func_t func, void *arg)
{
  ares__thread_t *thr = NULL;

  if (func == NULL || thread == NULL) {
    return ARES_EFORMERR;
  }

  thr = ares_malloc_zero(sizeof(*thr));
  if (thr == NULL) {
    return ARES_ENOMEM;
  }
  if (pthread_create(&thr->thread, NULL, func, arg) != 0) {
    ares_free(thr);
    return ARES_ESERVFAIL;
  }

  *thread = thr;
  return ARES_SUCCESS;
}

ares_status_t ares__thread_join(ares__thread_t *thread, void **rv)
{
  void         *ret    = NULL;
  ares_status_t status = ARES_SUCCESS;

  if (thread == NULL) {
    return ARES_EFORMERR;
  }

  if (pthread_join(thread->thread, &ret) != 0) {
    status = ARES_ENOTFOUND;
  }
  ares_free(thread);

  if (status == ARES_SUCCESS && rv != NULL) {
    *rv = ret;
  }
  return status;
}

#  endif

ares_bool_t ares_threadsafety(void)
{
  return ARES_TRUE;
}

#else /* !CARES_THREADS */

/* NoOp */
ares__thread_mutex_t *ares__thread_mutex_create(void)
{
  return NULL;
}

void ares__thread_mutex_destroy(ares__thread_mutex_t *mut)
{
  (void)mut;
}

void ares__thread_mutex_lock(ares__thread_mutex_t *mut)
{
  (void)mut;
}

void ares__thread_mutex_unlock(ares__thread_mutex_t *mut)
{
  (void)mut;
}

ares_status_t ares__thread_create(ares__thread_t    **thread,
                                  ares__thread_func_t func, void *arg)
{
  (void)thread;
  (void)func;
  (void)arg;
  return ARES_ENOTIMP;
}

ares_status_t ares__thread_join(ares__thread_t *thread, void **rv)
{
  (void)thread;
  (void)rv;
  return ARES_ENOTIMP;
}

ares_bool_t ares_threadsafety(void)
{
  return ARES_FALSE;
}
#endif


ares_status_t ares__channel_threading_init(ares_channel_t *channel)
{
  if (!ares_threadsafety()) {
    return ARES_ENOTIMP;
  }

  channel->lock = ares__thread_mutex_create();
  if (channel->lock == NULL) {
    return ARES_ENOMEM;
  }
  return ARES_SUCCESS;
}

void ares__channel_threading_destroy(ares_channel_t *channel)
{
  ares__thread_mutex_destroy(channel->lock);
  channel->lock = NULL;
}

void ares__channel_lock(ares_channel_t *channel)
{
  ares__thread_mutex_lock(channel->lock);
}

void ares__channel_unlock(ares_channel_t *channel)
{
  ares__thread_mutex_unlock(channel->lock);
}
