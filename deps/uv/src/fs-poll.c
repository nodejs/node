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

struct poll_ctx {
  uv_fs_poll_t* parent_handle; /* NULL if parent has been stopped or closed */
  int busy_polling;
  unsigned int interval;
  uint64_t start_time;
  uv_loop_t* loop;
  uv_fs_poll_cb poll_cb;
  uv_timer_t timer_handle;
  uv_fs_t fs_req; /* TODO(bnoordhuis) mark fs_req internal */
  uv_stat_t statbuf;
  char path[1]; /* variable length */
};

static int statbuf_eq(const uv_stat_t* a, const uv_stat_t* b);
static void poll_cb(uv_fs_t* req);
static void timer_cb(uv_timer_t* timer);
static void timer_close_cb(uv_handle_t* handle);

static uv_stat_t zero_statbuf;


int uv_fs_poll_init(uv_loop_t* loop, uv_fs_poll_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_POLL);
  return 0;
}


int uv_fs_poll_start(uv_fs_poll_t* handle,
                     uv_fs_poll_cb cb,
                     const char* path,
                     unsigned int interval) {
  struct poll_ctx* ctx;
  uv_loop_t* loop;
  size_t len;
  int err;

  if (uv__is_active(handle))
    return 0;

  loop = handle->loop;
  len = strlen(path);
  ctx = calloc(1, sizeof(*ctx) + len);

  if (ctx == NULL)
    return UV_ENOMEM;

  ctx->loop = loop;
  ctx->poll_cb = cb;
  ctx->interval = interval ? interval : 1;
  ctx->start_time = uv_now(loop);
  ctx->parent_handle = handle;
  memcpy(ctx->path, path, len + 1);

  err = uv_timer_init(loop, &ctx->timer_handle);
  if (err < 0)
    goto error;

  ctx->timer_handle.flags |= UV__HANDLE_INTERNAL;
  uv__handle_unref(&ctx->timer_handle);

  err = uv_fs_stat(loop, &ctx->fs_req, ctx->path, poll_cb);
  if (err < 0)
    goto error;

  handle->poll_ctx = ctx;
  uv__handle_start(handle);

  return 0;

error:
  free(ctx);
  return err;
}


int uv_fs_poll_stop(uv_fs_poll_t* handle) {
  struct poll_ctx* ctx;

  if (!uv__is_active(handle))
    return 0;

  ctx = handle->poll_ctx;
  assert(ctx != NULL);
  assert(ctx->parent_handle != NULL);
  ctx->parent_handle = NULL;
  handle->poll_ctx = NULL;

  /* Close the timer if it's active. If it's inactive, there's a stat request
   * in progress and poll_cb will take care of the cleanup.
   */
  if (uv__is_active(&ctx->timer_handle))
    uv_close((uv_handle_t*)&ctx->timer_handle, timer_close_cb);

  uv__handle_stop(handle);

  return 0;
}


int uv_fs_poll_getpath(uv_fs_poll_t* handle, char* buf, size_t* len) {
  struct poll_ctx* ctx;
  size_t required_len;

  if (!uv__is_active(handle)) {
    *len = 0;
    return UV_EINVAL;
  }

  ctx = handle->poll_ctx;
  assert(ctx != NULL);

  required_len = strlen(ctx->path) + 1;
  if (required_len > *len) {
    *len = required_len;
    return UV_ENOBUFS;
  }

  memcpy(buf, ctx->path, required_len);
  *len = required_len;

  return 0;
}


void uv__fs_poll_close(uv_fs_poll_t* handle) {
  uv_fs_poll_stop(handle);
}


static void timer_cb(uv_timer_t* timer) {
  struct poll_ctx* ctx;

  ctx = container_of(timer, struct poll_ctx, timer_handle);
  assert(ctx->parent_handle != NULL);
  assert(ctx->parent_handle->poll_ctx == ctx);
  ctx->start_time = uv_now(ctx->loop);

  if (uv_fs_stat(ctx->loop, &ctx->fs_req, ctx->path, poll_cb))
    abort();
}


static void poll_cb(uv_fs_t* req) {
  uv_stat_t* statbuf;
  struct poll_ctx* ctx;
  uint64_t interval;

  ctx = container_of(req, struct poll_ctx, fs_req);

  if (ctx->parent_handle == NULL) { /* handle has been stopped or closed */
    uv_close((uv_handle_t*)&ctx->timer_handle, timer_close_cb);
    uv_fs_req_cleanup(req);
    return;
  }

  if (req->result != 0) {
    if (ctx->busy_polling != req->result) {
      ctx->poll_cb(ctx->parent_handle,
                   req->result,
                   &ctx->statbuf,
                   &zero_statbuf);
      ctx->busy_polling = req->result;
    }
    goto out;
  }

  statbuf = &req->statbuf;

  if (ctx->busy_polling != 0)
    if (ctx->busy_polling < 0 || !statbuf_eq(&ctx->statbuf, statbuf))
      ctx->poll_cb(ctx->parent_handle, 0, &ctx->statbuf, statbuf);

  ctx->statbuf = *statbuf;
  ctx->busy_polling = 1;

out:
  uv_fs_req_cleanup(req);

  if (ctx->parent_handle == NULL) { /* handle has been stopped by callback */
    uv_close((uv_handle_t*)&ctx->timer_handle, timer_close_cb);
    return;
  }

  /* Reschedule timer, subtract the delay from doing the stat(). */
  interval = ctx->interval;
  interval -= (uv_now(ctx->loop) - ctx->start_time) % interval;

  if (uv_timer_start(&ctx->timer_handle, timer_cb, interval, 0))
    abort();
}


static void timer_close_cb(uv_handle_t* handle) {
  free(container_of(handle, struct poll_ctx, timer_handle));
}


static int statbuf_eq(const uv_stat_t* a, const uv_stat_t* b) {
  return a->st_ctim.tv_nsec == b->st_ctim.tv_nsec
      && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec
      && a->st_birthtim.tv_nsec == b->st_birthtim.tv_nsec
      && a->st_ctim.tv_sec == b->st_ctim.tv_sec
      && a->st_mtim.tv_sec == b->st_mtim.tv_sec
      && a->st_birthtim.tv_sec == b->st_birthtim.tv_sec
      && a->st_size == b->st_size
      && a->st_mode == b->st_mode
      && a->st_uid == b->st_uid
      && a->st_gid == b->st_gid
      && a->st_ino == b->st_ino
      && a->st_dev == b->st_dev
      && a->st_flags == b->st_flags
      && a->st_gen == b->st_gen;
}


#if defined(_WIN32)

#include "win/internal.h"
#include "win/handle-inl.h"

void uv__fs_poll_endgame(uv_loop_t* loop, uv_fs_poll_t* handle) {
  assert(handle->flags & UV__HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));
  uv__handle_close(handle);
}

#endif /* _WIN32 */
