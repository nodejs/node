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

#ifndef UV_UNIX_H
#define UV_UNIX_H

#include "ngx-queue.h"

#include "ev/ev.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


/* Note: May be cast to struct iovec. See writev(2). */
typedef struct {
  char* base;
  size_t len;
} uv_buf;


#define uv_req_private_fields \
  int write_index; \
  ev_timer timer; \
  ngx_queue_t queue; \
  uv_buf* bufs; \
  int bufcnt;


/* TODO: union or classes please! */
#define uv_handle_private_fields \
  int fd; \
  int flags; \
  ev_idle next_watcher; \
/* UV_TCP */ \
  int delayed_error; \
  uv_read_cb read_cb; \
  uv_accept_cb accept_cb; \
  int accepted_fd; \
  uv_req_t *connect_req; \
  uv_req_t *shutdown_req; \
  ev_io read_watcher; \
  ev_io write_watcher; \
  ngx_queue_t write_queue; \
/* UV_PREPARE */ \
  ev_prepare prepare_watcher; \
  uv_loop_cb prepare_cb; \
/* UV_CHECK */ \
  ev_check check_watcher; \
  uv_loop_cb check_cb; \
/* UV_IDLE */ \
  ev_idle idle_watcher; \
  uv_loop_cb idle_cb; \
/* UV_ASYNC */ \
  ev_async async_watcher; \
  uv_loop_cb async_cb;


#endif /* UV_UNIX_H */
