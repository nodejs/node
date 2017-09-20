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

/* For Windows we test only signum handling */
#ifdef _WIN32
static void signum_test_cb(uv_signal_t* handle, int signum) {
  FATAL("signum_test_cb should not be called");
}

TEST_IMPL(win32_signum_number) {
  uv_signal_t signal;
  uv_loop_t* loop;

  loop = uv_default_loop();
  uv_signal_init(loop, &signal);

  ASSERT(uv_signal_start(&signal, signum_test_cb, 0) == UV_EINVAL);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGINT) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGBREAK) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGHUP) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGWINCH) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGILL) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGABRT_COMPAT) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGFPE) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGSEGV) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGTERM) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, SIGABRT) == 0);
  ASSERT(uv_signal_start(&signal, signum_test_cb, -1) == UV_EINVAL);
  ASSERT(uv_signal_start(&signal, signum_test_cb, NSIG) == UV_EINVAL);
  ASSERT(uv_signal_start(&signal, signum_test_cb, 1024) == UV_EINVAL);
  MAKE_VALGRIND_HAPPY();
  return 0;
}
#else
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
  enum { CLOSE, STOP, NOOP } stop_or_close;
  unsigned int ncalls;
  uv_signal_t handle;
  int signum;
  int one_shot;
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

static void signal_cb_one_shot(uv_signal_t* handle, int signum) {
  struct signal_ctx* ctx = container_of(handle, struct signal_ctx, handle);
  ASSERT(signum == ctx->signum);
  ASSERT(++ctx->ncalls == 1);
  if (ctx->stop_or_close == CLOSE)
    uv_close((uv_handle_t*)handle, NULL);
}


static void timer_cb(uv_timer_t* handle) {
  struct timer_ctx* ctx = container_of(handle, struct timer_ctx, handle);

  raise(ctx->signum);

  if (++ctx->ncalls == NSIGNALS)
    uv_close((uv_handle_t*)handle, NULL);
}


static void start_watcher(uv_loop_t* loop,
                          int signum,
                          struct signal_ctx* ctx,
                          int one_shot) {
  ctx->ncalls = 0;
  ctx->signum = signum;
  ctx->stop_or_close = CLOSE;
  ctx->one_shot = one_shot;
  ASSERT(0 == uv_signal_init(loop, &ctx->handle));
  if (one_shot)
    ASSERT(0 == uv_signal_start_oneshot(&ctx->handle, signal_cb_one_shot, signum));
  else
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
  start_watcher(loop, SIGCHLD, &sc, 0);
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
  start_watcher(loop, SIGUSR1, sc + 0, 0);
  start_watcher(loop, SIGUSR1, sc + 1, 0);
  start_watcher(loop, SIGUSR2, sc + 2, 0);
  start_watcher(loop, SIGUSR2, sc + 3, 0);
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

TEST_IMPL(we_get_signal_one_shot) {
  struct signal_ctx sc;
  struct timer_ctx tc;
  uv_loop_t* loop;

  loop = uv_default_loop();
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, &sc, 1);
  sc.stop_or_close = NOOP;
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc.ncalls == 1);

  start_timer(loop, SIGCHLD, &tc);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(sc.ncalls == 1);

  sc.ncalls = 0;
  sc.stop_or_close = CLOSE; /* now close it when it's done */
  uv_signal_start_oneshot(&sc.handle, signal_cb_one_shot, SIGCHLD);
  start_timer(loop, SIGCHLD, &tc);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc.ncalls == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(we_get_signals_mixed) {
  struct signal_ctx sc[4];
  struct timer_ctx tc;
  uv_loop_t* loop;

  loop = uv_default_loop();

  /* 2 one-shot */
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, sc + 0, 1);
  start_watcher(loop, SIGCHLD, sc + 1, 1);
  sc[0].stop_or_close = CLOSE;
  sc[1].stop_or_close = CLOSE;
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc[0].ncalls == 1);
  ASSERT(sc[1].ncalls == 1);

  /* 2 one-shot, 1 normal then remove normal */
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, sc + 0, 1);
  start_watcher(loop, SIGCHLD, sc + 1, 1);
  sc[0].stop_or_close = CLOSE;
  sc[1].stop_or_close = CLOSE;
  start_watcher(loop, SIGCHLD, sc + 2, 0);
  uv_close((uv_handle_t*)&(sc[2]).handle, NULL);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc[0].ncalls == 1);
  ASSERT(sc[1].ncalls == 1);
  ASSERT(sc[2].ncalls == 0);

  /* 2 normal, 1 one-shot then remove one-shot */
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, sc + 0, 0);
  start_watcher(loop, SIGCHLD, sc + 1, 0);
  sc[0].stop_or_close = CLOSE;
  sc[1].stop_or_close = CLOSE;
  start_watcher(loop, SIGCHLD, sc + 2, 1);
  uv_close((uv_handle_t*)&(sc[2]).handle, NULL);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc[0].ncalls == NSIGNALS);
  ASSERT(sc[1].ncalls == NSIGNALS);
  ASSERT(sc[2].ncalls == 0);

  /* 2 normal, 2 one-shot then remove 2 normal */
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, sc + 0, 0);
  start_watcher(loop, SIGCHLD, sc + 1, 0);
  start_watcher(loop, SIGCHLD, sc + 2, 1);
  start_watcher(loop, SIGCHLD, sc + 3, 1);
  sc[2].stop_or_close = CLOSE;
  sc[3].stop_or_close = CLOSE;
  uv_close((uv_handle_t*)&(sc[0]).handle, NULL);
  uv_close((uv_handle_t*)&(sc[1]).handle, NULL);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc[0].ncalls == 0);
  ASSERT(sc[1].ncalls == 0);
  ASSERT(sc[2].ncalls == 1);
  ASSERT(sc[2].ncalls == 1);

  /* 1 normal, 1 one-shot, 2 normal then remove 1st normal, 2nd normal */
  start_timer(loop, SIGCHLD, &tc);
  start_watcher(loop, SIGCHLD, sc + 0, 0);
  start_watcher(loop, SIGCHLD, sc + 1, 1);
  start_watcher(loop, SIGCHLD, sc + 2, 0);
  start_watcher(loop, SIGCHLD, sc + 3, 0);
  sc[3].stop_or_close = CLOSE;
  uv_close((uv_handle_t*)&(sc[0]).handle, NULL);
  uv_close((uv_handle_t*)&(sc[2]).handle, NULL);
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(tc.ncalls == NSIGNALS);
  ASSERT(sc[0].ncalls == 0);
  ASSERT(sc[1].ncalls == 1);
  ASSERT(sc[2].ncalls == 0);
  ASSERT(sc[3].ncalls == NSIGNALS);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif /* _WIN32 */
