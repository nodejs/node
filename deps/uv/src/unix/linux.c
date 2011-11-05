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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <time.h>

#undef NANOSEC
#define NANOSEC 1000000000


/* Don't look aghast, this is exactly how glibc's basename() works. */
static char* basename_r(const char* path) {
  char* s = strrchr(path, '/');
  return s ? (s + 1) : (char*)path;
}


/*
 * There's probably some way to get time from Linux than gettimeofday(). What
 * it is, I don't know.
 */
uint64_t uv_hrtime() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * NANOSEC + ts.tv_nsec);
}

void uv_loadavg(double avg[3]) {
  struct sysinfo info;

  if (sysinfo(&info) < 0) return;

  avg[0] = (double) info.loads[0] / 65536.0;
  avg[1] = (double) info.loads[1] / 65536.0;
  avg[2] = (double) info.loads[2] / 65536.0;
}


int uv_exepath(char* buffer, size_t* size) {
  if (!buffer || !size) {
    return -1;
  }

  *size = readlink("/proc/self/exe", buffer, *size - 1);
  if (*size <= 0) return -1;
  buffer[*size] = '\0';
  return 0;
}

uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}

uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}

static int new_inotify_fd(void) {
#if defined(IN_NONBLOCK) && defined(IN_CLOEXEC)
  return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#else
  int fd;

  if ((fd = inotify_init()) == -1)
    return -1;

  if (uv__cloexec(fd, 1) || uv__nonblock(fd, 1)) {
    SAVE_ERRNO(uv__close(fd));
    fd = -1;
  }

  return fd;
#endif
}


static void uv__inotify_read(EV_P_ ev_io* w, int revents) {
  struct inotify_event* e;
  uv_fs_event_t* handle;
  const char* filename;
  ssize_t size;
  int events;
  char *p;
  /* needs to be large enough for sizeof(inotify_event) + strlen(filename) */
  char buf[4096];

  handle = container_of(w, uv_fs_event_t, read_watcher);

  do {
    do {
      size = read(handle->fd, buf, sizeof buf);
    }
    while (size == -1 && errno == EINTR);

    if (size == -1) {
      assert(errno == EAGAIN || errno == EWOULDBLOCK);
      break;
    }

    assert(size > 0); /* pre-2.6.21 thing, size=0 == read buffer too small */

    /* Now we have one or more inotify_event structs. */
    for (p = buf; p < buf + size; p += sizeof(*e) + e->len) {
      e = (void*)p;

      events = 0;
      if (e->mask & (IN_ATTRIB|IN_MODIFY))
        events |= UV_CHANGE;
      if (e->mask & ~(IN_ATTRIB|IN_MODIFY))
        events |= UV_RENAME;

      /* inotify does not return the filename when monitoring a single file
       * for modifications. Repurpose the filename for API compatibility.
       * I'm not convinced this is a good thing, maybe it should go.
       */
      filename = e->len ? e->name : basename_r(handle->filename);

      handle->cb(handle, filename, events, 0);

      if (handle->fd == -1)
        break;
    }
  }
  while (handle->fd != -1); /* handle might've been closed by callback */
}


int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  int events;
  int fd;

  /* We don't support any flags yet. */
  assert(!flags);

  /*
   * TODO share a single inotify fd across the event loop?
   * We'll run into fs.inotify.max_user_instances if we
   * keep creating new inotify fds.
   */
  if ((fd = new_inotify_fd()) == -1) {
    uv__set_sys_error(loop, errno);
    return -1;
  }

  events = IN_ATTRIB
         | IN_CREATE
         | IN_MODIFY
         | IN_DELETE
         | IN_DELETE_SELF
         | IN_MOVED_FROM
         | IN_MOVED_TO;

  if (inotify_add_watch(fd, filename, events) == -1) {
    uv__set_sys_error(loop, errno);
    uv__close(fd);
    return -1;
  }

  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  handle->filename = strdup(filename); /* this should go! */
  handle->cb = cb;
  handle->fd = fd;

  ev_io_init(&handle->read_watcher, uv__inotify_read, fd, EV_READ);
  ev_io_start(loop->ev, &handle->read_watcher);

  return 0;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  ev_io_stop(handle->loop->ev, &handle->read_watcher);
  uv__close(handle->fd);
  handle->fd = -1;
  free(handle->filename);
  handle->filename = NULL;
}
