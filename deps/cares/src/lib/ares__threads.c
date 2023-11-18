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

static ares__thread_mutex_t *ares__thread_mutex_create(void)
{
  ares__thread_mutex_t *mut = ares_malloc_zero(sizeof(*mut));
  if (mut == NULL) {
    return NULL;
  }

  InitializeCriticalSection(&mut->mutex);
  return mut;
}

static void ares__thread_mutex_destroy(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  DeleteCriticalSection(&mut->mutex);
  ares_free(mut);
}

static void ares__thread_mutex_lock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  EnterCriticalSection(&mut->mutex);
}

static void ares__thread_mutex_unlock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  LeaveCriticalSection(&mut->mutex);
}

#  else
#    include <pthread.h>

struct ares__thread_mutex {
  pthread_mutex_t mutex;
};

static ares__thread_mutex_t *ares__thread_mutex_create(void)
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

static void ares__thread_mutex_destroy(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_destroy(&mut->mutex);
  ares_free(mut);
}

static void ares__thread_mutex_lock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_lock(&mut->mutex);
}

static void ares__thread_mutex_unlock(ares__thread_mutex_t *mut)
{
  if (mut == NULL) {
    return;
  }
  pthread_mutex_unlock(&mut->mutex);
}
#  endif

ares_status_t ares__channel_threading_init(ares_channel_t *channel)
{
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

ares_bool_t ares_threadsafety(void)
{
  return ARES_TRUE;
}

#else
/* NoOp */
ares_status_t ares__channel_threading_init(ares_channel_t *channel)
{
  (void)channel;
  return ARES_SUCCESS;
}

void ares__channel_threading_destroy(ares_channel_t *channel)
{
  (void)channel;
}

void ares__channel_lock(ares_channel_t *channel)
{
  (void)channel;
}

void ares__channel_unlock(ares_channel_t *channel)
{
  (void)channel;
}

ares_bool_t ares_threadsafety(void)
{
  return ARES_FALSE;
}
#endif
