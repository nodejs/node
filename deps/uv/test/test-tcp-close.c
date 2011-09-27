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

#include <errno.h>
#include <string.h> /* memset */

#define NUM_WRITE_REQS 32

static uv_tcp_t tcp_handle;
static uv_connect_t connect_req;

static int write_cb_called;
static int close_cb_called;

static void connect_cb(uv_connect_t* req, int status);
static void write_cb(uv_write_t* req, int status);
static void close_cb(uv_handle_t* handle);


static void connect_cb(uv_connect_t* conn_req, int status) {
  uv_write_t* req;
  uv_buf_t buf;
  int i, r;

  buf = uv_buf_init("PING", 4);
  for (i = 0; i < NUM_WRITE_REQS; i++) {
    req = malloc(sizeof *req);
    ASSERT(req != NULL);

    r = uv_write(req, (uv_stream_t*)&tcp_handle, &buf, 1, write_cb);
    ASSERT(r == 0);
  }

  uv_close((uv_handle_t*)&tcp_handle, close_cb);
}


static void write_cb(uv_write_t* req, int status) {
  /* write callbacks should run before the close callback */
  ASSERT(close_cb_called == 0);
  ASSERT(req->handle == (uv_stream_t*)&tcp_handle);
  write_cb_called++;
  free(req);
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*)&tcp_handle);
  close_cb_called++;
}


static void connection_cb(uv_stream_t* server, int status) {
  ASSERT(status == 0);
}


static void start_server(uv_loop_t* loop, uv_tcp_t* handle) {
  int r;

  r = uv_tcp_init(loop, handle);
  ASSERT(r == 0);

  r = uv_tcp_bind(handle, uv_ip4_addr("127.0.0.1", TEST_PORT));
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)handle, 128, connection_cb);
  ASSERT(r == 0);

  uv_unref(loop);
}


/* Check that pending write requests have their callbacks
 * invoked when the handle is closed.
 */
TEST_IMPL(tcp_close) {
  uv_loop_t* loop;
  uv_tcp_t tcp_server;
  int r;

  loop = uv_default_loop();

  /* We can't use the echo server, it doesn't handle ECONNRESET. */
  start_server(loop, &tcp_server);

  r = uv_tcp_init(loop, &tcp_handle);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &tcp_handle,
                     uv_ip4_addr("127.0.0.1", TEST_PORT),
                     connect_cb);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 0);
  ASSERT(close_cb_called == 0);

  r = uv_run(loop);
  ASSERT(r == 0);

  printf("%d of %d write reqs seen\n", write_cb_called, NUM_WRITE_REQS);

  ASSERT(write_cb_called == NUM_WRITE_REQS);
  ASSERT(close_cb_called == 1);

  return 0;
}


TEST_IMPL(tcp_ref) {
  uv_tcp_t never;
  int r;

  /* A tcp just initialized should count as one reference. */
  r = uv_tcp_init(uv_default_loop(), &never);
  ASSERT(r == 0);

  /* One unref should set the loop ref count to zero. */
  uv_unref(uv_default_loop());

  /* Therefore this does not block */
  uv_run(uv_default_loop());

  return 0;
}


static void never_cb(uv_connect_t* conn_req, int status) {
  FATAL("never_cb should never be called");
}


TEST_IMPL(tcp_ref2) {
  uv_tcp_t never;
  int r;

  /* A tcp just initialized should count as one reference. */
  r = uv_tcp_init(uv_default_loop(), &never);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &never,
                     uv_ip4_addr("127.0.0.1", TEST_PORT),
                     never_cb);
  ASSERT(r == 0);

  /* One unref should set the loop ref count to zero. */
  uv_unref(uv_default_loop());

  /* Therefore this does not block */
  uv_run(uv_default_loop());

  return 0;
}
