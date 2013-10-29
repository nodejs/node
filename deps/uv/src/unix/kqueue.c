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
#include <errno.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

static void uv__fs_event(uv_loop_t* loop, uv__io_t* w, unsigned int fflags);


int uv__kqueue_init(uv_loop_t* loop) {
  loop->backend_fd = kqueue();
  if (loop->backend_fd == -1)
    return -errno;

  uv__cloexec(loop->backend_fd, 1);

  return 0;
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  struct kevent events[1024];
  struct kevent* ev;
  struct timespec spec;
  unsigned int nevents;
  unsigned int revents;
  QUEUE* q;
  uint64_t base;
  uint64_t diff;
  uv__io_t* w;
  int filter;
  int fflags;
  int count;
  int nfds;
  int fd;
  int op;
  int i;

  if (loop->nfds == 0) {
    assert(QUEUE_EMPTY(&loop->watcher_queue));
    return;
  }

  nevents = 0;

  while (!QUEUE_EMPTY(&loop->watcher_queue)) {
    q = QUEUE_HEAD(&loop->watcher_queue);
    QUEUE_REMOVE(q);
    QUEUE_INIT(q);

    w = QUEUE_DATA(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    if ((w->events & UV__POLLIN) == 0 && (w->pevents & UV__POLLIN) != 0) {
      filter = EVFILT_READ;
      fflags = 0;
      op = EV_ADD;

      if (w->cb == uv__fs_event) {
        filter = EVFILT_VNODE;
        fflags = NOTE_ATTRIB | NOTE_WRITE  | NOTE_RENAME
               | NOTE_DELETE | NOTE_EXTEND | NOTE_REVOKE;
        op = EV_ADD | EV_ONESHOT; /* Stop the event from firing repeatedly. */
      }

      EV_SET(events + nevents, w->fd, filter, op, fflags, 0, 0);

      if (++nevents == ARRAY_SIZE(events)) {
        if (kevent(loop->backend_fd, events, nevents, NULL, 0, NULL))
          abort();
        nevents = 0;
      }
    }

    if ((w->events & UV__POLLOUT) == 0 && (w->pevents & UV__POLLOUT) != 0) {
      EV_SET(events + nevents, w->fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);

      if (++nevents == ARRAY_SIZE(events)) {
        if (kevent(loop->backend_fd, events, nevents, NULL, 0, NULL))
          abort();
        nevents = 0;
      }
    }

    w->events = w->pevents;
  }

  assert(timeout >= -1);
  base = loop->time;
  count = 48; /* Benchmarks suggest this gives the best throughput. */

  for (;; nevents = 0) {
    if (timeout != -1) {
      spec.tv_sec = timeout / 1000;
      spec.tv_nsec = (timeout % 1000) * 1000000;
    }

    nfds = kevent(loop->backend_fd,
                  events,
                  nevents,
                  events,
                  ARRAY_SIZE(events),
                  timeout == -1 ? NULL : &spec);

    /* Update loop->time unconditionally. It's tempting to skip the update when
     * timeout == 0 (i.e. non-blocking poll) but there is no guarantee that the
     * operating system didn't reschedule our process while in the syscall.
     */
    SAVE_ERRNO(uv__update_time(loop));

    if (nfds == 0) {
      assert(timeout != -1);
      return;
    }

    if (nfds == -1) {
      if (errno != EINTR)
        abort();

      if (timeout == 0)
        return;

      if (timeout == -1)
        continue;

      /* Interrupted by a signal. Update timeout and poll again. */
      goto update_timeout;
    }

    nevents = 0;

    for (i = 0; i < nfds; i++) {
      ev = events + i;
      fd = ev->ident;
      w = loop->watchers[fd];

      if (w == NULL) {
        /* File descriptor that we've stopped watching, disarm it. */
        /* TODO batch up */
        struct kevent events[1];

        EV_SET(events + 0, fd, ev->filter, EV_DELETE, 0, 0, 0);
        if (kevent(loop->backend_fd, events, 1, NULL, 0, NULL))
          if (errno != EBADF && errno != ENOENT)
            abort();

        continue;
      }

      if (ev->filter == EVFILT_VNODE) {
        assert(w->events == UV__POLLIN);
        assert(w->pevents == UV__POLLIN);
        w->cb(loop, w, ev->fflags); /* XXX always uv__fs_event() */
        nevents++;
        continue;
      }

      revents = 0;

      if (ev->filter == EVFILT_READ) {
        if (w->events & UV__POLLIN) {
          revents |= UV__POLLIN;
          w->rcount = ev->data;
        } else {
          /* TODO batch up */
          struct kevent events[1];
          EV_SET(events + 0, fd, ev->filter, EV_DELETE, 0, 0, 0);
          if (kevent(loop->backend_fd, events, 1, NULL, 0, NULL))
            if (errno != ENOENT)
              abort();
        }
      }

      if (ev->filter == EVFILT_WRITE) {
        if (w->events & UV__POLLOUT) {
          revents |= UV__POLLOUT;
          w->wcount = ev->data;
        } else {
          /* TODO batch up */
          struct kevent events[1];
          EV_SET(events + 0, fd, ev->filter, EV_DELETE, 0, 0, 0);
          if (kevent(loop->backend_fd, events, 1, NULL, 0, NULL))
            if (errno != ENOENT)
              abort();
        }
      }

      if (ev->flags & EV_ERROR)
        revents |= UV__POLLERR;

      if (revents == 0)
        continue;

      w->cb(loop, w, revents);
      nevents++;
    }

    if (nevents != 0) {
      if (nfds == ARRAY_SIZE(events) && --count != 0) {
        /* Poll for more events but don't block this time. */
        timeout = 0;
        continue;
      }
      return;
    }

    if (timeout == 0)
      return;

    if (timeout == -1)
      continue;

update_timeout:
    assert(timeout > 0);

    diff = loop->time - base;
    if (diff >= (uint64_t) timeout)
      return;

    timeout -= diff;
  }
}


static void uv__fs_event(uv_loop_t* loop, uv__io_t* w, unsigned int fflags) {
  uv_fs_event_t* handle;
  struct kevent ev;
  int events;
  const char* path;
#if defined(F_GETPATH)
  /* MAXPATHLEN == PATH_MAX but the former is what XNU calls it internally. */
  char pathbuf[MAXPATHLEN];
#endif

  handle = container_of(w, uv_fs_event_t, event_watcher);

  if (fflags & (NOTE_ATTRIB | NOTE_EXTEND))
    events = UV_CHANGE;
  else
    events = UV_RENAME;

  path = NULL;
#if defined(F_GETPATH)
  /* Also works when the file has been unlinked from the file system. Passing
   * in the path when the file has been deleted is arguably a little strange
   * but it's consistent with what the inotify backend does.
   */
  if (fcntl(handle->event_watcher.fd, F_GETPATH, pathbuf) == 0)
    path = uv__basename_r(pathbuf);
#endif
  handle->cb(handle, path, events, 0);

  if (handle->event_watcher.fd == -1)
    return;

  /* Watcher operates in one-shot mode, re-arm it. */
  fflags = NOTE_ATTRIB | NOTE_WRITE  | NOTE_RENAME
         | NOTE_DELETE | NOTE_EXTEND | NOTE_REVOKE;

  EV_SET(&ev, w->fd, EVFILT_VNODE, EV_ADD | EV_ONESHOT, fflags, 0, 0);

  if (kevent(loop->backend_fd, &ev, 1, NULL, 0, NULL))
    abort();
}


int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  return 0;
}


