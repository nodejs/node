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

static uv_timer_t timer;
static uv_tcp_t tcp;
static uv_connect_t connect_req;
static uv_write_t write_req;
static uv_shutdown_t shutdown_req;
static uv_buf_t qbuf;
static int got_q;
static int got_eof;
static int called_connect_cb;
static int called_shutdown_cb;
static int called_tcp_close_cb;
static int called_timer_close_cb;
static int called_timer_cb;


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}


static void read_cb(uv_stream_t* t, ssize_t nread, uv_buf_t buf) {
  uv_err_t err = uv_last_error(uv_default_loop());

  ASSERT((uv_tcp_t*)t == &tcp);

  if (nread == 0) {
    ASSERT(err.code == UV_EAGAIN);
    free(buf.base);
    return;
  }

  if (!got_q) {
    ASSERT(nread == 1);
    ASSERT(!got_eof);
    ASSERT(buf.base[0] == 'Q');
    free(buf.base);
    got_q = 1;
    puts("got Q");
  } else {
    ASSERT(err.code == UV_EOF);
    if (buf.base) {
      free(buf.base);
    }
    got_eof = 1;
    puts("got EOF");
  }
}


static void shutdown_cb(uv_shutdown_t *req, int status) {
  ASSERT(req == &shutdown_req);

  ASSERT(called_connect_cb == 1);
  ASSERT(!got_eof);
  ASSERT(called_tcp_close_cb == 0);
  ASSERT(called_timer_close_cb == 0);
  ASSERT(called_timer_cb == 0);

  called_shutdown_cb++;
}


static void connect_cb(uv_connect_t *req, int status) {
  ASSERT(status == 0);
  ASSERT(req == &connect_req);

  /* Start reading from our connection so we can receive the EOF.  */
  uv_read_start((uv_stream_t*)&tcp, alloc_cb, read_cb);

  /*
   * Write the letter 'Q' to gracefully kill the echo-server. This will not
   * effect our connection.
   */
  uv_write(&write_req, (uv_stream_t*) &tcp, &qbuf, 1, NULL);

  /* Shutdown our end of the connection.  */
  uv_shutdown(&shutdown_req, (uv_stream_t*) &tcp, shutdown_cb);

  called_connect_cb++;
  ASSERT(called_shutdown_cb == 0);
}


static void tcp_close_cb(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*) &tcp);

  ASSERT(called_connect_cb == 1);
  ASSERT(got_q);
  ASSERT(got_eof);
  ASSERT(called_timer_cb == 1);

  called_tcp_close_cb++;
}


static void timer_close_cb(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*) &timer);
  called_timer_close_cb++;
}


static void timer_cb(uv_timer_t* handle, int status) {
  ASSERT(handle == &timer);
  uv_close((uv_handle_t*) handle, timer_close_cb);

  /*
   * The most important assert of the test: we have not received
   * tcp_close_cb yet.
   */
  ASSERT(called_tcp_close_cb == 0);
  uv_close((uv_handle_t*) &tcp, tcp_close_cb);

  called_timer_cb++;
}


/*
 * This test has a client which connects to the echo_server and immediately
 * issues a shutdown. The echo-server, in response, will also shutdown their
 * connection. We check, with a timer, that libuv is not automatically
 * calling uv_close when the client receives the EOF from echo-server.
 */
TEST_IMPL(shutdown_eof) {
  struct sockaddr_in server_addr;
  int r;

  qbuf.base = "Q";
  qbuf.len = 1;

  r = uv_timer_init(uv_default_loop(), &timer);
  ASSERT(r == 0);

  uv_timer_start(&timer, timer_cb, 100, 0);

  server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  r = uv_tcp_init(uv_default_loop(), &tcp);
  ASSERT(!r);

  r = uv_tcp_connect(&connect_req, &tcp, server_addr, connect_cb);
  ASSERT(!r);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(called_connect_cb == 1);
  ASSERT(called_shutdown_cb == 1);
  ASSERT(got_eof);
  ASSERT(got_q);
  ASSERT(called_tcp_close_cb == 1);
  ASSERT(called_timer_close_cb == 1);
  ASSERT(called_timer_cb == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

