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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#undef HAVE_INOTIFY_INIT
#undef HAVE_INOTIFY_INIT1
#undef HAVE_INOTIFY_ADD_WATCH
#undef HAVE_INOTIFY_RM_WATCH

#if __NR_inotify_init
# define HAVE_INOTIFY_INIT 1
#endif
#if __NR_inotify_init1
# define HAVE_INOTIFY_INIT1 1
#endif
#if __NR_inotify_add_watch
# define HAVE_INOTIFY_ADD_WATCH 1
#endif
#if __NR_inotify_rm_watch
# define HAVE_INOTIFY_RM_WATCH 1
#endif

#if HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1
# undef IN_ACCESS
# undef IN_MODIFY
# undef IN_ATTRIB
# undef IN_CLOSE_WRITE
# undef IN_CLOSE_NOWRITE
# undef IN_OPEN
# undef IN_MOVED_FROM
# undef IN_MOVED_TO
# undef IN_CREATE
# undef IN_DELETE
# undef IN_DELETE_SELF
# undef IN_MOVE_SELF
# define IN_ACCESS         0x001
# define IN_MODIFY         0x002
# define IN_ATTRIB         0x004
# define IN_CLOSE_WRITE    0x008
# define IN_CLOSE_NOWRITE  0x010
# define IN_OPEN           0x020
# define IN_MOVED_FROM     0x040
# define IN_MOVED_TO       0x080
# define IN_CREATE         0x100
# define IN_DELETE         0x200
# define IN_DELETE_SELF    0x400
# define IN_MOVE_SELF      0x800
struct inotify_event {
  int32_t wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  /* char name[0]; */
};
#endif /* HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */

#undef IN_CLOEXEC
#undef IN_NONBLOCK

#if HAVE_INOTIFY_INIT1
# define IN_CLOEXEC O_CLOEXEC
# define IN_NONBLOCK O_NONBLOCK
#endif /* HAVE_INOTIFY_INIT1 */

#if HAVE_INOTIFY_INIT
inline static int inotify_init(void) {
  return syscall(__NR_inotify_init);
}
#endif /* HAVE_INOTIFY_INIT */

#if HAVE_INOTIFY_INIT1
inline static int inotify_init1(int flags) {
  return syscall(__NR_inotify_init1, flags);
}
#endif /* HAVE_INOTIFY_INIT1 */

#if HAVE_INOTIFY_ADD_WATCH
inline static int inotify_add_watch(int fd, const char* path, uint32_t mask) {
  return syscall(__NR_inotify_add_watch, fd, path, mask);
}
#endif /* HAVE_INOTIFY_ADD_WATCH */

#if HAVE_INOTIFY_RM_WATCH
inline static int inotify_rm_watch(int fd, uint32_t wd) {
  return syscall(__NR_inotify_rm_watch, fd, wd);
}
#endif /* HAVE_INOTIFY_RM_WATCH */


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


RB_GENERATE_INTERNAL(uv__inotify_watchers, uv_fs_event_s, node, compare_watchers,
  inline static __attribute__((unused)))


void uv__inotify_loop_init(uv_loop_t* loop) {
  RB_INIT(&loop->inotify_watchers);
  loop->inotify_fd = -1;
}


void uv__inotify_loop_delete(uv_loop_t* loop) {
  if (loop->inotify_fd == -1) return;
  ev_io_stop(loop->ev, &loop->inotify_read_watcher);
  close(loop->inotify_fd);
  loop->inotify_fd = -1;
}


#if HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1

static void uv__inotify_read(EV_P_ ev_io* w, int revents);


static int new_inotify_fd(void) {
#if HAVE_INOTIFY_INIT1
  return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#else
  int fd;

  if ((fd = inotify_init()) == -1)
    return -1;

  if (uv__cloexec(fd, 1) || uv__nonblock(fd, 1)) {
    SAVE_ERRNO(uv__close(fd));
    return -1;
  }

  return fd;
#endif
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
  const struct inotify_event* e;
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
      e = (const struct inotify_event*)p;

      events = 0;
      if (e->mask & (IN_ATTRIB|IN_MODIFY))
        events |= UV_CHANGE;
      if (e->mask & ~(IN_ATTRIB|IN_MODIFY))
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

  events = IN_ATTRIB
         | IN_CREATE
         | IN_MODIFY
         | IN_DELETE
         | IN_DELETE_SELF
         | IN_MOVED_FROM
         | IN_MOVED_TO;

  wd = inotify_add_watch(loop->inotify_fd, filename, events);
  if (wd == -1) return uv__set_sys_error(loop, errno);

  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  handle->filename = strdup(filename);
  handle->cb = cb;
  handle->fd = wd;
  add_watcher(handle);

  return 0;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  inotify_rm_watch(handle->loop->inotify_fd, handle->fd);
  remove_watcher(handle);
  handle->fd = -1;

  free(handle->filename);
  handle->filename = NULL;
}

#else /* !HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */

int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  loop->counters.fs_event_init++;
  uv__set_sys_error(loop, ENOSYS);
  return -1;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  UNREACHABLE();
}

#endif /* HAVE_INOTIFY_INIT || HAVE_INOTIFY_INIT1 */
