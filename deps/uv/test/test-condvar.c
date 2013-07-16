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

#include <string.h>
#include <errno.h>

typedef struct {
  uv_mutex_t mutex;
  uv_cond_t cond;
  int delay;
  int use_broadcast;
  volatile int posted;
} worker_config;


static void worker(void* arg) {
  worker_config* c = arg;

  if (c->delay)
    uv_sleep(c->delay);

  uv_mutex_lock(&c->mutex);
  ASSERT(c->posted == 0);
  c->posted = 1;
  if (c->use_broadcast)
    uv_cond_broadcast(&c->cond);
  else
    uv_cond_signal(&c->cond);
  uv_mutex_unlock(&c->mutex);
}


TEST_IMPL(condvar_1) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_mutex_lock(&wc.mutex);
  uv_sleep(100);
  uv_cond_wait(&wc.cond, &wc.mutex);
  ASSERT(wc.posted == 1);
  uv_mutex_unlock(&wc.mutex);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_2) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.delay = 100;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_mutex_lock(&wc.mutex);
  uv_cond_wait(&wc.cond, &wc.mutex);
  uv_mutex_unlock(&wc.mutex);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_3) {
  uv_thread_t thread;
  worker_config wc;
  int r;

  memset(&wc, 0, sizeof(wc));
  wc.delay = 100;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_mutex_lock(&wc.mutex);
  r = uv_cond_timedwait(&wc.cond, &wc.mutex, (uint64_t)(50 * 1e6));
  ASSERT(r == UV_ETIMEDOUT);
  uv_mutex_unlock(&wc.mutex);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_4) {
  uv_thread_t thread;
  worker_config wc;
  int r;

  memset(&wc, 0, sizeof(wc));
  wc.delay = 100;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_mutex_lock(&wc.mutex);
  r = uv_cond_timedwait(&wc.cond, &wc.mutex, (uint64_t)(150 * 1e6));
  ASSERT(r == 0);
  uv_mutex_unlock(&wc.mutex);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_5) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.use_broadcast = 1;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_mutex_lock(&wc.mutex);
  uv_sleep(100);
  uv_cond_wait(&wc.cond, &wc.mutex);
  ASSERT(wc.posted == 1);
  uv_mutex_unlock(&wc.mutex);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}
