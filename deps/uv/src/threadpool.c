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

#include "uv-common.h"

#if !defined(_WIN32)
# include "unix/internal.h"
#endif

#include <stdlib.h>

#define MAX_THREADPOOL_SIZE 128

/* executor */
uv_once_t init_default_executor_once = UV_ONCE_INIT;
static uv_executor_t default_executor;

/* default_executor.data */
uv_once_t start_workers_once = UV_ONCE_INIT;
static struct default_executor_fields {
  uv_once_t init;
  uv_cond_t cond;
  uv_mutex_t mutex;
  unsigned int idle_workers;
  unsigned int nworkers;
  uv_thread_t* workers;
  uv_thread_t default_workers[4];
  QUEUE exit_message;
  QUEUE wq;
  int used;
} _fields;

/* For worker initialization. */
static struct worker_arg {
  uv_executor_t* executor;
  uv_sem_t* ready;
} worker_arg;

/* Helpers for the default executor implementation. */

/* Post item q to the TP queue.
 * Caller must hold fields->lock. */
static void post(struct default_executor_fields* fields, QUEUE* q) {
  QUEUE_INSERT_TAIL(&fields->wq, q);
  if (0 < fields->idle_workers)
    uv_cond_signal(&fields->cond);
}

/* This is the entry point for each worker in the threadpool.
 * arg is a worker_arg*. */
static void worker(void* arg) {
  struct worker_arg* warg;
  uv_executor_t* executor;
  struct uv__work* w;
  uv_work_t* req;
  QUEUE* q;
  struct default_executor_fields* fields;

  /* Extract fields from warg. */
  warg = arg;
  executor = warg->executor;
  assert(executor != NULL);
  fields = executor->data;
  assert(fields != NULL);

  /* Signal we're ready. */
  uv_sem_post(warg->ready);
  arg = NULL;
  warg = NULL;

  for (;;) {
    /* Get the next work. */
    uv_mutex_lock(&fields->mutex);

    while (QUEUE_EMPTY(&fields->wq)) {
      fields->idle_workers += 1;
      uv_cond_wait(&fields->cond, &fields->mutex);
      fields->idle_workers -= 1;
    }

    q = QUEUE_HEAD(&fields->wq);

    if (q == &fields->exit_message) {
      /* Wake up another thread. */
      uv_cond_signal(&fields->cond);
    }
    else {
      QUEUE_REMOVE(q);
      QUEUE_INIT(q);  /* Signal uv_cancel() that the work req is
                             executing. */
    }

    uv_mutex_unlock(&fields->mutex);

    /* Are we done? */
    if (q == &fields->exit_message)
      break;

    w = QUEUE_DATA(q, struct uv__work, wq);
    req = container_of(w, uv_work_t, work_req);

    /* Do the work. */
    LOG_1("Worker: running work_cb for req %p\n", req);
    req->work_cb(req);
    LOG_1("Worker: Done with req %p\n", req);

    /* Signal uv_cancel() that the work req is done executing. */
    uv_mutex_lock(&fields->mutex);
    w->work = NULL;
    uv_mutex_unlock(&fields->mutex);

    /* Tell event loop we finished with this request. */
    uv_executor_return_work(req);
  }
}

/* (Initialize _fields and) start the workers. */
static void start_workers(void) {
  unsigned int i;
  const char* val;
  uv_sem_t sem;
  unsigned int n_default_workers;

  /* Initialize various fields members. */
  _fields.used = 1;

  /* How many workers? */
  n_default_workers = ARRAY_SIZE(_fields.default_workers);
  _fields.nworkers = n_default_workers;
  val = getenv("UV_THREADPOOL_SIZE");
  if (val != NULL)
    _fields.nworkers = atoi(val);
  if (_fields.nworkers == 0)
    _fields.nworkers = 1;
  if (_fields.nworkers > MAX_THREADPOOL_SIZE)
    _fields.nworkers = MAX_THREADPOOL_SIZE;

  /* Try to use the statically declared workers instead of malloc. */
  _fields.workers = _fields.default_workers;
  if (_fields.nworkers > n_default_workers) {
    _fields.workers = uv__malloc(_fields.nworkers * sizeof(_fields.workers[0]));
    if (_fields.workers == NULL) {
      _fields.nworkers = n_default_workers;
      _fields.workers = _fields.default_workers;
    }
  }

  if (uv_cond_init(&_fields.cond))
    abort();

  if (uv_mutex_init(&_fields.mutex))
    abort();

  QUEUE_INIT(&_fields.wq);

  if (uv_sem_init(&sem, 0))
    abort();

  /* Start the workers. */
  worker_arg.executor = &default_executor;
  worker_arg.ready = &sem;
  for (i = 0; i < _fields.nworkers; i++)
    if (uv_thread_create(_fields.workers + i, worker, &worker_arg))
      abort();

  /* Wait for workers to start. */
  for (i = 0; i < _fields.nworkers; i++)
    uv_sem_wait(&sem);
  uv_sem_destroy(&sem);
}

