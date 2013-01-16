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


/* This test does not pretend to be cross-platform. */
#ifndef _WIN32

#include "uv.h"
#include "task.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NSIGNALS  10

struct timer_ctx {
  unsigned int ncalls;
  uv_timer_t handle;
  int signum;
};

struct signal_ctx {
  enum { CLOSE, STOP } stop_or_close;
  unsigned int ncalls;
  uv_signal_t handle;
  int signum;
};


static void signal_cb(uv_signal_t* handle, int signum) {
  struct signal_ctx* ctx = container_of(handle, struct signal_ctx, handle);
  ASSERT(signum == ctx->signum);

  if (++ctx->ncalls == NSIGNALS) {
    if (ctx->stop_or_close == STOP)
      uv_signal_stop(handle);
    else if (ctx->stop_or_close == CLOSE)
      uv_close((uv_handle_t*)handle, NULL);
    else
      ASSERT(0);
  }
}


static void timer_cb(uv_timer_t* handle, int status) {
  struct timer_ctx* ctx = container_of(handle, struct timer_ctx, handle);

  raise(ctx->signum);

  if (++ctx->ncalls == NSIGNALS)
    uv_close((uv_handle_t*)handle, NULL);
}


static void start_watcher(uv_loop_t* loop, int signum, struct signal_ctx* ctx) {
  ctx->ncalls = 0;
  ctx->signum = signum;
  ctx->stop_or_close = CLOSE;
  ASSERT(0 == uv_signal_init(loop, &ctx->handle));
  ASSERT(0 == uv_signal_start(&ctx->handle, signal_cb, signum));
}


static void start_timer(uv_loop_t* loop, int signum, struct timer_ctx* ctx) {
  ctx->ncalls = 0;
  ctx->signum = signum;
  ASSERT(0 == uv_timer_init(loop, &ctx->handle));
  ASSERT(0 == uv_timer_start(&ctx->handle, timer_cb, 5, 5));
}


TEST_IMPL(we_get_signal) {
  struct signal_ctx sc;
  struct timer_ctx tc;
  uv_loop_t* loop;

  loop = uv_default_loop();
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, &sc);
  sc.stop_or_close = STOP; /* stop, don't close the signal handle */
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc.ncalls == NSIGNALS);

  start_timer(loop, SIGCHLD, &tc);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc.ncalls == NSIGNALS);

  sc.ncalls = 0;
  sc.stop_or_close = CLOSE; /* now close it when it's done */
  uv_signal_start(&sc.handle, signal_cb, SIGCHLD);

  start_timer(loop, SIGCHLD, &tc);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc.ncalls == NSIGNALS);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(we_get_signals) {
  struct signal_ctx sc[4];
  struct timer_ctx tc[2];
  uv_loop_t* loop;
  unsigned int i;

  loop = uv_default_loop();
  start_watcher(loop, SIGUSR1, sc + 0);
  start_watcher(loop, SIGUSR1, sc + 1);
  start_watcher(loop, SIGUSR2, sc + 2);
  start_watcher(loop, SIGUSR2, sc + 3);
  start_timer(loop, SIGUSR1, tc + 0);
  start_timer(loop, SIGUSR2, tc + 1);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  for (i = 0; i < ARRAY_SIZE(sc); i++)
    ASSERT(sc[i].ncalls == NSIGNALS);

  for (i = 0; i < ARRAY_SIZE(tc); i++)
    ASSERT(tc[i].ncalls == NSIGNALS);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif /* _WIN32 */
