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

#ifndef GOOGLE_BREAKPAD_COMMON_ANDROID_TESTING_PTHREAD_FIXES_H
#define GOOGLE_BREAKPAD_COMMON_ANDROID_TESTING_PTHREAD_FIXES_H

#include <pthread.h>


/*Android doesn't provide pthread_barrier_t for now.*/
#ifndef PTHREAD_BARRIER_SERIAL_THREAD

/* Anything except 0 will do here.*/
#define PTHREAD_BARRIER_SERIAL_THREAD  0x12345

typedef struct {
  pthread_mutex_t  mutex;
  pthread_cond_t   cond;
  unsigned         count;
} pthread_barrier_t;

int pthread_barrier_init(pthread_barrier_t* barrier,
                         const void* barrier_attr,
                         unsigned count);

int pthread_barrier_wait(pthread_barrier_t* barrier);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
#endif  /* defined(PTHREAD_BARRIER_SERIAL_THREAD) */

int pthread_yield(void);
#endif  /* GOOGLE_BREAKPAD_COMMON_ANDROID_TESTING_PTHREAD_FIXES_H */
