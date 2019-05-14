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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifndef HAVE_KQUEUE
# if defined(__APPLE__) ||                                                    \
     defined(__DragonFly__) ||                                                \
     defined(__FreeBSD__) ||                                                  \
     defined(__FreeBSD_kernel__) ||                                           \
     defined(__OpenBSD__) ||                                                  \
     defined(__NetBSD__)
#  define HAVE_KQUEUE 1
# endif
#endif

#ifndef HAVE_EPOLL
# if defined(__linux__)
#  define HAVE_EPOLL 1
# endif
#endif

#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)

#if defined(HAVE_KQUEUE)
# include <sys/types.h>
# include <sys/event.h>
# include <sys/time.h>
#endif

#if defined(HAVE_EPOLL)
# include <sys/epoll.h>
#endif

static uv_thread_t embed_thread;
static uv_sem_t embed_sem;
static uv_timer_t embed_timer;
static uv_async_t embed_async;
static volatile int embed_closed;

static int embed_timer_called;


static void embed_thread_runner(void* arg) {
  int r;
  int fd;
  int timeout;

  while (!embed_closed) {
    fd = uv_backend_fd(uv_default_loop());
    timeout = uv_backend_timeout(uv_default_loop());

    do {
#if defined(HAVE_KQUEUE)
      struct timespec ts;
      ts.tv_sec = timeout / 1000;
      ts.tv_nsec = (timeout % 1000) * 1000000;
      r = kevent(fd, NULL, 0, NULL, 0, &ts);
#elif defined(HAVE_EPOLL)
      {
        struct epoll_event ev;
        r = epoll_wait(fd, &ev, 1, timeout);
      }
#endif
    } while (r == -1 && errno == EINTR);
    uv_async_send(&embed_async);
    uv_sem_wait(&embed_sem);
  }
}


static void embed_cb(uv_async_t* async) {
  uv_run(uv_default_loop(), UV_RUN_ONCE);

  uv_sem_post(&embed_sem);
}


static void embed_timer_cb(uv_timer_t* timer) {
  embed_timer_called++;
  embed_closed = 1;

  uv_close((uv_handle_t*) &embed_async, NULL);
}
#endif


TEST_IMPL(embed) {
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
  uv_loop_t external;

  ASSERT(0 == uv_loop_init(&external));

  embed_timer_called = 0;
  embed_closed = 0;

  uv_async_init(&external, &embed_async, embed_cb);

  /* Start timer in default loop */
  uv_timer_init(uv_default_loop(), &embed_timer);
  uv_timer_start(&embed_timer, embed_timer_cb, 250, 0);

  /* Start worker that will interrupt external loop */
  uv_sem_init(&embed_sem, 0);
  uv_thread_create(&embed_thread, embed_thread_runner, NULL);

  /* But run external loop */
  uv_run(&external, UV_RUN_DEFAULT);

  uv_thread_join(&embed_thread);
  uv_loop_close(&external);

  ASSERT(embed_timer_called == 1);
#endif

  return 0;
}
