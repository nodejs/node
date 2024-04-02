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

/*
 * This is a regression test for issue #1113 (calling uv_shutdown twice will
 * leave a ghost request in the system)
 */

#include "uv.h"
#include "task.h"

static uv_shutdown_t req1;
static uv_shutdown_t req2;

static int shutdown_cb_called = 0;

static void close_cb(uv_handle_t* handle) {

}

static void shutdown_cb(uv_shutdown_t* req, int status) {
  ASSERT(req == &req1);
  ASSERT(status == 0);
  shutdown_cb_called++;
  uv_close((uv_handle_t*) req->handle, close_cb);
}

static void connect_cb(uv_connect_t* req, int status) {
  int r;

  ASSERT(status == 0);

  r = uv_shutdown(&req1, req->handle, shutdown_cb);
  ASSERT(r == 0);
  r = uv_shutdown(&req2, req->handle, shutdown_cb);
  ASSERT(r != 0);

}

TEST_IMPL(shutdown_twice) {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int r;
  uv_tcp_t h;

  uv_connect_t connect_req;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  loop = uv_default_loop();

  r = uv_tcp_init(loop, &h);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &h,
                     (const struct sockaddr*) &addr,
                     connect_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(shutdown_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
