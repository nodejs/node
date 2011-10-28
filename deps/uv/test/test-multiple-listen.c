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

static int connection_cb_called = 0;
static int close_cb_called = 0;
static int connect_cb_called = 0;
static uv_tcp_t server;
static uv_tcp_t client;


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void connection_cb(uv_stream_t* tcp, int status) {
  ASSERT(status == 0);
  uv_close((uv_handle_t*)&server, close_cb);
  connection_cb_called++;
}


static void start_server() {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);
  int r;

  r = uv_tcp_init(uv_default_loop(), &server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&server, addr);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&server, 128, connection_cb);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&server, 128, connection_cb);
  ASSERT(r == 0);
}


static void connect_cb(uv_connect_t* req, int status) {
  ASSERT(req != NULL);
  ASSERT(status == 0);
  free(req);
  uv_close((uv_handle_t*)&client, close_cb);
  connect_cb_called++;
}


static void client_connect() {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_connect_t* connect_req = malloc(sizeof *connect_req);
  int r;

  ASSERT(connect_req != NULL);

  r = uv_tcp_init(uv_default_loop(), &client);
  ASSERT(r == 0);

  r = uv_tcp_connect(connect_req, &client, addr, connect_cb);
  ASSERT(r == 0);
}



TEST_IMPL(multiple_listen) {
  start_server();

  client_connect();

  uv_run(uv_default_loop());

  ASSERT(connection_cb_called == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(close_cb_called == 2);

  return 0;
}
