/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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
#include "internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

struct signal_ctx {
  int pipefd[2];
  uv__io_t io_watcher;
  unsigned int nqueues;
  ngx_queue_t queues[1]; /* variable length */
};

static void uv__signal_handler(int signum);
static void uv__signal_event(uv_loop_t* loop, uv__io_t* w, int events);
static struct signal_ctx* uv__signal_ctx_new(uv_loop_t* loop);
static void uv__signal_ctx_delete(struct signal_ctx* ctx);
static void uv__signal_write(int fd, unsigned int val);
static unsigned int uv__signal_read(int fd);
static unsigned int uv__signal_max(void);


int uv_signal_init(uv_loop_t* loop, uv_signal_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_SIGNAL);
  handle->signum = 0;
  return 0;
}


int uv_signal_start(uv_signal_t* handle, uv_signal_cb signal_cb, int signum_) {
  struct signal_ctx* ctx;
  struct sigaction sa;
  unsigned int signum;
  uv_loop_t* loop;
  ngx_queue_t* q;

  /* XXX doing this check in uv_signal_init() - the logical place for it -
   * leads to an infinite loop when uv__loop_init() inits a signal watcher
   */
  /* FIXME */
  assert(handle->loop == uv_default_loop() &&
         "uv_signal_t is currently only supported by the default loop");

  loop = handle->loop;
  signum = signum_;

  if (uv__is_active(handle))
    return uv__set_artificial_error(loop, UV_EBUSY);

  if (signal_cb == NULL)
    return uv__set_artificial_error(loop, UV_EINVAL);

  if (signum <= 0)
    return uv__set_artificial_error(loop, UV_EINVAL);

  ctx = loop->signal_ctx;

  if (ctx == NULL) {
    ctx = uv__signal_ctx_new(loop);

    if (ctx == NULL)
      return uv__set_artificial_error(loop, UV_ENOMEM);

    loop->signal_ctx = ctx;
  }

  if (signum > ctx->nqueues)
    return uv__set_artificial_error(loop, UV_EINVAL);

  q = ctx->queues + signum;

  if (!ngx_queue_empty(q))
    goto skip;

  /* XXX use a separate signal stack? */
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = uv__signal_handler;

  /* XXX save old action so we can restore it later on? */
  if (sigaction(signum, &sa, NULL))
    return uv__set_artificial_error(loop, UV_EINVAL);

skip:
  ngx_queue_insert_tail(q, &handle->queue);
  uv__handle_start(handle);
  handle->signum = signum;
  handle->signal_cb = signal_cb;

  return 0;
}


int uv_signal_stop(uv_signal_t* handle) {
  struct signal_ctx* ctx;
  struct sigaction sa;
  unsigned int signum;
  uv_loop_t* loop;

  if (!uv__is_active(handle))
    return 0;

  signum = handle->signum;
  loop = handle->loop;
  ctx = loop->signal_ctx;
  assert(signum > 0);
  assert(signum <= ctx->nqueues);

  ngx_queue_remove(&handle->queue);
  uv__handle_stop(handle);
  handle->signum = 0;

  if (!ngx_queue_empty(ctx->queues + signum))
    goto skip;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL; /* XXX restore previous action? */

  if (sigaction(signum, &sa, NULL))
    return uv__set_artificial_error(loop, UV_EINVAL);

skip:
  return 0;
}


void uv__signal_close(uv_signal_t* handle) {
  uv_signal_stop(handle);
}


void uv__signal_unregister(uv_loop_t* loop) {
  uv__signal_ctx_delete(loop->signal_ctx);
  loop->signal_ctx = NULL;
}


static void uv__signal_handler(int signum) {
  struct signal_ctx* ctx = uv_default_loop()->signal_ctx;
  uv__signal_write(ctx->pipefd[1], (unsigned int) signum);
}


static void uv__signal_event(uv_loop_t* loop, uv__io_t* w, int events) {
  struct signal_ctx* ctx;
  unsigned int signum;
  uv_signal_t* h;
  ngx_queue_t* q;

  ctx = container_of(w, struct signal_ctx, io_watcher);
  signum = uv__signal_read(ctx->pipefd[0]);
  assert(signum > 0);
  assert(signum <= ctx->nqueues);

  ngx_queue_foreach(q, ctx->queues + signum) {
    h = ngx_queue_data(q, uv_signal_t, queue);
    h->signal_cb(h, signum);
  }
}


static struct signal_ctx* uv__signal_ctx_new(uv_loop_t* loop) {
  struct signal_ctx* ctx;
  unsigned int nqueues;
  unsigned int i;

  nqueues = uv__signal_max();
  assert(nqueues > 0);

  /* The first ctx->queues entry is never used. It wastes a few bytes of memory
   * but it saves us from having to substract 1 from the signum all the time -
   * which inevitably someone will forget to do.
   */
  ctx = calloc(1, sizeof(*ctx) + sizeof(ctx->queues[0]) * (nqueues + 1));
  if (ctx == NULL)
    return NULL;

  if (uv__make_pipe(ctx->pipefd, UV__F_NONBLOCK)) {
    free(ctx);
    return NULL;
  }

  uv__io_init(&ctx->io_watcher, uv__signal_event, ctx->pipefd[0], UV__IO_READ);
  uv__io_start(loop, &ctx->io_watcher);
  ctx->nqueues = nqueues;

  for (i = 1; i <= nqueues; i++)
    ngx_queue_init(ctx->queues + i);

  return ctx;
}


static void uv__signal_ctx_delete(struct signal_ctx* ctx) {
  if (ctx == NULL) return;
  close(ctx->pipefd[0]);
  close(ctx->pipefd[1]);
  free(ctx);
}


static void uv__signal_write(int fd, unsigned int val) {
  ssize_t n;

  do
    n = write(fd, &val, sizeof(val));
  while (n == -1 && errno == EINTR);

  if (n == sizeof(val))
    return;

  if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return; /* pipe full - nothing we can do about that */

  abort();
}


static unsigned int uv__signal_read(int fd) {
  unsigned int val;
  ssize_t n;

  do
    n = read(fd, &val, sizeof(val));
  while (n == -1 && errno == EINTR);

  if (n == sizeof(val))
    return val;

  abort();
}


static unsigned int uv__signal_max(void) {
#if defined(_SC_RTSIG_MAX)
  int max = sysconf(_SC_RTSIG_MAX);
  if (max != -1) return max;
#endif
#if defined(SIGRTMAX)
  return SIGRTMAX;
#elif defined(NSIG)
  return NSIG;
#else
  return 32;
#endif
}
