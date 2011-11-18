/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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
#include "task.h"

#include <stdio.h>
#include <stdlib.h>


/* The mutex and rwlock tests are really poor.
 * They're very basic sanity checks and nothing more.
 * Apologies if that rhymes.
 */

TEST_IMPL(thread_mutex) {
  uv_mutex_t mutex;
  int r;

  r = uv_mutex_init(&mutex);
  ASSERT(r == 0);

  uv_mutex_lock(&mutex);
  uv_mutex_unlock(&mutex);
  uv_mutex_destroy(&mutex);

  return 0;
}


TEST_IMPL(thread_rwlock) {
  uv_rwlock_t rwlock;
  int r;

  r = uv_rwlock_init(&rwlock);
  ASSERT(r == 0);

  uv_rwlock_rdlock(&rwlock);
  uv_rwlock_rdunlock(&rwlock);
  uv_rwlock_wrlock(&rwlock);
  uv_rwlock_wrunlock(&rwlock);
  uv_rwlock_destroy(&rwlock);

  return 0;
}
