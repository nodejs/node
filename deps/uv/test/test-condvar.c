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

typedef struct worker_config {
  uv_mutex_t mutex;
  uv_cond_t cond;
  int signal_delay;
  int wait_delay;
  int use_broadcast;
  volatile int posted_1;
  volatile int posted_2;
  void (*signal_cond)(struct worker_config* c, volatile int* flag);
  int (*wait_cond)(struct worker_config* c, const volatile int* flag);
} worker_config;


static void worker(void* arg) {
  worker_config* c = arg;
  c->signal_cond(c, &c->posted_1);
  c->wait_cond(c, &c->posted_2);
}

static void noop_worker(void* arg) {
  return;
}

static void condvar_signal(worker_config* c, volatile int* flag) {
  if (c->signal_delay)
    uv_sleep(c->signal_delay);

  uv_mutex_lock(&c->mutex);
  ASSERT(*flag == 0);
  *flag = 1;
  if (c->use_broadcast)
    uv_cond_broadcast(&c->cond);
  else
    uv_cond_signal(&c->cond);
  uv_mutex_unlock(&c->mutex);
}


static int condvar_wait(worker_config* c, const volatile int* flag) {
  uv_mutex_lock(&c->mutex);
  if (c->wait_delay)
    uv_sleep(c->wait_delay);
  while (*flag == 0) {
    uv_cond_wait(&c->cond, &c->mutex);
  }
  ASSERT(*flag == 1);
  uv_mutex_unlock(&c->mutex);

  return 0;
}


TEST_IMPL(condvar_1) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.wait_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_wait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  ASSERT(0 == wc.wait_cond(&wc, &wc.posted_1));
  wc.signal_cond(&wc, &wc.posted_2);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_2) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.signal_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_wait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  ASSERT(0 == wc.wait_cond(&wc, &wc.posted_1));
  wc.signal_cond(&wc, &wc.posted_2);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


static int condvar_timedwait(worker_config* c, const volatile int* flag) {
  int r;

  r = 0;

  uv_mutex_lock(&c->mutex);
  if (c->wait_delay)
    uv_sleep(c->wait_delay);
  while (*flag == 0) {
    r = uv_cond_timedwait(&c->cond, &c->mutex, (uint64_t)(150 * 1e6));
    ASSERT(r == 0 || r == UV_ETIMEDOUT);
    if (r == UV_ETIMEDOUT)
      break;
  }
  uv_mutex_unlock(&c->mutex);

  return r;
}

/* Test that uv_cond_timedwait will return early when cond is signaled. */
TEST_IMPL(condvar_3) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.signal_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_timedwait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  ASSERT(0 == wc.wait_cond(&wc, &wc.posted_1));
  wc.signal_cond(&wc, &wc.posted_2);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}


TEST_IMPL(condvar_4) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.signal_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_timedwait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  wc.wait_cond(&wc, &wc.posted_1);
  wc.signal_cond(&wc, &wc.posted_2);

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
  wc.signal_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_wait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  wc.wait_cond(&wc, &wc.posted_1);
  wc.signal_cond(&wc, &wc.posted_2);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}

/* Test that uv_cond_timedwait will time out when cond is not signaled. */
TEST_IMPL(condvar_6) {
  uv_thread_t thread;
  worker_config wc;
  int r;

  memset(&wc, 0, sizeof(wc));
  wc.signal_delay = 100;
  wc.signal_cond = condvar_signal;
  wc.wait_cond = condvar_timedwait;

  ASSERT(0 == uv_cond_init(&wc.cond));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, noop_worker, &wc));

  /* This can only return having timed out, because otherwise we
   * loop forever in condvar_timedwait. */
  r = wc.wait_cond(&wc, &wc.posted_1);
  ASSERT(r == UV_ETIMEDOUT);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_cond_destroy(&wc.cond);

  return 0;
}
