/* Copyright (c) 2013, Sony Mobile Communications AB
 * Copyright (c) 2012, Google Inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

     * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.
       * Neither the name of Google Inc. nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Android versions < 4.1 have a broken pthread_sigmask.
 * Note that this block of code must come before any inclusion of
 * pthread-fixes.h so that the real pthread_sigmask can be referenced.
 * */
#include <errno.h>
#include <pthread.h>

int uv__pthread_sigmask(int how, const sigset_t* set, sigset_t* oset) {
  static int workaround;

  if (workaround) {
    return sigprocmask(how, set, oset);
  } else if (pthread_sigmask(how, set, oset)) {
    if (errno == EINVAL && sigprocmask(how, set, oset) == 0) {
      workaround = 1;
      return 0;
    } else {
      return -1;
    }
  } else {
    return 0;
  }
}

/*Android doesn't provide pthread_barrier_t for now.*/
#ifndef PTHREAD_BARRIER_SERIAL_THREAD

#include "pthread-fixes.h"

int pthread_barrier_init(pthread_barrier_t* barrier,
                         const void* barrier_attr,
                         unsigned count) {
  barrier->count = count;
  pthread_mutex_init(&barrier->mutex, NULL);
  pthread_cond_init(&barrier->cond, NULL);
  return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
  /* Lock the mutex*/
  pthread_mutex_lock(&barrier->mutex);
  /* Decrement the count. If this is the first thread to reach 0, wake up
     waiters, unlock the mutex, then return PTHREAD_BARRIER_SERIAL_THREAD.*/
  if (--barrier->count == 0) {
    /* First thread to reach the barrier */
    pthread_cond_broadcast(&barrier->cond);
    pthread_mutex_unlock(&barrier->mutex);
    return PTHREAD_BARRIER_SERIAL_THREAD;
  }
  /* Otherwise, wait for other threads until the count reaches 0, then
     return 0 to indicate this is not the first thread.*/
  do {
    pthread_cond_wait(&barrier->cond, &barrier->mutex);
  } while (barrier->count > 0);

  pthread_mutex_unlock(&barrier->mutex);
  return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
  barrier->count = 0;
  pthread_cond_destroy(&barrier->cond);
  pthread_mutex_destroy(&barrier->mutex);
  return 0;
}

#endif  /* defined(PTHREAD_BARRIER_SERIAL_THREAD) */

int pthread_yield(void) {
  sched_yield();
  return 0;
}
