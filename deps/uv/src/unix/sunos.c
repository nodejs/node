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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/loadavg.h>
#include <sys/time.h>
#include <unistd.h>
#include <kstat.h>

#if HAVE_PORTS_FS
# include <sys/port.h>
# include <port.h>
#endif


uint64_t uv_hrtime() {
  return (gethrtime());
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 */
int uv_exepath(char* buffer, size_t* size) {
  ssize_t res;
  pid_t pid;
  char buf[128];

  if (buffer == NULL)
    return (-1);

  if (size == NULL)
    return (-1);

  pid = getpid();
  (void) snprintf(buf, sizeof (buf), "/proc/%d/path/a.out", pid);
  res = readlink(buf, buffer, *size - 1);

  if (res < 0)
    return (res);

  buffer[res] = '\0';
  *size = res;
  return (0);
}


uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}


uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}


void uv_loadavg(double avg[3]) {
  (void) getloadavg(avg, 3);
}


#if HAVE_PORTS_FS
static void uv__fs_event_rearm(uv_fs_event_t *handle) {
  if (port_associate(handle->fd,
                     PORT_SOURCE_FILE,
                     (uintptr_t) &handle->fo,
                     FILE_ATTRIB | FILE_MODIFIED,
                     NULL) == -1) {
    uv__set_sys_error(handle->loop, errno);
  }
}


static void uv__fs_event_read(EV_P_ ev_io* w, int revents) {
  uv_fs_event_t *handle;
  timespec_t timeout;
  port_event_t pe;
  int events;
  int r;

  handle = container_of(w, uv_fs_event_t, event_watcher);

  do {
    /* TODO use port_getn() */
    do {
      memset(&timeout, 0, sizeof timeout);
      r = port_get(handle->fd, &pe, &timeout);
    }
    while (r == -1 && errno == EINTR);

    if (r == -1 && errno == ETIME)
      break;

    assert((r == 0) && "unexpected port_get() error");

    events = 0;
    if (pe.portev_events & (FILE_ATTRIB | FILE_MODIFIED))
      events |= UV_CHANGE;
    if (pe.portev_events & ~(FILE_ATTRIB | FILE_MODIFIED))
      events |= UV_RENAME;
    assert(events != 0);

    handle->cb(handle, NULL, events, 0);
  }
  while (handle->fd != -1);

  if (handle->fd != -1)
    uv__fs_event_rearm(handle);
}


int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb,
                     int flags) {
  int portfd;

  loop->counters.fs_event_init++;

  /* We don't support any flags yet. */
  assert(!flags);

  if ((portfd = port_create()) == -1) {
    uv__set_sys_error(loop, errno);
    return -1;
  }

  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  handle->filename = strdup(filename);
  handle->fd = portfd;
  handle->cb = cb;

  memset(&handle->fo, 0, sizeof handle->fo);
  handle->fo.fo_name = handle->filename;
  uv__fs_event_rearm(handle);

  ev_io_init(&handle->event_watcher, uv__fs_event_read, portfd, EV_READ);
  ev_io_start(loop->ev, &handle->event_watcher);
  ev_unref(loop->ev);

  return 0;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  ev_ref(handle->loop->ev);
  ev_io_stop(handle->loop->ev, &handle->event_watcher);
  uv__close(handle->fd);
  handle->fd = -1;
  free(handle->filename);
  handle->filename = NULL;
  handle->fo.fo_name = NULL;
}

#else /* !HAVE_PORTS_FS */

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
  assert(0 && "unreachable"); /* should never be called */
}

#endif /* HAVE_PORTS_FS */
