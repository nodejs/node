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
#include "task.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

typedef struct {
  uv_tcp_t handle;
  uv_shutdown_t shutdown_req;
} conn_rec;

static uv_tcp_t tcp_server;

static void connection_cb(uv_stream_t* stream, int status);
static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size);
static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
static void shutdown_cb(uv_shutdown_t* req, int status);
static void close_cb(uv_handle_t* handle);


static void connection_cb(uv_stream_t* stream, int status) {
  conn_rec* conn;
  int r;

  ASSERT(status == 0);
  ASSERT(stream == (uv_stream_t*)&tcp_server);

  conn = malloc(sizeof *conn);
  ASSERT(conn != NULL);

  r = uv_tcp_init(stream->loop, &conn->handle);
  ASSERT(r == 0);

  r = uv_accept(stream, (uv_stream_t*)&conn->handle);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&conn->handle, alloc_cb, read_cb);
  ASSERT(r == 0);
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  static char buf[65536];
  return uv_buf_init(buf, sizeof buf);
}


static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  conn_rec* conn;
  int r;

  if (nread >= 0)
    return;

  ASSERT(uv_last_error(stream->loop).code == UV_EOF);

  conn = container_of(stream, conn_rec, handle);

  r = uv_shutdown(&conn->shutdown_req, stream, shutdown_cb);
  ASSERT(r == 0);
}


static void shutdown_cb(uv_shutdown_t* req, int status) {
  conn_rec* conn = container_of(req, conn_rec, shutdown_req);
  uv_close((uv_handle_t*)&conn->handle, close_cb);
}


static void close_cb(uv_handle_t* handle) {
  conn_rec* conn = container_of(handle, conn_rec, handle);
  free(conn);
}


HELPER_IMPL(tcp4_blackhole_server) {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  r = uv_tcp_init(loop, &tcp_server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&tcp_server, addr);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&tcp_server, 128, connection_cb);
  ASSERT(r == 0);

  r = uv_run(loop);
  ASSERT(0 && "Blackhole server dropped out of event loop.");

  return 0;
}
