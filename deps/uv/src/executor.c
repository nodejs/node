/* Copyright libuv project contributors. All rights reserved.
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

#include <stdlib.h>  /* abort() */

#include <stdio.h> /* Debug */

static uv_executor_t* executor = NULL;

static uv_once_t once = UV_ONCE_INIT;
static volatile int initialized = 0; /* Protected by once in
                    uv_executor_queue_work, but not in uv_replace_executor. */

uv_executor_t* uv__executor(void) {
  return executor;
}

/* Executor has finished this request. */
void uv_executor_return_work(uv_work_t* req) {
  uv_mutex_lock(&req->loop->wq_mutex);

  /* Place in the associated loop's queue.
   * NB We are re-purposing req->work_req.wq here.
   * This field is also used by the default executor, but
   * after this call the executor will no longer touch it. */
  QUEUE_INSERT_TAIL(&req->loop->wq, &req->work_req.wq);

  /* Signal to loop that there's a pending task. */
  uv_async_send(&req->loop->wq_async);

  uv_mutex_unlock(&req->loop->wq_mutex);
}

/* CB: Event loop is handling completed requests. */
void uv__executor_work_done(uv_async_t* handle) {
  struct uv__work* w;
  uv_work_t* req;
  uv_loop_t* loop;
  QUEUE* q;
  QUEUE wq;
  int err;

  /* Grab this loop's completed work. */
  loop = container_of(handle, uv_loop_t, wq_async);
  uv_mutex_lock(&loop->wq_mutex);
  QUEUE_MOVE(&loop->wq, &wq);
  uv_mutex_unlock(&loop->wq_mutex);

  /* Handle each uv__work on wq. */
  while (!QUEUE_EMPTY(&wq)) {
    q = QUEUE_HEAD(&wq);
    QUEUE_REMOVE(q);

    w = container_of(q, struct uv__work, wq);
    req = container_of(w, uv_work_t, work_req);
    err = (req->work_cb == uv__executor_work_cancelled) ? UV_ECANCELED : 0;

    uv__req_unregister(req->loop, req);

    if (req->after_work_cb != NULL)
      req->after_work_cb(req, err);
  }
}

int uv_replace_executor(uv_executor_t* _executor) {
  /* Reject if no longer safe to replace. */
  if (initialized)
    return UV_EINVAL;

  /* Check validity of _executor. */
  if (_executor == NULL)
    return UV_EINVAL;
  if (_executor->submit == NULL)
    return UV_EINVAL;

  /* Replace our executor. */
  executor = _executor;

  return 0;
}

static void uv__executor_init(void) {
  int rc;

  /* Assign executor to default if none was set. */
  if (executor == NULL) {
    rc = uv_replace_executor(uv__default_executor());
    assert(!rc);
  }

  /* Once initialized, it is no longer safe to replace. */
  initialized = 1;
}

int uv_executor_queue_work(uv_loop_t* loop,
                           uv_work_t* req,
                           uv_work_options_t* opts,
                           uv_work_cb work_cb,
                           uv_after_work_cb after_work_cb) {
  char work_type[32];
  /* Initialize the executor once. */
  uv_once(&once, uv__executor_init);

  /* Check validity. */
  if (loop == NULL || req == NULL || work_cb == NULL)
    return UV_EINVAL;

  /* Register req on loop. */
  LOG_1("uv_executor_queue_work: req %p\n", (void*) req);
  uv__req_init(loop, req, UV_WORK);
  req->loop = loop;
  req->work_cb = work_cb;
  req->after_work_cb = after_work_cb;

  /* TODO Just some logging. */
  if (opts) {
    switch(opts->type) {
    case UV_WORK_UNKNOWN:
      sprintf(work_type, "%s", "UV_WORK_UNKNOWN");
      break;
    case UV_WORK_FS:
      sprintf(work_type, "%s", "UV_WORK_FS");
      break;
    case UV_WORK_DNS:
      sprintf(work_type, "%s", "UV_WORK_DNS");
      break;
    case UV_WORK_USER_IO:
      sprintf(work_type, "%s", "UV_WORK_USER_IO");
      break;
    case UV_WORK_USER_CPU:
      sprintf(work_type, "%s", "UV_WORK_USER_CPU");
      break;
    case UV_WORK_PRIVATE:
      sprintf(work_type, "%s", "UV_WORK_PRIVATE");
      break;
    default:
      sprintf(work_type, "%s", "UNKNOWN");
      break;
    }
    LOG_2("uv_executor_queue_work: type %d: %s\n", opts->type, work_type);
  }
  else
    LOG_0("uv_executor_queue_work: no options provided\n");

  /* Submit to the executor. */
  executor->submit(executor, req, opts);

  return 0;
}

int uv_queue_work(uv_loop_t* loop,
                  uv_work_t* req,
                  uv_work_cb work_cb,
                  uv_after_work_cb after_work_cb) {
  uv_work_options_t options;
  options.type = UV_WORK_UNKNOWN;
  options.priority = -1;
  options.cancelable = 0;
  options.data = NULL;
  return uv_executor_queue_work(loop, req, &options, work_cb, after_work_cb);
}

static int uv__cancel_ask_executor(uv_work_t* work) {
  int r;

  r = UV_ENOSYS;
  LOG_0("Trying to call cancel\n");
  if (executor->cancel != NULL) {
    LOG_0("Calling cancel!\n");
    r = executor->cancel(executor, work);
    if (r == 0)
      work->work_cb = uv__executor_work_cancelled;
  }

  return r;
}

int uv_cancel(uv_req_t* req) {
  uv_work_t* work;
  int r;

  LOG_1("uv_cancel: req %p\n", (void*) req);

  r = UV_EINVAL;
  switch (req->type) {
  case UV_FS:
  case UV_GETADDRINFO:
  case UV_GETNAMEINFO:
    /* These internal users prepare and submit requests to the executor. */
    work = req->executor_data;
    r = uv__cancel_ask_executor(work);
    break;
  case UV_WORK:
    /* This is a direct request to the executor. */
    work = (uv_work_t*) req;
    r = uv__cancel_ask_executor(work);
    break;
  default:
    return UV_EINVAL;
  }

  return r;
}

/* This is just a magic, it should never be called. */
void uv__executor_work_cancelled(uv_work_t* work) {
  abort();
}
