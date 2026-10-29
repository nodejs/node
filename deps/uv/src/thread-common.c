/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "uv-common.h"

#include <stdlib.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#if defined(PTHREAD_BARRIER_SERIAL_THREAD)
STATIC_ASSERT(sizeof(uv_barrier_t) == sizeof(pthread_barrier_t));
#endif

/* Note: guard clauses should match uv_barrier_t's in include/uv/unix.h. */
#if defined(_AIX) || \
    defined(__OpenBSD__) || \
    !defined(PTHREAD_BARRIER_SERIAL_THREAD)
int uv_barrier_init(uv_barrier_t* barrier, unsigned int count) {
  int rc;
#ifdef _WIN32
  uv_barrier_t* b;
  b = barrier;

  if (barrier == NULL || count == 0)
    return UV_EINVAL;
#else
  struct _uv_barrier* b;

  if (barrier == NULL || count == 0)
    return UV_EINVAL;

  b = uv__malloc(sizeof(*b));
  if (b == NULL)
    return UV_ENOMEM;
#endif

  b->in = 0;
  b->out = 0;
  b->threshold = count;

  rc = uv_mutex_init(&b->mutex);
  if (rc != 0)
    goto error2;

  /* TODO(vjnash): remove these uv_cond_t casts in v2. */
  rc = uv_cond_init((uv_cond_t*) &b->cond);
  if (rc != 0)
    goto error;

#ifndef _WIN32
  barrier->b = b;
#endif
  return 0;

error:
  uv_mutex_destroy(&b->mutex);
error2:
#ifndef _WIN32
  uv__free(b);
#endif
  return rc;
}


int uv_barrier_wait(uv_barrier_t* barrier) {
  int last;
#ifdef _WIN32
  uv_barrier_t* b;
  b = barrier;
#else
  struct _uv_barrier* b;

  if (barrier == NULL || barrier->b == NULL)
    return UV_EINVAL;

  b = barrier->b;
#endif

  uv_mutex_lock(&b->mutex);

  while (b->out != 0)
    uv_cond_wait((uv_cond_t*) &b->cond, &b->mutex);

  if (++b->in == b->threshold) {
    b->in = 0;
    b->out = b->threshold;
    uv_cond_broadcast((uv_cond_t*) &b->cond);
  } else {
    do
      uv_cond_wait((uv_cond_t*) &b->cond, &b->mutex);
    while (b->in != 0);
  }

  last = (--b->out == 0);
  if (last)
    uv_cond_broadcast((uv_cond_t*) &b->cond);

  uv_mutex_unlock(&b->mutex);
  return last;
}


void uv_barrier_destroy(uv_barrier_t* barrier) {
#ifdef _WIN32
  uv_barrier_t* b;
  b = barrier;
#else
  struct _uv_barrier* b;
  b = barrier->b;
#endif

  uv_mutex_lock(&b->mutex);

  assert(b->in == 0);
  while (b->out != 0)
    uv_cond_wait((uv_cond_t*) &b->cond, &b->mutex);

  if (b->in != 0)
    abort();

  uv_mutex_unlock(&b->mutex);
  uv_mutex_destroy(&b->mutex);
  uv_cond_destroy((uv_cond_t*) &b->cond);

#ifndef  _WIN32
  uv__free(barrier->b);
  barrier->b = NULL;
#endif
}

#else

int uv_barrier_init(uv_barrier_t* barrier, unsigned int count) {
  return UV__ERR(pthread_barrier_init(barrier, NULL, count));
}


int uv_barrier_wait(uv_barrier_t* barrier) {
  int rc;

  rc = pthread_barrier_wait(barrier);
  if (rc != 0)
    if (rc != PTHREAD_BARRIER_SERIAL_THREAD)
      abort();

  return rc == PTHREAD_BARRIER_SERIAL_THREAD;
}


void uv_barrier_destroy(uv_barrier_t* barrier) {
  if (pthread_barrier_destroy(barrier))
    abort();
}

#endif
