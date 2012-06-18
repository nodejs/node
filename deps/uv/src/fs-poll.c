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
#include "uv-common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int statbuf_eq(const uv_statbuf_t* a, const uv_statbuf_t* b);
static void timer_cb(uv_timer_t* timer, int status);
static void poll_cb(uv_fs_t* req);


int uv_fs_poll_init(uv_loop_t* loop, uv_fs_poll_t* handle) {
  /* TODO(bnoordhuis) Mark fs_req internal. */
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_POLL);
  loop->counters.fs_poll_init++;

  if (uv_timer_init(loop, &handle->timer_handle))
    return -1;

  handle->timer_handle.flags |= UV__HANDLE_INTERNAL;
  uv__handle_unref(&handle->timer_handle);

  return 0;
}


int uv_fs_poll_start(uv_fs_poll_t* handle,
                     uv_fs_poll_cb cb,
                     const char* path,
                     unsigned int interval) {
  uv_fs_t* req;
  size_t len;

  if (uv__is_active(handle))
    return 0;

  len = strlen(path) + 1;
  req = malloc(sizeof(*req) + len);

  if (req == NULL)
    return uv__set_artificial_error(handle->loop, UV_ENOMEM);

  req->data = handle;
  handle->path = memcpy(req + 1, path, len);
  handle->fs_req = req;
  handle->poll_cb = cb;
  handle->interval = interval ? interval : 1;
  handle->start_time = uv_now(handle->loop);
  handle->busy_polling = 0;
  memset(&handle->statbuf, 0, sizeof(handle->statbuf));

  if (uv_fs_stat(handle->loop, handle->fs_req, handle->path, poll_cb))
    abort();

  uv__handle_start(handle);

  return 0;
}


int uv_fs_poll_stop(uv_fs_poll_t* handle) {
  if (!uv__is_active(handle))
    return 0;

  /* Don't free the fs req if it's active. Signal poll_cb that it needs to free
   * the req by removing the handle backlink.
   *
   * TODO(bnoordhuis) Have uv-unix postpone the close callback until the req
   * finishes so we don't need this pointer / lifecycle hackery. The callback
   * always runs on the next tick now.
   */
  if (handle->fs_req->data)
    handle->fs_req->data = NULL;
  else
    free(handle->fs_req);

  handle->fs_req = NULL;
  handle->path = NULL;

  uv_timer_stop(&handle->timer_handle);
  uv__handle_stop(handle);

  return 0;
}


void uv__fs_poll_close(uv_fs_poll_t* handle) {
  uv_fs_poll_stop(handle);
  uv_close((uv_handle_t*)&handle->timer_handle, NULL);
}


static void timer_cb(uv_timer_t* timer, int status) {
  uv_fs_poll_t* handle;

  handle = container_of(timer, uv_fs_poll_t, timer_handle);
  handle->start_time = uv_now(handle->loop);
  handle->fs_req->data = handle;

  if (uv_fs_stat(handle->loop, handle->fs_req, handle->path, poll_cb))
    abort();

  assert(uv__is_active(handle));
}


static void poll_cb(uv_fs_t* req) {
  uv_statbuf_t* statbuf;
  uv_fs_poll_t* handle;
  uint64_t interval;

  handle = req->data;

  if (handle == NULL) /* Handle has been stopped or closed. */
    goto out;

  assert(req == handle->fs_req);

  if (req->result != 0) {
    if (handle->busy_polling != -req->errorno) {
      uv__set_artificial_error(handle->loop, req->errorno);
      handle->poll_cb(handle, -1, NULL, NULL);
      handle->busy_polling = -req->errorno;
    }
    goto out;
  }

  statbuf = req->ptr;

  if (handle->busy_polling != 0)
    if (handle->busy_polling < 0 || !statbuf_eq(&handle->statbuf, statbuf))
      handle->poll_cb(handle, 0, &handle->statbuf, statbuf);

  handle->statbuf = *statbuf;
  handle->busy_polling = 1;

out:
  uv_fs_req_cleanup(req);

  if (req->data == NULL) { /* Handle has been stopped or closed. */
    free(req);
    return;
  }

  req->data = NULL; /* Tell uv_fs_poll_stop() it's safe to free the req. */

  /* Reschedule timer, subtract the delay from doing the stat(). */
  interval = handle->interval;
  interval -= (uv_now(handle->loop) - handle->start_time) % interval;

  if (uv_timer_start(&handle->timer_handle, timer_cb, interval, 0))
    abort();
}


static int statbuf_eq(const uv_statbuf_t* a, const uv_statbuf_t* b) {
#ifdef _WIN32
  return a->st_mtime == b->st_mtime
      && a->st_size == b->st_size
      && a->st_mode == b->st_mode;
#else

  /* Jump through a few hoops to get sub-second granularity on Linux. */
# if __linux__
#  if __USE_MISC /* _BSD_SOURCE || _SVID_SOURCE */
  if (a->st_ctim.tv_nsec != b->st_ctim.tv_nsec) return 0;
  if (a->st_mtim.tv_nsec != b->st_mtim.tv_nsec) return 0;
#  else
  if (a->st_ctimensec != b->st_ctimensec) return 0;
  if (a->st_mtimensec != b->st_mtimensec) return 0;
#  endif
# endif

  /* Jump through different hoops on OS X. */
# if __APPLE__
#  if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
  if (a->st_ctimespec.tv_nsec != b->st_ctimespec.tv_nsec) return 0;
  if (a->st_mtimespec.tv_nsec != b->st_mtimespec.tv_nsec) return 0;
#  else
  if (a->st_ctimensec != b->st_ctimensec) return 0;
  if (a->st_mtimensec != b->st_mtimensec) return 0;
#  endif
# endif

  /* TODO(bnoordhuis) Other Unices have st_ctim and friends too, provided
   * the stars and compiler flags are right...
   */

  return a->st_ctime == b->st_ctime
      && a->st_mtime == b->st_mtime
      && a->st_size == b->st_size
      && a->st_mode == b->st_mode
      && a->st_uid == b->st_uid
      && a->st_gid == b->st_gid
      && a->st_ino == b->st_ino
      && a->st_dev == b->st_dev;
#endif
}


#ifdef _WIN32

#include "win/internal.h"
#include "win/handle-inl.h"

void uv__fs_poll_endgame(uv_loop_t* loop, uv_fs_poll_t* handle) {
  assert(handle->flags & UV_HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));
  uv__handle_stop(handle);
  uv__handle_close(handle);
}

#endif /* _WIN32 */
