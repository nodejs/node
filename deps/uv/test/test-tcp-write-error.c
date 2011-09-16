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
#include <stdlib.h>
#include <string.h>

static void connection_cb(uv_stream_t* server, int status);
static void connect_cb(uv_connect_t* req, int status);
static void write_cb(uv_write_t* req, int status);
static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size);

static uv_tcp_t tcp_server;
static uv_tcp_t tcp_client;
static uv_tcp_t tcp_peer; /* client socket as accept()-ed by server */
static uv_write_t write_req;
static uv_connect_t connect_req;

static int write_cb_called;


static void connection_cb(uv_stream_t* server, int status) {
  int r;

  ASSERT(server == (uv_stream_t*)&tcp_server);
  ASSERT(status == 0);

  r = uv_tcp_init(server->loop, &tcp_peer);
  ASSERT(r == 0);

  r = uv_accept(server, (uv_stream_t*)&tcp_peer);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&tcp_peer, alloc_cb, read_cb);
  ASSERT(r == 0);
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  static char slab[1024];
  return uv_buf_init(slab, sizeof slab);
}


static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  uv_close((uv_handle_t*)&tcp_server, NULL);
  uv_close((uv_handle_t*)&tcp_peer, NULL);
}


static void connect_cb(uv_connect_t* req, int status) {
  uv_buf_t buf;
  size_t size;
  char* data;
  int r;

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  size = 10*1024*1024;
  data = malloc(size);
  ASSERT(data != NULL);

  memset(data, '$', size);
  buf = uv_buf_init(data, size);

  write_req.data = data;

  r = uv_write(&write_req, req->handle, &buf, 1, write_cb);
  ASSERT(r == 0);

  /* Write queue should have been updated. */
  ASSERT(req->handle->write_queue_size > 0);

  /* write_queue_size <= size, part may have already been written. */
  ASSERT(req->handle->write_queue_size <= size);
}


static void write_cb(uv_write_t* req, int status) {
  ASSERT(req == &write_req);
  ASSERT(status == -1);

  /* This is what this test is all about. */
  ASSERT(tcp_client.write_queue_size == 0);

  free(write_req.data);

  uv_close((uv_handle_t*)&tcp_client, NULL);

  write_cb_called++;
}


/*
 * Assert that a failing write does not leave
 * the stream's write_queue_size in an inconsistent state.
 */
TEST_IMPL(tcp_write_error) {
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();
  ASSERT(loop != NULL);

  r = uv_tcp_init(loop, &tcp_server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&tcp_server, uv_ip4_addr("127.0.0.1", TEST_PORT));
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&tcp_server, 1, connection_cb);
  ASSERT(r == 0);

  r = uv_tcp_init(loop, &tcp_client);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &tcp_client,
                     uv_ip4_addr("127.0.0.1", TEST_PORT),
                     connect_cb);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 0);

  r = uv_run(loop);
  ASSERT(r == 0);

  ASSERT(write_cb_called == 1);

  return 0;
}
