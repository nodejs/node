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
#include "tree.h"
#include "../internal.h"
#include "syscalls.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>


/* Don't look aghast, this is exactly how glibc's basename() works. */
static char* basename_r(const char* path) {
  char* s = strrchr(path, '/');
  return s ? (s + 1) : (char*)path;
}


static int compare_watchers(const uv_fs_event_t* a, const uv_fs_event_t* b) {
  if (a->fd < b->fd) return -1;
  if (a->fd > b->fd) return 1;
  return 0;
}


RB_GENERATE_STATIC(uv__inotify_watchers, uv_fs_event_s, node, compare_watchers)


static void uv__inotify_read(EV_P_ ev_io* w, int revents);


static int new_inotify_fd(void) {
  int fd;

  fd = uv__inotify_init1(UV__IN_NONBLOCK | UV__IN_CLOEXEC);
  if (fd != -1)
    return fd;
  if (errno != ENOSYS)
    return -1;

  if ((fd = uv__inotify_init()) == -1)
    return -1;

  if (uv__cloexec(fd, 1) || uv__nonblock(fd, 1)) {
    SAVE_ERRNO(close(fd));
    return -1;
  }

  return fd;
}


static int init_inotify(uv_loop_t* loop) {
  if (loop->inotify_fd != -1)
    return 0;

  loop->inotify_fd = new_inotify_fd();
  if (loop->inotify_fd == -1) {
    uv__set_sys_error(loop, errno);
    return -1;
  }

  ev_io_init(&loop->inotify_read_watcher,
             uv__inotify_read,
             loop->inotify_fd,
             EV_READ);
  ev_io_start(loop->ev, &loop->inotify_read_watcher);
  ev_unref(loop->ev);

  return 0;
}


static void add_watcher(uv_fs_event_t* handle) {
  RB_INSERT(uv__inotify_watchers, &handle->loop->inotify_watchers, handle);
}


static uv_fs_event_t* find_watcher(uv_loop_t* loop, int wd) {
  uv_fs_event_t handle;
  handle.fd = wd;
  return RB_FIND(uv__inotify_watchers, &loop->inotify_watchers, &handle);
}


static void remove_watcher(uv_fs_event_t* handle) {
  RB_REMOVE(uv__inotify_watchers, &handle->loop->inotify_watchers, handle);
}


static void uv__inotify_read(EV_P_ ev_io* w, int revents) {
  const struct uv__inotify_event* e;
  uv_fs_event_t* handle;
  uv_loop_t* uv_loop;
  const char* filename;
  ssize_t size;
  int events;
  const char *p;
  /* needs to be large enough for sizeof(inotify_event) + strlen(filename) */
  char buf[4096];

  uv_loop = container_of(w, uv_loop_t, inotify_read_watcher);

  while (1) {
    do {
      size = read(uv_loop->inotify_fd, buf, sizeof buf);
    }
    while (size == -1 && errno == EINTR);

    if (size == -1) {
      assert(errno == EAGAIN || errno == EWOULDBLOCK);
      break;
    }

    assert(size > 0); /* pre-2.6.21 thing, size=0 == read buffer too small */

    /* Now we have one or more inotify_event structs. */
    for (p = buf; p < buf + size; p += sizeof(*e) + e->len) {
      e = (const struct uv__inotify_event*)p;

      events = 0;
      if (e->mask & (UV__IN_ATTRIB|UV__IN_MODIFY))
        events |= UV_CHANGE;
      if (e->mask & ~(UV__IN_ATTRIB|UV__IN_MODIFY))
        events |= UV_RENAME;

      handle = find_watcher(uv_loop, e->wd);
      if (handle == NULL)
        continue; /* Handle has already been closed. */

      /* inotify does not return the filename when monitoring a single file
       * for modifications. Repurpose the filename for API compatibility.
       * I'm not convinced this is a good thing, maybe it should go.
       */
      filename = e->len ? (const char*) (e + 1) : basename_r(handle->filename);

      handle->cb(handle, filename, events, 0);
    }
  }
}


int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  int events;
  int wd;

  loop->counters.fs_event_init++;

  /* We don't support any flags yet. */
  assert(!flags);

  if (init_inotify(loop)) return -1;

  events = UV__IN_ATTRIB
         | UV__IN_CREATE
         | UV__IN_MODIFY
         | UV__IN_DELETE
         | UV__IN_DELETE_SELF
         | UV__IN_MOVE_SELF
         | UV__IN_MOVED_FROM
         | UV__IN_MOVED_TO;

  wd = uv__inotify_add_watch(loop->inotify_fd, filename, events);
  if (wd == -1) return uv__set_sys_error(loop, errno);

  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  handle->filename = strdup(filename);
  handle->cb = cb;
  handle->fd = wd;
  add_watcher(handle);

  return 0;
}


void uv__fs_event_close(uv_fs_event_t* handle) {
  uv__inotify_rm_watch(handle->loop->inotify_fd, handle->fd);
  remove_watcher(handle);
  handle->fd = -1;

  free(handle->filename);
  handle->filename = NULL;
}
