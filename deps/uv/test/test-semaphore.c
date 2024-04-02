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

#include <stdlib.h>
#include <string.h>

typedef struct {
  uv_mutex_t mutex;
  uv_sem_t sem;
  int delay;
  volatile int posted;
} worker_config;


static void worker(void* arg) {
  worker_config* c = arg;

  if (c->delay)
    uv_sleep(c->delay);

  uv_mutex_lock(&c->mutex);
  ASSERT(c->posted == 0);
  uv_sem_post(&c->sem);
  c->posted = 1;
  uv_mutex_unlock(&c->mutex);
}


TEST_IMPL(semaphore_1) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));

  ASSERT(0 == uv_sem_init(&wc.sem, 0));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_sleep(100);
  uv_mutex_lock(&wc.mutex);
  ASSERT(wc.posted == 1);
  uv_sem_wait(&wc.sem); /* should not block */
  uv_mutex_unlock(&wc.mutex); /* ergo, it should be ok to unlock after wait */

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_sem_destroy(&wc.sem);

  return 0;
}


TEST_IMPL(semaphore_2) {
  uv_thread_t thread;
  worker_config wc;

  memset(&wc, 0, sizeof(wc));
  wc.delay = 100;

  ASSERT(0 == uv_sem_init(&wc.sem, 0));
  ASSERT(0 == uv_mutex_init(&wc.mutex));
  ASSERT(0 == uv_thread_create(&thread, worker, &wc));

  uv_sem_wait(&wc.sem);

  ASSERT(0 == uv_thread_join(&thread));
  uv_mutex_destroy(&wc.mutex);
  uv_sem_destroy(&wc.sem);

  return 0;
}


TEST_IMPL(semaphore_3) {
  uv_sem_t sem;

  ASSERT(0 == uv_sem_init(&sem, 3));
  uv_sem_wait(&sem); /* should not block */
  uv_sem_wait(&sem); /* should not block */
  ASSERT(0 == uv_sem_trywait(&sem));
  ASSERT(UV_EAGAIN == uv_sem_trywait(&sem));

  uv_sem_post(&sem);
  ASSERT(0 == uv_sem_trywait(&sem));
  ASSERT(UV_EAGAIN == uv_sem_trywait(&sem));

  uv_sem_destroy(&sem);

  return 0;
}
