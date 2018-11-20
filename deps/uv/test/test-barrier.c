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
  uv_barrier_t barrier;
  int delay;
  volatile int posted;
  int main_barrier_wait_rval;
  int worker_barrier_wait_rval;
} worker_config;


static void worker(void* arg) {
  worker_config* c = arg;

  if (c->delay)
    uv_sleep(c->delay);

  c->worker_barrier_wait_rval = uv_barrier_wait(&c->barrier);
}


TEST_IMPL(barrier_1) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));

  ASSERT(0 == uv_barrier_init(&wc.barrier, 2));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_sleep(100);
  wc.main_barrier_wait_rval = uv_barrier_wait(&wc.barrier);

  ASSERT(0 == uv_thread_join(&thread));
  uv_barrier_destroy(&wc.barrier);

  ASSERT(1 == (wc.main_barrier_wait_rval ^ wc.worker_barrier_wait_rval));

  return 0;
}


TEST_IMPL(barrier_2) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.delay = 100;

  ASSERT(0 == uv_barrier_init(&wc.barrier, 2));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  wc.main_barrier_wait_rval = uv_barrier_wait(&wc.barrier);

  ASSERT(0 == uv_thread_join(&thread));
  uv_barrier_destroy(&wc.barrier);

  ASSERT(1 == (wc.main_barrier_wait_rval ^ wc.worker_barrier_wait_rval));

  return 0;
}


TEST_IMPL(barrier_3) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));

  ASSERT(0 == uv_barrier_init(&wc.barrier, 2));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  wc.main_barrier_wait_rval = uv_barrier_wait(&wc.barrier);

  ASSERT(0 == uv_thread_join(&thread));
  uv_barrier_destroy(&wc.barrier);

  ASSERT(1 == (wc.main_barrier_wait_rval ^ wc.worker_barrier_wait_rval));

  return 0;
}
