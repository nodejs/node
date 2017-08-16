/*
Copyright (c) 2016, Kari Tristan Helgason <kthelgason@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "uv-common.h"
#include "pthread-barrier.h"

#include <stdlib.h>
#include <assert.h>

/* TODO: support barrier_attr */
int pthread_barrier_init(pthread_barrier_t* barrier,
                         const void* barrier_attr,
                         unsigned count) {
  int rc;
  _uv_barrier* b;

  if (barrier == NULL || count == 0)
    return EINVAL;

  if (barrier_attr != NULL)
    return ENOTSUP;

  b = uv__malloc(sizeof(*b));
  if (b == NULL)
    return ENOMEM;

  b->in = 0;
  b->out = 0;
  b->threshold = count;

  if ((rc = pthread_mutex_init(&b->mutex, NULL)) != 0)
    goto error2;
  if ((rc = pthread_cond_init(&b->cond, NULL)) != 0)
    goto error;

  barrier->b = b;
  return 0;

error:
  pthread_mutex_destroy(&b->mutex);
error2:
  uv__free(b);
  return rc;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
  int rc;
  _uv_barrier* b;

  if (barrier == NULL || barrier->b == NULL)
    return EINVAL;

  b = barrier->b;
  /* Lock the mutex*/
  if ((rc = pthread_mutex_lock(&b->mutex)) != 0)
    return rc;

  /* Increment the count. If this is the first thread to reach the threshold,
     wake up waiters, unlock the mutex, then return
     PTHREAD_BARRIER_SERIAL_THREAD. */
  if (++b->in == b->threshold) {
    b->in = 0;
    b->out = b->threshold - 1;
    rc = pthread_cond_signal(&b->cond);
    assert(rc == 0);

    pthread_mutex_unlock(&b->mutex);
    return PTHREAD_BARRIER_SERIAL_THREAD;
  }
  /* Otherwise, wait for other threads until in is set to 0,
     then return 0 to indicate this is not the first thread. */
  do {
    if ((rc = pthread_cond_wait(&b->cond, &b->mutex)) != 0)
      break;
  } while (b->in != 0);

  /* mark thread exit */
  b->out--;
  pthread_cond_signal(&b->cond);
  pthread_mutex_unlock(&b->mutex);
  return rc;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
  int rc;
  _uv_barrier* b;

  if (barrier == NULL || barrier->b == NULL)
    return EINVAL;

  b = barrier->b;

  if ((rc = pthread_mutex_lock(&b->mutex)) != 0)
    return rc;

  if (b->in > 0 || b->out > 0)
    rc = EBUSY;

  pthread_mutex_unlock(&b->mutex);

  if (rc)
    return rc;

  pthread_cond_destroy(&b->cond);
  pthread_mutex_destroy(&b->mutex);
  uv__free(barrier->b);
  barrier->b = NULL;
  return 0;
}
