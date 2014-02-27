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
#include "internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>

struct watcher_list {
  RB_ENTRY(watcher_list) entry;
  QUEUE watchers;
  char* path;
  int wd;
};

struct watcher_root {
  struct watcher_list* rbh_root;
};
#define CAST(p) ((struct watcher_root*)(p))


static int compare_watchers(const struct watcher_list* a,
                            const struct watcher_list* b) {
  if (a->wd < b->wd) return -1;
  if (a->wd > b->wd) return 1;
  return 0;
}


RB_GENERATE_STATIC(watcher_root, watcher_list, entry, compare_watchers)


static void uv__inotify_read(uv_loop_t* loop,
                             uv__io_t* w,
                             unsigned int revents);


static int new_inotify_fd(void) {
  int err;
  int fd;

  fd = uv__inotify_init1(UV__IN_NONBLOCK | UV__IN_CLOEXEC);
  if (fd != -1)
    return fd;

  if (errno != ENOSYS)
    return -errno;

  fd = uv__inotify_init();
  if (fd == -1)
    return -errno;

  err = uv__cloexec(fd, 1);
  if (err == 0)
    err = uv__nonblock(fd, 1);

  if (err) {
    uv__close(fd);
    return err;
  }

  return fd;
}


static int init_inotify(uv_loop_t* loop) {
  int err;

  if (loop->inotify_fd != -1)
    return 0;

  err = new_inotify_fd();
  if (err < 0)
    return err;

  loop->inotify_fd = err;
  uv__io_init(&loop->inotify_read_watcher, uv__inotify_read, loop->inotify_fd);
  uv__io_start(loop, &loop->inotify_read_watcher, UV__POLLIN);

  return 0;
}


static struct watcher_list* find_watcher(uv_loop_t* loop, int wd) {
  struct watcher_list w;
  w.wd = wd;
  return RB_FIND(watcher_root, CAST(&loop->inotify_watchers), &w);
}


static void uv__inotify_read(uv_loop_t* loop,
                             uv__io_t* dummy,
                             unsigned int events) {
  const struct uv__inotify_event* e;
  struct watcher_list* w;
  uv_fs_event_t* h;
  QUEUE* q;
  const char* path;
  ssize_t size;
  const char *p;
  /* needs to be large enough for sizeof(inotify_event) + strlen(path) */
  char buf[4096];

  while (1) {
    do
      size = read(loop->inotify_fd, buf, sizeof(buf));
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

      w = find_watcher(loop, e->wd);
      if (w == NULL)
        continue; /* Stale event, no watchers left. */

      /* inotify does not return the filename when monitoring a single file
       * for modifications. Repurpose the filename for API compatibility.
       * I'm not convinced this is a good thing, maybe it should go.
       */
      path = e->len ? (const char*) (e + 1) : uv__basename_r(w->path);

      QUEUE_FOREACH(q, &w->watchers) {
        h = QUEUE_DATA(q, uv_fs_event_t, watchers);
        h->cb(h, path, events, 0);
      }
    }
  }
}


int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  return 0;
}


int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* path,
                      unsigned int flags) {
  struct watcher_list* w;
  int events;
  int err;
  int wd;

  if (uv__is_active(handle))
    return -EINVAL;

  err = init_inotify(handle->loop);
  if (err)
    return err;

  events = UV__IN_ATTRIB
         | UV__IN_CREATE
         | UV__IN_MODIFY
         | UV__IN_DELETE
         | UV__IN_DELETE_SELF
         | UV__IN_MOVE_SELF
         | UV__IN_MOVED_FROM
         | UV__IN_MOVED_TO;

  wd = uv__inotify_add_watch(handle->loop->inotify_fd, path, events);
  if (wd == -1)
    return -errno;

  w = find_watcher(handle->loop, wd);
  if (w)
    goto no_insert;

  w = malloc(sizeof(*w) + strlen(path) + 1);
  if (w == NULL)
    return -ENOMEM;

  w->wd = wd;
  w->path = strcpy((char*)(w + 1), path);
  QUEUE_INIT(&w->watchers);
  RB_INSERT(watcher_root, CAST(&handle->loop->inotify_watchers), w);

no_insert:
  uv__handle_start(handle);
  QUEUE_INSERT_TAIL(&w->watchers, &handle->watchers);
  handle->path = w->path;
  handle->cb = cb;
  handle->wd = wd;

  return 0;
}


int uv_fs_event_stop(uv_fs_event_t* handle) {
  struct watcher_list* w;

  if (!uv__is_active(handle))
    return -EINVAL;

  w = find_watcher(handle->loop, handle->wd);
  assert(w != NULL);

  handle->wd = -1;
  handle->path = NULL;
  uv__handle_stop(handle);
  QUEUE_REMOVE(&handle->watchers);

  if (QUEUE_EMPTY(&w->watchers)) {
    /* No watchers left for this path. Clean up. */
    RB_REMOVE(watcher_root, CAST(&handle->loop->inotify_watchers), w);
    uv__inotify_rm_watch(handle->loop->inotify_fd, w->wd);
    free(w);
  }

  return 0;
}


void uv__fs_event_close(uv_fs_event_t* handle) {
  uv_fs_event_stop(handle);
}
