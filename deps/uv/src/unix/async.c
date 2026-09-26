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

/* This file contains both the uv__async internal infrastructure and the
 * user-facing uv_async_t functions.
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>  /* snprintf() */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/eventfd.h>
#endif

#if UV__KQUEUE_EVFILT_USER
static uv_once_t kqueue_runtime_detection_guard = UV_ONCE_INIT;
static int kqueue_evfilt_user_support = 1;


static void uv__kqueue_runtime_detection(void) {
  int kq;
  struct kevent ev[2];
  struct timespec timeout = {0, 0};

  /* Perform the runtime detection to ensure that kqueue with
   * EVFILT_USER actually works. */
  kq = kqueue();
  EV_SET(ev, UV__KQUEUE_EVFILT_USER_IDENT, EVFILT_USER,
         EV_ADD | EV_CLEAR, 0, 0, 0);
  EV_SET(ev + 1, UV__KQUEUE_EVFILT_USER_IDENT, EVFILT_USER,
         0, NOTE_TRIGGER, 0, 0);
  if (kevent(kq, ev, 2, ev, 1, &timeout) < 1 ||
      ev[0].filter != EVFILT_USER ||
      ev[0].ident != UV__KQUEUE_EVFILT_USER_IDENT ||
      ev[0].flags & EV_ERROR)
    /* If we wind up here, we can assume that EVFILT_USER is defined but
     * broken on the current system. */
    kqueue_evfilt_user_support = 0;
  uv__close(kq);
}
#endif

static void uv__async_send(uv_loop_t* loop);
static int uv__async_start(uv_loop_t* loop);


int uv_async_init(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb) {
  int err;

  err = uv__async_start(loop);
  if (err)
    return err;

  uv__handle_init(loop, (uv_handle_t*)handle, UV_ASYNC);
  handle->async_cb = async_cb;
  handle->pending = 0;

  uv__queue_insert_tail(&loop->async_handles, &handle->queue);
  uv__handle_start(handle);

  return 0;
}


void uv__async_notify(uv_async_t* handle) {
  uv__async_send(handle->loop);
}


void uv__async_close(uv_async_t* handle) {
  uv__async_spin(handle);
  uv__queue_remove(&handle->queue);
  uv__handle_stop(handle);
}


void uv__async_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
#ifndef __linux__
  char buf[1024];
  ssize_t r;
#endif
  struct uv__queue queue;
  struct uv__queue* q;
  uv_async_t* h;
  _Atomic int *pending;

  assert(w == &loop->async_io_watcher);

#ifndef __linux__
#if UV__KQUEUE_EVFILT_USER
  for (;!kqueue_evfilt_user_support;) {
#else
  for (;;) {
#endif
    r = read(w->fd, buf, sizeof(buf));

    if (r == sizeof(buf))
      continue;

    if (r != -1)
      break;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
      break;

    if (errno == EINTR)
      continue;

    abort();
  }
#endif /* !__linux__ */

  uv__queue_move(&loop->async_handles, &queue);
  while (!uv__queue_empty(&queue)) {
    q = uv__queue_head(&queue);
    h = uv__queue_data(q, uv_async_t, queue);

    uv__queue_remove(q);
    uv__queue_insert_tail(&loop->async_handles, q);

    /* Atomically clear the pending flag (bit 0) and check if it was set.
     * The seq_cst (default order) synchronizes with the seq_cst CAS in
     * uv_async_send, making all accesses before that call visible here and
     * ensuring no access here can get reordered before this as visible to
     * another thread. */
    pending = (_Atomic int*) &h->pending;
    if (!(atomic_fetch_and(pending, ~1) & 1))
      continue;

    if (h->async_cb == NULL)
      continue;

    h->async_cb(h);
  }
}


static void uv__async_send(uv_loop_t* loop) {
  int fd;
  int r;
#if !(defined(__linux__) || UV__KQUEUE_EVFILT_USER)
  static char buf = '\0';
#endif

#if defined(__linux__)
  uint64_t val;

  fd = loop->async_io_watcher.fd;  /* eventfd */
  for (val = 1; /* empty */; val = 1) {
    r = write(fd, &val, sizeof(uint64_t));
    if (r < 0) {
      /* When EAGAIN occurs, the eventfd counter hits the maximum value of the unsigned 64-bit.
       * We need to first drain the eventfd and then write again.
       *
       * Check out https://man7.org/linux/man-pages/man2/eventfd.2.html for details.
       */
      if (errno == EAGAIN) {
        /* It's ready to retry. */
        if (read(fd, &val, sizeof(uint64_t)) > 0 || errno == EAGAIN) {
          continue;
        }
      }
      /* Unknown error occurs. */
      break;
    }
    return;
  }

#elif UV__KQUEUE_EVFILT_USER
  struct kevent ev;

  if (kqueue_evfilt_user_support) {
    fd = loop->async_io_watcher.fd; /* magic number for EVFILT_USER */
    EV_SET(&ev, fd, EVFILT_USER, 0, NOTE_TRIGGER, 0, 0);
    r = kevent(loop->backend_fd, &ev, 1, NULL, 0, NULL);
    if (r == 0)
      return;
    abort();
  }

#else
  fd = loop->async_wfd;
  do
    r = write(fd, &buf, 1);
  while (r == -1 && errno == EINTR);

  if (r == 1)
    return;

  if (r == -1)
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;
#endif

  abort();
}


