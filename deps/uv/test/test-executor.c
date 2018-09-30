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

#include "strings.h" /* bzero */

static int work_cb_count;
static int after_work_cb_count;
static uv_work_t work_req;
static char data;


static void work_cb(uv_work_t* req) {
  ASSERT(req == &work_req);
  ASSERT(req->data == &data);
  work_cb_count++;
}


static void after_work_cb(uv_work_t* req, int status) {
  ASSERT(status == 0);
  ASSERT(req == &work_req);
  ASSERT(req->data == &data);
  after_work_cb_count++;
}


TEST_IMPL(executor_queue_work_simple) {
  int r;

  work_req.data = &data;
  r = uv_queue_work(uv_default_loop(), &work_req, work_cb, after_work_cb);
  ASSERT(r == 0);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 1);
  ASSERT(after_work_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(executor_queue_work_einval) {
  int r;

  work_req.data = &data;
  r = uv_queue_work(uv_default_loop(), &work_req, NULL, after_work_cb);
  ASSERT(r == UV_EINVAL);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(work_cb_count == 0);
  ASSERT(after_work_cb_count == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

/* Define an toy_executor.
 * This is a trivial one-thread producer-consumer setup with a fixed buffer of work. */
#define TOY_EXECUTOR_MAX_REQUESTS 100
static uv_executor_t toy_executor;
static struct toy_executor_data {
  /* Do not hold while executing work. */
  uv_mutex_t mutex;

  int times_submit_called;
  int times_cancel_called;

  unsigned n_completed;

  unsigned head;
  unsigned tail;
  /* Queue with space for some extras. */
  uv_work_t* queued_work[TOY_EXECUTOR_MAX_REQUESTS + 10];

  unsigned no_more_work_coming;
  uv_sem_t thread_exiting;

  /* For signaling toy_slow_work. */
  uv_sem_t finish_slow_work;

  uv_thread_t thread;
  uv_executor_t *executor;
} toy_executor_data;

static void worker(void* arg) {
  struct toy_executor_data* data;
  uv_work_t* work;
  int should_break;

  data = arg;

  should_break = 0;
  for (;;) {
    /* Run until we (1) have no work, and (2) see data->no_more_work_coming. */

    /* Run pending work. */
    for (;;) {
      uv_mutex_lock(&data->mutex);
      if (data->head == data->tail) {
        uv_mutex_unlock(&data->mutex);
        break;
      }

      printf("worker: found %d work -- head %d tail %d\n", data->tail - data->head, data->head, data->tail);
      ASSERT(0 <= data->head && data->head <= data->tail);
      uv_mutex_unlock(&data->mutex);

      /* Handle one. */
      work = data->queued_work[data->head];
      printf("worker: Running work %p\n", work);
      work->work_cb(work);

      /* Tell libuv we're done with this work. */
      printf("worker: Telling libuv we're done with %p\n", work);
      uv_executor_return_work(work);

      /* Advance. */
      uv_mutex_lock(&data->mutex);
      printf("worker: Advancing\n");
      data->head++;
      data->n_completed++;
      uv_mutex_unlock(&data->mutex);
    }

    /* Loop unless (1) no work, and (2) no_more_work_coming. */
    uv_mutex_lock(&data->mutex);
    if (data->head == data->tail && data->no_more_work_coming) {
      printf("worker exiting\n");
      fflush(stdout);
      should_break = 1;
      uv_sem_post(&data->thread_exiting);
    }
    uv_mutex_unlock(&data->mutex);

    if (should_break)
      break;
  }
}

static void toy_executor_init(void) {
  bzero(&toy_executor, sizeof(toy_executor));

  toy_executor_data.times_submit_called = 0;
  toy_executor_data.times_cancel_called = 0;
  toy_executor_data.n_completed = 0;
  toy_executor_data.head = 0;
  toy_executor_data.tail = 0;
  toy_executor_data.no_more_work_coming = 0;
  ASSERT(0 == uv_sem_init(&toy_executor_data.finish_slow_work, 0));
  toy_executor_data.executor = &toy_executor;
  ASSERT(0 == uv_mutex_init(&toy_executor_data.mutex));
  ASSERT(0 == uv_sem_init(&toy_executor_data.thread_exiting, 0));

  ASSERT(0 == uv_thread_create(&toy_executor_data.thread, worker, &toy_executor_data));
}

static void toy_executor_destroy(uv_executor_t* executor) {
  struct toy_executor_data* data;
  
  data = executor->data;
  uv_thread_join(&data->thread);
  uv_mutex_destroy(&data->mutex);
}

static void toy_executor_submit(uv_executor_t* executor,
                                uv_work_t* req,
                                const uv_work_options_t* opts) {
  struct toy_executor_data* data;
  printf("toy_executor_submit: req %p\n", req);
  
  data = executor->data;
  data->times_submit_called++;

  uv_mutex_lock(&data->mutex);
  ASSERT(data->tail < ARRAY_SIZE(data->queued_work));
  data->queued_work[data->tail] = req;
  data->tail++;
  uv_mutex_unlock(&data->mutex);
}

static int toy_executor_cancel(uv_executor_t* executor, uv_work_t* req) {
  struct toy_executor_data* data;
  
  data = executor->data;
  printf("toy_executor_cancel: req %p\n", req);
  data->times_cancel_called++;

  return UV_EINVAL;
}

static void toy_work(uv_work_t* req) {
  printf("toy_work: req %p\n", req);
  ASSERT(req);
}

static void toy_slow_work(uv_work_t* req) {
  printf("toy_slow_work: req %p\n", req);
  ASSERT(req);
  uv_sem_wait(&toy_executor_data.finish_slow_work);
}

TEST_IMPL(executor_replace) {
  uv_work_t work[100];
  int n_extra_requests;
  uv_work_t slow_work;
  uv_work_t cancel_work;
  int i;

  n_extra_requests = 0;

  /* Replace the builtin executor with our toy_executor. */
  toy_executor_init();
  toy_executor.submit = toy_executor_submit;
  toy_executor.cancel = toy_executor_cancel;
  toy_executor.data = &toy_executor_data;
  ASSERT(0 == uv_replace_executor(&toy_executor));

  /* Submit work. */
  for (i = 0; i < TOY_EXECUTOR_MAX_REQUESTS; i++) {
    printf("Queuing work %p\n", &work[i]);
    if (i < TOY_EXECUTOR_MAX_REQUESTS/2)
      ASSERT(0 == uv_queue_work(uv_default_loop(), &work[i], toy_work, NULL));
    else
      ASSERT(0 == uv_executor_queue_work(uv_default_loop(), &work[i], NULL, toy_work, NULL));
  }

  /* Having queued work, we should no longer be able to replace. */
  ASSERT(0 != uv_replace_executor(&toy_executor));

  /* Submit a slow request so a subsequent request will be cancelable. */
  n_extra_requests++;
  ASSERT(0 == uv_queue_work(uv_default_loop(), &slow_work, toy_slow_work, NULL));

  /* Submit and try to cancel some work. */
  n_extra_requests++;
  ASSERT(0 == uv_queue_work(uv_default_loop(), &cancel_work, toy_work, NULL));
  ASSERT(UV_EINVAL == uv_cancel((uv_req_t *) &cancel_work));

  /* Let the slow work finish. */
  uv_sem_post(&toy_executor_data.finish_slow_work);

  /* Side channel: tell pool we're done and wait until it finishes. */
  printf("Telling pool we're done\n");
  uv_mutex_lock(&toy_executor_data.mutex);
  toy_executor_data.no_more_work_coming = 1;
  uv_mutex_unlock(&toy_executor_data.mutex);

  printf("Waiting for pool\n");
  uv_sem_wait(&toy_executor_data.thread_exiting);

  /* Validate. */
  printf("Validating\n");
  ASSERT(TOY_EXECUTOR_MAX_REQUESTS + n_extra_requests == toy_executor_data.times_submit_called);
  ASSERT(TOY_EXECUTOR_MAX_REQUESTS + n_extra_requests == toy_executor_data.n_completed);
  ASSERT(1 == toy_executor_data.times_cancel_called);

  toy_executor_destroy(&toy_executor);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(executor_replace_nocancel) {
  uv_work_t slow_work;
  uv_work_t cancel_work;

  /* Replace the builtin executor with our toy_executor.
   * This executor does not implement a cancel CB.
   * uv_cancel should fail with UV_ENOSYS. */
  toy_executor_init();
  toy_executor.submit = toy_executor_submit;
  toy_executor.cancel = NULL;
  toy_executor.data = &toy_executor_data;
  ASSERT(0 == uv_replace_executor(&toy_executor));

  /* Submit a slow request so a subsequent request will be cancelable. */
  ASSERT(0 == uv_queue_work(uv_default_loop(), &slow_work, toy_slow_work, NULL));

  /* Submit and then try to cancel a slow request.
   * With toy_executor.cancel == NULL, should return UV_ENOSYS. */
  ASSERT(0 == uv_queue_work(uv_default_loop(), &cancel_work, toy_work, NULL));
  ASSERT(UV_ENOSYS == uv_cancel((uv_req_t *) &cancel_work));

  /* Let the slow work finish. */
  uv_sem_post(&toy_executor_data.finish_slow_work);

  /* Side channel: tell pool we're done and wait until it finishes. */
  printf("Telling pool we're done\n");
  uv_mutex_lock(&toy_executor_data.mutex);
  toy_executor_data.no_more_work_coming = 1;
  uv_mutex_unlock(&toy_executor_data.mutex);

  printf("Waiting for pool\n");
  uv_sem_wait(&toy_executor_data.thread_exiting);

  toy_executor_destroy(&toy_executor);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
