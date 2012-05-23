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
#include "tree.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int uv__loop_init(uv_loop_t* loop, int default_loop) {
#if HAVE_KQUEUE
  int flags = EVBACKEND_KQUEUE;
#else
  int flags = EVFLAG_AUTO;
#endif

  memset(loop, 0, sizeof(*loop));

#ifndef UV_LEAN_AND_MEAN
  ngx_queue_init(&loop->active_handles);
  ngx_queue_init(&loop->active_reqs);
#endif

  RB_INIT(&loop->ares_handles);
  ngx_queue_init(&loop->idle_handles);
  ngx_queue_init(&loop->check_handles);
  ngx_queue_init(&loop->prepare_handles);
  loop->pending_handles = NULL;
  loop->channel = NULL;
  loop->ev = (default_loop ? ev_default_loop : ev_loop_new)(flags);
  ev_set_userdata(loop->ev, loop);
  eio_channel_init(&loop->uv_eio_channel, loop);

#if __linux__
  RB_INIT(&loop->inotify_watchers);
  loop->inotify_fd = -1;
#endif
#if HAVE_PORTS_FS
  loop->fs_fd = -1;
#endif
  return 0;
}


void uv__loop_delete(uv_loop_t* loop) {
  uv_ares_destroy(loop, loop->channel);
  ev_loop_destroy(loop->ev);
#if __linux__
  if (loop->inotify_fd != -1) {
    uv__io_stop(loop, &loop->inotify_read_watcher);
    close(loop->inotify_fd);
    loop->inotify_fd = -1;
  }
#endif
#if HAVE_PORTS_FS
  if (loop->fs_fd != -1)
    close(loop->fs_fd);
#endif
}


#define X(name, type) \
  int uv_##name##_init(uv_loop_t* loop, uv_##name##_t* handle) {              \
    uv__handle_init(loop, (uv_handle_t*)handle, type);                        \
    loop->counters.name##_init++;                                             \
    handle->name##_cb = NULL;                                                 \
    return 0;                                                                 \
  }                                                                           \
  int uv_##name##_start(uv_##name##_t* handle, uv_##name##_cb cb) {           \
    if (uv__is_active(handle)) return 0;                                      \
    ngx_queue_insert_head(&handle->loop->name##_handles, &handle->queue);     \
    handle->name##_cb = cb;                                                   \
    uv__handle_start(handle);                                                 \
    return 0;                                                                 \
  }                                                                           \
  int uv_##name##_stop(uv_##name##_t* handle) {                               \
    if (!uv__is_active(handle)) return 0;                                     \
    ngx_queue_remove(&handle->queue);                                         \
    uv__handle_stop(handle);                                                  \
    return 0;                                                                 \
  }                                                                           \
  void uv__run_##name(uv_loop_t* loop) {                                      \
    uv_##name##_t* h;                                                         \
    ngx_queue_t* q;                                                           \
    ngx_queue_foreach(q, &loop->name##_handles) {                             \
      h = ngx_queue_data(q, uv_##name##_t, queue);                            \
      if (h->name##_cb) h->name##_cb(h, 0);                                   \
    }                                                                         \
  }                                                                           \
  void uv__##name##_close(uv_##name##_t* handle) {                            \
    uv_##name##_stop(handle);                                                 \
  }
X(idle, UV_IDLE)
X(check, UV_CHECK)
X(prepare, UV_PREPARE)
#undef X
