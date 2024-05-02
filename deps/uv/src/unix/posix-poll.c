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

#include "uv.h"
#include "internal.h"

/* POSIX defines poll() as a portable way to wait on file descriptors.
 * Here we maintain a dynamically sized array of file descriptors and
 * events to pass as the first argument to poll().
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

int uv__platform_loop_init(uv_loop_t* loop) {
  loop->poll_fds = NULL;
  loop->poll_fds_used = 0;
  loop->poll_fds_size = 0;
  loop->poll_fds_iterating = 0;
  return 0;
}

void uv__platform_loop_delete(uv_loop_t* loop) {
  uv__free(loop->poll_fds);
  loop->poll_fds = NULL;
}

int uv__io_fork(uv_loop_t* loop) {
  uv__platform_loop_delete(loop);
  return uv__platform_loop_init(loop);
}

/* Allocate or dynamically resize our poll fds array.  */
static void uv__pollfds_maybe_resize(uv_loop_t* loop) {
  size_t i;
  size_t n;
  struct pollfd* p;

  if (loop->poll_fds_used < loop->poll_fds_size)
    return;

  n = loop->poll_fds_size ? loop->poll_fds_size * 2 : 64;
  p = uv__reallocf(loop->poll_fds, n * sizeof(*loop->poll_fds));
  if (p == NULL)
    abort();

  loop->poll_fds = p;
  for (i = loop->poll_fds_size; i < n; i++) {
    loop->poll_fds[i].fd = -1;
    loop->poll_fds[i].events = 0;
    loop->poll_fds[i].revents = 0;
  }
  loop->poll_fds_size = n;
}

/* Primitive swap operation on poll fds array elements.  */
static void uv__pollfds_swap(uv_loop_t* loop, size_t l, size_t r) {
  struct pollfd pfd;
  pfd = loop->poll_fds[l];
  loop->poll_fds[l] = loop->poll_fds[r];
  loop->poll_fds[r] = pfd;
}

/* Add a watcher's fd to our poll fds array with its pending events.  */
static void uv__pollfds_add(uv_loop_t* loop, uv__io_t* w) {
  size_t i;
  struct pollfd* pe;

  /* If the fd is already in the set just update its events.  */
  assert(!loop->poll_fds_iterating);
  for (i = 0; i < loop->poll_fds_used; ++i) {
    if (loop->poll_fds[i].fd == w->fd) {
      loop->poll_fds[i].events = w->pevents;
      return;
    }
  }

  /* Otherwise, allocate a new slot in the set for the fd.  */
  uv__pollfds_maybe_resize(loop);
  pe = &loop->poll_fds[loop->poll_fds_used++];
  pe->fd = w->fd;
  pe->events = w->pevents;
}

