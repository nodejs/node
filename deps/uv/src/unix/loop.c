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
#include "tree.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int uv__loop_init(uv_loop_t* loop, int default_loop);
static void uv__loop_delete(uv_loop_t* loop);

static uv_loop_t default_loop_struct;
static uv_loop_t* default_loop_ptr;


uv_loop_t* uv_default_loop(void) {
  if (default_loop_ptr != NULL)
    return default_loop_ptr;

  if (uv__loop_init(&default_loop_struct, /* default_loop? */ 1))
    return NULL;

  default_loop_ptr = &default_loop_struct;
  return default_loop_ptr;
}


uv_loop_t* uv_loop_new(void) {
  uv_loop_t* loop;

  loop = malloc(sizeof(*loop));
  if (loop == NULL)
    return NULL;

  if (uv__loop_init(loop, /* default_loop? */ 0)) {
    free(loop);
    return NULL;
  }

  return loop;
}


void uv_loop_delete(uv_loop_t* loop) {
  uv__loop_delete(loop);
#ifndef NDEBUG
  memset(loop, -1, sizeof(*loop));
#endif
  if (loop == default_loop_ptr)
    default_loop_ptr = NULL;
  else
    free(loop);
}


static int uv__loop_init(uv_loop_t* loop, int default_loop) {
  unsigned int i;
  int err;

  uv__signal_global_once_init();

  memset(loop, 0, sizeof(*loop));
  RB_INIT(&loop->timer_handles);
  QUEUE_INIT(&loop->wq);
  QUEUE_INIT(&loop->active_reqs);
  QUEUE_INIT(&loop->idle_handles);
  QUEUE_INIT(&loop->async_handles);
  QUEUE_INIT(&loop->check_handles);
  QUEUE_INIT(&loop->prepare_handles);
  QUEUE_INIT(&loop->handle_queue);

  loop->nfds = 0;
  loop->watchers = NULL;
  loop->nwatchers = 0;
  QUEUE_INIT(&loop->pending_queue);
  QUEUE_INIT(&loop->watcher_queue);

  loop->closing_handles = NULL;
  uv__update_time(loop);
  uv__async_init(&loop->async_watcher);
  loop->signal_pipefd[0] = -1;
  loop->signal_pipefd[1] = -1;
  loop->backend_fd = -1;
  loop->emfile_fd = -1;

  loop->timer_counter = 0;
  loop->stop_flag = 0;

  err = uv__platform_loop_init(loop, default_loop);
  if (err)
    return err;

  uv_signal_init(loop, &loop->child_watcher);
  uv__handle_unref(&loop->child_watcher);
  loop->child_watcher.flags |= UV__HANDLE_INTERNAL;

  for (i = 0; i < ARRAY_SIZE(loop->process_handles); i++)
    QUEUE_INIT(loop->process_handles + i);

  if (uv_mutex_init(&loop->wq_mutex))
    abort();

  if (uv_async_init(loop, &loop->wq_async, uv__work_done))
    abort();

  uv__handle_unref(&loop->wq_async);
  loop->wq_async.flags |= UV__HANDLE_INTERNAL;

  return 0;
}


static void uv__loop_delete(uv_loop_t* loop) {
  uv__signal_loop_cleanup(loop);
  uv__platform_loop_delete(loop);
  uv__async_stop(loop, &loop->async_watcher);

  if (loop->emfile_fd != -1) {
    uv__close(loop->emfile_fd);
    loop->emfile_fd = -1;
  }

  if (loop->backend_fd != -1) {
    uv__close(loop->backend_fd);
    loop->backend_fd = -1;
  }

  uv_mutex_lock(&loop->wq_mutex);
  assert(QUEUE_EMPTY(&loop->wq) && "thread pool work queue not empty!");
  assert(!uv__has_active_reqs(loop));
  uv_mutex_unlock(&loop->wq_mutex);
  uv_mutex_destroy(&loop->wq_mutex);

#if 0
  assert(QUEUE_EMPTY(&loop->pending_queue));
  assert(QUEUE_EMPTY(&loop->watcher_queue));
  assert(loop->nfds == 0);
#endif

  free(loop->watchers);
  loop->watchers = NULL;
  loop->nwatchers = 0;
}