#ifndef _WIN32
/* cleanup of the default_executor if necessary. */
UV_DESTRUCTOR(static void cleanup(void)) {
  unsigned int i;

  if (!_fields.used)
    return;

  if (_fields.nworkers == 0)
    return;

  uv_mutex_lock(&_fields.mutex);
  post(&_fields, &_fields.exit_message);
  uv_mutex_unlock(&_fields.mutex);

  for (i = 0; i < _fields.nworkers; i++)
    if (uv_thread_join(_fields.workers + i))
      abort();

  if (_fields.workers != _fields.default_workers)
    uv__free(_fields.workers);

  uv_mutex_destroy(&_fields.mutex);
  uv_cond_destroy(&_fields.cond);

  _fields.workers = NULL;
  _fields.nworkers = 0;
}
#endif

/******************************
 * Default libuv threadpool, implemented using the executor API.
*******************************/

static void uv__default_executor_submit(uv_executor_t* executor,
                                        uv_work_t* req,
                                        const uv_work_options_t* opts) {
  struct default_executor_fields* fields;
  struct uv__work* wreq;

  assert(executor == &default_executor);
  /* Make sure we are initialized internally. */
  uv_once(&start_workers_once, start_workers);

  fields = executor->data;
  assert(fields != NULL);

  /* Put executor-specific data into req->executor_data. */
  wreq = &req->work_req;
  req->executor_data = wreq;
  /* TODO Don't do this. */
  wreq->work = 0xdeadbeef; /* Non-NULL: "Not yet completed". */

  uv_mutex_lock(&fields->mutex);

  /* Add to our queue. */
  post(fields, &wreq->wq);

  uv_mutex_unlock(&fields->mutex);
}

static int uv__default_executor_cancel(uv_executor_t* executor, uv_work_t* req) {
  struct default_executor_fields* fields;
  struct uv__work* wreq;
  int assigned;
  int already_completed;
  int still_on_queue;
  int can_cancel;

  assert(executor == &default_executor);
  /* Make sure we are initialized internally. */
  uv_once(&start_workers_once, start_workers);

  fields = executor->data;
  assert(fields != NULL);
  wreq = req->executor_data;
  assert(wreq != NULL);

  uv_mutex_lock(&fields->mutex);

  /* Check if we can cancel it. Determine what state req is in. */
  assigned = QUEUE_EMPTY(&wreq->wq);
  already_completed = (wreq->work == NULL);
  still_on_queue = !assigned && !already_completed;
  
  LOG_3("assigned %d already_completed %d still_on_queue\n", assigned, already_completed, still_on_queue); 

  can_cancel = still_on_queue;
  if (can_cancel)
    QUEUE_REMOVE(&wreq->wq);

  uv_mutex_unlock(&fields->mutex);

  LOG_1("uv__default_executor_cancel: can_cancel %d\n", can_cancel);
  if (can_cancel) {
    /* We are now done with req. Notify libuv.
     * The cancellation is not yet complete, but that's OK because
     * this API must be called by the event loop (single-threaded). */
    uv_executor_return_work(req);
    return 0;
  } else {
    /* Failed to cancel.
     * Work is either already done or is still to be executed.
     * Either way we need not call done here. */
    return UV_EBUSY;
  }
}

void uv__default_executor_init(void) {
  /* TODO Behavior on fork? */
  bzero(&default_executor, sizeof(default_executor));
  default_executor.data = &_fields;
  default_executor.submit = uv__default_executor_submit;
  default_executor.cancel = uv__default_executor_cancel;
}

uv_executor_t* uv__default_executor(void) {
  uv_once(&init_default_executor_once, uv__default_executor_init);
  return &default_executor;
}
