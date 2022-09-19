/* Copyright libuv contributors. All rights reserved.
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

static uv_tcp_t server;
static uv_tcp_t client;
static uv_tcp_t incoming;
static int connect_cb_called;
static int close_cb_called;
static int connection_cb_called;


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

static void incoming_close_cb(uv_handle_t* handle) {
  uv_buf_t buf;
  int r = 1;

  close_cb_called++;

  buf = uv_buf_init("meow", 4);
  while (r > 0)
    r = uv_try_write((uv_stream_t*) &client, &buf, 1);
  fprintf(stderr, "uv_try_write error: %d %s\n", r, uv_strerror(r));
  ASSERT(r == UV_EPIPE || r == UV_ECONNABORTED || r == UV_ECONNRESET);
  ASSERT(client.write_queue_size == 0);
}


static void connect_cb(uv_connect_t* req, int status) {
  ASSERT(status == 0);
  connect_cb_called++;
}


static void connection_cb(uv_stream_t* tcp, int status) {
  ASSERT(status == 0);

  ASSERT(0 == uv_tcp_init(tcp->loop, &incoming));
  ASSERT(0 == uv_accept(tcp, (uv_stream_t*) &incoming));

  connection_cb_called++;
  uv_close((uv_handle_t*) &incoming, incoming_close_cb);
  uv_close((uv_handle_t*) tcp, close_cb);
}


static void start_server(void) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));

  ASSERT(0 == uv_tcp_init(uv_default_loop(), &server));
  ASSERT(0 == uv_tcp_bind(&server, (struct sockaddr*) &addr, 0));
  ASSERT(0 == uv_listen((uv_stream_t*) &server, 128, connection_cb));
}


TEST_IMPL(tcp_try_write_error) {
  uv_connect_t connect_req;
  struct sockaddr_in addr;

  start_server();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  ASSERT(0 == uv_tcp_init(uv_default_loop(), &client));
  ASSERT(0 == uv_tcp_connect(&connect_req,
                             &client,
                             (struct sockaddr*) &addr,
                             connect_cb));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  uv_close((uv_handle_t*) &client, close_cb);
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(close_cb_called == 3);
  ASSERT(connection_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