int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* filename,
                      unsigned int flags) {
#if defined(__APPLE__)
  struct stat statbuf;
#endif /* defined(__APPLE__) */
  int fd;

  if (uv__is_active(handle))
    return -EINVAL;

  /* TODO open asynchronously - but how do we report back errors? */
  fd = open(filename, O_RDONLY);
  if (fd == -1)
    return -errno;

  uv__handle_start(handle);
  uv__io_init(&handle->event_watcher, uv__fs_event, fd);
  handle->filename = strdup(filename);
  handle->cb = cb;

#if defined(__APPLE__)
  /* Nullify field to perform checks later */
  handle->cf_cb = NULL;
  handle->realpath = NULL;
  handle->realpath_len = 0;
  handle->cf_flags = flags;

  if (fstat(fd, &statbuf))
    goto fallback;
  /* FSEvents works only with directories */
  if (!(statbuf.st_mode & S_IFDIR))
    goto fallback;

  return uv__fsevents_init(handle);

fallback:
#endif /* defined(__APPLE__) */

  uv__io_start(handle->loop, &handle->event_watcher, UV__POLLIN);

  return 0;
}


int uv_fs_event_stop(uv_fs_event_t* handle) {
  if (!uv__is_active(handle))
    return -EINVAL;

  uv__handle_stop(handle);

#if defined(__APPLE__)
  if (uv__fsevents_close(handle))
    uv__io_stop(handle->loop, &handle->event_watcher, UV__POLLIN);
#else
  uv__io_stop(handle->loop, &handle->event_watcher, UV__POLLIN);
#endif /* defined(__APPLE__) */

  free(handle->filename);
  handle->filename = NULL;

  uv__close(handle->event_watcher.fd);
  handle->event_watcher.fd = -1;

  return 0;
}


void uv__fs_event_close(uv_fs_event_t* handle) {
  uv_fs_event_stop(handle);
}
