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
static int do_accept_called = 0;
static int close_cb_called = 0;
static int connect_cb_called = 0;


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);

  free(handle);

  close_cb_called++;
}


static void do_accept(uv_timer_t* timer_handle, int status) {
  uv_tcp_t* server;
  uv_tcp_t* accepted_handle = (uv_tcp_t*)malloc(sizeof *accepted_handle);
  uint64_t tcpcnt;
  int r;

  ASSERT(timer_handle != NULL);
  ASSERT(status == 0);
  ASSERT(accepted_handle != NULL);

  r = uv_tcp_init(uv_default_loop(), accepted_handle);
  ASSERT(r == 0);

  /* Test to that uv_default_loop()->counters.tcp_init does not increase across the uv_accept. */
  tcpcnt = uv_default_loop()->counters.tcp_init;

  server = (uv_tcp_t*)timer_handle->data;
  r = uv_accept((uv_stream_t*)server, (uv_stream_t*)accepted_handle);
  ASSERT(r == 0);

  ASSERT(uv_default_loop()->counters.tcp_init == tcpcnt);

  do_accept_called++;

  /* Immediately close the accepted handle. */
  uv_close((uv_handle_t*)accepted_handle, close_cb);

  /* After accepting the two clients close the server handle */
  if (do_accept_called == 2) {
    uv_close((uv_handle_t*)server, close_cb);
  }

  /* Dispose the timer. */
  uv_close((uv_handle_t*)timer_handle, close_cb);
}


static void connection_cb(uv_stream_t* tcp, int status) {
  int r;
  uv_timer_t* timer_handle;

  ASSERT(status == 0);

  timer_handle = (uv_timer_t*)malloc(sizeof *timer_handle);
  ASSERT(timer_handle != NULL);

  /* Accept the client after 1 second */
  r = uv_timer_init(uv_default_loop(), timer_handle);
  ASSERT(r == 0);

  timer_handle->data = tcp;

  r = uv_timer_start(timer_handle, do_accept, 1000, 0);
  ASSERT(r == 0);

  connection_cb_called++;
}


static void start_server() {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);
  uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof *server);
  int r;

  ASSERT(server != NULL);

  r = uv_tcp_init(uv_default_loop(), server);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.tcp_init == 1);
  ASSERT(uv_default_loop()->counters.handle_init == 1);

  r = uv_tcp_bind(server, addr);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)server, 128, connection_cb);
  ASSERT(r == 0);
}


static void read_cb(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  /* The server will not send anything, it should close gracefully. */

  if (buf.base) {
    free(buf.base);
  }

  if (nread != -1) {
    ASSERT(nread == 0);
    ASSERT(uv_last_error(uv_default_loop()).code == UV_EAGAIN);
  } else {
    ASSERT(tcp != NULL);
    ASSERT(nread == -1);
    ASSERT(uv_last_error(uv_default_loop()).code == UV_EOF);
    uv_close((uv_handle_t*)tcp, close_cb);
  }
}


static void connect_cb(uv_connect_t* req, int status) {
  int r;

  ASSERT(req != NULL);
  ASSERT(status == 0);

  /* Not that the server will send anything, but otherwise we'll never know */
  /* when te server closes the connection. */
  r = uv_read_start((uv_stream_t*)(req->handle), alloc_cb, read_cb);
  ASSERT(r == 0);

  connect_cb_called++;

  free(req);
}


static void client_connect() {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof *client);
  uv_connect_t* connect_req = malloc(sizeof *connect_req);
  int r;

  ASSERT(client != NULL);
  ASSERT(connect_req != NULL);

  r = uv_tcp_init(uv_default_loop(), client);
  ASSERT(r == 0);

  r = uv_tcp_connect(connect_req, client, addr, connect_cb);
  ASSERT(r == 0);
}



TEST_IMPL(delayed_accept) {
  start_server();

  client_connect();
  client_connect();

  uv_run(uv_default_loop());

  ASSERT(connection_cb_called == 2);
  ASSERT(do_accept_called == 2);
  ASSERT(connect_cb_called == 2);
  ASSERT(close_cb_called == 7);

  return 0;
}
