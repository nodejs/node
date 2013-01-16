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


static int connect_cb_called;
static int close_cb_called;

static uv_connect_t connect_req;
static uv_timer_t timer;
static uv_tcp_t conn;

static void connect_cb(uv_connect_t* req, int status);
static void timer_cb(uv_timer_t* handle, int status);
static void close_cb(uv_handle_t* handle);


static void connect_cb(uv_connect_t* req, int status) {
  ASSERT(req == &connect_req);
  ASSERT(status == -1);
  connect_cb_called++;
}


static void timer_cb(uv_timer_t* handle, int status) {
  ASSERT(handle == &timer);
  uv_close((uv_handle_t*)&conn, close_cb);
  uv_close((uv_handle_t*)&timer, close_cb);
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*)&conn || handle == (uv_handle_t*)&timer);
  close_cb_called++;
}


/* Verify that connecting to an unreachable address or port doesn't hang
 * the event loop.
 */
TEST_IMPL(tcp_connect_timeout) {
  struct sockaddr_in addr;
  int r;

  addr = uv_ip4_addr("8.8.8.8", 9999);

  r = uv_timer_init(uv_default_loop(), &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 50, 0);
  ASSERT(r == 0);

  r = uv_tcp_init(uv_default_loop(), &conn);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req, &conn, addr, connect_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
