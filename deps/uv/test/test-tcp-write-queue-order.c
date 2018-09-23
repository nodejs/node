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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uv.h"
#include "task.h"

#define REQ_COUNT 10000

static uv_timer_t timer;
static uv_tcp_t server;
static uv_tcp_t client;
static uv_tcp_t incoming;
static int connect_cb_called;
static int close_cb_called;
static int connection_cb_called;
static int write_callbacks;
static int write_cancelled_callbacks;
static int write_error_callbacks;

static uv_write_t write_requests[REQ_COUNT];


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

void timer_cb(uv_timer_t* handle) {
  uv_close((uv_handle_t*) &client, close_cb);
  uv_close((uv_handle_t*) &server, close_cb);
  uv_close((uv_handle_t*) &incoming, close_cb);
}

void write_cb(uv_write_t* req, int status) {
  if (status == 0)
    write_callbacks++;
  else if (status == UV_ECANCELED)
    write_cancelled_callbacks++;
  else
    write_error_callbacks++;
}

static void connect_cb(uv_connect_t* req, int status) {
  static char base[1024];
  int r;
  int i;
  uv_buf_t buf;

  ASSERT(status == 0);
  connect_cb_called++;

  buf = uv_buf_init(base, sizeof(base));

  for (i = 0; i < REQ_COUNT; i++) {
    r = uv_write(&write_requests[i],
                 req->handle,
                 &buf,
                 1,
                 write_cb);
    ASSERT(r == 0);
  }
}


static void connection_cb(uv_stream_t* tcp, int status) {
  ASSERT(status == 0);

  ASSERT(0 == uv_tcp_init(tcp->loop, &incoming));
  ASSERT(0 == uv_accept(tcp, (uv_stream_t*) &incoming));

  connection_cb_called++;
}


static void start_server(void) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));

  ASSERT(0 == uv_tcp_init(uv_default_loop(), &server));
  ASSERT(0 == uv_tcp_bind(&server, (struct sockaddr*) &addr, 0));
  ASSERT(0 == uv_listen((uv_stream_t*) &server, 128, connection_cb));
}


TEST_IMPL(tcp_write_queue_order) {
  uv_connect_t connect_req;
  struct sockaddr_in addr;

  start_server();

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  ASSERT(0 == uv_tcp_init(uv_default_loop(), &client));
  ASSERT(0 == uv_tcp_connect(&connect_req,
                             &client,
                             (struct sockaddr*) &addr,
                             connect_cb));

  ASSERT(0 == uv_timer_init(uv_default_loop(), &timer));
  ASSERT(0 == uv_timer_start(&timer, timer_cb, 100, 0));

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(connection_cb_called == 1);
  ASSERT(write_callbacks > 0);
  ASSERT(write_cancelled_callbacks > 0);
  ASSERT(write_callbacks +
         write_error_callbacks +
         write_cancelled_callbacks == REQ_COUNT);
  ASSERT(close_cb_called == 3);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