static int uv__async_start(uv_loop_t* loop) {
  int pipefd[2];
  int err;
#if UV__KQUEUE_EVFILT_USER
  struct kevent ev;
#endif

  if (loop->async_io_watcher.fd != -1)
    return 0;

#ifdef __linux__
  err = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (err < 0)
    return UV__ERR(errno);

  pipefd[0] = err;
  pipefd[1] = -1;
#elif UV__KQUEUE_EVFILT_USER
  uv_once(&kqueue_runtime_detection_guard, uv__kqueue_runtime_detection);
  if (kqueue_evfilt_user_support) {
    /* In order not to break the generic pattern of I/O polling, a valid
     * file descriptor is required to take up a room in loop->watchers,
     * thus we create one for that, but this fd will not be actually used,
     * it's just a placeholder and magic number which is going to be closed
     * during the cleanup, as other FDs. */
    err = uv__open_cloexec("/", O_RDONLY);
    if (err < 0)
      return err;

    pipefd[0] = err;
    pipefd[1] = -1;

    /* When using EVFILT_USER event to wake up the kqueue, this event must be
     * registered beforehand. Otherwise, calling kevent() to issue an
     * unregistered EVFILT_USER event will get an ENOENT.
     * Since uv__async_send() may happen before uv__io_poll() with multi-threads,
     * we can't defer this registration of EVFILT_USER event as we did for other
     * events, but must perform it right away. */
    EV_SET(&ev, err, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, 0);
    err = kevent(loop->backend_fd, &ev, 1, NULL, 0, NULL);
    if (err < 0)
      return UV__ERR(errno);
  } else {
    err = uv__make_pipe(pipefd, UV_NONBLOCK_PIPE);
    if (err < 0)
      return err;
  }
#else
  err = uv__make_pipe(pipefd, UV_NONBLOCK_PIPE);
  if (err < 0)
    return err;
#endif

  err = uv__io_init_start(loop, &loop->async_io_watcher, UV__ASYNC_IO,
                          pipefd[0], POLLIN);
  if (err < 0) {
    uv__close(pipefd[0]);
    if (pipefd[1] != -1)
      uv__close(pipefd[1]);
    return err;
  }
  loop->async_wfd = pipefd[1];

#if UV__KQUEUE_EVFILT_USER
  /* Prevent the EVFILT_USER event from being added to kqueue redundantly
   * and mistakenly later in uv__io_poll(). */
  if (kqueue_evfilt_user_support)
    loop->async_io_watcher.events = loop->async_io_watcher.pevents;
#endif

  return 0;
}


void uv__async_stop(uv_loop_t* loop) {
  struct uv__queue queue;
  struct uv__queue* q;
  uv_async_t* h;

  if (loop->async_io_watcher.fd == -1)
    return;

  /* Make sure no other thread is accessing the async handle fd after the loop
   * cleanup.
   */
  uv__queue_move(&loop->async_handles, &queue);
  while (!uv__queue_empty(&queue)) {
    q = uv__queue_head(&queue);
    h = uv__queue_data(q, uv_async_t, queue);

    uv__queue_remove(q);
    uv__queue_insert_tail(&loop->async_handles, q);

    uv__async_spin(h);
  }

  if (loop->async_wfd != -1) {
    if (loop->async_wfd != loop->async_io_watcher.fd)
      uv__close(loop->async_wfd);
    loop->async_wfd = -1;
  }

  uv__io_stop(loop, &loop->async_io_watcher, POLLIN);
  uv__close(loop->async_io_watcher.fd);
  loop->async_io_watcher.fd = -1;
}


int uv__async_fork(uv_loop_t* loop) {
  struct uv__queue queue;
  struct uv__queue* q;
  uv_async_t* h;

  if (loop->async_io_watcher.fd == -1) /* never started */
    return 0;

  uv__queue_move(&loop->async_handles, &queue);
  while (!uv__queue_empty(&queue)) {
    q = uv__queue_head(&queue);
    h = uv__queue_data(q, uv_async_t, queue);

    uv__queue_remove(q);
    uv__queue_insert_tail(&loop->async_handles, q);

    /* The state of any thread that set pending is now likely corrupt in this
     * child because the user called fork, so just clear these flags and move
     * on. Calling most libc functions after `fork` is declared to be undefined
     * behavior anyways, unless async-signal-safe, for multithreaded programs
     * like libuv, and nothing interesting in pthreads is async-signal-safe.
     */
    h->pending = 0; /* Clears both the pending flag and busy counter. */
  }

  /* Recreate these, since they still exist, but belong to the wrong pid now. */
  if (loop->async_wfd != -1) {
    if (loop->async_wfd != loop->async_io_watcher.fd)
      uv__close(loop->async_wfd);
    loop->async_wfd = -1;
  }

  uv__io_stop(loop, &loop->async_io_watcher, POLLIN);
  uv__close(loop->async_io_watcher.fd);
  loop->async_io_watcher.fd = -1;

  return uv__async_start(loop);
}
