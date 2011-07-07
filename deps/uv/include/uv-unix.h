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

#include "ev.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


/* Note: May be cast to struct iovec. See writev(2). */
typedef struct {
  char* base;
  size_t len;
} uv_buf_t;

#define UV_REQ_BUFSML_SIZE (4)

#define UV_REQ_PRIVATE_FIELDS \
  int write_index; \
  ev_timer timer; \
  ngx_queue_t queue; \
  uv_buf_t* bufs; \
  int bufcnt; \
  uv_buf_t bufsml[UV_REQ_BUFSML_SIZE];


/* TODO: union or classes please! */
#define UV_HANDLE_PRIVATE_FIELDS \
  int fd; \
  int flags; \
  ev_idle next_watcher;


#define UV_STREAM_PRIVATE_FIELDS \
  uv_read_cb read_cb; \
  uv_alloc_cb alloc_cb;


/* UV_TCP */
#define UV_TCP_PRIVATE_FIELDS \
  int delayed_error; \
  uv_connection_cb connection_cb; \
  int accepted_fd; \
  uv_req_t *connect_req; \
  uv_req_t *shutdown_req; \
  ev_io read_watcher; \
  ev_io write_watcher; \
  ngx_queue_t write_queue; \
  ngx_queue_t write_completed_queue;


/* UV_PREPARE */ \
#define UV_PREPARE_PRIVATE_FIELDS \
  ev_prepare prepare_watcher; \
  uv_prepare_cb prepare_cb;


/* UV_CHECK */
#define UV_CHECK_PRIVATE_FIELDS \
  ev_check check_watcher; \
  uv_check_cb check_cb;


/* UV_IDLE */
#define UV_IDLE_PRIVATE_FIELDS \
  ev_idle idle_watcher; \
  uv_idle_cb idle_cb;


/* UV_ASYNC */
#define UV_ASYNC_PRIVATE_FIELDS \
  ev_async async_watcher; \
  uv_async_cb async_cb;


/* UV_TIMER */
#define UV_TIMER_PRIVATE_FIELDS \
  ev_timer timer_watcher; \
  uv_timer_cb timer_cb;

#define UV_ARES_TASK_PRIVATE_FIELDS \
  int sock; \
  ev_io read_watcher; \
  ev_io write_watcher;

#define UV_GETADDRINFO_PRIVATE_FIELDS \
  uv_getaddrinfo_cb cb; \
  struct addrinfo* hints; \
  char* hostname; \
  char* service; \
  struct addrinfo* res; \
  int retcode;

#endif /* UV_UNIX_H */
