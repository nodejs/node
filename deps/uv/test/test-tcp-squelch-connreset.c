/* Copyright (c) 2015, Santiago Gimeno <santiago.gimeno@gmail.com>
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


static uv_tcp_t tcp_server;
static uv_tcp_t tcp_client;
static uv_tcp_t tcp_server_client;
static uv_connect_t connect_req;
static uv_write_t write_req;

static void alloc_cb(uv_handle_t* handle,
                     size_t size,
                     uv_buf_t* buf) {
  buf->base = malloc(size);
  buf->len = size;
}

static void read_cb(uv_stream_t* tcp, ssize_t nread, const uv_buf_t* buf) {
  free(buf->base);
  ASSERT(nread == UV_EOF);
}

static void on_connect(uv_connect_t* req, int status) {
  int r;
  uv_buf_t outbuf;

  ASSERT(req != NULL);
  ASSERT(status == 0);

  outbuf = uv_buf_init("ping", 4);
  r = uv_write(&write_req, (uv_stream_t*) req->handle, &outbuf, 1, NULL);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) req->handle, alloc_cb, read_cb);
  ASSERT(r == 0);
}

static void on_connection(uv_stream_t* server, int status) {
  int r;

  ASSERT(status == 0);

  r = uv_tcp_init(uv_default_loop(), &tcp_server_client);
  ASSERT(r == 0);

  r = uv_accept(server, (uv_stream_t*) &tcp_server_client);
  ASSERT(r == 0);

  uv_close((uv_handle_t*) &tcp_server_client, NULL);
  uv_close((uv_handle_t*) &tcp_server, NULL);
}

static void start_server(void) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));

  r = uv_tcp_init(uv_default_loop(), &tcp_server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&tcp_server, (const struct sockaddr*) &addr, 0);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*) &tcp_server, SOMAXCONN, on_connection);
  ASSERT(r == 0);
}

static void start_client(void) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  r = uv_tcp_init(uv_default_loop(), &tcp_client);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &tcp_client,
                     (const struct sockaddr*) &addr,
                     on_connect);
  ASSERT(r == 0);
}


TEST_IMPL(tcp_squelch_connreset) {

  start_server();

  start_client();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