/* Remove a watcher's fd from our poll fds array.  */
static void uv__pollfds_del(uv_loop_t* loop, int fd) {
  size_t i;
  assert(!loop->poll_fds_iterating);
  for (i = 0; i < loop->poll_fds_used;) {
    if (loop->poll_fds[i].fd == fd) {
      /* swap to last position and remove */
      --loop->poll_fds_used;
      uv__pollfds_swap(loop, i, loop->poll_fds_used);
      loop->poll_fds[loop->poll_fds_used].fd = -1;
      loop->poll_fds[loop->poll_fds_used].events = 0;
      loop->poll_fds[loop->poll_fds_used].revents = 0;
      /* This method is called with an fd of -1 to purge the invalidated fds,
       * so we may possibly have multiples to remove.
       */
      if (-1 != fd)
        return;
    } else {
      /* We must only increment the loop counter when the fds do not match.
       * Otherwise, when we are purging an invalidated fd, the value just
       * swapped here from the previous end of the array will be skipped.
       */
       ++i;
    }
  }
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  uv__loop_internal_fields_t* lfields;
  sigset_t* pset;
  sigset_t set;
  uint64_t time_base;
  uint64_t time_diff;
  struct uv__queue* q;
  uv__io_t* w;
  size_t i;
  unsigned int nevents;
  int nfds;
  int have_signals;
  struct pollfd* pe;
  int fd;
  int user_timeout;
  int reset_timeout;

  if (loop->nfds == 0) {
    assert(uv__queue_empty(&loop->watcher_queue));
    return;
  }

  lfields = uv__get_internal_fields(loop);

  /* Take queued watchers and add their fds to our poll fds array.  */
  while (!uv__queue_empty(&loop->watcher_queue)) {
    q = uv__queue_head(&loop->watcher_queue);
    uv__queue_remove(q);
    uv__queue_init(q);

    w = uv__queue_data(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    uv__pollfds_add(loop, w);

    w->events = w->pevents;
  }

  /* Prepare a set of signals to block around poll(), if any.  */
  pset = NULL;
  if (loop->flags & UV_LOOP_BLOCK_SIGPROF) {
    pset = &set;
    sigemptyset(pset);
    sigaddset(pset, SIGPROF);
  }

  assert(timeout >= -1);
  time_base = loop->time;

  if (lfields->flags & UV_METRICS_IDLE_TIME) {
    reset_timeout = 1;
    user_timeout = timeout;
    timeout = 0;
  } else {
    reset_timeout = 0;
  }

  /* Loop calls to poll() and processing of results.  If we get some
   * results from poll() but they turn out not to be interesting to
   * our caller then we need to loop around and poll() again.
   */
  for (;;) {
    /* Only need to set the provider_entry_time if timeout != 0. The function
     * will return early if the loop isn't configured with UV_METRICS_IDLE_TIME.
     */
    if (timeout != 0)
      uv__metrics_set_provider_entry_time(loop);

    /* Store the current timeout in a location that's globally accessible so
     * other locations like uv__work_done() can determine whether the queue
     * of events in the callback were waiting when poll was called.
     */
    lfields->current_timeout = timeout;

    if (pset != NULL)
      if (pthread_sigmask(SIG_BLOCK, pset, NULL))
        abort();
    nfds = poll(loop->poll_fds, (nfds_t)loop->poll_fds_used, timeout);
    if (pset != NULL)
      if (pthread_sigmask(SIG_UNBLOCK, pset, NULL))
        abort();

    /* Update loop->time unconditionally. It's tempting to skip the update when
     * timeout == 0 (i.e. non-blocking poll) but there is no guarantee that the
     * operating system didn't reschedule our process while in the syscall.
     */
    SAVE_ERRNO(uv__update_time(loop));

    if (nfds == 0) {
      if (reset_timeout != 0) {
        timeout = user_timeout;
        reset_timeout = 0;
        if (timeout == -1)
          continue;
        if (timeout > 0)
          goto update_timeout;
      }

      assert(timeout != -1);
      return;
    }

    if (nfds == -1) {
      if (errno != EINTR)
        abort();

      if (reset_timeout != 0) {
        timeout = user_timeout;
        reset_timeout = 0;
      }

      if (timeout == -1)
        continue;

      if (timeout == 0)
        return;

      /* Interrupted by a signal. Update timeout and poll again. */
      goto update_timeout;
    }

    /* Tell uv__platform_invalidate_fd not to manipulate our array
     * while we are iterating over it.
     */
    loop->poll_fds_iterating = 1;

    /* Initialize a count of events that we care about.  */
    nevents = 0;
    have_signals = 0;

    /* Loop over the entire poll fds array looking for returned events.  */
    for (i = 0; i < loop->poll_fds_used; i++) {
      pe = loop->poll_fds + i;
      fd = pe->fd;

      /* Skip invalidated events, see uv__platform_invalidate_fd.  */
      if (fd == -1)
        continue;

      assert(fd >= 0);
      assert((unsigned) fd < loop->nwatchers);

      w = loop->watchers[fd];

      if (w == NULL) {
        /* File descriptor that we've stopped watching, ignore.  */
        uv__platform_invalidate_fd(loop, fd);
        continue;
      }

      /* Filter out events that user has not requested us to watch
       * (e.g. POLLNVAL).
       */
      pe->revents &= w->pevents | POLLERR | POLLHUP;

      if (pe->revents != 0) {
        /* Run signal watchers last.  */
        if (w == &loop->signal_io_watcher) {
          have_signals = 1;
        } else {
          uv__metrics_update_idle_time(loop);
          w->cb(loop, w, pe->revents);
        }

        nevents++;
      }
    }

    uv__metrics_inc_events(loop, nevents);
    if (reset_timeout != 0) {
      timeout = user_timeout;
      reset_timeout = 0;
      uv__metrics_inc_events_waiting(loop, nevents);
    }

    if (have_signals != 0) {
      uv__metrics_update_idle_time(loop);
      loop->signal_io_watcher.cb(loop, &loop->signal_io_watcher, POLLIN);
    }

    loop->poll_fds_iterating = 0;

    /* Purge invalidated fds from our poll fds array.  */
    uv__pollfds_del(loop, -1);

    if (have_signals != 0)
      return;  /* Event loop should cycle now so don't poll again. */

    if (nevents != 0)
      return;

    if (timeout == 0)
      return;

    if (timeout == -1)
      continue;

update_timeout:
    assert(timeout > 0);

    time_diff = loop->time - time_base;
    if (time_diff >= (uint64_t) timeout)
      return;

    timeout -= time_diff;
  }
}

/* Remove the given fd from our poll fds array because no one
 * is interested in its events anymore.
 */
void uv__platform_invalidate_fd(uv_loop_t* loop, int fd) {
  size_t i;

  assert(fd >= 0);

  if (loop->poll_fds_iterating) {
    /* uv__io_poll is currently iterating.  Just invalidate fd.  */
    for (i = 0; i < loop->poll_fds_used; i++)
      if (loop->poll_fds[i].fd == fd) {
        loop->poll_fds[i].fd = -1;
        loop->poll_fds[i].events = 0;
        loop->poll_fds[i].revents = 0;
      }
  } else {
    /* uv__io_poll is not iterating.  Delete fd from the set.  */
    uv__pollfds_del(loop, fd);
  }
}

/* Check whether the given fd is supported by poll().  */
int uv__io_check_fd(uv_loop_t* loop, int fd) {
  struct pollfd p[1];
  int rv;

  p[0].fd = fd;
  p[0].events = POLLIN;

  do
    rv = poll(p, 1, 0);
  while (rv == -1 && (errno == EINTR || errno == EAGAIN));

  if (rv == -1)
    return UV__ERR(errno);

  if (p[0].revents & POLLNVAL)
    return UV_EINVAL;

  return 0;
}
